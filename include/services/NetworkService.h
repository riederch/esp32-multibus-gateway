#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include "core/DeviceConfig.h"
#include "core/DeviceIdentity.h"
#include "core/SecurityStore.h"

namespace multibus {

class NetworkService {
public:
    bool begin(DeviceConfig& config, SecurityStore& security) {
        config_ = &config;
        security_ = &security;

        if (config.network.hostname.isEmpty()) {
            config.network.hostname = defaultHostname();
        }

        if (config.network.configured() && connectClient()) {
            startMdns();
            return true;
        }

        return startAccessPoint();
    }

    void loop() {
        if (apActive_) dns_.processNextRequest();
    }

    bool clientConnected() const {
        return WiFi.status() == WL_CONNECTED;
    }

    bool apActive() const { return apActive_; }

    IPAddress address() const {
        return apActive_ ? WiFi.softAPIP() : WiFi.localIP();
    }

    String hostname() const {
        if (config_ == nullptr) return "";
        return config_->network.hostname;
    }

    String apSsid() const { return apSsid_; }

    String apPassword() const {
        return security_ == nullptr ? "" : security_->apPassword();
    }

    bool startAccessPoint() {
        if (security_ == nullptr || config_ == nullptr) return false;

        WiFi.disconnect(true, false);
        delay(50);
        WiFi.mode(WIFI_AP);

        apSsid_ = defaultApSsid();
        if (!WiFi.softAP(apSsid_.c_str(), security_->apPassword().c_str())) {
            return false;
        }

        apActive_ = true;
        dns_.start(53, "*", WiFi.softAPIP());
        return true;
    }

private:
    bool connectClient() {
        WiFi.mode(WIFI_STA);
        WiFi.setHostname(config_->network.hostname.c_str());
        WiFi.begin(config_->network.ssid.c_str(), config_->network.password.c_str());

        const uint32_t started = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - started < kConnectTimeoutMs) {
            delay(100);
        }

        apActive_ = false;
        return WiFi.status() == WL_CONNECTED;
    }

    void startMdns() {
        if (MDNS.begin(config_->network.hostname.c_str())) {
            MDNS.addService("http", "tcp", 80);
        }
    }

    static constexpr uint32_t kConnectTimeoutMs = 12000;

    DeviceConfig* config_ = nullptr;
    SecurityStore* security_ = nullptr;
    DNSServer dns_;
    bool apActive_ = false;
    String apSsid_;
};

} // namespace multibus
