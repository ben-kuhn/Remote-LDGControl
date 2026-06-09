#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"
#include "config_manager.h"
#include "tuner_protocol.h"
#include "mqtt_handler.h"
#include "web_server.h"
#include "portal_cert.h"

#ifdef WITH_DISPLAY
#include "display_ui.h"
#endif

// Global instances
TunerProtocol tuner;
MqttHandler mqtt;
LdgWebServer webServer;

#ifdef WITH_DISPLAY
DisplayUI display;
#endif

// WiFi management
static uint32_t lastWiFiCheck = 0;
static bool wifiConnected = false;

// Meter update tracking
static uint32_t lastMeterPublish = 0;
static uint32_t lastHeartbeat = 0;

// Remote unit state
static bool remoteUnitConnected = false;
static bool usingRemoteTuner = false;
static tuner_meter_t s_remoteMeter;

#ifdef REMOTE_UNIT
// SSE client for remote unit.
//
// Threat model for the display↔remote link:
//   - The connection is TLS-encrypted (WiFiClientSecure).
//   - The display unit's cert is self-signed and unique-per-device; the
//     remote unit accepts it without verification via setInsecure(). This
//     protects against passive sniffing on the shared WiFi but not active
//     MITM (an attacker on the same AP who can intercept TCP could swap
//     in their own cert and watch the Basic Auth password go by).
//   - Authentication of *requests* is HTTP Basic Auth using the user-
//     configured webUsername/webPassword (set during portal setup).
//   - This is intentional for a "mostly-private-network" deployment with
//     no user-managed PKI. Don't expose the link across an untrusted hop.
#include <WiFiClientSecure.h>
static WiFiClientSecure s_sseClient;
static bool s_sseConnected = false;
static uint32_t s_lastSseReconnect = 0;
#endif

// ============================================================
// Meter Getter (for web server)
// ============================================================

const tuner_meter_t* getActiveMeter() {
    if (usingRemoteTuner) {
        return &s_remoteMeter;
    }
    return tuner.getMeterData();
}

// ============================================================
// WiFi Setup (using WiFiManager)
// ============================================================

void setupWiFi() {
    String hostname = HOSTNAME_PREFIX;
    hostname += "-";
    hostname += WiFi.macAddress();
    hostname.replace(":", "");

    WiFi.setHostname(hostname.c_str());

    // WiFiManager handles captive portal and credential storage
    bool connected = configManager.setupWiFi();

    if (connected) {
        wifiConnected = true;
        Serial.printf("\nWiFi connected: %s\n", WiFi.localIP().toString().c_str());

        if (MDNS.begin(hostname.c_str())) {
            Serial.printf("mDNS responder started: %s.local\n", hostname.c_str());
            MDNS.addService("http", "tcp", HTTP_PORT);
        }
    } else {
        wifiConnected = false;
        Serial.println("\nWiFi connection failed or config portal timed out");
    }
}

// ============================================================
// Filesystem
// ============================================================

bool setupFS() {
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed, using embedded resources");
        return false;
    }
    Serial.println("LittleFS mounted");
    return true;
}

// ============================================================
// Remote Telemetry Callback (display unit)
// ============================================================

void onRemoteTelemetry(const tuner_meter_t* meter) {
    usingRemoteTuner = true;
    memcpy(&s_remoteMeter, meter, sizeof(tuner_meter_t));

#ifdef WITH_DISPLAY
    display.updateMeter(meter);
    display.updateStatus(tuner.getMode(), tuner.getAntenna());
#endif
}

void onRemoteStatus(bool connected) {
    remoteUnitConnected = connected;
    usingRemoteTuner = connected;

    if (!connected) {
        memset(&s_remoteMeter, 0, sizeof(tuner_meter_t));
    }

    Serial.printf("Remote unit %s\n", connected ? "connected" : "disconnected");
}

// ============================================================
// Local Meter Callback
// ============================================================

void onMeterUpdate(const tuner_meter_t* meter) {
    if (usingRemoteTuner) return;

#ifdef WITH_DISPLAY
    display.updateMeter(meter);
    display.updateStatus(tuner.getMode(), tuner.getAntenna());
#endif
}

