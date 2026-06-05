#include "web_server.h"
#include "config_manager.h"
#include "config.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

LdgWebServer::LdgWebServer()
    : m_server(nullptr), m_tuner(nullptr), m_cmdHandler(nullptr),
      m_meterGetter(nullptr), m_remoteTelemetryHandler(nullptr),
      m_events(nullptr), m_lastMeterUpdate(0) {
}

LdgWebServer::~LdgWebServer() {
    if (m_server) {
        delete m_server;
    }
}

bool LdgWebServer::begin(TunerProtocol* tuner, command_handler_t cmdHandler,
                      meter_getter_t meterGetter, remote_telemetry_handler_t remoteTelemetryHandler) {
    m_tuner = tuner;
    m_cmdHandler = cmdHandler;
    m_meterGetter = meterGetter;
    m_remoteTelemetryHandler = remoteTelemetryHandler;

    m_server = new AsyncWebServer(443);
    m_events = new AsyncEventSource("/api/events");
    m_server->addHandler(m_events);

    // HTTP→HTTPS redirect on port 80
    AsyncWebServer* httpRedirect = new AsyncWebServer(80);
    httpRedirect->onNotFound([](AsyncWebServerRequest* request) {
        String url = "https://" + request->client()->localIP().toString() + request->url();
        request->redirect(url.c_str());
    });
    httpRedirect->begin();

    m_server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!request->authenticate(WEB_AUTH_USERNAME, WEB_AUTH_PASSWORD)) {
            return request->requestAuthentication();
        }
        handleRoot(request);
    });

    m_server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!request->authenticate(WEB_AUTH_USERNAME, WEB_AUTH_PASSWORD)) {
            return request->requestAuthentication();
        }
        handleStatus(request);
    });

    m_server->on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!request->authenticate(WEB_AUTH_USERNAME, WEB_AUTH_PASSWORD)) {
            return request->requestAuthentication();
        }
        handleConfig(request);
    });

    m_server->on("/api/config", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!request->authenticate(WEB_AUTH_USERNAME, WEB_AUTH_PASSWORD)) {
            return request->requestAuthentication();
        }
        handleConfig(request);
    });

    // SSE endpoint - handled by AsyncEventSource directly
    m_events->onConnect([this](AsyncEventSourceClient* client) {
        const tuner_meter_t* meter = nullptr;
        if (m_meterGetter) {
            meter = m_meterGetter();
        } else if (m_tuner) {
            meter = m_tuner->getMeterData();
        }
        if (meter) {
            JsonDocument doc;
            doc["type"] = "init";
            doc["fwd_power"] = meter->forward_power_watts;
            doc["ref_power"] = meter->reflected_power_watts;
            doc["swr"] = meter->swr;
            doc["band"] = TunerProtocol::bandToString(meter->band);
            String payload;
            serializeJson(doc, payload);
            client->send(payload.c_str(), "meter", millis());
        }
    });
    m_server->addHandler(m_events);

    m_server->on("/api/command/toggle", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!request->authenticate(WEB_AUTH_USERNAME, WEB_AUTH_PASSWORD)) {
            return request->requestAuthentication();
        }
        handleCommand(request, TUNER_CMD_TOGGLE_ANT);
    });

    m_server->on("/api/command/memory_tune", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!request->authenticate(WEB_AUTH_USERNAME, WEB_AUTH_PASSWORD)) {
            return request->requestAuthentication();
        }
        handleCommand(request, TUNER_CMD_MEM_TUNE);
    });

    m_server->on("/api/command/full_tune", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!request->authenticate(WEB_AUTH_USERNAME, WEB_AUTH_PASSWORD)) {
            return request->requestAuthentication();
        }
        handleCommand(request, TUNER_CMD_FULL_TUNE);
    });

    m_server->on("/api/command/bypass", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!request->authenticate(WEB_AUTH_USERNAME, WEB_AUTH_PASSWORD)) {
            return request->requestAuthentication();
        }
        handleCommand(request, TUNER_CMD_BYPASS);
    });

    m_server->on("/api/command/auto", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!request->authenticate(WEB_AUTH_USERNAME, WEB_AUTH_PASSWORD)) {
            return request->requestAuthentication();
        }
        handleCommand(request, TUNER_CMD_AUTO_MODE);
    });

    m_server->on("/api/command/manual", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!request->authenticate(WEB_AUTH_USERNAME, WEB_AUTH_PASSWORD)) {
            return request->requestAuthentication();
        }
        handleCommand(request, TUNER_CMD_MANUAL_MODE);
    });

    m_server->on("/api/telemetry", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!request->authenticate(WEB_AUTH_USERNAME, WEB_AUTH_PASSWORD)) {
            return request->requestAuthentication();
        }
        handleRemoteTelemetry(request);
    });

    m_server->onNotFound([this](AsyncWebServerRequest* request) {
        handleNotFound(request);
    });

    m_server->begin();
    return true;
}

