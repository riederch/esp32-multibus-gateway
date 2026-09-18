#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "core/ChannelRegistry.h"
#include "core/DeviceConfig.h"
#include "mqtt/MqttProtocol.h"

namespace multibus {

class MqttService {
public:
    using ReadHandler = bool (*)(void*, const ChannelBinding&, DataValue&);
    using DescribeHandler = bool (*)(void*, const ChannelBinding&, DataPointDescriptor&);
    using CommandHandler = bool (*)(void*, const ChannelBinding&, const uint8_t*, size_t);

    bool begin(const MqttConfig& config,
               const String& deviceId,
               ChannelRegistry& channels,
               ReadHandler readHandler,
               DescribeHandler describeHandler,
               CommandHandler commandHandler,
               void* context) {
        config_ = &config;
        channels_ = &channels;
        readHandler_ = readHandler;
        describeHandler_ = describeHandler;
        commandHandler_ = commandHandler;
        context_ = context;
        deviceId_ = deviceId;

        if (!config_->valid()) return false;
        if (!config_->enabled) return true;
        if (!mqtt::validTopicPrefix(config_->topicPrefix.c_str()) ||
            !mqtt::validDeviceId(deviceId_.c_str())) {
            return false;
        }

        if (config_->tlsEnabled) {
            secureClient_.setCACert(config_->caCertificate.c_str());
            client_.setClient(secureClient_);
        } else {
            client_.setClient(networkClient_);
        }
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
        refreshHomeAssistantDiscovery();

        const uint32_t now = millis();
        if (static_cast<int32_t>(now - nextPublishAtMs_) >= 0) {
            publishAll();
            nextPublishAtMs_ =
                now + static_cast<uint32_t>(config_->publishIntervalSeconds) * 1000UL;
        }
    }

    bool connected() const {
        return config_ != nullptr && config_->enabled && connected_;
    }

    uint32_t publishes() const { return publishes_; }
    uint32_t publishFailures() const { return publishFailures_; }
    uint32_t commandMessages() const { return commandMessages_; }
    uint32_t commandFailures() const { return commandFailures_; }
    uint32_t discoveryPublishes() const { return discoveryPublishes_; }
    uint32_t discoveryFailures() const { return discoveryFailures_; }

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
        subscribeCommandTopics();
        discoverySignature_ = 0;
        refreshHomeAssistantDiscovery(true);
        publishAll();
        nextPublishAtMs_ =
            millis() + static_cast<uint32_t>(config_->publishIntervalSeconds) * 1000UL;
        return true;
    }

    void subscribeCommandTopics() {
        if (config_ == nullptr) return;

        char topic[192] = {0};
        if (!mqtt::makeChannelCommandWildcard(
                config_->topicPrefix.c_str(),
                deviceId_.c_str(),
                topic,
                sizeof(topic))) {
            return;
        }
        client_.subscribe(topic);
    }

    static uint32_t hashBytes(uint32_t hash, const uint8_t* data, size_t length) {
        for (size_t i = 0; i < length; ++i) {
            hash ^= data[i];
            hash *= 16777619UL;
        }
        return hash;
    }

    static uint32_t hashString(uint32_t hash, const String& value) {
        return hashBytes(
            hash,
            reinterpret_cast<const uint8_t*>(value.c_str()),
            value.length());
    }

    uint32_t discoverySignature() {
        if (channels_ == nullptr || describeHandler_ == nullptr) return 0;

        uint32_t hash = 2166136261UL;
        for (const auto& binding : channels_->all()) {
            if (!binding.enabled) continue;
            DataPointDescriptor descriptor;
            if (!describeHandler_(context_, binding, descriptor)) continue;

            const uint8_t idBytes[2] = {
                static_cast<uint8_t>(binding.channelId & 0xffU),
                static_cast<uint8_t>((binding.channelId >> 8U) & 0xffU)
            };
            hash = hashBytes(hash, idBytes, sizeof(idBytes));
            const uint8_t flags[2] = {
                static_cast<uint8_t>(binding.writable ? 1 : 0),
                static_cast<uint8_t>(descriptor.type)
            };
            hash = hashBytes(hash, flags, sizeof(flags));
            hash = hashString(hash, binding.sourceId);
            hash = hashString(hash, binding.pointId);
            hash = hashString(hash, binding.unit);
        }
        return hash;
    }