// ============================================================
// SSE Client (Remote Unit)
// ============================================================
#ifdef REMOTE_UNIT

void connectSSEClient() {
    if (s_sseConnected || !wifiConnected) return;

    uint32_t now = millis();
    if (now - s_lastSseReconnect < 5000) return;
    s_lastSseReconnect = now;

    const DeviceConfig& cfg = configManager.get();

    // Try to connect to display unit - use mDNS or AP IP
    IPAddress displayIP;
    if (WiFi.hostByName("ldg-tuner.local", displayIP)) {
        Serial.printf("Found display unit at %s\n", displayIP.toString().c_str());
    } else {
        displayIP = IPAddress(192, 168, 4, 1); // Fallback to AP IP
    }

    Serial.printf("Connecting to display unit SSE at %s:%d...\n", displayIP.toString().c_str(), HTTPS_PORT);
    s_sseClient.setInsecure();  // see threat-model note at the top of the file

    if (s_sseClient.connect(displayIP, HTTPS_PORT)) {
        // Build auth header
        String auth = cfg.webUsername;
        auth += ":";
        auth += cfg.webPassword;
        String authBase64;
        // Simple base64 encode (no library needed for this)
        const char* chars = auth.c_str();
        int len = auth.length();
        const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < len; i += 3) {
            uint32_t n = (uint8_t)chars[i] << 16;
            if (i + 1 < len) n |= (uint8_t)chars[i + 1] << 8;
            if (i + 2 < len) n |= (uint8_t)chars[i + 2];
            authBase64 += b64[(n >> 18) & 0x3F];
            authBase64 += b64[(n >> 12) & 0x3F];
            authBase64 += (i + 1 < len) ? b64[(n >> 6) & 0x3F] : '=';
            authBase64 += (i + 2 < len) ? b64[n & 0x3F] : '=';
        }

        s_sseClient.printf(
            "GET /api/events HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Authorization: Basic %s\r\n"
            "Accept: text/event-stream\r\n"
            "Connection: keep-alive\r\n"
            "\r\n",
            displayIP.toString().c_str(),
            authBase64.c_str()
        );
        s_sseConnected = true;
        Serial.println("SSE connected");
    }
}

void processSSEClient() {
    if (!s_sseConnected || !s_sseClient.connected()) {
        s_sseConnected = false;
        s_sseClient.stop();
        return;
    }

    while (s_sseClient.available()) {
        String line = s_sseClient.readStringUntil('\n');
        line.trim();

        if (line.startsWith("data:")) {
            String data = line.substring(5);
            data.trim();

            JsonDocument doc;
            if (!deserializeJson(doc, data)) {
                const char* eventType = doc["type"];
                if (eventType && strcmp(eventType, "command") == 0) {
                    uint8_t cmd = doc["command"];
                    if (cmd) {
                        bool result = false;
                        switch (cmd) {
                            case TUNER_CMD_TOGGLE_ANT:  result = tuner.toggleAntenna(); break;
                            case TUNER_CMD_MEM_TUNE:    result = tuner.memoryTune(); break;
                            case TUNER_CMD_FULL_TUNE:   result = tuner.fullTune(); break;
                            case TUNER_CMD_BYPASS:      result = tuner.bypass(); break;
                            case TUNER_CMD_AUTO_MODE:   result = tuner.setAutoMode(); break;
                            case TUNER_CMD_MANUAL_MODE: result = tuner.setManualMode(); break;
                        }
                        Serial.printf("Command %c executed: %s\n", cmd, result ? "ok" : "fail");
                    }
                }
            }
        }
    }
}

