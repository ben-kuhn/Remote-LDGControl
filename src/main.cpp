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

// SSE client for remote unit
static WiFiClient s_sseClient;
static bool s_sseConnected = false;
static uint32_t s_lastSseReconnect = 0;

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

    Serial.printf("Connecting to display unit SSE at %s:%d...\n", displayIP.toString().c_str(), HTTP_PORT);

    if (s_sseClient.connect(displayIP, HTTP_PORT)) {
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

    WiFiClient client;
    IPAddress displayIP;
    if (WiFi.hostByName("ldg-tuner.local", displayIP)) {
        // Found via mDNS
    } else {
        displayIP = IPAddress(192, 168, 4, 1);
    }

    if (!client.connect(displayIP, HTTP_PORT)) return;

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
    setupWiFi();

    Serial.println("Initializing tuner interface...");
    if (!tuner.begin(TUNER_RX_PIN, TUNER_TX_PIN, TUNER_SERIAL_BAUD)) {
        Serial.println("ERROR: Failed to initialize tuner serial");
    } else {
        Serial.println("Tuner interface ready");
    }

    const DeviceConfig& cfg = configManager.get();

    if (wifiConnected) {
        WiFiClient mqttClient;
        mqtt.begin(mqttClient, executeCommand);

#ifndef REMOTE_UNIT
        mqtt.subscribeRemoteTelemetry(onRemoteTelemetry);
        mqtt.subscribeRemoteStatus(onRemoteStatus);
#endif

        mqtt.connect();
    }

#ifdef REMOTE_UNIT
    tuner.setMeterCallback(nullptr);
#else
    tuner.setMeterCallback(onMeterUpdate);
    webServer.begin(&tuner, executeCommand, getActiveMeter, onRemoteTelemetry);
#endif

#ifdef WITH_DISPLAY
    Serial.println("Initializing display...");
    display.begin(&tuner);
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