    const char* discoveryComponent(const ChannelBinding& binding,
                                   const DataPointDescriptor& descriptor) const {
        if (binding.writable && descriptor.writable) {
            if (descriptor.type == DataType::Boolean) return "switch";
            if (descriptor.type == DataType::Int64 ||
                descriptor.type == DataType::UInt64 ||
                descriptor.type == DataType::Float64) {
                return "number";
            }
        }
        if (descriptor.type == DataType::Boolean) return "binary_sensor";
        return "sensor";
    }

    struct DiscoveryEntry {
        uint16_t channelId = 0;
        char component[16] = {0};
    };

    static bool sameDiscoveryEntry(const DiscoveryEntry& a,
                                   const DiscoveryEntry& b) {
        return a.channelId == b.channelId &&
               strcmp(a.component, b.component) == 0;
    }

    bool clearHomeAssistantDiscovery(const DiscoveryEntry& entry) {
        char topic[224] = {0};
        if (!mqtt::makeHomeAssistantDiscoveryTopic(
                config_->homeAssistantPrefix.c_str(),
                entry.component,
                deviceId_.c_str(),
                entry.channelId,
                topic,
                sizeof(topic))) {
            ++discoveryFailures_;
            return false;
        }
        if (!client_.publish(topic, "", true)) {
            ++discoveryFailures_;
            return false;
        }
        return true;
    }

    void refreshHomeAssistantDiscovery(bool force = false) {
        if (config_ == nullptr || !config_->homeAssistantDiscovery ||
            channels_ == nullptr || describeHandler_ == nullptr ||
            !client_.connected()) {
            return;
        }

        const uint32_t signature = discoverySignature();
        if (!force && signature == discoverySignature_) return;

        DiscoveryEntry desired[ChannelRegistry::kRecommendedMaxChannels];
        size_t desiredCount = 0;

        for (const auto& binding : channels_->all()) {
            if (!binding.enabled) continue;
            DataPointDescriptor descriptor;
            if (!describeHandler_(context_, binding, descriptor)) continue;
            if (desiredCount >= ChannelRegistry::kRecommendedMaxChannels) {
                ++discoveryFailures_;
                break;
            }

            DiscoveryEntry& entry = desired[desiredCount++];
            entry.channelId = binding.channelId;
            snprintf(
                entry.component,
                sizeof(entry.component),
                "%s",
                discoveryComponent(binding, descriptor));
        }

        for (size_t i = 0; i < discoveredCount_; ++i) {
            bool stillPresent = false;
            for (size_t j = 0; j < desiredCount; ++j) {
                if (sameDiscoveryEntry(discovered_[i], desired[j])) {
                    stillPresent = true;
                    break;
                }
            }
            if (!stillPresent) clearHomeAssistantDiscovery(discovered_[i]);
        }

        size_t desiredIndex = 0;
        for (const auto& binding : channels_->all()) {
            if (!binding.enabled) continue;
            DataPointDescriptor descriptor;
            if (!describeHandler_(context_, binding, descriptor)) continue;
            if (desiredIndex >= desiredCount) break;
            publishHomeAssistantDiscovery(binding, descriptor);
            ++desiredIndex;
        }

        discoveredCount_ = desiredCount;
        for (size_t i = 0; i < desiredCount; ++i) {
            discovered_[i] = desired[i];
        }
        discoverySignature_ = signature;
    }