void postTelemetry(const tuner_meter_t* meter) {
    if (!wifiConnected) return;

    const DeviceConfig& cfg = configManager.get();

    WiFiClientSecure client;
    client.setInsecure();  // see threat-model note at the top of the file
    IPAddress displayIP;
    if (WiFi.hostByName("ldg-tuner.local", displayIP)) {
        // Found via mDNS
    } else {
        displayIP = IPAddress(192, 168, 4, 1);
    }

    if (!client.connect(displayIP, HTTPS_PORT)) return;

    JsonDocument doc;
    doc["fwd_power"] = meter->forward_power_watts;
    doc["ref_power"] = meter->reflected_power_watts;
    doc["swr"] = meter->swr;
    doc["band"] = TunerProtocol::bandToString(meter->band);
    doc["fwd_raw"] = meter->forward_power_raw;
    doc["ref_raw"] = meter->reflected_power_raw;

    String body;
    serializeJson(doc, body);

    // Build auth header
    String auth = cfg.webUsername;
    auth += ":";
    auth += cfg.webPassword;
    String authBase64;
    const char* chars = auth.c_str();
    int len = auth.length();
    const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; i < len; i += 3) {
        uint32_t n = (uint8_t)chars[i] << 16;
        if (i + 1 < len) n |= (uint8_t)chars[i + 1] << 8;
        if (i + 2 < len) n |= (uint8_t)chars[i + 2];
        authBase64 += b64[(n >> 18) & 0x3F];
        authBase64 += b64[(n >> 12) & 0x3F];
        authBase64 += (i + 1 < len) ? b64[(n >> 6) & 0x3F] : '=';
        authBase64 += (i + 2 < len) ? b64[n & 0x3F] : '=';
    }

    String req = String("POST /api/telemetry HTTP/1.1\r\n") +
                 "Host: " + displayIP.toString() + "\r\n" +
                 "Authorization: Basic " + authBase64 + "\r\n" +
                 "Content-Type: application/json\r\n" +
                 "Content-Length: " + body.length() + "\r\n" +
                 "Connection: close\r\n\r\n" +
                 body;

    client.print(req);
    delay(10);
    client.stop();
}

#endif // REMOTE_UNIT

// ============================================================
// Command Execution (for web/MQTT)
// ============================================================

bool executeCommand(uint8_t cmd) {
#ifdef REMOTE_UNIT
    bool result = false;
    switch (cmd) {
        case TUNER_CMD_TOGGLE_ANT:  result = tuner.toggleAntenna(); break;
        case TUNER_CMD_MEM_TUNE:    result = tuner.memoryTune(); break;
        case TUNER_CMD_FULL_TUNE:   result = tuner.fullTune(); break;
        case TUNER_CMD_BYPASS:      result = tuner.bypass(); break;
        case TUNER_CMD_AUTO_MODE:   result = tuner.setAutoMode(); break;
        case TUNER_CMD_MANUAL_MODE: result = tuner.setManualMode(); break;
    }
    return result;
#else
    if (remoteUnitConnected) {
        webServer.pushCommandEvent(cmd);
        return true;
    } else {
        bool result = false;
        switch (cmd) {
            case TUNER_CMD_TOGGLE_ANT:  result = tuner.toggleAntenna(); break;
            case TUNER_CMD_MEM_TUNE:    result = tuner.memoryTune(); break;
            case TUNER_CMD_FULL_TUNE:   result = tuner.fullTune(); break;
            case TUNER_CMD_BYPASS:      result = tuner.bypass(); break;
            case TUNER_CMD_AUTO_MODE:   result = tuner.setAutoMode(); break;
            case TUNER_CMD_MANUAL_MODE: result = tuner.setManualMode(); break;
        }
        return result;
    }
#endif
}

// ============================================================
// Setup
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(100);

    Serial.println("\n=== LDG Tuner Controller ===");
    Serial.printf("Build: %s %s\n",
#ifdef WITH_DISPLAY
        "WITH_DISPLAY"
#elif defined(REMOTE_UNIT)
        "REMOTE_UNIT"
#else
        "NO_DISPLAY"
#endif
    , __DATE__);

    // Load configuration from NVS
    configManager.begin();

    setupFS();

    // Ensure the device's self-signed cert exists before any HTTPS server
    // starts. Both the captive portal and the runtime web server use it; the
    // portal would normally trigger this lazily, but a device with saved STA
    // credentials skips the portal entirely.
    if (!portalCert_init()) {
        Serial.println("ERROR: portal cert init failed; HTTPS will not work");
    }

    // Tuner and display are UART/SPI — no WiFi dependency. Init them early so
    // the display is live during portal setup and for users who never use WiFi.
    Serial.println("Initializing tuner interface...");
    if (!tuner.begin(TUNER_RX_PIN, TUNER_TX_PIN, TUNER_SERIAL_BAUD)) {
        Serial.println("ERROR: Failed to initialize tuner serial");
    } else {
        Serial.println("Tuner interface ready");
    }

