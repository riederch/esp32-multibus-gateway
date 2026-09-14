#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "FPort85ModbusUplink.h"
#include "FPort85ReportSettings.h"

namespace multibus::lorawan {

constexpr size_t kCompatibilityReportPayloadLimit = 51;
constexpr uint32_t kCompatibilityReportRetryMs = 5000UL;

enum class ReportBuildStatus : uint8_t {
    NotDue,
    PacketReady,
    EmptyReport,
    EncodeError,
    BufferTooSmall,
};

class FPort85ReportScheduler {
public:
    void begin(const ReportIntervalSettings& settings, uint32_t nowMs) {
        settings_ = validReportIntervalSettings(settings) ? settings : ReportIntervalSettings{};
        nextReportAtMs_ = nowMs + intervalMs();
        reportInProgress_ = false;
        pending_ = false;
        cursor_ = 0;
        retryAtMs_ = 0;
    }

    bool applySettings(const ReportIntervalSettings& settings, uint32_t nowMs) {
        if (!validReportIntervalSettings(settings)) return false;
        settings_ = settings;
        nextReportAtMs_ = nowMs + intervalMs();
        return true;
    }

    const ReportIntervalSettings& settings() const { return settings_; }

    void recordPoll(const modbus::ChannelConfig& channel,
                    bool success,
                    const modbus::DecodedScalar* values,
                    uint8_t valueCount) {
        if (!modbus::validChannelConfig(channel) || channel.slot >= modbus::kCompatibilitySlotCount) return;
        Sample& sample = samples_[channel.slot];
        sample.present = true;
        sample.channel = channel;
        sample.success = success;
        sample.valueCount = success ? valueCount : 0;
        if (success && values != nullptr && valueCount > 0) {
            sample.values[0] = values[0];
            if (valueCount > 1) sample.values[1] = values[1];
        }
    }

    void clearSlot(uint8_t slot) {
        if (slot >= modbus::kCompatibilitySlotCount) return;
        samples_[slot] = Sample{};
    }

    ReportBuildStatus preparePacket(uint32_t nowMs,
                                    uint8_t* output,
                                    size_t capacity,
                                    size_t& written) {
        written = 0;
        if (output == nullptr || capacity < kCompatibilityReportPayloadLimit) {
            return ReportBuildStatus::BufferTooSmall;
        }

        if (pending_) {
            if (static_cast<int32_t>(nowMs - retryAtMs_) < 0) return ReportBuildStatus::NotDue;
            memcpy(output, pendingPayload_, pendingLength_);
            written = pendingLength_;
            return ReportBuildStatus::PacketReady;
        }

        if (!reportInProgress_) {
            if (static_cast<int32_t>(nowMs - nextReportAtMs_) < 0) return ReportBuildStatus::NotDue;
            reportInProgress_ = true;
            cursor_ = 0;
            nextReportAtMs_ = nowMs + intervalMs();
        }

        size_t length = 0;
        uint8_t scan = cursor_;
        while (scan < modbus::kCompatibilitySlotCount) {
            const Sample& sample = samples_[scan];
            if (!sample.present) {
                ++scan;
                continue;
            }

            uint8_t encoded[24] = {0};
            size_t encodedLength = 0;
            const EncodeStatus status = sample.success
                ? FPort85ModbusUplink::encodePollSuccess(
                      sample.channel,
                      sample.values,
                      sample.valueCount,
                      encoded,
                      sizeof(encoded),
                      encodedLength)
                : FPort85ModbusUplink::encodePollFailure(
                      sample.channel,
                      encoded,
                      sizeof(encoded),
                      encodedLength);
            if (status != EncodeStatus::Ok || encodedLength == 0) {
                return ReportBuildStatus::EncodeError;
            }

            if (length + encodedLength > kCompatibilityReportPayloadLimit) {
                if (length == 0) return ReportBuildStatus::BufferTooSmall;
                break;
            }

            memcpy(pendingPayload_ + length, encoded, encodedLength);
            length += encodedLength;
            ++scan;
        }

        if (length == 0) {
            reportInProgress_ = false;
            cursor_ = 0;
            return ReportBuildStatus::EmptyReport;
        }

        pending_ = true;
        pendingLength_ = length;
        pendingNextCursor_ = scan;
        retryAtMs_ = nowMs;
        memcpy(output, pendingPayload_, pendingLength_);
        written = pendingLength_;
        return ReportBuildStatus::PacketReady;
    }

    void markPacketSent(uint32_t nowMs) {
        if (!pending_) return;
        cursor_ = pendingNextCursor_;
        pending_ = false;
        pendingLength_ = 0;
        retryAtMs_ = nowMs;
        if (cursor_ >= modbus::kCompatibilitySlotCount) {
            reportInProgress_ = false;
            cursor_ = 0;
        }
    }

    void markPacketFailed(uint32_t nowMs) {
        if (!pending_) return;
        retryAtMs_ = nowMs + kCompatibilityReportRetryMs;
    }

    void abortReport() {
        pending_ = false;
        pendingLength_ = 0;
        reportInProgress_ = false;
        cursor_ = 0;
    }

    bool reportInProgress() const { return reportInProgress_ || pending_; }
    uint32_t nextReportAtMs() const { return nextReportAtMs_; }

private:
    struct Sample {
        bool present = false;
        modbus::ChannelConfig channel;
        bool success = false;
        uint8_t valueCount = 0;
        modbus::DecodedScalar values[2];
    };

    uint32_t intervalMs() const {
        return static_cast<uint32_t>(settings_.seconds) * 1000UL;
    }

    ReportIntervalSettings settings_;
    Sample samples_[modbus::kCompatibilitySlotCount];
    uint32_t nextReportAtMs_ = 0;
    uint32_t retryAtMs_ = 0;
    uint8_t cursor_ = 0;
    uint8_t pendingNextCursor_ = 0;
    bool reportInProgress_ = false;
    bool pending_ = false;
    uint8_t pendingPayload_[kCompatibilityReportPayloadLimit] = {0};
    size_t pendingLength_ = 0;
};

} // namespace multibus::lorawan
