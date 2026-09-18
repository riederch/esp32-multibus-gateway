#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include "core/ChannelRegistry.h"
#include "core/DeviceConfig.h"
#include "mqtt/MqttProtocol.h"

namespace multibus {

class MqttService {
public:
    using ReadHandler = bool (*)(void*, const ChannelBinding&, DataValue&);
    using CommandHandler = bool (*)(void*, const ChannelBinding&, const uint8_t*, size_t);

    bool begin(const MqttConfig& config,
               const String& deviceId,
               ChannelRegistry& channels,
               ReadHandler readHandler,
               CommandHandler commandHandler,
               void* context) {
        config_ = &config;
        channels_ = &channels;
        readHandler_ = readHandler;
        commandHandler_ = commandHandler;
        context_ = context;
        deviceId_ = deviceId;

        if (!config_->valid() ||
            !mqtt::validTopicPrefix(config_->topicPrefix.c_str()) ||
            !mqtt::validDeviceId(deviceId_.c_str())) {
            return false;
        }

        client_.setClient(networkClient_);
        client_.setServer(config_->host.c_str(), config_->port);
        client_.setBufferSize(768);
        client_.setCallback([this](char* topic, uint8_t* payload, unsigned int length) {
            handleMessage(topic, payload, length);
        });

        nextReconnectAtMs_ = millis();
        nextPublishAtMs_ = millis();
        return true;
    }

    void loop() {
        if (config_ == nullptr || !config_->enabled) return;
        if (WiFi.status() != WL_CONNECTED) {
            connected_ = false;
            return;
        }

        if (!client_.connected()) {
            connected_ = false;
            const uint32_t now = millis();
            if (static_cast<int32_t>(now - nextReconnectAtMs_) < 0) return;
            nextReconnectAtMs_ = now + kReconnectIntervalMs;
            connect();
            return;
        }

        client_.loop();
        connected_ = true;

        const uint32_t now = millis();
        if (static_cast<int32_t>(now - nextPublishAtMs_) >= 0) {
            publishAll();
            nextPublishAtMs_ =
                now + static_cast<uint32_t>(config_->publishIntervalSeconds) * 1000UL;
        }
    }

    bool connected() const {
        return config_ != nullptr && config_->enabled && connected_ && client_.connected();
    }

    uint32_t publishes() const { return publishes_; }
    uint32_t publishFailures() const { return publishFailures_; }
    uint32_t commandMessages() const { return commandMessages_; }
    uint32_t commandFailures() const { return commandFailures_; }

private:
    bool connect() {
        if (config_ == nullptr || !config_->enabled || config_->host.isEmpty()) return false;

        char statusTopic[192] = {0};
        if (!mqtt::makeAvailabilityTopic(
                config_->topicPrefix.c_str(),
                deviceId_.c_str(),
                statusTopic,
                sizeof(statusTopic))) {
            return false;
        }

        String clientId = deviceId_ + "-mqtt";
        bool ok = false;
        if (config_->username.isEmpty()) {
            ok = client_.connect(
                clientId.c_str(),
                statusTopic,
                0,
                true,
                "offline");
        } else {
            ok = client_.connect(
                clientId.c_str(),
                config_->username.c_str(),
                config_->password.c_str(),
                statusTopic,
                0,
                true,
                "offline");
        }
        if (!ok) return false;

        connected_ = true;
        client_.publish(statusTopic, "online", true);
        subscribeWritableChannels();
        publishAll();
        nextPublishAtMs_ =
            millis() + static_cast<uint32_t>(config_->publishIntervalSeconds) * 1000UL;
        return true;
    }

    void subscribeWritableChannels() {
        if (channels_ == nullptr || config_ == nullptr) return;

        char topic[192] = {0};
        for (const auto& binding : channels_->all()) {
            if (!binding.enabled || !binding.writable) continue;
            if (!mqtt::makeChannelCommandTopic(
                    config_->topicPrefix.c_str(),
                    deviceId_.c_str(),
                    binding.channelId,
                    topic,
                    sizeof(topic))) {
                continue;
            }
            client_.subscribe(topic);
        }
    }

    void publishAll() {
        if (channels_ == nullptr || readHandler_ == nullptr || config_ == nullptr) return;

        for (const auto& binding : channels_->all()) {
            if (!binding.enabled) continue;

            DataValue value;
            if (!readHandler_(context_, binding, value) || !value.valid) continue;
            publishChannel(binding, value);
        }
    }

    bool publishChannel(const ChannelBinding& binding, const DataValue& value) {
        char topic[192] = {0};
        if (!mqtt::makeChannelStateTopic(
                config_->topicPrefix.c_str(),
                deviceId_.c_str(),
                binding.channelId,
                topic,
                sizeof(topic))) {
            ++publishFailures_;
            return false;
        }

        JsonDocument doc;
        doc["channel_id"] = binding.channelId;
        doc["source"] = binding.sourceId;
        doc["point"] = binding.pointId;
        doc["unit"] = binding.unit;
        doc["valid"] = value.valid;

        switch (value.type) {
            case DataType::Boolean:
                doc["value"] = value.booleanValue;
                doc["type"] = "bool";
                break;
            case DataType::Int64:
                doc["value"] = value.intValue;
                doc["type"] = "int64";
                break;
            case DataType::UInt64:
                doc["value"] = value.uintValue;
                doc["type"] = "uint64";
                break;
            case DataType::Float64:
                doc["value"] = value.floatValue;
                doc["type"] = "float64";
                break;
            case DataType::Text:
                doc["value"] = value.textValue;
                doc["type"] = "text";
                break;
        }

        String payload;
        serializeJson(doc, payload);
        if (!client_.publish(topic, payload.c_str(), config_->retainState)) {
            ++publishFailures_;
            return false;
        }
        ++publishes_;
        return true;
    }

    void handleMessage(const char* topic, const uint8_t* payload, size_t length) {
        if (config_ == nullptr || channels_ == nullptr || commandHandler_ == nullptr) return;

        const mqtt::CommandTopic parsed = mqtt::parseChannelCommandTopic(
            config_->topicPrefix.c_str(),
            deviceId_.c_str(),
            topic);
        if (!parsed.valid) return;

        const ChannelBinding* binding = channels_->find(parsed.channelId);
        if (binding == nullptr || !binding->enabled || !binding->writable) {
            ++commandFailures_;
            return;
        }

        ++commandMessages_;
        if (!commandHandler_(context_, *binding, payload, length)) {
            ++commandFailures_;
        }
    }

    static constexpr uint32_t kReconnectIntervalMs = 5000;

    const MqttConfig* config_ = nullptr;
    ChannelRegistry* channels_ = nullptr;
    ReadHandler readHandler_ = nullptr;
    CommandHandler commandHandler_ = nullptr;
    void* context_ = nullptr;
    String deviceId_;
    WiFiClient networkClient_;
    PubSubClient client_;
    uint32_t nextReconnectAtMs_ = 0;
    uint32_t nextPublishAtMs_ = 0;
    uint32_t publishes_ = 0;
    uint32_t publishFailures_ = 0;
    uint32_t commandMessages_ = 0;
    uint32_t commandFailures_ = 0;
    bool connected_ = false;
};

} // namespace multibus
