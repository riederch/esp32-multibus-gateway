#pragma once

#include <Arduino.h>
#include <time.h>
#include <vector>
#include "AppConfig.h"
#include "CapabilityRegistry.h"
#include "ChannelRegistry.h"
#include "ConfigStore.h"
#include "DeviceConfig.h"
#include "DeviceIdentity.h"
#include "LoRaWanIdentity.h"
#include "SecurityStore.h"
#include "components/Components.h"
#include "lorawan/FPort85Codec.h"
#include "history/HistorySettings.h"
#include "history/HistorySettingsStore.h"
#include "history/HistoryRetransmissionState.h"
#include "history/HistoryRetransmissionStateStore.h"
#include "history/PersistentHistoryStore.h"
#include "lorawan/FPort85History.h"
#include "lorawan/FPort85ReportScheduler.h"
#include "lorawan/FPort85ReportSettingsStore.h"
#include "modbus/ModbusChannelStore.h"
#include "modbus/ModbusMasterSettingsStore.h"
#include "modbus/Rs485SettingsStore.h"
#include "services/BoardService.h"
#include "services/NetworkService.h"
#include "services/WebService.h"
#include "time/TimeSettings.h"
#include "time/TimeSettingsStore.h"

namespace multibus {

class Application {
public:
    bool begin() {
        if (!configStore_.begin()) {
            Serial.println("Failed to open configuration store.");
            return false;
        }
        if (!modbusChannelStore_.begin()) {
            Serial.println("Failed to open Modbus channel store.");
            return false;
        }
        if (!rs485SettingsStore_.begin()) {
            Serial.println("Failed to open RS485 settings store.");
            return false;
        }
        if (!modbusMasterSettingsStore_.begin()) {
            Serial.println("Failed to open Modbus master settings store.");
            return false;
        }
        if (!reportSettingsStore_.begin()) {
            Serial.println("Failed to open FPort 85 report settings store.");
            return false;
        }
        if (!timeSettingsStore_.begin()) {
            Serial.println("Failed to open time settings store.");
            return false;
        }
        if (!historySettingsStore_.begin()) {
            Serial.println("Failed to open history settings store.");
            return false;
        }
        if (!historyStore_.begin()) {
            Serial.println("Failed to open persistent history store.");
            return false;
        }
        if (!historyRetransmissionStore_.begin()) {
            Serial.println("Failed to open history retransmission state store.");
            return false;
        }

        bool configChanged = false;
        if (!configStore_.load(config_)) {
            config_ = DeviceConfig{};
            config_.network.hostname = defaultHostname();
            config_.network.friendlyName = "MultiBus Gateway";
            configChanged = true;
        }

        if (ensureLoRaWanConfig(config_.lorawan)) {
            configChanged = true;
        }
        if (configStore_.requiresSave()) {
            configChanged = true;
        }
        if (configChanged && !configStore_.save(config_)) {
            Serial.println("Failed to initialize or migrate configuration.");
            return false;
        }

        if (!security_.begin()) {
            Serial.println("Failed to initialize security store.");
            return false;
        }

        if (!board_.begin(configStore_, security_, &Application::factoryResetThunk, this)) {
            Serial.println("Failed to initialize board service.");
            return false;
        }

        const auto validation = validateConfig(config_.components);
        if (validation != ConfigValidationResult::Ok) {
            Serial.printf("Configuration rejected: %s\n", toString(validation));
            return false;
        }
        if (!validateLoRaWanConfig(config_.lorawan)) {
            Serial.println("LoRaWAN configuration rejected.");
            return false;
        }

        configureComponents();
        lora_.setDownlinkHandler(&Application::downlinkThunk, this);
        registerCoreCapabilities();

        if (!loadRs485Settings()) {
            Serial.println("Failed to load persisted RS485 settings.");
            return false;
        }
        if (!loadModbusMasterSettings()) {
            Serial.println("Failed to load persisted Modbus master settings.");
            return false;
        }
        if (!loadReportSettings()) {
            Serial.println("Failed to load persisted FPort 85 report settings.");
            return false;
        }
        if (!loadTimeSettings()) {
            Serial.println("Failed to load persisted time settings.");
            return false;
        }
        if (!loadHistorySettings()) {
            Serial.println("Failed to load persisted history settings.");
            return false;
        }
        if (!historyStore_.load(historyRing_)) {
            Serial.println("Failed to load persistent history.");
            return false;
        }
        if (!historyRetransmissionStore_.load(historyRetransmissionState_)) {
            Serial.println("Failed to load history retransmission state.");
            return false;
        }
        if (!loadModbusChannels()) {
            Serial.println("Failed to load persisted Modbus channels.");
            return false;
        }

        if (!beginComponent(victron_)) return false;
        if (!beginComponent(lora_)) return false;
        if (!beginComponent(modbus_)) return false;
        if (!beginComponent(gnss_)) return false;

        if (!network_.begin(config_, security_)) {
            Serial.println("Failed to initialize Wi-Fi networking.");
            return false;
        }

        capabilities_.add("board.user-button");
        capabilities_.add("board.status-led");
        capabilities_.add("board.factory-reset");
        capabilities_.add("network.wifi");
        capabilities_.add("network.webui");
        if (network_.apActive()) capabilities_.add("network.ap");
        if (network_.clientConnected()) capabilities_.add("network.client");

        if (!web_.begin(config_, configStore_, security_, network_)) {
            Serial.println("Failed to initialize Web UI.");
            return false;
        }

        printStatus();
        return true;
    }

