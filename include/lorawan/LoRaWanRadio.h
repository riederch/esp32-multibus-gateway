#pragma once

#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include <stdint.h>
#include "board/BoardPins.h"
#include "core/DeviceConfig.h"
#include "core/LoRaWanIdentity.h"
#include "core/Transport.h"
#include "lorawan/LoRaWanStateStore.h"

namespace multibus::lorawan {

class LoRaWanRadio {
public:
    using DownlinkHandler = bool (*)(void* context, uint8_t fport, const uint8_t* payload, size_t length);

    void configure(const LoRaWanConfig& config, const String& devEui) {
        config_ = config;
        devEuiText_ = devEui;
        stateIdentity_ = LoRaWanStateStore::identityFingerprint(config_, devEuiText_);
        provisioned_ = parseProvisioning();
    }

    void setDownlinkHandler(DownlinkHandler handler, void* context) {
        downlinkHandler_ = handler;
        downlinkContext_ = context;
    }

    bool begin() {
        joined_ = false;
        radioReady_ = false;
        persistenceHealthy_ = false;
        lastState_ = RADIOLIB_ERR_NONE;

        if (!provisioned_) return false;
        if (!stateStore_.begin()) return false;

        pinMode(board::LORA_FEM_POWER, OUTPUT);
        pinMode(board::LORA_FEM_ENABLE, OUTPUT);
        pinMode(board::LORA_FEM_PA, OUTPUT);
        digitalWrite(board::LORA_FEM_POWER, HIGH);
        digitalWrite(board::LORA_FEM_ENABLE, HIGH);
        digitalWrite(board::LORA_FEM_PA, LOW);

        SPI.begin(board::LORA_SCK, board::LORA_MISO, board::LORA_MOSI, board::LORA_NSS);

        static const uint32_t rfSwitchPins[Module::RFSWITCH_MAX_PINS] = {
            static_cast<uint32_t>(board::LORA_FEM_POWER),
            static_cast<uint32_t>(board::LORA_FEM_ENABLE),
            static_cast<uint32_t>(board::LORA_FEM_PA),
            RADIOLIB_NC,
            RADIOLIB_NC,
        };
        static const Module::RfSwitchMode_t rfSwitchTable[] = {
            {Module::MODE_IDLE, {HIGH, HIGH, LOW}},
            {Module::MODE_RX,   {HIGH, HIGH, LOW}},
            {Module::MODE_TX,   {HIGH, HIGH, HIGH}},
            END_OF_MODE_TABLE,
        };
        radio_.setRfSwitchTable(rfSwitchPins, rfSwitchTable);

        radio_.tcxoVoltage = 1.8f;
        ConfigLoRa_t radioConfig;
        radioConfig.frequency = 868.0f;
        lastState_ = radio_.begin(radioConfig);
        if (lastState_ != RADIOLIB_ERR_NONE) return false;

        lastState_ = radio_.setDio2AsRfSwitch(true);
        if (lastState_ != RADIOLIB_ERR_NONE) return false;

        radioReady_ = true;
        node_.beginOTAA(joinEui_, devEui_, nullptr, appKey_);
        node_.setADR(true);
        node_.setDutyCycle(true, 0);

        uint8_t persistedNonces[RADIOLIB_LORAWAN_NONCES_BUF_SIZE] = {0};
        uint8_t persistedSession[RADIOLIB_LORAWAN_SESSION_BUF_SIZE] = {0};
        bool hasNonces = false;
        bool hasSession = false;
        if (!stateStore_.load(
                stateIdentity_, persistedNonces, hasNonces, persistedSession, hasSession)) {
            return false;
        }

        if (hasNonces) {
            lastState_ = node_.setBufferNonces(persistedNonces);
            if (lastState_ != RADIOLIB_ERR_NONE) {
                // Never continue with reset/corrupt DevNonce state: doing so can
                // cause an OTAA nonce reuse after reboot.
                return false;
            }
        }

        if (hasNonces && hasSession) {
            lastState_ = node_.setBufferSession(persistedSession);
            if (lastState_ == RADIOLIB_ERR_NONE) {
                lastState_ = node_.activateOTAA();
                if (lastState_ == RADIOLIB_LORAWAN_SESSION_RESTORED) {
                    joined_ = true;
                    persistenceHealthy_ = true;
                    return true;
                }
                return false;
            }

            // The nonce state is still authoritative, but a malformed/stale
            // session may be discarded and replaced by a fresh OTAA session.
            if (!stateStore_.clearSession(stateIdentity_)) return false;
            node_.clearSession();
        }

        persistenceHealthy_ = true;
        tryJoin();
        return persistenceHealthy_;
    }

    void loop() {
        if (!radioReady_ || !provisioned_ || !persistenceHealthy_ || joined_) return;
        const uint32_t now = millis();
        if (static_cast<int32_t>(now - nextJoinAtMs_) < 0) return;
        tryJoin();
    }