    bool publishHomeAssistantDiscovery(const ChannelBinding& binding,
                                       const DataPointDescriptor& descriptor) {
        const char* component = discoveryComponent(binding, descriptor);

        char discoveryTopic[224] = {0};
        char stateTopic[192] = {0};
        char availabilityTopic[192] = {0};
        char commandTopic[192] = {0};

        if (!mqtt::makeHomeAssistantDiscoveryTopic(
                config_->homeAssistantPrefix.c_str(),
                component,
                deviceId_.c_str(),
                binding.channelId,
                discoveryTopic,
                sizeof(discoveryTopic)) ||
            !mqtt::makeChannelStateTopic(
                config_->topicPrefix.c_str(),
                deviceId_.c_str(),
                binding.channelId,
                stateTopic,
                sizeof(stateTopic)) ||
            !mqtt::makeAvailabilityTopic(
                config_->topicPrefix.c_str(),
                deviceId_.c_str(),
                availabilityTopic,
                sizeof(availabilityTopic))) {
            ++discoveryFailures_;
            return false;
        }

        const bool writableEntity =
            strcmp(component, "switch") == 0 || strcmp(component, "number") == 0;
        if (writableEntity &&
            !mqtt::makeChannelCommandTopic(
                config_->topicPrefix.c_str(),
                deviceId_.c_str(),
                binding.channelId,
                commandTopic,
                sizeof(commandTopic))) {
            ++discoveryFailures_;
            return false;
        }

        JsonDocument doc;
        String uniqueId = deviceId_ + "_ch" + String(binding.channelId);
        doc["name"] = binding.pointId.isEmpty()
            ? String("Channel ") + String(binding.channelId)
            : binding.pointId;
        doc["unique_id"] = uniqueId;
        doc["state_topic"] = stateTopic;
        doc["availability_topic"] = availabilityTopic;

        if (!binding.unit.isEmpty()) {
            doc["unit_of_measurement"] = binding.unit;
        }

        if (strcmp(component, "binary_sensor") == 0) {
            doc["value_template"] = "{{ value_json.value | string | lower }}";
            doc["payload_on"] = "true";
            doc["payload_off"] = "false";
        } else if (strcmp(component, "switch") == 0) {
            doc["value_template"] = "{{ value_json.value | string | lower }}";
            doc["state_on"] = "true";
            doc["state_off"] = "false";
            doc["command_topic"] = commandTopic;
            doc["payload_on"] = "{\"value\":true}";
            doc["payload_off"] = "{\"value\":false}";
        } else if (strcmp(component, "number") == 0) {
            doc["value_template"] = "{{ value_json.value }}";
            doc["command_topic"] = commandTopic;
            doc["command_template"] = "{\"value\": {{ value }}}";
        } else {
            doc["value_template"] = "{{ value_json.value }}";
        }

        JsonObject device = doc["device"].to<JsonObject>();
        JsonArray identifiers = device["identifiers"].to<JsonArray>();
        identifiers.add(deviceId_);
        device["name"] = deviceId_;
        device["manufacturer"] = "MultiBus";
        device["model"] = "ESP32 MultiBus Gateway";

        String payload;
        serializeJson(doc, payload);
        if (!client_.publish(discoveryTopic, payload.c_str(), true)) {
            ++discoveryFailures_;
            return false;
        }

        ++discoveryPublishes_;
        return true;
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
    DescribeHandler describeHandler_ = nullptr;
    CommandHandler commandHandler_ = nullptr;
    void* context_ = nullptr;
    String deviceId_;
    WiFiClient networkClient_;
    WiFiClientSecure secureClient_;
    PubSubClient client_;
    uint32_t nextReconnectAtMs_ = 0;
    uint32_t nextPublishAtMs_ = 0;
    uint32_t publishes_ = 0;
    uint32_t publishFailures_ = 0;
    uint32_t commandMessages_ = 0;
    uint32_t commandFailures_ = 0;
    uint32_t discoveryPublishes_ = 0;
    uint32_t discoveryFailures_ = 0;
    uint32_t discoverySignature_ = 0;
    DiscoveryEntry discovered_[ChannelRegistry::kRecommendedMaxChannels];
    size_t discoveredCount_ = 0;
    bool connected_ = false;
};

} // namespace multibus
