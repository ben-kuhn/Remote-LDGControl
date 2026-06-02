#include "config_manager.h"
#include <DNSServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>

ConfigManager configManager;

static DNSServer* dnsServer = nullptr;
static uint32_t portalStart = 0;
static const uint32_t PORTAL_TIMEOUT = 300000; // 5 minutes

void ConfigManager::startPortalServer() {
    m_portalServer = new AsyncWebServer(80);

    // Captive portal redirect
    m_portalServer->onNotFound([this](AsyncWebServerRequest* request) {
        if (request->host() != WiFi.softAPIP().toString()) {
            request->redirect(String("http://") + WiFi.softAPIP().toString());
        } else {
            request->send(200, "text/html",
                "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
                "<title>LDG Tuner Setup</title>"
                "<style>body{font-family:sans-serif;max-width:400px;margin:40px auto;padding:20px}"
                "input{width:100%;padding:8px;margin:5px 0 15px;box-sizing:border-box}"
                "button{width:100%;padding:12px;background:#0f3460;color:#fff;border:none;border-radius:5px;cursor:pointer}"
                "</style></head><body>"
                "<h2>LDG Tuner Setup</h2>"
                "<form id='wifiForm'><label>WiFi SSID</label><input name='ssid' required>"
                "<label>WiFi Password</label><input name='pass' type='password' required>"
                "<button type='submit'>Connect</button></form>"
                "<script>document.getElementById('wifiForm').onsubmit=function(e){"
                "e.preventDefault();var f=new FormData(this);"
                "fetch('/connect',{method:'POST',body:f}).then(r=>r.json()).then(d=>{"
                "if(d.status=='connecting'){document.body.innerHTML='<h2>Connecting...</h2>';"
                "setTimeout(()=>location.reload(),15000)}})}</script></body></html>"
            );
        }
    });

    // Handle WiFi connection
    m_portalServer->on("/connect", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (request->hasParam("ssid", true) && request->hasParam("pass", true)) {
            String ssid = request->getParam("ssid", true)->value();
            String pass = request->getParam("pass", true)->value();

            WiFi.begin(ssid.c_str(), pass.c_str());

            JsonDocument doc;
            doc["status"] = "connecting";
            String response;
            serializeJson(doc, response);
            request->send(200, "application/json", response);
        } else {
            request->send(400, "application/json", "{\"error\":\"missing credentials\"}");
        }
    });

    m_portalServer->begin();
}

void ConfigManager::stopPortalServer() {
    if (m_portalServer) {
        delete m_portalServer;
        m_portalServer = nullptr;
    }
}

ConfigManager::ConfigManager() {
    memset(&m_config, 0, sizeof(DeviceConfig));
}

bool ConfigManager::begin() {
    m_prefs.begin("ldg-config", false);

    if (!load()) {
        loadDefaults();
        save();
    }

    m_prefs.end();
    return true;
}

const DeviceConfig& ConfigManager::get() const {
    return m_config;
}

bool ConfigManager::update(const DeviceConfig& config) {
    m_config = config;
    m_config.configured = true;
    return save();
}

bool ConfigManager::setupWiFi() {
    // Try to connect with saved credentials
    String ssid = WiFi.SSID();
    if (!ssid.isEmpty()) {
        Serial.printf("Attempting to connect to saved network: %s\n", ssid.c_str());
        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
            delay(100);
            Serial.print(".");
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\nConnected: %s\n", WiFi.localIP().toString().c_str());
            return true;
        }
        Serial.println("\nFailed to connect to saved network");
    }

    // Start configuration portal
    Serial.println("Starting configuration portal...");
    WiFi.softAP("LDGConfig", "configure");
    delay(100);
    Serial.printf("AP started: %s\n", WiFi.softAPIP().toString().c_str());

    dnsServer = new DNSServer();
    dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer->start(53, "*", WiFi.softAPIP());

    // Start captive portal web server
    startPortalServer();

    portalStart = millis();

    // Captive portal loop
    while (millis() - portalStart < PORTAL_TIMEOUT) {
        dnsServer->processNextRequest();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\nConnected: %s\n", WiFi.localIP().toString().c_str());
            stopPortalServer();
            if (dnsServer) {
                delete dnsServer;
                dnsServer = nullptr;
            }
            return true;
        }

        delay(10);
    }

    // Timeout
    Serial.println("\nConfiguration portal timed out");
    stopPortalServer();
    if (dnsServer) {
        delete dnsServer;
        dnsServer = nullptr;
    }
    return false;
}

