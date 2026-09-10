#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <esp_system.h>
#include "core/ConfigStore.h"
#include "core/DeviceConfig.h"
#include "core/SecurityStore.h"
#include "services/NetworkService.h"

namespace multibus {

class WebService {
public:
    bool begin(DeviceConfig& config,
               ConfigStore& configStore,
               SecurityStore& security,
               NetworkService& network) {
        config_ = &config;
        configStore_ = &configStore;
        security_ = &security;
        network_ = &network;

        const char* headers[] = {"Cookie"};
        server_.collectHeaders(headers, 1);

        server_.on("/", HTTP_GET, [this]() { handleRoot(); });
        server_.on("/login", HTTP_POST, [this]() { handleLogin(); });
        server_.on("/logout", HTTP_POST, [this]() { handleLogout(); });
        server_.on("/change-password", HTTP_GET, [this]() { handleChangePasswordPage(); });
        server_.on("/change-password", HTTP_POST, [this]() { handleChangePassword(); });
        server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
        server_.on("/api/config/components", HTTP_POST, [this]() { handleComponentConfig(); });
        server_.on("/api/config/network", HTTP_POST, [this]() { handleNetworkConfig(); });
        server_.on("/api/reboot", HTTP_POST, [this]() { handleReboot(); });
        server_.onNotFound([this]() { handleNotFound(); });

        server_.begin();
        return true;
    }

    void loop() {
        server_.handleClient();
        if (rebootAt_ != 0 && static_cast<int32_t>(millis() - rebootAt_) >= 0) {
            ESP.restart();
        }
    }

private:
    static String htmlHead(const String& title) {
        return "<!doctype html><html><head><meta charset='utf-8'>"
               "<meta name='viewport' content='width=device-width,initial-scale=1'>"
               "<title>" + title + "</title>"
               "<style>body{font-family:sans-serif;max-width:760px;margin:2rem auto;padding:0 1rem}"
               "fieldset{margin:1rem 0;padding:1rem}label{display:block;margin:.6rem 0}"
               "input,select,button{font:inherit;padding:.45rem}code{background:#eee;padding:.15rem .3rem}</style>"
               "</head><body>";
    }

    bool authenticated() const {
        if (sessionToken_.isEmpty()) return false;
        const String cookie = server_.header("Cookie");
        return cookie.indexOf("MBSESSION=" + sessionToken_) >= 0;
    }

    bool requireAuth() {
        if (authenticated()) return true;
        server_.sendHeader("Location", "/");
        server_.send(303, "text/plain", "Authentication required");
        return false;
    }

    static String randomToken() {
        char token[33];
        for (size_t i = 0; i < 16; ++i) {
            const uint8_t value = static_cast<uint8_t>(esp_random() & 0xff);
            snprintf(token + i * 2, 3, "%02x", value);
        }
        token[32] = '\0';
        return String(token);
    }

    void setSessionCookie() {
        server_.sendHeader("Set-Cookie", "MBSESSION=" + sessionToken_ + "; HttpOnly; SameSite=Strict; Path=/");
    }

    void clearSessionCookie() {
        server_.sendHeader("Set-Cookie", "MBSESSION=; Max-Age=0; HttpOnly; SameSite=Strict; Path=/");
    }