    void loop() {
        board_.loop();
        network_.loop();
        web_.loop();
        victron_.loop();
        lora_.loop();
        processHistoryNetworkState();
        modbus_.loop();
        captureModbusCompatibilitySample();
        processHistoryStorage();
        const bool retransmissionActive = processHistoryRetransmission();
        if (!retransmissionActive) processModbusCompatibilityReport();
        gnss_.loop();
        if (rebootRequested_) {
            delay(20);
            ESP.restart();
        }
    }

    const DeviceConfig& config() const { return config_; }
    const CapabilityRegistry& capabilities() const { return capabilities_; }
    const ChannelRegistry& channels() const { return channels_; }
    const SecurityStore& security() const { return security_; }
    const NetworkService& network() const { return network_; }
    const BoardService& board() const { return board_; }

private:
    enum class ParsedCommandKind : uint8_t {
        ReportInterval,
        BasicControl,
        PeriodicReportEnquiry,
        UtcTimezone,
        LnsTimeSync,
        DstSettings,
        DataStorage,
        DataRetransmission,
        RetransmissionInterval,
        ModbusChannel,
        Rs485Settings,
        ModbusMasterSettings,
        Rs485SettingsEnquiry,
    };

    struct ParsedCommand {
        ParsedCommandKind kind = ParsedCommandKind::ModbusChannel;
        lorawan::ReportIntervalCommand reportInterval;
        lorawan::BasicControlCommand basicControl = lorawan::BasicControlCommand::Rejoin;
        lorawan::PeriodicReportEnquiryCommand periodicReportEnquiry;
        lorawan::UtcTimezoneCommand utcTimezone;
        lorawan::LnsTimeSyncCommand lnsTimeSync;
        lorawan::DstSettingsCommand dstSettings;
        lorawan::HistoryToggleCommand historyToggle;
        lorawan::RetransmissionIntervalCommand retransmissionInterval;
        lorawan::ModbusChannelCommand modbusChannel;
        lorawan::Rs485SettingsCommand rs485Settings;
        lorawan::ModbusMasterSettingsCommand modbusMasterSettings;
        lorawan::Rs485SettingsEnquiryCommand rs485SettingsEnquiry;
    };

    struct HistorySample {
        bool present = false;
        modbus::ChannelConfig channel;
        bool success = false;
        uint8_t valueCount = 0;
        modbus::DecodedScalar values[2];
    };

    static void factoryResetThunk(void* context) {
        if (context != nullptr) static_cast<Application*>(context)->clearSubsystemStores();
    }

    void clearSubsystemStores() {
        modbusChannelStore_.clear();
        rs485SettingsStore_.clear();
        modbusMasterSettingsStore_.clear();
        reportSettingsStore_.clear();
        timeSettingsStore_.clear();
        historySettingsStore_.clear();
        historyStore_.clear();
        historyRetransmissionStore_.clear();
    }

    static bool downlinkThunk(void* context, uint8_t fport, const uint8_t* payload, size_t length) {
        if (context == nullptr) return false;
        return static_cast<Application*>(context)->handleLoRaDownlink(fport, payload, length);
    }

    void captureModbusCompatibilitySample() {
        ModbusComponent::PollCompletion completion;
        if (!modbus_.takeCompletedPoll(completion)) return;

        reportScheduler_.recordPoll(
            completion.channel,
            completion.success,
            completion.values,
            completion.valueCount);

        if (completion.channel.slot < modbus::kCompatibilitySlotCount) {
            HistorySample& sample = historySamples_[completion.channel.slot];
            sample.present = true;
            sample.channel = completion.channel;
            sample.success = completion.success;
            sample.valueCount = completion.success ? completion.valueCount : 0;
            if (completion.success && completion.valueCount > 0) {
                sample.values[0] = completion.values[0];
                if (completion.valueCount > 1) sample.values[1] = completion.values[1];
            }
        }
    }

    void processHistoryNetworkState() {
        const bool connected = lora_.connected();

        if (!historyNetworkStateInitialized_) {
            historyNetworkStateInitialized_ = true;
            historyWasConnected_ = connected;
            if (connected && historyRetransmissionState_.pending) {
                retransmissionCursorInitialized_ = false;
                nextHistoryRetransmissionAtMs_ = millis();
            }
            return;
        }

        if (historyWasConnected_ && !connected &&
            historySettings_.storageEnabled &&
            historySettings_.retransmissionEnabled) {
            const time_t unixNow = ::time(nullptr);
            if (unixNow > 0) {
                history::RetransmissionState updated;
                updated.pending = true;
                updated.lostAtUnix = static_cast<uint32_t>(unixNow);
                if (historyRetransmissionStore_.save(updated)) {
                    historyRetransmissionState_ = updated;
                    retransmissionCursorInitialized_ = false;
                } else {
                    ++historyRetransmissionStateFailures_;
                }
            }
        } else if (!historyWasConnected_ && connected && historyRetransmissionState_.pending) {
            retransmissionCursorInitialized_ = false;
            nextHistoryRetransmissionAtMs_ = millis();
        }

        historyWasConnected_ = connected;
    }