void ConfigManager::reset() {
    m_prefs.begin("ldg-config", false);
    m_prefs.clear();
    m_prefs.end();

    loadDefaults();
    m_config.configured = false;
}

bool ConfigManager::isConfigured() const {
    return m_config.configured;
}

void ConfigManager::loadDefaults() {
    strncpy(m_config.mqttBroker, MQTT_BROKER, sizeof(m_config.mqttBroker) - 1);
    strncpy(m_config.mqttUsername, MQTT_USERNAME, sizeof(m_config.mqttUsername) - 1);
    strncpy(m_config.mqttPassword, MQTT_PASSWORD, sizeof(m_config.mqttPassword) - 1);
    strncpy(m_config.webUsername, WEB_AUTH_USERNAME, sizeof(m_config.webUsername) - 1);
    strncpy(m_config.webPassword, WEB_AUTH_PASSWORD, sizeof(m_config.webPassword) - 1);
    m_config.meterPsuVoltage = METER_PSU_VOLTAGE;
    m_config.mqttPort = MQTT_PORT;
    m_config.remoteUnitId = REMOTE_UNIT_ID;
    m_config.configured = false;
}

bool ConfigManager::save() {
    m_prefs.begin("ldg-config", false);

    m_prefs.putString("mqttBroker", m_config.mqttBroker);
    m_prefs.putString("mqttUsername", m_config.mqttUsername);
    m_prefs.putString("mqttPassword", m_config.mqttPassword);
    m_prefs.putString("webUsername", m_config.webUsername);
    m_prefs.putString("webPassword", m_config.webPassword);
    m_prefs.putFloat("meterPsuVoltage", m_config.meterPsuVoltage);
    m_prefs.putUShort("mqttPort", m_config.mqttPort);
    m_prefs.putUChar("remoteUnitId", m_config.remoteUnitId);
    m_prefs.putBool("configured", m_config.configured);

    m_prefs.end();
    return true;
}

bool ConfigManager::load() {
    m_prefs.begin("ldg-config", false);

    if (!m_prefs.isKey("configured")) {
        m_prefs.end();
        return false;
    }

    String mqttBroker = m_prefs.getString("mqttBroker", "");
    if (mqttBroker.isEmpty()) {
        m_prefs.end();
        return false;
    }

    strncpy(m_config.mqttBroker, mqttBroker.c_str(), sizeof(m_config.mqttBroker) - 1);
    strncpy(m_config.mqttUsername, m_prefs.getString("mqttUsername", "").c_str(), sizeof(m_config.mqttUsername) - 1);
    strncpy(m_config.mqttPassword, m_prefs.getString("mqttPassword", "").c_str(), sizeof(m_config.mqttPassword) - 1);
    strncpy(m_config.webUsername, m_prefs.getString("webUsername", "").c_str(), sizeof(m_config.webUsername) - 1);
    strncpy(m_config.webPassword, m_prefs.getString("webPassword", "").c_str(), sizeof(m_config.webPassword) - 1);
    m_config.meterPsuVoltage = m_prefs.getFloat("meterPsuVoltage", METER_PSU_VOLTAGE);
    m_config.mqttPort = m_prefs.getUShort("mqttPort", MQTT_PORT);
    m_config.remoteUnitId = m_prefs.getUChar("remoteUnitId", REMOTE_UNIT_ID);
    m_config.configured = m_prefs.getBool("configured", false);

    m_prefs.end();
    return true;
}
