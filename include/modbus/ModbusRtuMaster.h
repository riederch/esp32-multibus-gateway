#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>

#include "board/BoardPins.h"
#include "ModbusChannel.h"
#include "ModbusRtuCodec.h"
#include "Rs485Settings.h"

namespace multibus {
namespace modbus {

struct RtuSerialConfig {
    uint32_t baudRate = 9600;
    uint32_t serialMode = SERIAL_8N1;
    uint32_t responseTimeoutMs = 500;
    uint32_t interFrameDelayUs = 0;
};

enum class RtuTransactionResult : uint8_t {
    Idle,
    Pending,
    Success,
    Failure,
};

inline bool makeRtuSerialConfig(const Rs485SerialSettings& settings,
                                RtuSerialConfig& config,
                                uint32_t responseTimeoutMs = 500) {
    if (!esp32SupportsRs485SerialSettings(settings) || responseTimeoutMs == 0) return false;

    uint32_t serialMode = 0;
    if (settings.dataBits == 7) {
        if (settings.stopBits == Rs485StopBits::One) {
            if (settings.parity == Rs485Parity::None) serialMode = SERIAL_7N1;
            else if (settings.parity == Rs485Parity::Even) serialMode = SERIAL_7E1;
            else serialMode = SERIAL_7O1;
        } else if (settings.stopBits == Rs485StopBits::Two) {
            if (settings.parity == Rs485Parity::None) serialMode = SERIAL_7N2;
            else if (settings.parity == Rs485Parity::Even) serialMode = SERIAL_7E2;
            else serialMode = SERIAL_7O2;
        }
    } else if (settings.dataBits == 8) {
        if (settings.stopBits == Rs485StopBits::One) {
            if (settings.parity == Rs485Parity::None) serialMode = SERIAL_8N1;
            else if (settings.parity == Rs485Parity::Even) serialMode = SERIAL_8E1;
            else serialMode = SERIAL_8O1;
        } else if (settings.stopBits == Rs485StopBits::Two) {
            if (settings.parity == Rs485Parity::None) serialMode = SERIAL_8N2;
            else if (settings.parity == Rs485Parity::Even) serialMode = SERIAL_8E2;
            else serialMode = SERIAL_8O2;
        }
    }

    if (serialMode == 0) return false;
    config = RtuSerialConfig{};
    config.baudRate = settings.baudRate;
    config.serialMode = serialMode;
    config.responseTimeoutMs = responseTimeoutMs;
    return true;
}

class ModbusRtuMaster {
public:
    explicit ModbusRtuMaster(uint8_t uartNumber = 1) : serial_(uartNumber) {}

    bool begin(const RtuSerialConfig& config = RtuSerialConfig{}) {
        config_ = config;
        if (config_.baudRate == 0 || config_.responseTimeoutMs == 0) return false;

        pinMode(board::MODBUS_DIR, OUTPUT);
        digitalWrite(board::MODBUS_DIR, LOW);

        serial_.begin(
            config_.baudRate,
            config_.serialMode,
            board::MODBUS_RX,
            board::MODBUS_TX
        );
        serial_.setTimeout(config_.responseTimeoutMs);
        started_ = true;
        return true;
    }

    void end() {
        cancelTransaction();
        if (started_) serial_.end();
        digitalWrite(board::MODBUS_DIR, LOW);
        started_ = false;
    }

    bool started() const { return started_; }
    bool online() const { return online_; }
    bool transactionPending() const { return transactionActive_; }
    uint32_t successfulTransactions() const { return successfulTransactions_; }
    uint32_t failedTransactions() const { return failedTransactions_; }
    RtuDecodeStatus lastStatus() const { return lastStatus_; }
    uint8_t lastExceptionCode() const { return lastExceptionCode_; }
    const RtuSerialConfig& config() const { return config_; }

    bool reconfigure(const RtuSerialConfig& config) {
        end();
        return begin(config);
    }

    bool startRead(const ChannelConfig& channel) {
        lastExceptionCode_ = 0;
        if (!started_ || transactionActive_ || !validChannelConfig(channel)) {
            if (!transactionActive_) fail(RtuDecodeStatus::InvalidConfig);
            return false;
        }

        uint8_t request[8] = {0};
        size_t requestLength = 0;
        const RtuDecodeStatus requestStatus = ModbusRtuCodec::buildReadRequest(
            channel, request, sizeof(request), requestLength);
        if (requestStatus != RtuDecodeStatus::Ok) return fail(requestStatus);

        drainReceiveBuffer();
        waitInterFrameGap();

        digitalWrite(board::MODBUS_DIR, HIGH);
        delayMicroseconds(txEnableGuardUs());
        const size_t sent = serial_.write(request, requestLength);
        serial_.flush();
        delayMicroseconds(txDisableGuardUs());
        digitalWrite(board::MODBUS_DIR, LOW);

        if (sent != requestLength) return fail(RtuDecodeStatus::Truncated);

        activeChannel_ = channel;
        responseLength_ = 0;
        expectedResponseLength_ = 0;
        transactionStartedAtMs_ = millis();
        transactionActive_ = true;
        return true;
    }