    bool processHistoryRetransmission() {
        if (!historySettings_.storageEnabled ||
            !historySettings_.retransmissionEnabled ||
            !historyRetransmissionState_.pending ||
            !lora_.connected()) {
            return false;
        }

        const uint32_t now = millis();
        if (static_cast<int32_t>(now - nextHistoryRetransmissionAtMs_) < 0) return false;

        if (!retransmissionCursorInitialized_) {
            retransmissionSnapshot_ = historyRing_;
            retransmissionCursor_ = 0;
            while (retransmissionCursor_ < retransmissionSnapshot_.size()) {
                const lorawan::HistoricalRecord* record =
                    retransmissionSnapshot_.oldest(retransmissionCursor_);
                if (record != nullptr && record->timestamp() >= historyRetransmissionState_.lostAtUnix) break;
                ++retransmissionCursor_;
            }
            retransmissionCursorInitialized_ = true;
        }

        if (retransmissionCursor_ >= retransmissionSnapshot_.size()) {
            history::RetransmissionState cleared;
            if (!historyRetransmissionStore_.save(cleared)) {
                ++historyRetransmissionStateFailures_;
                nextHistoryRetransmissionAtMs_ =
                    now + static_cast<uint32_t>(historySettings_.retransmissionIntervalSeconds) * 1000UL;
                return true;
            }
            historyRetransmissionState_ = cleared;
            retransmissionCursorInitialized_ = false;
            return false;
        }

        uint8_t payload[lorawan::kCompatibilityReportPayloadLimit] = {0};
        size_t written = 0;
        size_t recordsAdded = 0;
        while (retransmissionCursor_ + recordsAdded < retransmissionSnapshot_.size()) {
            const lorawan::HistoricalRecord* record =
                retransmissionSnapshot_.oldest(retransmissionCursor_ + recordsAdded);
            if (record == nullptr) break;
            if (written + lorawan::kHistoricalModbusRecordLength >
                lorawan::kCompatibilityReportPayloadLimit) {
                break;
            }
            memcpy(payload + written, record->payload, lorawan::kHistoricalModbusRecordLength);
            written += lorawan::kHistoricalModbusRecordLength;
            ++recordsAdded;
        }

        if (recordsAdded == 0 || written == 0) {
            ++historyRetransmissionEncodeFailures_;
            nextHistoryRetransmissionAtMs_ =
                now + static_cast<uint32_t>(historySettings_.retransmissionIntervalSeconds) * 1000UL;
            return true;
        }

        TransportEnvelope envelope;
        envelope.endpoint = lorawan::kCompatibilityFPort;
        envelope.payload = payload;
        envelope.length = written;
        envelope.confirmed = false;

        if (!lora_.send(envelope)) {
            ++historyRetransmissionSendFailures_;
            nextHistoryRetransmissionAtMs_ =
                now + static_cast<uint32_t>(historySettings_.retransmissionIntervalSeconds) * 1000UL;
            return true;
        }

        retransmissionCursor_ += recordsAdded;
        ++historyRetransmissionPacketsSent_;
        nextHistoryRetransmissionAtMs_ =
            now + static_cast<uint32_t>(historySettings_.retransmissionIntervalSeconds) * 1000UL;

        if (retransmissionCursor_ >= retransmissionSnapshot_.size()) {
            history::RetransmissionState cleared;
            if (historyRetransmissionStore_.save(cleared)) {
                historyRetransmissionState_ = cleared;
                retransmissionCursorInitialized_ = false;
            } else {
                ++historyRetransmissionStateFailures_;
            }
        }

        return true;
    }

    void processHistoryStorage() {
        if (!historySettings_.storageEnabled) return;

        const uint32_t now = millis();
        if (static_cast<int32_t>(now - nextHistorySnapshotAtMs_) < 0) return;
        nextHistorySnapshotAtMs_ = now + static_cast<uint32_t>(reportScheduler_.settings().seconds) * 1000UL;

        const time_t unixNow = ::time(nullptr);
        if (unixNow <= 0) {
            ++historySnapshotsSkippedNoTime_;
            return;
        }

        history::PersistentHistoryStore::Ring updated = historyRing_;
        bool added = false;
        for (uint8_t slot = 0; slot < modbus::kCompatibilitySlotCount; ++slot) {
            const HistorySample& sample = historySamples_[slot];
            if (!sample.present) continue;

            lorawan::HistoricalRecord record;
            size_t written = 0;
            if (lorawan::FPort85History::encodeModbusRecord(
                    static_cast<uint32_t>(unixNow),
                    sample.channel,
                    sample.success,
                    sample.success ? sample.values : nullptr,
                    sample.valueCount,
                    record.payload,
                    sizeof(record.payload),
                    written) != lorawan::EncodeStatus::Ok ||
                written != lorawan::kHistoricalModbusRecordLength) {
                ++historyEncodeFailures_;
                continue;
            }

            updated.push(record);
            added = true;
        }

        if (!added) return;
        if (!historyStore_.save(updated)) {
            ++historyPersistFailures_;
            return;
        }

        historyRing_ = updated;
        ++historySnapshotsStored_;
    }