void LdgWebServer::loop() {
    if (millis() - m_lastMeterUpdate > 500) {
        const tuner_meter_t* meter = nullptr;
        if (m_meterGetter) {
            meter = m_meterGetter();
        } else if (m_tuner) {
            meter = m_tuner->getMeterData();
        }

        if (meter && m_events->count() > 0) {
            pushMeterEvent(meter);
        }
        m_lastMeterUpdate = millis();
    }
}

void LdgWebServer::pushMeterEvent(const tuner_meter_t* meter) {
    if (!m_events || m_events->count() == 0) return;

    JsonDocument doc;
    doc["fwd_power"] = meter->forward_power_watts;
    doc["ref_power"] = meter->reflected_power_watts;
    doc["swr"] = meter->swr;
    doc["band"] = TunerProtocol::bandToString(meter->band);

    if (m_tuner) {
        doc["mode"] = m_tuner->getMode();
        doc["antenna"] = m_tuner->getAntenna();
    }

    String payload;
    serializeJson(doc, payload);
    m_events->send(payload.c_str(), "meter", millis());
}

void LdgWebServer::pushStatusEvent(const char* status) {
    if (!m_events || m_events->count() == 0) return;
    m_events->send(status, "status", millis());
}

void LdgWebServer::handleRoot(AsyncWebServerRequest* request) {
    if (LittleFS.exists("/index.html")) {
        request->send(LittleFS, "/index.html", "text/html");
    } else {
        request->send(200, "text/html",
            "<!DOCTYPE html>"
            "<html><head><title>LDG Tuner Control</title>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<style>"
            "body{font-family:sans-serif;max-width:600px;margin:0 auto;padding:20px;background:#1a1a2e;color:#eee}"
            ".meter{display:flex;gap:20px;margin:20px 0}"
            ".meter-box{flex:1;background:#16213e;padding:15px;border-radius:8px;text-align:center}"
            ".meter-value{font-size:2em;font-weight:bold;color:#0f0}"
            ".btn{display:inline-block;padding:12px 24px;margin:5px;background:#0f3460;color:#fff;"
            "border:none;border-radius:5px;cursor:pointer;font-size:1em;text-decoration:none}"
            ".btn:hover{background:#1a5276}.btn-tune{background:#e94560}.btn-tune:hover{background:#c0392b}"
            ".btn-bypass{background:#f39c12}.btn-bypass:hover{background:#e67e22}"
            "#swr{color:#0f0}#swr.warn{color:#f39c12}#swr.bad{color:#e74c3c}"
            "</style></head><body>"
            "<h1>LDG AT-1000ProII Control</h1>"
            "<div class='meter'>"
            "<div class='meter-box'><div>Forward Power</div><div class='meter-value' id='fwd'>0W</div></div>"
            "<div class='meter-box'><div>Reflected Power</div><div class='meter-value' id='ref'>0W</div></div>"
            "<div class='meter-box'><div>SWR</div><div class='meter-value' id='swr'>1.0:1</div></div>"
            "</div>"
            "<div><div>Band: <span id='band'>-</span></div></div>"
            "<div style='margin:20px 0'>"
            "<button class='btn btn-tune' onclick='cmd(\"full_tune\")'>Full Tune</button>"
            "<button class='btn btn-tune' onclick='cmd(\"memory_tune\")'>Memory Tune</button>"
            "<button class='btn' onclick='cmd(\"toggle\")'>Toggle Antenna</button>"
            "<button class='btn btn-bypass' onclick='cmd(\"bypass\")'>Bypass</button>"
            "<button class='btn' onclick='cmd(\"auto\")'>Auto Mode</button>"
            "<button class='btn' onclick='cmd(\"manual\")'>Manual Mode</button>"
            "</div>"
            "<script>"
            "const es=new EventSource('/api/events');"
            "es.onmessage=e=>{const d=JSON.parse(e.data);"
            "document.getElementById('fwd').textContent=d.fwd_power.toFixed(1)+'W';"
            "document.getElementById('ref').textContent=d.ref_power.toFixed(1)+'W';"
            "const swrEl=document.getElementById('swr');"
            "swrEl.textContent=d.swr.toFixed(1)+':1';"
            "swrEl.className=d.swr>2?'bad':d.swr>1.5?'warn':'';"
            "document.getElementById('band').textContent=d.band};"
            "function cmd(c){fetch('/api/command/'+c,{method:'POST'})}"
            "</script></body></html>"
        );
    }
}

