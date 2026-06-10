#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
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

static WiFiClientSecure s_remoteClient;
static bool             s_remoteConnected  = false;
static uint32_t         s_lastRemoteReconnect = 0;

static String buildBasicAuth(const char* user, const char* pass) {
    String src = String(user) + ":" + pass;
    const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    String out;
    const uint8_t* bytes = (const uint8_t*)src.c_str();
    int len = src.length();
    for (int i = 0; i < len; i += 3) {
        uint32_t n = (uint32_t)bytes[i] << 16;
        if (i + 1 < len) n |= (uint32_t)bytes[i + 1] << 8;
        if (i + 2 < len) n |= (uint32_t)bytes[i + 2];
        out += b64[(n >> 18) & 0x3F];
        out += b64[(n >> 12) & 0x3F];
        out += (i + 1 < len) ? b64[(n >>  6) & 0x3F] : '=';
        out += (i + 2 < len) ? b64[ n        & 0x3F] : '=';
    }
    return out;
}
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
#ifdef WITH_DISPLAY
    const char* hostname = "ldgcontrol-display";
#else
    const char* hostname = "ldgcontrol-remote";
#endif
    WiFi.setHostname(hostname);

    // WiFiManager handles captive portal and credential storage
    bool connected = configManager.setupWiFi();

    if (connected) {
        wifiConnected = true;
        Serial.printf("\nWiFi connected: %s\n", WiFi.localIP().toString().c_str());

        if (MDNS.begin(hostname)) {
            Serial.printf("mDNS responder started: %s.local\n", hostname);
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
// Remote Unit SSE Client (display build only)
// ============================================================

#ifdef WITH_DISPLAY
void connectRemoteUnit() {
    if (s_remoteConnected || !wifiConnected) return;
    uint32_t now = millis();
    if (now - s_lastRemoteReconnect < 5000) return;
    s_lastRemoteReconnect = now;

    const DeviceConfig& cfg = configManager.get();
    if (cfg.remoteHost[0] == '\0') return;

    IPAddress remoteIP;
    const char* host = cfg.remoteHost;
    bool resolved = false;

    if (remoteIP.fromString(host)) {
        resolved = true;
    } else if (WiFi.hostByName(host, remoteIP)) {
        resolved = true;
    }
    if (!resolved) {
        Serial.printf("Remote: can't resolve %s\n", host);
        return;
    }

    Serial.printf("Connecting to remote SSE at %s:%d...\n",
                  remoteIP.toString().c_str(), HTTPS_PORT);
    s_remoteClient.setInsecure();

    if (!s_remoteClient.connect(remoteIP, HTTPS_PORT)) {
        Serial.println("Remote: connect failed");
        return;
    }

    String auth = buildBasicAuth(cfg.webUsername, cfg.webPassword);
    s_remoteClient.printf(
        "GET /api/events HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Authorization: Basic %s\r\n"
        "Accept: text/event-stream\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",
        remoteIP.toString().c_str(),
        auth.c_str()
    );
    s_remoteConnected = true;
    Serial.println("Remote: SSE connected");
}

void processRemoteSSE() {
    if (!s_remoteConnected || !s_remoteClient.connected()) {
        if (s_remoteConnected) {
            Serial.println("Remote: SSE disconnected");
            s_remoteClient.stop();
            s_remoteConnected = false;
            onRemoteStatus(false);
        }
        return;
    }

    while (s_remoteClient.available()) {
        String line = s_remoteClient.readStringUntil('\n');
        line.trim();
        if (!line.startsWith("data:")) continue;

        String payload = line.substring(5);
        payload.trim();
        JsonDocument doc;
        if (deserializeJson(doc, payload)) continue;

        if (doc["fwd_power"].is<float>() || doc["fwd_power"].is<int>()) {
            tuner_meter_t m = {};
            m.forward_power_watts   = doc["fwd_power"]  | 0.0f;
            m.reflected_power_watts = doc["ref_power"]  | 0.0f;
            m.swr                   = doc["swr"]        | 1.0f;
            m.forward_power_raw     = doc["fwd_raw"]    | 0;
            m.reflected_power_raw   = doc["ref_raw"]    | 0;
            onRemoteTelemetry(&m);

            if (doc["mode"].is<int>())
                display.updateStatus((tuner_mode_t)doc["mode"].as<int>(),
                                     (tuner_ant_t)doc["antenna"].as<int>());

            if (!remoteUnitConnected) onRemoteStatus(true);
        }
    }
}

bool proxyCommandToRemote(uint8_t cmd) {
    const DeviceConfig& cfg = configManager.get();
    if (cfg.remoteHost[0] == '\0') return false;

    IPAddress remoteIP;
    if (!remoteIP.fromString(cfg.remoteHost)) {
        if (!WiFi.hostByName(cfg.remoteHost, remoteIP)) return false;
    }

    const char* endpoint = nullptr;
    switch (cmd) {
        case TUNER_CMD_TOGGLE_ANT:  endpoint = "/api/command/toggle";      break;
        case TUNER_CMD_MEM_TUNE:    endpoint = "/api/command/memory_tune"; break;
        case TUNER_CMD_FULL_TUNE:   endpoint = "/api/command/full_tune";   break;
        case TUNER_CMD_BYPASS:      endpoint = "/api/command/bypass";      break;
        case TUNER_CMD_AUTO_MODE:   endpoint = "/api/command/auto";        break;
        case TUNER_CMD_MANUAL_MODE: endpoint = "/api/command/manual";      break;
        default: return false;
    }

    WiFiClientSecure client;
    client.setInsecure();
    if (!client.connect(remoteIP, HTTPS_PORT)) return false;

    String auth = buildBasicAuth(cfg.webUsername, cfg.webPassword);
    client.printf(
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Authorization: Basic %s\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n",
        endpoint,
        remoteIP.toString().c_str(),
        auth.c_str()
    );

    uint32_t deadline = millis() + 3000;
    while (!client.available() && millis() < deadline) delay(10);
    String status = client.readStringUntil('\n');
    client.stop();
    return status.indexOf("200") >= 0;
}
#endif // WITH_DISPLAY

// ============================================================
// Command Execution (for web/MQTT)
// ============================================================

bool executeCommand(uint8_t cmd) {
#ifdef WITH_DISPLAY
    if (remoteUnitConnected) {
        return proxyCommandToRemote(cmd);
    }
#endif
    bool result = false;
    switch (cmd) {
        case TUNER_CMD_TOGGLE_ANT:  result = tuner.toggleAntenna(); break;
        case TUNER_CMD_MEM_TUNE:    result = tuner.memoryTune();    break;
        case TUNER_CMD_FULL_TUNE:   result = tuner.fullTune();      break;
        case TUNER_CMD_BYPASS:      result = tuner.bypass();        break;
        case TUNER_CMD_AUTO_MODE:   result = tuner.setAutoMode();   break;
        case TUNER_CMD_MANUAL_MODE: result = tuner.setManualMode(); break;
    }
    return result;
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
        "display"
#else
        "remote"
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

    tuner.setMeterCallback(onMeterUpdate);

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

    if (wifiConnected) {
        const DeviceConfig& cfg = configManager.get();
        if (cfg.mqttBroker[0] != '\0') {
            // mqttClient must outlive setup() — PubSubClient stores a reference.
            static WiFiClient mqttClient;
            mqtt.begin(mqttClient, executeCommand);
            mqtt.subscribeRemoteTelemetry(onRemoteTelemetry);
            mqtt.subscribeRemoteStatus(onRemoteStatus);

            // mqtt.connect() blocks on TCP connect to the broker. Don't call
            // it here — main loop()'s mqtt.loop() will reconnect on its own
            // 5 s backoff, and an unreachable broker would otherwise hang
            // setup() long enough to trip the IDLE-task watchdog.
        } else {
            Serial.println("MQTT broker not configured; MQTT disabled");
        }
    }

    if (!webServer.begin(&tuner, executeCommand, getActiveMeter, onRemoteTelemetry)) {
        Serial.println("ERROR: web server failed to start - port 443 may still be bound");
    }

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

#ifdef WITH_DISPLAY
    connectRemoteUnit();
    processRemoteSSE();
    display.loop();
#endif

    delay(1);
}