    void processModbusCompatibilityReport() {
        if (config_.components.lora != LoRaMode::LoRaWAN || !lora_.active()) return;

        uint8_t payload[lorawan::kCompatibilityReportPayloadLimit] = {0};
        size_t written = 0;
        const uint32_t now = millis();
        const lorawan::ReportBuildStatus status = reportScheduler_.preparePacket(
            now,
            payload,
            sizeof(payload),
            written);

        switch (status) {
            case lorawan::ReportBuildStatus::NotDue:
            case lorawan::ReportBuildStatus::EmptyReport:
                return;
            case lorawan::ReportBuildStatus::EncodeError:
            case lorawan::ReportBuildStatus::BufferTooSmall:
                ++compatibilityUplinkEncodeFailures_;
                reportScheduler_.abortReport();
                return;
            case lorawan::ReportBuildStatus::PacketReady:
                break;
        }

        if (written == 0) {
            ++compatibilityUplinkEncodeFailures_;
            reportScheduler_.abortReport();
            return;
        }

        ++compatibilityUplinksBuilt_;
        TransportEnvelope envelope;
        envelope.endpoint = lorawan::kCompatibilityFPort;
        envelope.payload = payload;
        envelope.length = written;
        envelope.confirmed = false;

        if (lora_.send(envelope)) {
            reportScheduler_.markPacketSent(now);
        } else {
            ++compatibilityUplinkSendFailures_;
            reportScheduler_.markPacketFailed(now);
        }
    }