    bool send(const TransportEnvelope& envelope) {
        if (!joined_ || !persistenceHealthy_ || envelope.payload == nullptr || envelope.length == 0 ||
            envelope.endpoint == 0 || envelope.endpoint > 223 || envelope.length > 242) {
            return false;
        }

        uint8_t downlink[242];
        size_t downlinkSize = 0;
        LoRaWANEvent_t uplinkDetails;
        LoRaWANEvent_t downlinkDetails;

        lastState_ = node_.sendReceive(
            envelope.payload,
            envelope.length,
            envelope.endpoint,
            downlink,
            &downlinkSize,
            envelope.confirmed,
            &uplinkDetails,
            &downlinkDetails);

        // The session buffer contains frame counters and MAC/session state.
        // Persist only when RadioLib's serialized state actually changed, which
        // avoids redundant NVS writes when duty-cycle handling rejects a send.
        if (!stateStore_.saveSessionIfChanged(stateIdentity_, node_.getBufferSession())) {
            persistenceHealthy_ = false;
            return false;
        }

        if (lastState_ < RADIOLIB_ERR_NONE) {
            if (lastState_ == RADIOLIB_ERR_NETWORK_NOT_JOINED ||
                lastState_ == RADIOLIB_ERR_SESSION_DISCARDED) {
                joined_ = false;
                node_.clearSession();
                if (!stateStore_.clearSession(stateIdentity_)) {
                    persistenceHealthy_ = false;
                    return false;
                }
                nextJoinAtMs_ = millis() + kJoinRetryMs;
            }
            return false;
        }

        if (downlinkSize > 0 && downlinkHandler_ != nullptr && downlinkDetails.fPort > 0) {
            downlinkHandler_(downlinkContext_, downlinkDetails.fPort, downlink, downlinkSize);
        }
        return true;
    }

    bool provisioned() const { return provisioned_; }
    bool radioReady() const { return radioReady_; }
    bool joined() const { return joined_; }
    bool persistenceHealthy() const { return persistenceHealthy_; }
    int16_t lastState() const { return lastState_; }
    uint32_t devAddr() const { return joined_ ? node_.getDevAddr() : 0; }

private:
    static constexpr uint32_t kJoinRetryMs = 60000UL;

    static bool hexNibble(char c, uint8_t& value) {
        if (c >= '0' && c <= '9') value = static_cast<uint8_t>(c - '0');
        else if (c >= 'a' && c <= 'f') value = static_cast<uint8_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') value = static_cast<uint8_t>(c - 'A' + 10);
        else return false;
        return true;
    }

    static bool parseEui(const String& text, uint64_t& value) {
        if (!isHexString(text, 16)) return false;
        value = 0;
        for (size_t i = 0; i < 16; ++i) {
            uint8_t nibble = 0;
            if (!hexNibble(text.charAt(i), nibble)) return false;
            value = (value << 4U) | nibble;
        }
        return true;
    }

    static bool parseKey(const String& text, uint8_t (&key)[16]) {
        if (!isHexString(text, 32)) return false;
        for (size_t i = 0; i < 16; ++i) {
            uint8_t high = 0;
            uint8_t low = 0;
            if (!hexNibble(text.charAt(i * 2U), high) ||
                !hexNibble(text.charAt(i * 2U + 1U), low)) {
                return false;
            }
            key[i] = static_cast<uint8_t>((high << 4U) | low);
        }
        return true;
    }

    bool parseProvisioning() {
        if (!validateLoRaWanConfig(config_) || !isHexString(devEuiText_, 16)) return false;
        return parseEui(config_.joinEui, joinEui_) &&
               parseEui(devEuiText_, devEui_) &&
               parseKey(config_.appKey, appKey_);
    }

    bool persistJoinState() {
        if (!stateStore_.saveNonces(stateIdentity_, node_.getBufferNonces())) return false;
        if (joined_ && !stateStore_.saveSessionIfChanged(stateIdentity_, node_.getBufferSession())) return false;
        return true;
    }

    bool tryJoin() {
        if (!radioReady_ || !provisioned_ || !persistenceHealthy_) return false;

        lastState_ = node_.activateOTAA();
        joined_ = lastState_ == RADIOLIB_LORAWAN_NEW_SESSION ||
                  lastState_ == RADIOLIB_LORAWAN_SESSION_RESTORED;

        // DevNonce changes on every OTAA attempt, successful or not. Commit the
        // nonce buffer immediately so an unexpected reset cannot reuse it.
        if (!persistJoinState()) {
            persistenceHealthy_ = false;
            joined_ = false;
            return false;
        }

        if (!joined_) nextJoinAtMs_ = millis() + kJoinRetryMs;
        return joined_;
    }

    LoRaWanConfig config_;
    String devEuiText_;
    uint64_t joinEui_ = 0;
    uint64_t devEui_ = 0;
    uint8_t appKey_[16] = {0};
    uint32_t stateIdentity_ = 0;

    Module module_{
        static_cast<uint32_t>(board::LORA_NSS),
        static_cast<uint32_t>(board::LORA_DIO1),
        static_cast<uint32_t>(board::LORA_RST),
        static_cast<uint32_t>(board::LORA_BUSY),
        SPI,
        RADIOLIB_DEFAULT_SPI_SETTINGS};
    SX1262 radio_{&module_};
    mutable LoRaWANNode node_{&radio_, &EU868, 0};
    LoRaWanStateStore stateStore_;

    DownlinkHandler downlinkHandler_ = nullptr;
    void* downlinkContext_ = nullptr;
    bool provisioned_ = false;
    bool radioReady_ = false;
    bool joined_ = false;
    bool persistenceHealthy_ = false;
    int16_t lastState_ = RADIOLIB_ERR_NONE;
    uint32_t nextJoinAtMs_ = 0;
};

} // namespace multibus::lorawan