void LdgWebServer::handleStatus(AsyncWebServerRequest* request) {
    const tuner_meter_t* meter = nullptr;
    if (m_meterGetter) {
        meter = m_meterGetter();
    } else if (m_tuner) {
        meter = m_tuner->getMeterData();
    }

    if (!meter) {
        request->send(500, "application/json", "{\"error\":\"no meter data\"}");
        return;
    }

    JsonDocument doc;
    doc["fwd_power"] = meter->forward_power_watts;
    doc["ref_power"] = meter->reflected_power_watts;
    doc["swr"] = meter->swr;
    doc["band"] = TunerProtocol::bandToString(meter->band);

    if (m_tuner) {
        doc["mode"] = m_tuner->getMode();
        doc["antenna"] = m_tuner->getAntenna();
        doc["connected"] = m_tuner->isConnected();
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void LdgWebServer::handleConfig(AsyncWebServerRequest* request) {
    if (request->method() == HTTP_GET) {
        // Return current configuration
        const DeviceConfig& cfg = configManager.get();

        JsonDocument doc;
        doc["mqttBroker"] = cfg.mqttBroker;
        doc["mqttPort"] = cfg.mqttPort;
        doc["mqttUsername"] = cfg.mqttUsername;
        doc["webUsername"] = cfg.webUsername;
        doc["meterPsuVoltage"] = cfg.meterPsuVoltage;
        doc["remoteUnitId"] = cfg.remoteUnitId;
        doc["configured"] = cfg.configured;
        doc["wifiSSID"] = WiFi.SSID();
        doc["wifiIP"] = WiFi.localIP().toString();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    } else if (request->method() == HTTP_POST) {
        // Update configuration
        if (!request->hasParam("body", true)) {
            request->send(400, "application/json", "{\"error\":\"missing body\"}");
            return;
        }

        String body = request->getParam("body", true)->value();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, body);

        if (error) {
            request->send(400, "application/json", "{\"error\":\"invalid json\"}");
            return;
        }

        DeviceConfig cfg = configManager.get();

        if (doc["reset"] && doc["reset"] == true) {
            configManager.reset();
            request->send(200, "application/json", "{\"result\":\"ok\",\"restart\":true}");
            delay(500);
            ESP.restart();
            return;
        }

        if (doc["mqttBroker"]) strncpy(cfg.mqttBroker, doc["mqttBroker"], sizeof(cfg.mqttBroker) - 1);
        if (doc["mqttPort"]) cfg.mqttPort = doc["mqttPort"];
        if (doc["mqttUsername"]) strncpy(cfg.mqttUsername, doc["mqttUsername"], sizeof(cfg.mqttUsername) - 1);
        if (doc["mqttPassword"]) strncpy(cfg.mqttPassword, doc["mqttPassword"], sizeof(cfg.mqttPassword) - 1);
        if (doc["webUsername"]) strncpy(cfg.webUsername, doc["webUsername"], sizeof(cfg.webUsername) - 1);
        if (doc["webPassword"]) strncpy(cfg.webPassword, doc["webPassword"], sizeof(cfg.webPassword) - 1);
        if (doc["meterPsuVoltage"]) cfg.meterPsuVoltage = doc["meterPsuVoltage"];
        if (doc["remoteUnitId"]) cfg.remoteUnitId = doc["remoteUnitId"];

        cfg.configured = true;

        if (configManager.update(cfg)) {
            request->send(200, "application/json", "{\"result\":\"ok\",\"restart\":true}");
            // Schedule restart after response is sent
            delay(500);
            ESP.restart();
        } else {
            request->send(500, "application/json", "{\"error\":\"save failed\"}");
        }
    }
}

void LdgWebServer::handleCommand(AsyncWebServerRequest* request, uint8_t cmd) {
    bool result = false;
    if (m_cmdHandler) {
        result = m_cmdHandler(cmd);
    } else if (m_tuner) {
        switch (cmd) {
            case TUNER_CMD_TOGGLE_ANT:  result = m_tuner->toggleAntenna(); break;
            case TUNER_CMD_MEM_TUNE:    result = m_tuner->memoryTune(); break;
            case TUNER_CMD_FULL_TUNE:   result = m_tuner->fullTune(); break;
            case TUNER_CMD_BYPASS:      result = m_tuner->bypass(); break;
            case TUNER_CMD_AUTO_MODE:   result = m_tuner->setAutoMode(); break;
            case TUNER_CMD_MANUAL_MODE: result = m_tuner->setManualMode(); break;
        }
    }

    JsonDocument doc;
    doc["command"] = cmd;
    doc["result"] = result ? "success" : "failed";

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void LdgWebServer::handleNotFound(AsyncWebServerRequest* request) {
    request->send(404, "text/plain", "Not Found");
}

void LdgWebServer::handleRemoteTelemetry(AsyncWebServerRequest* request) {
    if (!request->hasParam("body", true)) {
        request->send(400, "application/json", "{\"error\":\"missing body\"}");
        return;
    }

    String body = request->getParam("body", true)->value();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        request->send(400, "application/json", "{\"error\":\"invalid json\"}");
        return;
    }

    tuner_meter_t meter;
    memset(&meter, 0, sizeof(tuner_meter_t));
    meter.forward_power_watts = doc["fwd_power"] | 0.0;
    meter.reflected_power_watts = doc["ref_power"] | 0.0;
    meter.swr = doc["swr"] | 1.0;
    meter.forward_power_raw = doc["fwd_raw"] | 0;
    meter.reflected_power_raw = doc["ref_raw"] | 0;

    const char* band = doc["band"];
    if (band) {
        if (strcmp(band, "160m") == 0) meter.band = BAND_160M;
        else if (strcmp(band, "80m") == 0) meter.band = BAND_80M;
        else if (strcmp(band, "60m") == 0) meter.band = BAND_60M;
        else if (strcmp(band, "40m") == 0) meter.band = BAND_40M;
        else if (strcmp(band, "30m") == 0) meter.band = BAND_30M;
        else if (strcmp(band, "20m") == 0) meter.band = BAND_20M;
        else if (strcmp(band, "17m") == 0) meter.band = BAND_17M;
        else if (strcmp(band, "15m") == 0) meter.band = BAND_15M;
        else if (strcmp(band, "12m") == 0) meter.band = BAND_12M;
        else if (strcmp(band, "10m") == 0) meter.band = BAND_10M;
        else if (strcmp(band, "6m") == 0) meter.band = BAND_6M;
        else meter.band = BAND_UNKNOWN;
    }

    // Push to SSE clients (browser UI)
    pushMeterEvent(&meter);

    // Forward to remote telemetry handler (MQTT, etc)
    if (m_remoteTelemetryHandler) {
        m_remoteTelemetryHandler(&meter);
    }

    request->send(200, "application/json", "{\"result\":\"ok\"}");
}

void LdgWebServer::pushCommandEvent(uint8_t cmd) {
    if (!m_events || m_events->count() == 0) return;

    JsonDocument doc;
    doc["command"] = cmd;
    String payload;
    serializeJson(doc, payload);
    m_events->send(payload.c_str(), "command", millis());
}