    bool handleLoRaDownlink(uint8_t fport, const uint8_t* payload, size_t length) {
        if (fport != lorawan::kCompatibilityFPort || payload == nullptr || length == 0) return false;

        std::vector<ParsedCommand> commands;
        size_t offset = 0;
        while (offset < length) {
            lorawan::CommandHeader header;
            if (!lorawan::FPort85Codec::readHeader(payload + offset, length - offset, header)) return false;

            ParsedCommand parsed;
            size_t consumed = 0;

            if (header.channelId == lorawan::FPort85Codec::kSystemChannel &&
                (header.type == lorawan::FPort85Codec::kRejoinType ||
                 header.type == lorawan::FPort85Codec::kRebootType)) {
                parsed.kind = ParsedCommandKind::BasicControl;
                if (lorawan::FPort85Codec::decodeBasicControlCommand(
                        payload + offset, length - offset, parsed.basicControl, consumed) != lorawan::DecodeStatus::Ok ||
                    consumed == 0) {
                    return false;
                }
            } else if (header.channelId == lorawan::FPort85Codec::kSystemChannel &&
                       header.type == lorawan::FPort85Codec::kUtcTimezoneType) {
                parsed.kind = ParsedCommandKind::UtcTimezone;
                if (lorawan::FPort85Codec::decodeUtcTimezoneCommand(
                        payload + offset, length - offset, parsed.utcTimezone, consumed) != lorawan::DecodeStatus::Ok ||
                    consumed == 0) {
                    return false;
                }
            } else if (header.channelId == lorawan::FPort85Codec::kSystemChannel &&
                       header.type == lorawan::FPort85Codec::kLnsTimeSyncType) {
                parsed.kind = ParsedCommandKind::LnsTimeSync;
                if (lorawan::FPort85Codec::decodeLnsTimeSyncCommand(
                        payload + offset, length - offset, parsed.lnsTimeSync, consumed) != lorawan::DecodeStatus::Ok ||
                    consumed == 0) {
                    return false;
                }
            } else if (header.channelId == lorawan::FPort85Codec::kModbusChannel &&
                       header.type == lorawan::FPort85Codec::kDstSettingsType) {
                parsed.kind = ParsedCommandKind::DstSettings;
                if (lorawan::FPort85Codec::decodeDstSettingsCommand(
                        payload + offset, length - offset, parsed.dstSettings, consumed) != lorawan::DecodeStatus::Ok ||
                    consumed == 0) {
                    return false;
                }
            } else if (header.channelId == lorawan::FPort85Codec::kSystemChannel &&
                       header.type == lorawan::FPort85Codec::kDataStorageType) {
                parsed.kind = ParsedCommandKind::DataStorage;
                if (lorawan::FPort85Codec::decodeHistoryToggleCommand(
                        payload + offset, length - offset,
                        lorawan::FPort85Codec::kDataStorageType,
                        parsed.historyToggle, consumed) != lorawan::DecodeStatus::Ok ||
                    consumed == 0) {
                    return false;
                }
            } else if (header.channelId == lorawan::FPort85Codec::kSystemChannel &&
                       header.type == lorawan::FPort85Codec::kDataRetransmissionType) {
                parsed.kind = ParsedCommandKind::DataRetransmission;
                if (lorawan::FPort85Codec::decodeHistoryToggleCommand(
                        payload + offset, length - offset,
                        lorawan::FPort85Codec::kDataRetransmissionType,
                        parsed.historyToggle, consumed) != lorawan::DecodeStatus::Ok ||
                    consumed == 0) {
                    return false;
                }
            } else if (header.channelId == lorawan::FPort85Codec::kModbusChannel &&
                       header.type == lorawan::FPort85Codec::kRetransmissionIntervalType) {
                parsed.kind = ParsedCommandKind::RetransmissionInterval;
                if (lorawan::FPort85Codec::decodeRetransmissionIntervalCommand(
                        payload + offset, length - offset,
                        parsed.retransmissionInterval, consumed) != lorawan::DecodeStatus::Ok ||
                    consumed == 0) {
                    return false;
                }
            } else if (header.channelId == lorawan::FPort85Codec::kSystemChannel &&
                       header.type == lorawan::FPort85Codec::kPeriodicReportEnquiryType) {
                parsed.kind = ParsedCommandKind::PeriodicReportEnquiry;
                if (lorawan::FPort85Codec::decodePeriodicReportEnquiryCommand(
                        payload + offset, length - offset, parsed.periodicReportEnquiry, consumed) != lorawan::DecodeStatus::Ok ||
                    consumed == 0) {
                    return false;
                }
            } else if (header.channelId == lorawan::FPort85Codec::kSystemChannel &&
                header.type == lorawan::FPort85Codec::kReportIntervalType) {
                parsed.kind = ParsedCommandKind::ReportInterval;
                if (!lorawan::decodeReportIntervalCommand(
                        payload + offset, length - offset, parsed.reportInterval, consumed) || consumed == 0) {
                    return false;
                }
            } else if (header.channelId == lorawan::FPort85Codec::kSystemChannel &&
                       header.type == lorawan::FPort85Codec::kModbusChannelConfigType) {
                parsed.kind = ParsedCommandKind::ModbusChannel;
                if (lorawan::FPort85Codec::decodeModbusChannelCommand(
                        payload + offset, length - offset, parsed.modbusChannel, consumed) != lorawan::DecodeStatus::Ok ||
                    consumed == 0) {
                    return false;
                }
            } else if (header.channelId == lorawan::FPort85Codec::kModbusChannel &&
                       header.type == lorawan::FPort85Codec::kRs485ConfigType) {
                parsed.kind = ParsedCommandKind::Rs485Settings;
                if (lorawan::FPort85Codec::decodeRs485SettingsCommand(
                        payload + offset, length - offset, parsed.rs485Settings, consumed) != lorawan::DecodeStatus::Ok ||
                    consumed == 0 ||
                    !modbus::esp32SupportsRs485SerialSettings(parsed.rs485Settings.settings)) {
                    return false;
                }
            } else if (header.channelId == lorawan::FPort85Codec::kModbusChannel &&
                       header.type == lorawan::FPort85Codec::kModbusGlobalConfigType) {
                parsed.kind = ParsedCommandKind::ModbusMasterSettings;
                if (lorawan::FPort85Codec::decodeModbusMasterSettingsCommand(
                        payload + offset, length - offset, parsed.modbusMasterSettings, consumed) != lorawan::DecodeStatus::Ok ||
                    consumed == 0 ||
                    !modbus::runtimeSupportsModbusMasterSettings(parsed.modbusMasterSettings.settings)) {
                    return false;
                }
            } else if (header.channelId == lorawan::FPort85Codec::kModbusChannel &&
                       header.type == lorawan::FPort85Codec::kRs485SettingsEnquiryType) {
                parsed.kind = ParsedCommandKind::Rs485SettingsEnquiry;
                if (lorawan::FPort85Codec::decodeRs485SettingsEnquiryCommand(
                        payload + offset, length - offset, parsed.rs485SettingsEnquiry, consumed) != lorawan::DecodeStatus::Ok ||
                    consumed == 0) {
                    return false;
                }
            } else {
                return false;
            }

            commands.push_back(parsed);
            offset += consumed;
        }

        for (const auto& command : commands) {
            switch (command.kind) {
                case ParsedCommandKind::BasicControl:
                    if (command.basicControl == lorawan::BasicControlCommand::Rejoin) {
                        if (!lora_.requestRejoin()) return false;
                    } else if (command.basicControl == lorawan::BasicControlCommand::Reboot) {
                        rebootRequested_ = true;
                    }
                    break;
                case ParsedCommandKind::PeriodicReportEnquiry:
                    reportScheduler_.requestImmediateReport();
                    break;
                case ParsedCommandKind::UtcTimezone:
                    if (!applyUtcTimezoneCommand(command.utcTimezone)) return false;
                    break;
                case ParsedCommandKind::LnsTimeSync:
                    if (!lora_.requestNetworkTimeSync()) return false;
                    break;
                case ParsedCommandKind::DstSettings:
                    if (!applyDstSettingsCommand(command.dstSettings)) return false;
                    break;
                case ParsedCommandKind::DataStorage:
                    if (!applyHistoryStorageCommand(command.historyToggle)) return false;
                    break;
                case ParsedCommandKind::DataRetransmission:
                    if (!applyHistoryRetransmissionCommand(command.historyToggle)) return false;
                    break;
                case ParsedCommandKind::RetransmissionInterval:
                    if (!applyRetransmissionIntervalCommand(command.retransmissionInterval)) return false;
                    break;
                case ParsedCommandKind::ReportInterval:
                    if (!applyReportIntervalCommand(command.reportInterval)) return false;
                    break;
                case ParsedCommandKind::ModbusChannel:
                    if (!applyModbusChannelCommand(command.modbusChannel)) return false;
                    break;
                case ParsedCommandKind::Rs485Settings:
                    if (!applyRs485SettingsCommand(command.rs485Settings)) return false;
                    break;
                case ParsedCommandKind::ModbusMasterSettings:
                    if (!applyModbusMasterSettingsCommand(command.modbusMasterSettings)) return false;
                    break;
                case ParsedCommandKind::Rs485SettingsEnquiry:
                    if (!replyToRs485SettingsEnquiry(command.rs485SettingsEnquiry)) return false;
                    break;
            }
        }
        return true;
    }