    RtuTransactionResult pollRead(DecodedScalar values[2], uint8_t& valueCount) {
        valueCount = 0;
        if (!transactionActive_) return RtuTransactionResult::Idle;
        if (values == nullptr) {
            finishFailure(RtuDecodeStatus::InvalidConfig);
            return RtuTransactionResult::Failure;
        }

        while (serial_.available() > 0) {
            const int raw = serial_.read();
            if (raw < 0) break;
            if (responseLength_ >= sizeof(response_)) {
                finishFailure(RtuDecodeStatus::BufferTooSmall);
                return RtuTransactionResult::Failure;
            }

            response_[responseLength_++] = static_cast<uint8_t>(raw);
            if (responseLength_ == 2 && (response_[1] & 0x80U) != 0) {
                expectedResponseLength_ = 5;
            } else if (responseLength_ == 3 && (response_[1] & 0x80U) == 0) {
                expectedResponseLength_ = static_cast<size_t>(response_[2]) + 5U;
                if (expectedResponseLength_ > sizeof(response_)) {
                    finishFailure(RtuDecodeStatus::BufferTooSmall);
                    return RtuTransactionResult::Failure;
                }
            }

            if (expectedResponseLength_ != 0 && responseLength_ >= expectedResponseLength_) {
                return finishDecode(values, valueCount);
            }
        }

        if (millis() - transactionStartedAtMs_ >= config_.responseTimeoutMs) {
            if (responseLength_ == 0) {
                finishFailure(RtuDecodeStatus::Truncated);
                return RtuTransactionResult::Failure;
            }
            return finishDecode(values, valueCount);
        }

        return RtuTransactionResult::Pending;
    }

    void cancelTransaction() {
        transactionActive_ = false;
        responseLength_ = 0;
        expectedResponseLength_ = 0;
        digitalWrite(board::MODBUS_DIR, LOW);
        if (started_) drainReceiveBuffer();
    }

    // Compatibility helper for callers that still require a synchronous API.
    // The component scheduler uses startRead()/pollRead() and never enters this loop.
    bool read(const ChannelConfig& channel,
              DecodedScalar values[2],
              uint8_t& valueCount) {
        valueCount = 0;
        if (!startRead(channel)) return false;

        while (true) {
            const RtuTransactionResult result = pollRead(values, valueCount);
            if (result == RtuTransactionResult::Success) return true;
            if (result == RtuTransactionResult::Failure || result == RtuTransactionResult::Idle) return false;
            delay(1);
        }
    }

private:
    RtuTransactionResult finishDecode(DecodedScalar values[2], uint8_t& valueCount) {
        uint8_t exceptionCode = 0;
        const RtuDecodeStatus status = ModbusRtuCodec::decodeReadResponse(
            activeChannel_,
            response_,
            responseLength_,
            values,
            valueCount,
            exceptionCode);
        lastExceptionCode_ = exceptionCode;
        transactionActive_ = false;
        responseLength_ = 0;
        expectedResponseLength_ = 0;

        if (status != RtuDecodeStatus::Ok) {
            fail(status);
            return RtuTransactionResult::Failure;
        }

        lastStatus_ = RtuDecodeStatus::Ok;
        online_ = true;
        ++successfulTransactions_;
        return RtuTransactionResult::Success;
    }

    void finishFailure(RtuDecodeStatus status) {
        transactionActive_ = false;
        responseLength_ = 0;
        expectedResponseLength_ = 0;
        fail(status);
    }

    void drainReceiveBuffer() {
        while (serial_.available() > 0) serial_.read();
    }

    void waitInterFrameGap() const {
        const uint32_t gap = config_.interFrameDelayUs != 0
            ? config_.interFrameDelayUs
            : calculatedInterFrameDelayUs();
        if (gap != 0) delayMicroseconds(gap);
    }

    uint32_t calculatedInterFrameDelayUs() const {
        if (config_.baudRate == 0) return 0;
        if (config_.baudRate > 19200) return 1750;
        return static_cast<uint32_t>((38500000ULL + config_.baudRate - 1ULL) / config_.baudRate);
    }

    uint32_t txEnableGuardUs() const {
        if (config_.baudRate == 0) return 100;
        const uint32_t oneBit = static_cast<uint32_t>((1000000ULL + config_.baudRate - 1ULL) / config_.baudRate);
        return oneBit < 10 ? 10 : oneBit;
    }

    uint32_t txDisableGuardUs() const {
        return txEnableGuardUs();
    }

    bool fail(RtuDecodeStatus status) {
        lastStatus_ = status;
        online_ = false;
        ++failedTransactions_;
        digitalWrite(board::MODBUS_DIR, LOW);
        return false;
    }

    HardwareSerial serial_;
    RtuSerialConfig config_;
    ChannelConfig activeChannel_;
    uint8_t response_[32] = {0};
    size_t responseLength_ = 0;
    size_t expectedResponseLength_ = 0;
    uint32_t transactionStartedAtMs_ = 0;
    bool transactionActive_ = false;
    bool started_ = false;
    bool online_ = false;
    uint32_t successfulTransactions_ = 0;
    uint32_t failedTransactions_ = 0;
    RtuDecodeStatus lastStatus_ = RtuDecodeStatus::Truncated;
    uint8_t lastExceptionCode_ = 0;
};

} // namespace modbus
} // namespace multibus
