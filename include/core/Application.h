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
#include "lorawan/FPort85BasicInfo.h"
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
#include "rules/RuleState.h"
#include "rules/RuleStore.h"
#include "rules/RuleExecution.h"
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
        if (!ruleStore_.begin()) {
            Serial.println("Failed to open rule store.");
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
        if (!ruleStore_.load(ruleState_)) {
            Serial.println("Failed to load persisted rules.");
            return false;
        }
        ruleBootEvaluationPending_ = true;
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
        updateCompatibilityConnectionState();
        processHistoryNetworkState();
        modbus_.loop();
        const bool passThroughHandled = processModbusPassThroughResponse();
        const bool basicInfoHandled = !passThroughHandled && processCompatibilityBasicInfo();
        captureModbusCompatibilitySample();
        processRuleExecution();
        processHistoryStorage();
        const bool ruleReplyActive =
            !passThroughHandled && !basicInfoHandled && processRuleReply();
        const bool queryActive =
            !passThroughHandled && !basicInfoHandled && !ruleReplyActive && processHistoryQuery();
        const bool retransmissionActive =
            !passThroughHandled && !basicInfoHandled && !ruleReplyActive &&
            !queryActive && processHistoryRetransmission();
        if (!passThroughHandled && !basicInfoHandled && !ruleReplyActive &&
            !queryActive && !retransmissionActive) {
            processModbusCompatibilityReport();
        }
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
        RetrievabilityInterval,
        HistoryQuery,
        RuleStatus,
        RuleEnquiry,
        RuleConfiguration,
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
        lorawan::RetrievabilityIntervalCommand retrievabilityInterval;
        lorawan::HistoryQueryCommand historyQuery;
        lorawan::RuleStatusCommand ruleStatus;
        lorawan::RuleEnquiryCommand ruleEnquiry;
        lorawan::RuleConfigurationCommand ruleConfiguration;
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
        ruleStore_.clear();
    }

    static bool downlinkThunk(void* context, uint8_t fport, const uint8_t* payload, size_t length) {
        if (context == nullptr) return false;
        return static_cast<Application*>(context)->handleLoRaDownlink(fport, payload, length);
    }

    void updateCompatibilityConnectionState() {
        const bool connected = lora_.connected();
        if (!compatibilityConnectionInitialized_) {
            compatibilityConnectionInitialized_ = true;
            compatibilityWasConnected_ = connected;
            if (connected) compatibilityBasicInfoPending_ = true;
            return;
        }

        if (!compatibilityWasConnected_ && connected) {
            compatibilityBasicInfoPending_ = true;
        }
        compatibilityWasConnected_ = connected;
    }

    bool processCompatibilityBasicInfo() {
        if (!compatibilityBasicInfoPending_ || !lora_.connected()) return false;

        lorawan::BasicInfo info;
        const uint64_t mac = hardwareMac48();
        info.serialNumber[0] = 0x02;
        info.serialNumber[1] = static_cast<uint8_t>((mac >> 40U) & 0xffU);
        info.serialNumber[2] = static_cast<uint8_t>((mac >> 32U) & 0xffU);
        info.serialNumber[3] = static_cast<uint8_t>((mac >> 24U) & 0xffU);
        info.serialNumber[4] = static_cast<uint8_t>((mac >> 16U) & 0xffU);
        info.serialNumber[5] = static_cast<uint8_t>((mac >> 8U) & 0xffU);
        info.serialNumber[6] = static_cast<uint8_t>(mac & 0xffU);
        info.serialNumber[7] = 0x01;
        info.protocolVersion = 0x01;
        info.tslMajor = 0x01;
        info.tslMinor = 0x00;
        info.hardwareMajor = 0x04;
        info.hardwareMinor = 0x20;
        info.softwareMajor = 0x00;
        info.softwareMinor = 0x00;
        info.deviceType = config_.lorawan.classC ? 0x02 : 0x00;

        uint8_t payload[lorawan::FPort85BasicInfo::kBasicInfoWithResetLength] = {0};
        size_t written = 0;
        if (lorawan::FPort85BasicInfo::encode(
                info,
                compatibilityResetEventPending_,
                payload,
                sizeof(payload),
                written) != lorawan::EncodeStatus::Ok ||
            written == 0) {
            ++compatibilityBasicInfoEncodeFailures_;
            return true;
        }

        TransportEnvelope envelope;
        envelope.endpoint = lorawan::kCompatibilityFPort;
        envelope.payload = payload;
        envelope.length = written;
        envelope.confirmed = false;

        if (!lora_.send(envelope)) {
            ++compatibilityBasicInfoSendFailures_;
            return true;
        }

        compatibilityBasicInfoPending_ = false;
        compatibilityResetEventPending_ = false;
        ++compatibilityBasicInfoSent_;
        return true;
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

    bool sendHistoryQueryReply(uint8_t queryType, uint8_t status) {
        uint8_t payload[3] = {0};
        size_t written = 0;
        if (lorawan::FPort85Codec::encodeHistoryQueryReply(
                queryType, status, payload, sizeof(payload), written) != lorawan::EncodeStatus::Ok ||
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

    bool applyHistoryQueryCommand(const lorawan::HistoryQueryCommand& command) {
        if (command.kind == lorawan::HistoryQueryKind::Stop) {
            historyQueryActive_ = false;
            historyQueryCount_ = 0;
            historyQueryCursor_ = 0;
            return true;
        }

        const uint8_t replyType =
            command.kind == lorawan::HistoryQueryKind::TimePoint
                ? lorawan::FPort85Codec::kHistoryPointType
                : lorawan::FPort85Codec::kHistoryRangeType;

        if (command.kind == lorawan::HistoryQueryKind::TimeRange &&
            command.startUnix > command.endUnix) {
            historyQueryActive_ = false;
            return sendHistoryQueryReply(replyType, 0x01);
        }

        historyQueryCount_ = 0;
        historyQueryCursor_ = 0;

        if (command.kind == lorawan::HistoryQueryKind::TimePoint) {
            bool found = false;
            uint32_t closestTimestamp = 0;
            uint64_t closestDistance = UINT64_MAX;
            const uint64_t tolerance = reportScheduler_.settings().seconds;

            for (size_t i = 0; i < historyRing_.size(); ++i) {
                const lorawan::HistoricalRecord* record = historyRing_.oldest(i);
                if (record == nullptr) continue;
                const uint32_t ts = record->timestamp();
                const uint64_t distance =
                    ts >= command.startUnix
                        ? static_cast<uint64_t>(ts - command.startUnix)
                        : static_cast<uint64_t>(command.startUnix - ts);
                if (distance <= tolerance && (!found || distance < closestDistance)) {
                    found = true;
                    closestDistance = distance;
                    closestTimestamp = ts;
                }
            }

            if (found) {
                for (size_t i = 0; i < historyRing_.size() &&
                                   historyQueryCount_ < history::kPersistentHistoryCapacity; ++i) {
                    const lorawan::HistoricalRecord* record = historyRing_.oldest(i);
                    if (record != nullptr && record->timestamp() == closestTimestamp) {
                        historyQueryRecords_[historyQueryCount_++] = *record;
                    }
                }
            }
        } else {
            for (size_t i = 0; i < historyRing_.size() &&
                               historyQueryCount_ < history::kPersistentHistoryCapacity; ++i) {
                const lorawan::HistoricalRecord* record = historyRing_.oldest(i);
                if (record == nullptr) continue;
                const uint32_t ts = record->timestamp();
                if (ts >= command.startUnix && ts <= command.endUnix) {
                    historyQueryRecords_[historyQueryCount_++] = *record;
                }
            }
        }

        if (historyQueryCount_ == 0) {
            historyQueryActive_ = false;
            return sendHistoryQueryReply(replyType, 0x02);
        }

        if (!sendHistoryQueryReply(replyType, 0x00)) {
            historyQueryActive_ = false;
            historyQueryCount_ = 0;
            return false;
        }

        historyQueryActive_ = true;
        nextHistoryQueryAtMs_ =
            millis() + static_cast<uint32_t>(historySettings_.retrievabilityIntervalSeconds) * 1000UL;
        return true;
    }

    bool processHistoryQuery() {
        if (!historyQueryActive_ || !lora_.connected()) return false;

        const uint32_t now = millis();
        if (static_cast<int32_t>(now - nextHistoryQueryAtMs_) < 0) return false;

        uint8_t payload[lorawan::kCompatibilityReportPayloadLimit] = {0};
        size_t written = 0;
        size_t recordsAdded = 0;
        while (historyQueryCursor_ + recordsAdded < historyQueryCount_) {
            if (written + lorawan::kHistoricalModbusRecordLength >
                lorawan::kCompatibilityReportPayloadLimit) {
                break;
            }
            memcpy(payload + written,
                   historyQueryRecords_[historyQueryCursor_ + recordsAdded].payload,
                   lorawan::kHistoricalModbusRecordLength);
            written += lorawan::kHistoricalModbusRecordLength;
            ++recordsAdded;
        }

        if (recordsAdded == 0) {
            historyQueryActive_ = false;
            return false;
        }

        TransportEnvelope envelope;
        envelope.endpoint = lorawan::kCompatibilityFPort;
        envelope.payload = payload;
        envelope.length = written;
        envelope.confirmed = false;

        nextHistoryQueryAtMs_ =
            now + static_cast<uint32_t>(historySettings_.retrievabilityIntervalSeconds) * 1000UL;

        if (!lora_.send(envelope)) {
            ++historyQuerySendFailures_;
            return true;
        }

        historyQueryCursor_ += recordsAdded;
        ++historyQueryPacketsSent_;
        if (historyQueryCursor_ >= historyQueryCount_) {
            historyQueryActive_ = false;
            historyQueryCount_ = 0;
            historyQueryCursor_ = 0;
        }
        return true;
    }

    struct ScheduledRuleAction {
        bool pending = false;
        rules::ExecutableAction action = rules::ExecutableAction::None;
        uint32_t dueAtMs = 0;
        uint8_t ruleId = 0;
        uint8_t actionSlot = 0;
    };

    void cancelScheduledRuleActions(uint8_t ruleId) {
        for (auto& scheduled : scheduledRuleActions_) {
            if (scheduled.pending && scheduled.ruleId == ruleId) {
                scheduled = ScheduledRuleAction{};
            }
        }
    }

    void scheduleRuleActions(uint8_t ruleId, const rules::RuleRecord& record) {
        for (uint8_t actionIndex = 0; actionIndex < 3; ++actionIndex) {
            const rules::ActionPlan plan =
                rules::decodeExecutableAction(record.frames[actionIndex + 1U]);
            if (plan.action == rules::ExecutableAction::None) continue;
            if (plan.action == rules::ExecutableAction::Unsupported) {
                ++ruleUnsupportedActions_;
                continue;
            }

            const size_t slot =
                (static_cast<size_t>(ruleId) - 1U) * 3U + actionIndex;
            ScheduledRuleAction& scheduled = scheduledRuleActions_[slot];
            scheduled.pending = true;
            scheduled.action = plan.action;
            scheduled.dueAtMs = millis() + plan.delayMs;
            scheduled.ruleId = ruleId;
            scheduled.actionSlot = actionIndex;
        }
        ++ruleTriggers_;
    }

    void executeDueRuleActions() {
        const uint32_t now = millis();
        for (auto& scheduled : scheduledRuleActions_) {
            if (!scheduled.pending ||
                static_cast<int32_t>(now - scheduled.dueAtMs) < 0) {
                continue;
            }

            const rules::RuleRecord* record = ruleState_.rule(scheduled.ruleId);
            if (record == nullptr || !record->enabled) {
                scheduled = ScheduledRuleAction{};
                continue;
            }

            switch (scheduled.action) {
                case rules::ExecutableAction::UploadData:
                    reportScheduler_.requestImmediateReport();
                    ++ruleActionsExecuted_;
                    break;
                case rules::ExecutableAction::Reboot:
                    rebootRequested_ = true;
                    ++ruleActionsExecuted_;
                    break;
                case rules::ExecutableAction::None:
                case rules::ExecutableAction::Unsupported:
                    ++ruleUnsupportedActions_;
                    break;
            }
            scheduled = ScheduledRuleAction{};
        }
    }

    void processRuleExecution() {
        const time_t unixNow = ::time(nullptr);

        if (ruleBootEvaluationPending_) {
            ruleBootEvaluationPending_ = false;
            for (uint8_t id = 1; id <= rules::kRuleCount; ++id) {
                const rules::RuleRecord* record = ruleState_.rule(id);
                if (record == nullptr || !record->enabled ||
                    !rules::isDeviceRestartCondition(record->frames[0])) {
                    continue;
                }
                scheduleRuleActions(id, *record);
            }
        }

        if (unixNow > 0) {
            const time::LocalDateTime local =
                time::localDateTime(static_cast<uint32_t>(unixNow), timeSettings_);
            const uint32_t minuteKey = rules::localMinuteKey(local);

            for (uint8_t id = 1; id <= rules::kRuleCount; ++id) {
                const rules::RuleRecord* record = ruleState_.rule(id);
                if (record == nullptr || !record->enabled) continue;
                if (!rules::matchesTimeCondition(record->frames[0], local)) continue;

                const size_t index = static_cast<size_t>(id - 1U);
                if (lastRuleMinuteKey_[index] == minuteKey) continue;
                lastRuleMinuteKey_[index] = minuteKey;
                scheduleRuleActions(id, *record);
            }
        }

        executeDueRuleActions();
    }

    bool processRuleReply() {
        if (ruleReplyCursor_ >= ruleReplyCount_ || !lora_.connected()) return false;

        const rules::StoredFrame& frame = ruleReplyFrames_[ruleReplyCursor_];
        if (!frame.present()) {
            ++ruleReplyCursor_;
            if (ruleReplyCursor_ >= ruleReplyCount_) clearRuleReplyQueue();
            return true;
        }

        TransportEnvelope envelope;
        envelope.endpoint = lorawan::kCompatibilityFPort;
        envelope.payload = frame.data;
        envelope.length = frame.length;
        envelope.confirmed = false;

        if (!lora_.send(envelope)) {
            ++ruleReplySendFailures_;
            return true;
        }

        ++ruleReplyCursor_;
        ++ruleReplyPacketsSent_;
        if (ruleReplyCursor_ >= ruleReplyCount_) clearRuleReplyQueue();
        return true;
    }

    void clearRuleReplyQueue() {
        for (auto& frame : ruleReplyFrames_) frame.clear();
        ruleReplyCount_ = 0;
        ruleReplyCursor_ = 0;
    }

    bool stageRuleEnquiryReply(uint8_t ruleId) {
        const rules::RuleRecord* record = ruleState_.rule(ruleId);
        if (record == nullptr) return false;

        clearRuleReplyQueue();
        for (size_t i = 0; i < rules::kRuleFrameSlots; ++i) {
            if (!record->frames[i].present()) continue;
            ruleReplyFrames_[ruleReplyCount_++] = record->frames[i];
        }
        return true;
    }

    bool stageRuleConfigurationAck(const lorawan::RuleConfigurationCommand& command,
                                   uint8_t status) {
        if (command.frame == nullptr || command.frameLength < 4 ||
            command.frameLength + 1U > rules::kMaxRuleFrameLength) {
            return false;
        }

        if (ruleReplyCount_ >= rules::kRuleFrameSlots) return false;
        rules::StoredFrame& reply = ruleReplyFrames_[ruleReplyCount_++];
        reply.length = static_cast<uint8_t>(command.frameLength + 1U);
        reply.data[0] = 0xf8;
        reply.data[1] = lorawan::FPort85Codec::kRuleConfigurationType;
        memcpy(reply.data + 2, command.frame + 2, command.frameLength - 2U);
        reply.data[command.frameLength] = status;
        return true;
    }

    bool applyRuleConfigurationCommand(const lorawan::RuleConfigurationCommand& command) {
        rules::RuleRecord* current = ruleState_.rule(command.ruleId);
        if (current == nullptr || command.frame == nullptr ||
            static_cast<size_t>(command.slot) >= rules::kRuleFrameSlots) {
            return false;
        }

        rules::RuleRecord updated = *current;
        updated.enabled = command.enabled;
        if (!updated.frames[static_cast<size_t>(command.slot)].set(
                command.frame, command.frameLength)) {
            return false;
        }
        if (!ruleStore_.saveRule(command.ruleId, updated)) return false;
        *current = updated;
        if (!updated.enabled) cancelScheduledRuleActions(command.ruleId);
        return stageRuleConfigurationAck(command, 0x00);
    }

    bool applyRuleStatusCommand(const lorawan::RuleStatusCommand& command) {
        rules::RuleRecord originals[rules::kRuleCount];
        bool touched[rules::kRuleCount] = {false};

        for (uint8_t bit = 0; bit < rules::kRuleCount; ++bit) {
            if ((command.ruleMask & (static_cast<uint16_t>(1U) << bit)) == 0) continue;
            rules::RuleRecord* current = ruleState_.rule(static_cast<uint8_t>(bit + 1U));
            if (current == nullptr) return false;
            originals[bit] = *current;
            touched[bit] = true;

            rules::RuleRecord updated = *current;
            if (command.operation == lorawan::RuleStatusOperation::Enable) {
                updated.enabled = true;
            } else if (command.operation == lorawan::RuleStatusOperation::Disable) {
                updated.enabled = false;
            } else {
                updated.clear();
            }

            if (!ruleStore_.saveRule(static_cast<uint8_t>(bit + 1U), updated)) {
                for (uint8_t rollback = 0; rollback < bit; ++rollback) {
                    if (!touched[rollback]) continue;
                    ruleStore_.saveRule(static_cast<uint8_t>(rollback + 1U), originals[rollback]);
                    *ruleState_.rule(static_cast<uint8_t>(rollback + 1U)) = originals[rollback];
                }
                return false;
            }
            *current = updated;
            if (!updated.enabled) {
                cancelScheduledRuleActions(static_cast<uint8_t>(bit + 1U));
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

    bool processModbusPassThroughResponse() {
        ModbusComponent::RawCompletion completion;
        if (!modbus_.takeRawCompletion(completion)) return false;
        if (!completion.success || completion.length == 0) {
            ++passThroughFailures_;
            return true;
        }

        TransportEnvelope envelope;
        envelope.endpoint = modbus_.modbusMasterSettings().passThroughPort;
        envelope.payload = completion.payload;
        envelope.length = completion.length;
        envelope.confirmed = false;

        if (lora_.send(envelope)) {
            ++passThroughResponsesSent_;
        } else {
            ++passThroughFailures_;
        }
        return true;
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
        if (payload == nullptr || length == 0) return false;

        if (fport != lorawan::kCompatibilityFPort) {
            const auto& settings = modbus_.modbusMasterSettings();
            if (config_.components.modbus == ModbusMode::Master &&
                settings.passThroughMode == modbus::PassThroughMode::Active &&
                fport == settings.passThroughPort) {
                return modbus_.queueRawRequest(payload, length);
            }
            return false;
        }

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
            } else if (header.channelId == lorawan::FPort85Codec::kModbusChannel &&
                       header.type == lorawan::FPort85Codec::kRetrievabilityIntervalType) {
                parsed.kind = ParsedCommandKind::RetrievabilityInterval;
                if (lorawan::FPort85Codec::decodeRetrievabilityIntervalCommand(
                        payload + offset, length - offset,
                        parsed.retrievabilityInterval, consumed) != lorawan::DecodeStatus::Ok ||
                    consumed == 0) {
                    return false;
                }
            } else if (header.channelId == lorawan::FPort85Codec::kHistoryQueryChannel &&
                       (header.type == lorawan::FPort85Codec::kHistoryPointType ||
                        header.type == lorawan::FPort85Codec::kHistoryRangeType ||
                        header.type == lorawan::FPort85Codec::kHistoryStopType)) {
                parsed.kind = ParsedCommandKind::HistoryQuery;
                if (lorawan::FPort85Codec::decodeHistoryQueryCommand(
                        payload + offset, length - offset,
                        parsed.historyQuery, consumed) != lorawan::DecodeStatus::Ok ||
                    consumed == 0) {
                    return false;
                }
            } else if (header.channelId == lorawan::FPort85Codec::kModbusChannel &&
                       header.type == lorawan::FPort85Codec::kRuleStatusType) {
                parsed.kind = ParsedCommandKind::RuleStatus;
                if (lorawan::FPort85Codec::decodeRuleStatusCommand(
                        payload + offset, length - offset, parsed.ruleStatus, consumed) != lorawan::DecodeStatus::Ok ||
                    consumed == 0) {
                    return false;
                }
            } else if (header.channelId == lorawan::FPort85Codec::kModbusChannel &&
                       header.type == lorawan::FPort85Codec::kRuleEnquiryType) {
                parsed.kind = ParsedCommandKind::RuleEnquiry;
                if (lorawan::FPort85Codec::decodeRuleEnquiryCommand(
                        payload + offset, length - offset, parsed.ruleEnquiry, consumed) != lorawan::DecodeStatus::Ok ||
                    consumed == 0) {
                    return false;
                }
            } else if (header.channelId == lorawan::FPort85Codec::kModbusChannel &&
                       header.type == lorawan::FPort85Codec::kRuleConfigurationType) {
                parsed.kind = ParsedCommandKind::RuleConfiguration;
                if (lorawan::FPort85Codec::decodeRuleConfigurationCommand(
                        payload + offset, length - offset, parsed.ruleConfiguration, consumed) != lorawan::DecodeStatus::Ok ||
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
                case ParsedCommandKind::RetrievabilityInterval:
                    if (!applyRetrievabilityIntervalCommand(command.retrievabilityInterval)) return false;
                    break;
                case ParsedCommandKind::HistoryQuery:
                    if (!applyHistoryQueryCommand(command.historyQuery)) return false;
                    break;
                case ParsedCommandKind::RuleStatus:
                    if (!applyRuleStatusCommand(command.ruleStatus)) return false;
                    break;
                case ParsedCommandKind::RuleEnquiry:
                    if (!stageRuleEnquiryReply(command.ruleEnquiry.ruleId)) return false;
                    break;
                case ParsedCommandKind::RuleConfiguration:
                    if (!applyRuleConfigurationCommand(command.ruleConfiguration)) return false;
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

    bool applyRetrievabilityIntervalCommand(
        const lorawan::RetrievabilityIntervalCommand& command) {
        history::Settings updated = historySettings_;
        updated.retrievabilityIntervalSeconds = command.seconds;
        if (!history::validSettings(updated) || !historySettingsStore_.save(updated)) return false;
        historySettings_ = updated;
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
        binding.writable = modbus::ModbusRtuCodec::writableType(channel.dataType);
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
        uint8_t configuredRules = 0;
        uint8_t enabledRules = 0;
        for (uint8_t id = 1; id <= rules::kRuleCount; ++id) {
            const rules::RuleRecord* rule = ruleState_.rule(id);
            if (rule == nullptr || !rule->configured()) continue;
            ++configuredRules;
            if (rule->enabled) ++enabledRules;
        }
        Serial.printf("  Rules: configured=%u/%u, enabled=%u\n",
                      static_cast<unsigned>(configuredRules),
                      static_cast<unsigned>(rules::kRuleCount),
                      static_cast<unsigned>(enabledRules));
        Serial.printf("  History: storage=%s, retransmission=%s, interval=%u s, query=%u s, records=%u/%u, pending=%s\n",
                      historySettings_.storageEnabled ? "enabled" : "disabled",
                      historySettings_.retransmissionEnabled ? "enabled" : "disabled",
                      static_cast<unsigned>(historySettings_.retransmissionIntervalSeconds),
                      static_cast<unsigned>(historySettings_.retrievabilityIntervalSeconds),
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
    rules::RuleStore ruleStore_;
    rules::RuleState ruleState_;
    rules::StoredFrame ruleReplyFrames_[rules::kRuleFrameSlots];
    size_t ruleReplyCount_ = 0;
    size_t ruleReplyCursor_ = 0;
    ScheduledRuleAction scheduledRuleActions_[rules::kRuleCount * 3U];
    uint32_t lastRuleMinuteKey_[rules::kRuleCount] = {0};
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
    uint32_t compatibilityBasicInfoSent_ = 0;
    uint32_t compatibilityBasicInfoEncodeFailures_ = 0;
    uint32_t compatibilityBasicInfoSendFailures_ = 0;
    uint32_t passThroughResponsesSent_ = 0;
    uint32_t passThroughFailures_ = 0;
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
    lorawan::HistoricalRecord historyQueryRecords_[history::kPersistentHistoryCapacity];
    size_t historyQueryCount_ = 0;
    size_t historyQueryCursor_ = 0;
    uint32_t nextHistoryQueryAtMs_ = 0;
    uint32_t historyQueryPacketsSent_ = 0;
    uint32_t historyQuerySendFailures_ = 0;
    uint32_t ruleReplyPacketsSent_ = 0;
    uint32_t ruleReplySendFailures_ = 0;
    uint32_t ruleTriggers_ = 0;
    uint32_t ruleActionsExecuted_ = 0;
    uint32_t ruleUnsupportedActions_ = 0;
    bool historyQueryActive_ = false;
    bool retransmissionCursorInitialized_ = false;
    bool historyNetworkStateInitialized_ = false;
    bool historyWasConnected_ = false;
    bool compatibilityConnectionInitialized_ = false;
    bool compatibilityWasConnected_ = false;
    bool compatibilityBasicInfoPending_ = false;
    bool compatibilityResetEventPending_ = true;
    bool ruleBootEvaluationPending_ = false;
    bool rebootRequested_ = false;
};

} // namespace multibus