    bool replyToRs485SettingsEnquiry(const lorawan::Rs485SettingsEnquiryCommand& command) {
        uint8_t payload[16] = {0};
        size_t written = 0;
        if (lorawan::FPort85Codec::encodeRs485SettingsEnquiryReply(
                command,
                modbus_.rs485SerialSettings(),
                modbus_.modbusMasterSettings(),
                payload,
                sizeof(payload),
                written) != lorawan::EncodeStatus::Ok ||
            written == 0) {
            return false;
        }

        TransportEnvelope envelope;
        envelope.endpoint = lorawan::kCompatibilityFPort;
        envelope.payload = payload;
        envelope.length = written;
        envelope.confirmed = false;
        return lora_.send(envelope);
    }

    bool applyReportIntervalCommand(const lorawan::ReportIntervalCommand& command) {
        if (!lorawan::validReportIntervalSettings(command.settings)) return false;

        const lorawan::ReportIntervalSettings previous = reportScheduler_.settings();
        if (!reportSettingsStore_.save(command.settings)) return false;

        const uint32_t now = millis();
        if (reportScheduler_.applySettings(command.settings, now)) {
            nextHistorySnapshotAtMs_ =
                now + static_cast<uint32_t>(command.settings.seconds) * 1000UL;
            return true;
        }

        reportSettingsStore_.save(previous);
        return false;
    }

    bool applyRs485SettingsCommand(const lorawan::Rs485SettingsCommand& command) {
        if (!modbus::esp32SupportsRs485SerialSettings(command.settings)) return false;

        const modbus::Rs485SerialSettings previous = modbus_.rs485SerialSettings();
        if (!modbus_.applyRs485SerialSettings(command.settings)) return false;
        if (rs485SettingsStore_.save(command.settings)) return true;

        modbus_.applyRs485SerialSettings(previous);
        return false;
    }

    bool applyModbusMasterSettingsCommand(const lorawan::ModbusMasterSettingsCommand& command) {
        if (!modbus::runtimeSupportsModbusMasterSettings(command.settings)) return false;

        const modbus::ModbusMasterSettings previous = modbus_.modbusMasterSettings();
        if (!modbus_.applyModbusMasterSettings(command.settings)) return false;
        if (modbusMasterSettingsStore_.save(command.settings)) return true;

        modbus_.applyModbusMasterSettings(previous);
        return false;
    }

    bool applyModbusChannelCommand(const lorawan::ModbusChannelCommand& command) {
        switch (command.operation) {
            case lorawan::ModbusChannelOperation::Upsert:
                if (!modbusChannelStore_.save(command.channel)) return false;
                if (!modbus_.upsertChannel(command.channel)) return false;
                reportScheduler_.clearSlot(command.channel.slot);
                return bindCompatibilityChannel(command.channel);

            case lorawan::ModbusChannelOperation::Remove:
                if (!modbusChannelStore_.remove(command.channel.slot)) return false;
                modbus_.removeChannel(command.channel.slot);
                reportScheduler_.clearSlot(command.channel.slot);
                channels_.removeCompatibilitySlot(command.channel.slot);
                return true;

            case lorawan::ModbusChannelOperation::SetName: {
                const modbus::ChannelConfig* current = modbus_.channelForSlot(command.channel.slot);
                if (current == nullptr) return false;
                modbus::ChannelConfig updated = *current;
                const size_t nameLength = strnlen(command.channel.name, modbus::kMaxChannelNameLength);
                if (!modbus::setChannelName(updated, command.channel.name, nameLength)) return false;
                if (!modbusChannelStore_.save(updated)) return false;
                return modbus_.upsertChannel(updated);
            }
        }
        return false;
    }

    bool applyHistoryStorageCommand(const lorawan::HistoryToggleCommand& command) {
        history::Settings updated = historySettings_;
        updated.storageEnabled = command.enabled;
        if (!historySettingsStore_.save(updated)) return false;
        historySettings_ = updated;

        if (!command.enabled && historyRetransmissionState_.pending) {
            history::RetransmissionState cleared;
            if (!historyRetransmissionStore_.save(cleared)) return false;
            historyRetransmissionState_ = cleared;
            retransmissionCursorInitialized_ = false;
        }
        return true;
    }