#ifdef REMOTE_UNIT
    tuner.setMeterCallback(nullptr);
#else
    tuner.setMeterCallback(onMeterUpdate);
#endif

#ifdef WITH_DISPLAY
    Serial.println("Initializing display...");
    display.begin(&tuner);
    configManager.setPortalIdleCallback([]() {
        tuner.process();
        display.loop();
    });
#endif

    setupWiFi();

#ifdef WITH_DISPLAY
    display.updateNetworkInfo();
#endif

    const DeviceConfig& cfg = configManager.get();

    if (wifiConnected) {
        // mqttClient must outlive setup() — PubSubClient stores a reference.
        static WiFiClient mqttClient;
        mqtt.begin(mqttClient, executeCommand);

#ifndef REMOTE_UNIT
        mqtt.subscribeRemoteTelemetry(onRemoteTelemetry);
        mqtt.subscribeRemoteStatus(onRemoteStatus);
#endif

        // mqtt.connect() blocks on TCP connect to the broker. Don't call it
        // here — main loop()'s mqtt.loop() will reconnect on its own 5 s
        // backoff, and an unreachable broker would otherwise hang setup()
        // long enough to trip the IDLE-task watchdog.
    }

#ifndef REMOTE_UNIT
    if (!webServer.begin(&tuner, executeCommand, getActiveMeter, onRemoteTelemetry)) {
        Serial.println("ERROR: web server failed to start - port 443 may still be bound");
    }
#endif

    Serial.println("=== System Ready ===");
}

// ============================================================
// Loop
// ============================================================

void loop() {
    tuner.process();

    // WiFi reconnection — only manage STA when STA is actually configured.
    // In AP-only mode (captive portal still running, never configured) there
    // is no STA to reconnect, and WiFi.status() returns disconnected forever.
    if (millis() - lastWiFiCheck > 10000) {
        lastWiFiCheck = millis();
        wifi_mode_t mode = WiFi.getMode();
        bool staActive = (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA);
        if (!staActive) {
            wifiConnected = false;
        } else if (WiFi.status() != WL_CONNECTED) {
            if (wifiConnected) {
                Serial.println("WiFi disconnected, attempting reconnect...");
            }
            wifiConnected = false;
            WiFi.reconnect();
        } else if (!wifiConnected) {
            wifiConnected = true;
            Serial.println("WiFi reconnected");
            mqtt.reconnect();
#ifdef WITH_DISPLAY
            display.updateNetworkInfo();
#endif
        }
    }

#ifdef REMOTE_UNIT
    // Remote unit: SSE client + telemetry POST
    connectSSEClient();
    processSSEClient();

    if (wifiConnected && millis() - lastMeterPublish > 200) {
        lastMeterPublish = millis();
        const tuner_meter_t* meter = tuner.getMeterData();
        if (meter->forward_power_raw > 0 || meter->reflected_power_raw > 0) {
            postTelemetry(meter);
        }
    }
#else
    // Display unit: MQTT + web server
    if (wifiConnected) {
        mqtt.loop();

        if (!usingRemoteTuner && millis() - lastMeterPublish > 2000) {
            lastMeterPublish = millis();
            const tuner_meter_t* meter = tuner.getMeterData();
            if (meter->forward_power_raw > 0 || meter->reflected_power_raw > 0) {
                mqtt.publishTelemetry(meter);
            }
        }

        if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL_MS) {
            lastHeartbeat = millis();
            mqtt.publishStatus(remoteUnitConnected ? "remote_connected" : "online");
        }
    }

    webServer.loop();
#endif

#ifdef WITH_DISPLAY
    display.loop();
#endif

    delay(1);
}
