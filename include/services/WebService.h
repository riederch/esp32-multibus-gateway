#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <esp_system.h>
#include <initializer_list>
#include "core/BackupService.h"
#include "core/ConfigStore.h"
#include "core/DeviceConfig.h"
#include "core/EventBus.h"
#include "core/SecurityStore.h"
#include "services/NetworkService.h"

namespace multibus {

class WebService {
public:
    bool begin(DeviceConfig& config,
               ConfigStore& configStore,
               SecurityStore& security,
               NetworkService& network,
               EventBus<32>& events) {
        config_ = &config;
        configStore_ = &configStore;
        security_ = &security;
        network_ = &network;
        events_ = &events;

        const char* headers[] = {"Cookie", "X-CSRF-Token"};
        server_.collectHeaders(headers, 2);

        server_.on("/", HTTP_GET, [this]() { handleRoot(); });
        server_.on("/login", HTTP_POST, [this]() { handleLogin(); });
        server_.on("/logout", HTTP_POST, [this]() { handleLogout(); });
        server_.on("/change-password", HTTP_GET, [this]() { handleChangePasswordPage(); });
        server_.on("/change-password", HTTP_POST, [this]() { handleChangePassword(); });
        server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
        server_.on("/api/events", HTTP_GET, [this]() { handleEvents(); });
        server_.on("/api/config/components", HTTP_POST, [this]() { handleComponentConfig(); });
        server_.on("/api/config/network", HTTP_POST, [this]() { handleNetworkConfig(); });
        server_.on("/api/config/mqtt", HTTP_POST, [this]() { handleMqttConfig(); });
        server_.on("/api/system/backup", HTTP_GET, [this]() { handleBackupDownload(); });
        server_.on("/api/system/restore", HTTP_POST, [this]() { handleBackupRestore(); });
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
               "input,select,button{font:inherit;padding:.45rem}code{background:#eee;padding:.15rem .3rem}"
               ".warning{background:#fff3cd;padding:.7rem;border-radius:.3rem}</style>"
               "</head><body>";
    }

    bool authenticated() {
        if (sessionToken_.isEmpty()) return false;

        const uint32_t now = millis();
        if (static_cast<uint32_t>(now - sessionCreatedAt_) > kSessionMaxLifetimeMs ||
            static_cast<uint32_t>(now - sessionLastActivityAt_) > kSessionIdleTimeoutMs) {
            invalidateSession();
            return false;
        }

        const String cookie = server_.header("Cookie");
        if (cookie.indexOf("MBSESSION=" + sessionToken_) < 0) return false;
        sessionLastActivityAt_ = now;
        return true;
    }

    bool requireAuth() {
        if (authenticated()) return true;
        server_.send(401, "text/plain", "Authentication required");
        return false;
    }