    bool applyHistoryRetransmissionCommand(const lorawan::HistoryToggleCommand& command) {
        history::Settings updated = historySettings_;
        updated.retransmissionEnabled = command.enabled;
        if (!historySettingsStore_.save(updated)) return false;
        historySettings_ = updated;

        if (!command.enabled && historyRetransmissionState_.pending) {
            history::RetransmissionState cleared;
            if (!historyRetransmissionStore_.save(cleared)) return false;
            historyRetransmissionState_ = cleared;
            retransmissionCursorInitialized_ = false;
        }
        return true;
    }

    bool applyRetransmissionIntervalCommand(const lorawan::RetransmissionIntervalCommand& command) {
        history::Settings updated = historySettings_;
        updated.retransmissionIntervalSeconds = command.seconds;
        if (!history::validSettings(updated) || !historySettingsStore_.save(updated)) return false;
        historySettings_ = updated;
        return true;
    }

    bool loadHistorySettings() {
        return historySettingsStore_.load(historySettings_);
    }

    bool applyDstSettingsCommand(const lorawan::DstSettingsCommand& command) {
        time::DstSettings dst;
        dst.enabled = command.enabled;
        dst.biasMinutes = command.biasMinutes;
        dst.start.month = command.startMonth;
        dst.start.week = command.startWeek;
        dst.start.weekday = command.startWeekday;
        dst.start.minuteOfDay = command.startMinuteOfDay;
        dst.end.month = command.endMonth;
        dst.end.week = command.endWeek;
        dst.end.weekday = command.endWeekday;
        dst.end.minuteOfDay = command.endMinuteOfDay;
        if (!time::validDstSettings(dst)) return false;

        time::Settings updated = timeSettings_;
        updated.dst = dst;
        if (!timeSettingsStore_.save(updated)) return false;
        timeSettings_ = updated;
        return true;
    }

    bool applyUtcTimezoneCommand(const lorawan::UtcTimezoneCommand& command) {
        if (!time::validUtcOffsetMinutes(command.offsetMinutes)) return false;

        time::Settings updated = timeSettings_;
        updated.utcOffsetMinutes = command.offsetMinutes;
        if (!timeSettingsStore_.save(updated)) return false;
        timeSettings_ = updated;
        return true;
    }

    bool loadTimeSettings() {
        return timeSettingsStore_.load(timeSettings_);
    }

    bool loadRs485Settings() {
        modbus::Rs485SerialSettings settings;
        if (!rs485SettingsStore_.load(settings)) return false;
        return modbus_.applyRs485SerialSettings(settings);
    }

    bool loadModbusMasterSettings() {
        modbus::ModbusMasterSettings settings;
        if (!modbusMasterSettingsStore_.load(settings)) return false;
        return modbus_.applyModbusMasterSettings(settings);
    }

    bool loadReportSettings() {
        lorawan::ReportIntervalSettings settings;
        if (!reportSettingsStore_.load(settings)) return false;
        const uint32_t now = millis();
        reportScheduler_.begin(settings, now);
        nextHistorySnapshotAtMs_ =
            now + static_cast<uint32_t>(settings.seconds) * 1000UL;
        return true;
    }

    bool loadModbusChannels() {
        std::vector<modbus::ChannelConfig> stored;
        if (!modbusChannelStore_.load(stored)) return false;
        for (const auto& channel : stored) {
            if (!modbus_.upsertChannel(channel) || !bindCompatibilityChannel(channel)) return false;
        }
        return true;
    }

    bool bindCompatibilityChannel(const modbus::ChannelConfig& channel) {
        ChannelBinding binding;
        binding.channelId = static_cast<uint16_t>(channel.slot) + 1U;
        binding.sourceId = modbus_.sourceId();
        binding.pointId = ModbusComponent::pointIdForSlot(channel.slot);
        binding.enabled = true;
        binding.writable = false;
        binding.compatibilityMapped = true;
        binding.compatibilitySlot = channel.slot;
        return channels_.upsert(binding);
    }

    void configureComponents() {
        victron_.setMode(config_.components.victron);
        lora_.setMode(config_.components.lora);
        lora_.setProvisioning(config_.lorawan, devEui());
        modbus_.setMode(config_.components.modbus);
        gnss_.setMode(config_.components.gnss);
    }

    void registerCoreCapabilities() {
        capabilities_.clear();
        capabilities_.add("core.config");
        capabilities_.add("core.capabilities");
        capabilities_.add("core.data-sources");
        capabilities_.add("core.transports");
        capabilities_.add("core.channels");
        capabilities_.add("core.events");
        capabilities_.add("core.commands");
        capabilities_.add("core.rules");
        capabilities_.add("core.history");
        capabilities_.add("core.security");
        capabilities_.add("core.backup-restore");
    }

    bool beginComponent(Component& component) {
        if (!component.begin(capabilities_)) {
            Serial.printf("Component failed to start: %s\n", component.name());
            return false;
        }
        return true;
    }