    void handleRoot() {
        if (!authenticated()) {
            String html = htmlHead("MultiBus Login");
            html += "<h1>MultiBus Gateway</h1><form method='post' action='/login'>"
                    "<label>Administrator password <input type='password' name='password' required autofocus></label>"
                    "<button type='submit'>Login</button></form></body></html>";
            server_.send(200, "text/html", html);
            return;
        }

        if (!security_->adminInitialized()) {
            server_.sendHeader("Location", "/change-password");
            server_.send(303, "text/plain", "Password change required");
            return;
        }

        String html = htmlHead("MultiBus Gateway");
        html += "<h1>MultiBus Gateway</h1>";
        html += "<p>Network: <code>" + String(network_->apActive() ? "AP" : "client") + "</code> &nbsp; ";
        html += "Address: <code>" + network_->address().toString() + "</code></p>";

        html += "<fieldset><legend>Components</legend><form method='post' action='/api/config/components'>";
        html += select("victron", "Victron", {{"0","Disabled"},{"1","Enabled"}},
                       config_->components.victron == VictronMode::Enabled ? "1" : "0");
        html += select("lora", "LoRa", {{"0","Disabled"},{"W","LoRaWAN"}}, toModeValue(config_->components.lora));
        html += select("modbus", "Modbus", {{"0","Disabled"},{"M","Master"},{"S","Slave"}}, toModeValue(config_->components.modbus));
        html += select("gnss", "GNSS", {{"0","Disabled"},{"1","Enabled"}},
                       config_->components.gnss == GnssMode::Enabled ? "1" : "0");
        html += "<button type='submit'>Save components</button></form></fieldset>";

        html += "<fieldset><legend>Wi-Fi client</legend><form method='post' action='/api/config/network'>"
                "<label>SSID <input name='ssid' value='" + escape(config_->network.ssid) + "' required></label>"
                "<label>Password <input type='password' name='password' placeholder='leave empty to keep current'></label>"
                "<label>Hostname <input name='hostname' value='" + escape(config_->network.hostname) + "'></label>"
                "<label>Friendly name <input name='friendly' value='" + escape(config_->network.friendlyName) + "'></label>"
                "<button type='submit'>Save network and reboot</button></form></fieldset>";

        html += "<form method='get' action='/change-password'><button>Change admin password</button></form> "
                "<form method='post' action='/logout' style='display:inline'><button>Logout</button></form> "
                "<form method='post' action='/api/reboot' style='display:inline'><button>Reboot</button></form>";
        html += "</body></html>";
        server_.send(200, "text/html", html);
    }

    void handleLogin() {
        if (millis() < lockUntil_) {
            server_.send(429, "text/plain", "Too many attempts; try again later.");
            return;
        }

        if (!server_.hasArg("password") || !security_->verifyAdminPassword(server_.arg("password"))) {
            ++failedLogins_;
            if (failedLogins_ >= 5) {
                failedLogins_ = 0;
                lockUntil_ = millis() + 30000;
            }
            server_.send(401, "text/plain", "Invalid credentials");
            return;
        }

        failedLogins_ = 0;
        sessionToken_ = randomToken();
        setSessionCookie();
        server_.sendHeader("Location", security_->adminInitialized() ? "/" : "/change-password");
        server_.send(303, "text/plain", "OK");
    }

    void handleLogout() {
        sessionToken_ = "";
        clearSessionCookie();
        server_.sendHeader("Location", "/");
        server_.send(303, "text/plain", "Logged out");
    }

    void handleChangePasswordPage() {
        if (!requireAuth()) return;
        String html = htmlHead("Change password");
        html += "<h1>Set administrator password</h1>"
                "<form method='post' action='/change-password'>"
                "<label>New password <input type='password' name='password' minlength='10' required></label>"
                "<label>Repeat password <input type='password' name='confirm' minlength='10' required></label>"
                "<button type='submit'>Save password</button></form></body></html>";
        server_.send(200, "text/html", html);
    }

    void handleChangePassword() {
        if (!requireAuth()) return;
        if (!server_.hasArg("password") || !server_.hasArg("confirm") ||
            server_.arg("password") != server_.arg("confirm") ||
            !security_->setAdminPassword(server_.arg("password"))) {
            server_.send(400, "text/plain", "Password must match and contain at least 10 characters.");
            return;
        }
        sessionToken_ = randomToken();
        setSessionCookie();
        server_.sendHeader("Location", "/");
        server_.send(303, "text/plain", "Password changed");
    }

    void handleStatus() {
        if (!requireAuth()) return;
        String body = "{";
        body += "\"network_mode\":\"" + String(network_->apActive() ? "ap" : "client") + "\",";
        body += "\"ip\":\"" + network_->address().toString() + "\",";
        body += "\"hostname\":\"" + jsonEscape(config_->network.hostname) + "\",";
        body += "\"victron\":\"" + String(toString(config_->components.victron)) + "\",";
        body += "\"lora\":\"" + String(toString(config_->components.lora)) + "\",";
        body += "\"modbus\":\"" + String(toString(config_->components.modbus)) + "\",";
        body += "\"gnss\":\"" + String(toString(config_->components.gnss)) + "\"}";
        server_.send(200, "application/json", body);
    }

