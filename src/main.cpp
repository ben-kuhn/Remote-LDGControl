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
// Command Execution (for web/MQTT)
// ============================================================

bool executeCommand(uint8_t cmd) {
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
        // mqttClient must outlive setup() — PubSubClient stores a reference.
        static WiFiClient mqttClient;
        mqtt.begin(mqttClient, executeCommand);
        mqtt.subscribeRemoteTelemetry(onRemoteTelemetry);
        mqtt.subscribeRemoteStatus(onRemoteStatus);

        // mqtt.connect() blocks on TCP connect to the broker. Don't call it
        // here — main loop()'s mqtt.loop() will reconnect on its own 5 s
        // backoff, and an unreachable broker would otherwise hang setup()
        // long enough to trip the IDLE-task watchdog.
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
    display.loop();
#endif

    delay(1);
}