    void printStatus() const {
        Serial.println("MultiBus component configuration:");
        Serial.printf("  V: %s\n", toString(config_.components.victron));
        Serial.printf("  L: %s\n", toString(config_.components.lora));
        Serial.printf("  M: %s\n", toString(config_.components.modbus));
        Serial.printf("  G: %s\n", toString(config_.components.gnss));
        Serial.printf("  DevEUI: %s\n", devEui().c_str());
        Serial.printf("  JoinEUI: %s\n", config_.lorawan.joinEui.c_str());
        Serial.printf("  LoRaWAN provisioned: %s\n", lora_.provisioned() ? "yes" : "no");
        Serial.printf("  Compatibility channels: %u\n", static_cast<unsigned>(channels_.size()));
        Serial.printf("  Compatibility report interval: %u s\n",
                      static_cast<unsigned>(reportScheduler_.settings().seconds));
        Serial.printf("  UTC offset: %+d min\n", static_cast<int>(timeSettings_.utcOffsetMinutes));
        Serial.printf("  DST: %s, bias=%u min\n",
                      timeSettings_.dst.enabled ? "enabled" : "disabled",
                      static_cast<unsigned>(timeSettings_.dst.biasMinutes));
        Serial.printf("  History: storage=%s, retransmission=%s, interval=%u s, records=%u/%u, pending=%s\n",
                      historySettings_.storageEnabled ? "enabled" : "disabled",
                      historySettings_.retransmissionEnabled ? "enabled" : "disabled",
                      static_cast<unsigned>(historySettings_.retransmissionIntervalSeconds),
                      static_cast<unsigned>(historyRing_.size()),
                      static_cast<unsigned>(historyRing_.capacity()),
                      historyRetransmissionState_.pending ? "yes" : "no");
        const auto& rs485 = modbus_.rs485SerialSettings();
        Serial.printf("  RS485: %lu baud, %u data bits, stop=%u, parity=%u\n",
                      static_cast<unsigned long>(rs485.baudRate),
                      static_cast<unsigned>(rs485.dataBits),
                      static_cast<unsigned>(rs485.stopBits),
                      static_cast<unsigned>(rs485.parity));
        const auto& master = modbus_.modbusMasterSettings();
        Serial.printf("  Modbus polling: interval=%u ms, timeout=%u ms, retries=%u\n",
                      static_cast<unsigned>(master.executionIntervalMs),
                      static_cast<unsigned>(master.maxResponseTimeMs),
                      static_cast<unsigned>(master.maxRetryTimes));
        Serial.printf("  Wi-Fi: %s\n", network_.apActive() ? "commissioning AP" : "client");
        Serial.printf("  Address: %s\n", network_.address().toString().c_str());
        Serial.println("Capabilities:");
        for (const auto& capability : capabilities_.all()) {
            Serial.printf("  - %s\n", capability.c_str());
        }

        if (security_.hasInitialAdminPasswordForDisplay()) {
            Serial.println("Initial administrator password pending display output.");
        }
    }

    DeviceConfig config_;
    CapabilityRegistry capabilities_;
    ChannelRegistry channels_;
    ConfigStore configStore_;
    modbus::ModbusChannelStore modbusChannelStore_;
    modbus::Rs485SettingsStore rs485SettingsStore_;
    modbus::ModbusMasterSettingsStore modbusMasterSettingsStore_;
    lorawan::FPort85ReportSettingsStore reportSettingsStore_;
    lorawan::FPort85ReportScheduler reportScheduler_;
    time::SettingsStore timeSettingsStore_;
    time::Settings timeSettings_;
    history::SettingsStore historySettingsStore_;
    history::Settings historySettings_;
    history::PersistentHistoryStore historyStore_;
    history::PersistentHistoryStore::Ring historyRing_;
    history::PersistentHistoryStore::Ring retransmissionSnapshot_;
    history::RetransmissionStateStore historyRetransmissionStore_;
    history::RetransmissionState historyRetransmissionState_;
    HistorySample historySamples_[modbus::kCompatibilitySlotCount];
    SecurityStore security_;
    BoardService board_;
    NetworkService network_;
    WebService web_;
    VictronComponent victron_;
    LoRaComponent lora_;
    ModbusComponent modbus_;
    GnssComponent gnss_;
    uint32_t compatibilityUplinksBuilt_ = 0;
    uint32_t compatibilityUplinkEncodeFailures_ = 0;
    uint32_t compatibilityUplinkSendFailures_ = 0;
    uint32_t nextHistorySnapshotAtMs_ = 0;
    uint32_t historySnapshotsStored_ = 0;
    uint32_t historySnapshotsSkippedNoTime_ = 0;
    uint32_t historyEncodeFailures_ = 0;
    uint32_t historyPersistFailures_ = 0;
    uint32_t nextHistoryRetransmissionAtMs_ = 0;
    size_t retransmissionCursor_ = 0;
    uint32_t historyRetransmissionPacketsSent_ = 0;
    uint32_t historyRetransmissionSendFailures_ = 0;
    uint32_t historyRetransmissionEncodeFailures_ = 0;
    uint32_t historyRetransmissionStateFailures_ = 0;
    bool retransmissionCursorInitialized_ = false;
    bool historyNetworkStateInitialized_ = false;
    bool historyWasConnected_ = false;
    bool rebootRequested_ = false;
};

} // namespace multibus
