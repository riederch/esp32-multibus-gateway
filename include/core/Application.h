#pragma once

#include <Arduino.h>
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
#include "lorawan/FPort85ReportScheduler.h"
#include "lorawan/FPort85ReportSettingsStore.h"
#include "modbus/ModbusChannelStore.h"
#include "modbus/ModbusMasterSettingsStore.h"
#include "modbus/Rs485SettingsStore.h"
#include "services/BoardService.h"
#include "services/NetworkService.h"
#include "services/WebService.h"

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
        modbus_.loop();
        captureModbusCompatibilitySample();
        processModbusCompatibilityReport();
        gnss_.loop();
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
        ModbusChannel,
        Rs485Settings,
        ModbusMasterSettings,
        Rs485SettingsEnquiry,
    };

    struct ParsedCommand {
        ParsedCommandKind kind = ParsedCommandKind::ModbusChannel;
        lorawan::ReportIntervalCommand reportInterval;
        lorawan::ModbusChannelCommand modbusChannel;
        lorawan::Rs485SettingsCommand rs485Settings;
        lorawan::ModbusMasterSettingsCommand modbusMasterSettings;
        lorawan::Rs485SettingsEnquiryCommand rs485SettingsEnquiry;
    };

    static void factoryResetThunk(void* context) {
        if (context != nullptr) static_cast<Application*>(context)->clearSubsystemStores();
    }

    void clearSubsystemStores() {
        modbusChannelStore_.clear();
        rs485SettingsStore_.clear();
        modbusMasterSettingsStore_.clear();
        reportSettingsStore_.clear();
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
        if (reportScheduler_.applySettings(command.settings, millis())) return true;

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
        reportScheduler_.begin(settings, millis());
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
};

} // namespace multibus