    bool requireCsrf() {
        if (!requireAuth()) return false;

        String supplied = server_.header("X-CSRF-Token");
        if (supplied.isEmpty() && server_.hasArg("csrf")) supplied = server_.arg("csrf");
        if (csrfToken_.isEmpty() || supplied != csrfToken_) {
            server_.send(403, "text/plain", "CSRF validation failed");
            return false;
        }
        return true;
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

    void createSession() {
        sessionToken_ = randomToken();
        csrfToken_ = randomToken();
        sessionCreatedAt_ = millis();
        sessionLastActivityAt_ = sessionCreatedAt_;
    }

    void invalidateSession() {
        sessionToken_ = "";
        csrfToken_ = "";
        sessionCreatedAt_ = 0;
        sessionLastActivityAt_ = 0;
    }

    String csrfField() const {
        return "<input type='hidden' name='csrf' value='" + csrfToken_ + "'>";
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
            server_.sendHeader("Cache-Control", "no-store");
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

        html += "<fieldset><legend>Components</legend><form method='post' action='/api/config/components'>" + csrfField();
        html += select("victron", "Victron", {{"0","Disabled"},{"1","Enabled"}},
                       config_->components.victron == VictronMode::Enabled ? "1" : "0");
        html += select("lora", "LoRa", {{"0","Disabled"},{"W","LoRaWAN"}}, toModeValue(config_->components.lora));
        html += select("modbus", "Modbus", {{"0","Disabled"},{"M","Master"},{"S","Slave"}}, toModeValue(config_->components.modbus));
        html += select("gnss", "GNSS", {{"0","Disabled"},{"1","Enabled"}},
                       config_->components.gnss == GnssMode::Enabled ? "1" : "0");
        html += "<button type='submit'>Save components</button></form></fieldset>";

        html += "<fieldset><legend>Wi-Fi client</legend><form method='post' action='/api/config/network'>" + csrfField() +
                "<label>SSID <input name='ssid' value='" + escape(config_->network.ssid) + "' required></label>"
                "<label>Password <input type='password' name='password' placeholder='leave empty to keep current'></label>"
                "<label>Hostname <input name='hostname' value='" + escape(config_->network.hostname) + "'></label>"
                "<label>Friendly name <input name='friendly' value='" + escape(config_->network.friendlyName) + "'></label>"
                "<button type='submit'>Save network and reboot</button></form></fieldset>";

        html += "<fieldset><legend>MQTT</legend><form method='post' action='/api/config/mqtt'>" + csrfField() +
                "<label><input type='checkbox' name='enabled' value='1'" +
                String(config_->mqtt.enabled ? " checked" : "") +
                "> Enable MQTT</label>"
                "<label>Broker host <input name='host' value='" + escape(config_->mqtt.host) + "'></label>"
                "<label>Broker port <input type='number' min='1' max='65535' name='port' value='" +
                String(config_->mqtt.port) + "' required></label>"
                "<label>Username <input name='username' value='" + escape(config_->mqtt.username) + "'></label>"
                "<label>Password <input type='password' name='password' placeholder='leave empty to keep current'></label>"
                "<label>Topic prefix <input name='topic_prefix' value='" + escape(config_->mqtt.topicPrefix) + "' required></label>"
                "<label>Publish interval (seconds) <input type='number' min='1' max='3600' name='publish_interval' value='" +
                String(config_->mqtt.publishIntervalSeconds) + "' required></label>"
                "<label><input type='checkbox' name='retain_state' value='1'" +
                String(config_->mqtt.retainState ? " checked" : "") +
                "> Retain channel state</label>"
                "<label><input type='checkbox' name='ha_discovery' value='1'" +
                String(config_->mqtt.homeAssistantDiscovery ? " checked" : "") +
                "> Enable Home Assistant discovery</label>"
                "<label>Home Assistant discovery prefix <input name='ha_prefix' value='" +
                escape(config_->mqtt.homeAssistantPrefix) + "' required></label>"
                "<label><input type='checkbox' name='tls_enabled' value='1'" +
                String(config_->mqtt.tlsEnabled ? " checked" : "") +
                "> Enable TLS with CA verification</label>"
                "<label>CA certificate (PEM)<br><textarea name='ca_certificate' rows='10' cols='72'>" +
                escape(config_->mqtt.caCertificate) + "</textarea></label>"
                "<button type='submit'>Save MQTT and reboot</button></form></fieldset>";

        html += "<fieldset><legend>Recent Events</legend>"
                "<div id='eventNotice'></div><pre id='eventList'>Loading...</pre></fieldset>";

        html += "<fieldset><legend>Backup / Restore</legend>"
                "<p class='warning'>Current backup files contain configuration secrets in clear text. Store them securely.</p>"
                "<p><a href='/api/system/backup'>Download configuration backup</a></p>"
                "<label>Restore backup <input id='restoreFile' type='file' accept='application/json,.json'></label>"
                "<button type='button' onclick='restoreBackup()'>Upload and restore</button>"
                "<pre id='restoreStatus'></pre></fieldset>";

        html += "<form method='get' action='/change-password'><button>Change admin password</button></form> ";
        html += "<form method='post' action='/logout' style='display:inline'>" + csrfField() +
                "<button>Logout</button></form> ";
        html += "<form method='post' action='/api/reboot' style='display:inline'>" + csrfField() +
                "<button>Reboot</button></form>";
        html += "<script>"
                "const csrfToken='" + csrfToken_ + "';"
                "let eventAfter=0,eventLines=[];"
                "async function loadEvents(){"
                "try{const r=await fetch('/api/events?after='+eventAfter,{cache:'no-store'});"
                "if(!r.ok)return;const d=await r.json();"
                "if(d.missed_before>0)document.getElementById('eventNotice').textContent="
                "'Missed '+d.missed_before+' event(s) before the current ring.';"
                "for(const e of d.events){"
                "eventAfter=Math.max(eventAfter,Number(e.sequence)||0);"
                "const ts=e.timestamp?new Date(e.timestamp*1000).toISOString():'no-time';"
                "eventLines.push('#'+e.sequence+' '+ts+' ['+e.severity+'] '+e.source+'/'+e.type+(e.detail?' '+e.detail:''));"
                "}if(eventLines.length>32)eventLines=eventLines.slice(-32);"
                "document.getElementById('eventList').textContent=eventLines.length?eventLines.join('\\n'):'No events';"
                "}catch(e){document.getElementById('eventNotice').textContent='Event API unavailable';}}"
                "async function restoreBackup(){const f=document.getElementById('restoreFile').files[0];"
                "if(!f)return;const s=document.getElementById('restoreStatus');s.textContent='Uploading...';"
                "const r=await fetch('/api/system/restore',{method:'POST',headers:{'Content-Type':'application/json','X-CSRF-Token':csrfToken},body:await f.text()});"
                "s.textContent=await r.text();}"
                "loadEvents();setInterval(loadEvents,5000);"
                "</script></body></html>";
        server_.sendHeader("Cache-Control", "no-store");
        server_.send(200, "text/html", html);
    }

    void handleLogin() {
        if (static_cast<int32_t>(millis() - lockUntil_) < 0) {
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
        createSession();
        setSessionCookie();
        server_.sendHeader("Location", security_->adminInitialized() ? "/" : "/change-password");
        server_.send(303, "text/plain", "OK");
    }

    void handleLogout() {
        if (!requireCsrf()) return;
        invalidateSession();
        clearSessionCookie();
        server_.sendHeader("Location", "/");
        server_.send(303, "text/plain", "Logged out");
    }

    void handleChangePasswordPage() {
        if (!requireAuth()) return;
        String html = htmlHead("Change password");
        html += "<h1>Set administrator password</h1>"
                "<form method='post' action='/change-password'>" + csrfField() +
                "<label>New password <input type='password' name='password' minlength='10' required></label>"
                "<label>Repeat password <input type='password' name='confirm' minlength='10' required></label>"
                "<button type='submit'>Save password</button></form></body></html>";
        server_.sendHeader("Cache-Control", "no-store");
        server_.send(200, "text/html", html);
    }

    void handleChangePassword() {
        if (!requireCsrf()) return;
        if (!server_.hasArg("password") || !server_.hasArg("confirm") ||
            server_.arg("password") != server_.arg("confirm") ||
            !security_->setAdminPassword(server_.arg("password"))) {
            server_.send(400, "text/plain", "Password must match and contain at least 10 characters.");
            return;
        }
        createSession();
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
        body += "\"gnss\":\"" + String(toString(config_->components.gnss)) + "\",";
        body += "\"mqtt_enabled\":" + String(config_->mqtt.enabled ? "true" : "false") + ",";
        body += "\"mqtt_host\":\"" + jsonEscape(config_->mqtt.host) + "\"}";
        server_.sendHeader("Cache-Control", "no-store");
        server_.send(200, "application/json", body);
    }

    static bool parseUint64(const String& value, uint64_t& output) {
        if (value.isEmpty()) {
            output = 0;
            return true;
        }

        uint64_t parsed = 0;
        for (size_t i = 0; i < value.length(); ++i) {
            const char ch = value.charAt(i);
            if (ch < '0' || ch > '9') return false;
            const uint8_t digit = static_cast<uint8_t>(ch - '0');
            if (parsed > (UINT64_MAX - digit) / 10U) return false;
            parsed = parsed * 10U + digit;
        }
        output = parsed;
        return true;
    }

    static const char* eventSeverityName(EventSeverity severity) {
        switch (severity) {
            case EventSeverity::Info: return "info";
            case EventSeverity::Warning: return "warning";
            case EventSeverity::Error: return "error";
            default: return "info";
        }
    }

    void handleEvents() {
        if (!requireAuth()) return;
        if (events_ == nullptr) {
            server_.send(503, "text/plain", "Event bus unavailable");
            return;
        }

        uint64_t after = 0;
        if (server_.hasArg("after") && !parseUint64(server_.arg("after"), after)) {
            server_.send(400, "text/plain", "Invalid after sequence");
            return;
        }

        JsonDocument doc;
        const uint64_t oldest = events_->oldestSequence();
        const uint64_t latest = events_->latestSequence();
        doc["oldest_sequence"] = oldest;
        doc["latest_sequence"] = latest;
        doc["missed_before"] = events_->missedSince(after);

        JsonArray items = doc["events"].to<JsonArray>();
        if (oldest != 0 && after < latest) {
            uint64_t sequence = after == UINT64_MAX ? UINT64_MAX : after + 1U;
            if (sequence < oldest) sequence = oldest;

            while (sequence <= latest && items.size() < events_->capacity()) {
                EventRecord event;
                if (events_->read(sequence, event)) {
                    JsonObject item = items.add<JsonObject>();
                    item["sequence"] = event.sequence;
                    item["timestamp"] = event.timestampUnix;
                    item["severity"] = eventSeverityName(event.severity);
                    item["source"] = event.source;
                    item["type"] = event.type;
                    if (event.detail[0] != '\0') item["detail"] = event.detail;
                }
                if (sequence == UINT64_MAX) break;
                ++sequence;
            }
        }

        String body;
        serializeJson(doc, body);
        server_.sendHeader("Cache-Control", "no-store");
        server_.send(200, "application/json", body);
    }

    void handleComponentConfig() {
        if (!requireCsrf()) return;
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
        if (!requireCsrf()) return;
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

    static bool parseUint16Arg(const String& value,
                               uint16_t minimum,
                               uint16_t maximum,
                               uint16_t& output) {
        if (value.isEmpty()) return false;
        uint32_t parsed = 0;
        for (size_t i = 0; i < value.length(); ++i) {
            const char ch = value.charAt(i);
            if (ch < '0' || ch > '9') return false;
            parsed = parsed * 10U + static_cast<uint32_t>(ch - '0');
            if (parsed > maximum) return false;
        }
        if (parsed < minimum || parsed > maximum) return false;
        output = static_cast<uint16_t>(parsed);
        return true;
    }

    void handleMqttConfig() {
        if (!requireCsrf()) return;

        MqttConfig next = config_->mqtt;
        next.enabled = server_.hasArg("enabled") && server_.arg("enabled") == "1";
        next.host = server_.arg("host");
        next.username = server_.arg("username");
        if (server_.hasArg("password") && !server_.arg("password").isEmpty()) {
            next.password = server_.arg("password");
        }
        next.topicPrefix = server_.arg("topic_prefix");
        next.retainState =
            server_.hasArg("retain_state") && server_.arg("retain_state") == "1";
        next.homeAssistantDiscovery =
            server_.hasArg("ha_discovery") && server_.arg("ha_discovery") == "1";
        next.homeAssistantPrefix = server_.arg("ha_prefix");
        next.tlsEnabled =
            server_.hasArg("tls_enabled") && server_.arg("tls_enabled") == "1";
        if (server_.hasArg("ca_certificate")) {
            next.caCertificate = server_.arg("ca_certificate");
        }

        uint16_t port = 0;
        uint16_t publishInterval = 0;
        if (!parseUint16Arg(server_.arg("port"), 1, 65535, port) ||
            !parseUint16Arg(server_.arg("publish_interval"), 1, 3600, publishInterval)) {
            server_.send(400, "text/plain", "Invalid MQTT port or publish interval");
            return;
        }
        next.port = port;
        next.publishIntervalSeconds = publishInterval;

        if (!next.valid()) {
            server_.send(400, "text/plain", "Invalid MQTT configuration");
            return;
        }

        config_->mqtt = next;
        if (!configStore_->save(*config_)) {
            server_.send(500, "text/plain", "Failed to persist MQTT configuration");
            return;
        }

        server_.send(200, "text/plain", "Saved. Rebooting...");
        scheduleReboot();
    }

    void handleBackupDownload() {
        if (!requireAuth()) return;
        String backup;
        if (!backupService_.exportConfig(*config_, backup)) {
            server_.send(500, "text/plain", "Failed to generate backup");
            return;
        }
        server_.sendHeader("Content-Disposition", "attachment; filename=multibus-backup.json");
        server_.sendHeader("Cache-Control", "no-store");
        server_.send(200, "application/json", backup);
    }

    void handleBackupRestore() {
        if (!requireCsrf()) return;
        const String body = server_.arg("plain");
        if (body.isEmpty()) {
            server_.send(400, "text/plain", "Backup payload is empty");
            return;
        }

        DeviceConfig restored;
        String error;
        if (!backupService_.importConfig(body, restored, error)) {
            server_.send(400, "text/plain", "Backup rejected: " + error);
            return;
        }

        if (!configStore_->save(restored)) {
            server_.send(500, "text/plain", "Validated backup could not be persisted");
            return;
        }

        *config_ = restored;
        createSession();
        setSessionCookie();
        server_.send(200, "text/plain", "Backup restored successfully. Rebooting...");
        scheduleReboot();
    }

    void handleReboot() {
        if (!requireCsrf()) return;
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
    BackupService backupService_;
    DeviceConfig* config_ = nullptr;
    ConfigStore* configStore_ = nullptr;
    SecurityStore* security_ = nullptr;
    NetworkService* network_ = nullptr;
    EventBus<32>* events_ = nullptr;
    static constexpr uint32_t kSessionIdleTimeoutMs = 30UL * 60UL * 1000UL;
    static constexpr uint32_t kSessionMaxLifetimeMs = 12UL * 60UL * 60UL * 1000UL;

    String sessionToken_;
    String csrfToken_;
    uint32_t sessionCreatedAt_ = 0;
    uint32_t sessionLastActivityAt_ = 0;
    uint8_t failedLogins_ = 0;
    uint32_t lockUntil_ = 0;
    uint32_t rebootAt_ = 0;
};

} // namespace multibus