    void handleComponentConfig() {
        if (!requireAuth()) return;
        AppConfig next = config_->components;
        next.victron = server_.arg("victron") == "1" ? VictronMode::Enabled : VictronMode::Disabled;
        next.lora = server_.arg("lora") == "W" ? LoRaMode::LoRaWAN : LoRaMode::Disabled;
        next.modbus = server_.arg("modbus") == "M" ? ModbusMode::Master :
                      server_.arg("modbus") == "S" ? ModbusMode::Slave : ModbusMode::Disabled;
        next.gnss = server_.arg("gnss") == "1" ? GnssMode::Enabled : GnssMode::Disabled;

        if (validateConfig(next) != ConfigValidationResult::Ok) {
            server_.send(400, "text/plain", "Invalid component configuration");
            return;
        }

        config_->components = next;
        if (!configStore_->save(*config_)) {
            server_.send(500, "text/plain", "Failed to persist configuration");
            return;
        }
        server_.send(200, "text/plain", "Saved. Reboot required to activate component changes.");
    }

    void handleNetworkConfig() {
        if (!requireAuth()) return;
        if (!server_.hasArg("ssid") || server_.arg("ssid").isEmpty()) {
            server_.send(400, "text/plain", "SSID is required");
            return;
        }

        config_->network.ssid = server_.arg("ssid");
        if (server_.hasArg("password") && !server_.arg("password").isEmpty()) {
            config_->network.password = server_.arg("password");
        }
        if (server_.hasArg("hostname")) config_->network.hostname = server_.arg("hostname");
        if (server_.hasArg("friendly")) config_->network.friendlyName = server_.arg("friendly");

        if (!configStore_->save(*config_)) {
            server_.send(500, "text/plain", "Failed to persist network configuration");
            return;
        }

        server_.send(200, "text/plain", "Saved. Rebooting...");
        scheduleReboot();
    }

    void handleReboot() {
        if (!requireAuth()) return;
        server_.send(200, "text/plain", "Rebooting...");
        scheduleReboot();
    }

    void handleNotFound() {
        if (network_ != nullptr && network_->apActive()) {
            server_.sendHeader("Location", "/");
            server_.send(302, "text/plain", "");
            return;
        }
        server_.send(404, "text/plain", "Not found");
    }

    void scheduleReboot() {
        rebootAt_ = millis() + 750;
    }

    struct Option { const char* value; const char* label; };

    static String select(const char* name, const char* label,
                         std::initializer_list<Option> options, const String& selected) {
        String html = "<label>" + String(label) + " <select name='" + name + "'>";
        for (const auto& option : options) {
            html += "<option value='" + String(option.value) + "'";
            if (selected == option.value) html += " selected";
            html += ">" + String(option.label) + "</option>";
        }
        html += "</select></label>";
        return html;
    }

    static String toModeValue(LoRaMode mode) {
        if (mode == LoRaMode::LoRaWAN) return "W";
        if (mode == LoRaMode::Meshtastic) return "M";
        return "0";
    }

    static String toModeValue(ModbusMode mode) {
        if (mode == ModbusMode::Master) return "M";
        if (mode == ModbusMode::Slave) return "S";
        return "0";
    }

    static String escape(String value) {
        value.replace("&", "&amp;");
        value.replace("\"", "&quot;");
        value.replace("'", "&#39;");
        value.replace("<", "&lt;");
        value.replace(">", "&gt;");
        return value;
    }

    static String jsonEscape(String value) {
        value.replace("\\", "\\\\");
        value.replace("\"", "\\\"");
        value.replace("\r", "\\r");
        value.replace("\n", "\\n");
        return value;
    }

    WebServer server_{80};
    DeviceConfig* config_ = nullptr;
    ConfigStore* configStore_ = nullptr;
    SecurityStore* security_ = nullptr;
    NetworkService* network_ = nullptr;
    String sessionToken_;
    uint8_t failedLogins_ = 0;
    uint32_t lockUntil_ = 0;
    uint32_t rebootAt_ = 0;
};

} // namespace multibus
