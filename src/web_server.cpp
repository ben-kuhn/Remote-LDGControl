#include "web_server.h"
#include "config_manager.h"
#include "portal_cert.h"
#include "config.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <HTTPSServer.hpp>
#include <SSLCert.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>
#include <freertos/FreeRTOS.h>

namespace hsv = httpsserver;

// ---------------------------------------------------------------------------
// Singleton + shared SSE state.
//
// The HTTPS library accepts plain function pointers for route handlers, so we
// can't capture `this` in lambdas. Handlers read from s_instance to reach
// LdgWebServer members, and from s_sse for the SSE push state. Each SSE
// subscriber runs its own connection task and polls s_sse seq counters.
// ---------------------------------------------------------------------------

static LdgWebServer* s_instance = nullptr;

struct SseState {
    portMUX_TYPE mux;
    uint32_t meterSeq;
    char meterJson[384];
    uint32_t statusSeq;
    char statusJson[96];
};
static SseState s_sse = {
    portMUX_INITIALIZER_UNLOCKED,
    0, "", 0, ""
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool authenticate(hsv::HTTPRequest* req, hsv::HTTPResponse* res) {
    std::string user = req->getBasicAuthUser();
    std::string pass = req->getBasicAuthPassword();
    const DeviceConfig& cfg = configManager.get();
    if (!user.empty() && !pass.empty() &&
        user == cfg.webUsername && pass == cfg.webPassword) {
        return true;
    }
    res->setStatusCode(401);
    res->setHeader("WWW-Authenticate", "Basic realm=\"LDG Tuner\"");
    res->setHeader("Content-Type", "text/plain");
    res->print("Unauthorized");
    return false;
}

static std::string readRequestBody(hsv::HTTPRequest* req) {
    // readBytes() is non-blocking and returns 0 between SSL records, not only
    // at end-of-body. Drive the loop off requestComplete() so we don't bail
    // mid-body when the receive buffer is briefly empty.
    std::string body;
    uint8_t buf[256];
    uint32_t deadline = millis() + 5000;
    while (!req->requestComplete() && millis() < deadline) {
        size_t n = req->readBytes(buf, sizeof(buf));
        if (n > 0) {
            body.append((char*)buf, n);
            if (body.size() > 8192) break;  // sanity cap
        } else {
            delay(1);
        }
    }
    return body;
}

static void sendJson(hsv::HTTPResponse* res, int code, const String& body) {
    res->setStatusCode(code);
    res->setHeader("Content-Type", "application/json");
    res->print(body.c_str());
}

static const char* bandToString(tuner_band_t band) {
    return TunerProtocol::bandToString(band);
}

// Convert a meter to the JSON payload we ship over SSE / /api/status.
static String serializeMeter(const tuner_meter_t* meter, TunerProtocol* tuner) {
    JsonDocument doc;
    doc["fwd_power"] = meter->forward_power_watts;
    doc["ref_power"] = meter->reflected_power_watts;
    doc["swr"]       = meter->swr;
    doc["band"]      = bandToString(meter->band);
    if (tuner) {
        doc["mode"]    = tuner->getMode();
        doc["antenna"] = tuner->getAntenna();
    }
    String out;
    serializeJson(doc, out);
    return out;
}

// ---------------------------------------------------------------------------
// Handlers (all plain functions; access state through s_instance / globals)
// ---------------------------------------------------------------------------

static void h_root(hsv::HTTPRequest* req, hsv::HTTPResponse* res) {
    if (!authenticate(req, res)) return;
    res->setHeader("Content-Type", "text/html; charset=utf-8");

    if (LittleFS.exists("/index.html")) {
        File f = LittleFS.open("/index.html", "r");
        if (f) {
            uint8_t buf[512];
            int n;
            while ((n = f.read(buf, sizeof(buf))) > 0) {
                res->write(buf, n);
            }
            f.close();
            return;
        }
    }

    res->print(
        "<!DOCTYPE html><html><head>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>LDG Tuner</title>"
        "</head><body><h1>LDG Tuner</h1>"
        "<p>The web UI file (/index.html) is missing from LittleFS. "
        "Re-upload the data partition.</p>"
        "</body></html>"
    );
}

static void h_status(hsv::HTTPRequest* req, hsv::HTTPResponse* res) {
    if (!authenticate(req, res)) return;

    LdgWebServer* self = s_instance;
    if (!self) { sendJson(res, 500, "{\"error\":\"no instance\"}"); return; }

    const tuner_meter_t* meter = nullptr;
    if (self->getMeterGetter()) meter = self->getMeterGetter()();
    else if (self->getTuner())  meter = self->getTuner()->getMeterData();

    if (!meter) { sendJson(res, 500, "{\"error\":\"no meter data\"}"); return; }

    JsonDocument doc;
    doc["fwd_power"] = meter->forward_power_watts;
    doc["ref_power"] = meter->reflected_power_watts;
    doc["swr"]       = meter->swr;
    doc["band"]      = bandToString(meter->band);
    if (self->getTuner()) {
        doc["mode"]      = self->getTuner()->getMode();
        doc["antenna"]   = self->getTuner()->getAntenna();
        doc["connected"] = self->getTuner()->isConnected();
    }
    String out;
    serializeJson(doc, out);
    sendJson(res, 200, out);
}

static void h_config_get(hsv::HTTPRequest* req, hsv::HTTPResponse* res) {
    if (!authenticate(req, res)) return;

    const DeviceConfig& cfg = configManager.get();
    JsonDocument doc;
    doc["mqttBroker"]      = cfg.mqttBroker;
    doc["mqttPort"]        = cfg.mqttPort;
    doc["mqttUsername"]    = cfg.mqttUsername;
    doc["webUsername"]     = cfg.webUsername;
    doc["meterPsuVoltage"] = cfg.meterPsuVoltage;
    doc["ant1Name"]        = cfg.ant1Name;
    doc["ant2Name"]        = cfg.ant2Name;
    doc["configured"]      = cfg.configured;
    doc["useStaticIP"]     = cfg.useStaticIP;
    doc["staticIP"]        = cfg.staticIP;
    doc["staticNetmask"]   = cfg.staticNetmask;
    doc["staticGateway"]   = cfg.staticGateway;
    doc["staticDNS"]       = cfg.staticDNS;
    doc["currentIP"]       = WiFi.localIP().toString();
    doc["currentNetmask"]  = WiFi.subnetMask().toString();
    doc["currentGateway"]  = WiFi.gatewayIP().toString();
    doc["currentDNS"]      = WiFi.dnsIP().toString();
    doc["wifiSSID"]        = WiFi.SSID();
#ifdef WITH_DISPLAY
    doc["remoteHost"]      = cfg.remoteHost;
    doc["build"]           = "display";
#else
    doc["build"]           = "remote";
#endif
    String out;
    serializeJson(doc, out);
    sendJson(res, 200, out);
}

static void h_config_post(hsv::HTTPRequest* req, hsv::HTTPResponse* res) {
    if (!authenticate(req, res)) return;

    std::string body = readRequestBody(req);
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) { sendJson(res, 400, "{\"error\":\"invalid json\"}"); return; }

    if (doc["reset"].is<bool>() && doc["reset"].as<bool>()) {
        configManager.reset();
        sendJson(res, 200, "{\"result\":\"ok\",\"restart\":true}");
        delay(500);
        ESP.restart();
        return;
    }

    DeviceConfig cfg = configManager.get();
    if (doc["mqttBroker"])      strncpy(cfg.mqttBroker,   doc["mqttBroker"],   sizeof(cfg.mqttBroker)   - 1);
    if (doc["mqttPort"])        cfg.mqttPort = doc["mqttPort"];
    if (doc["mqttUsername"])    strncpy(cfg.mqttUsername, doc["mqttUsername"], sizeof(cfg.mqttUsername) - 1);
    if (doc["mqttPassword"])    strncpy(cfg.mqttPassword, doc["mqttPassword"], sizeof(cfg.mqttPassword) - 1);
    if (doc["webUsername"])     strncpy(cfg.webUsername,  doc["webUsername"],  sizeof(cfg.webUsername)  - 1);
    if (doc["webPassword"])     strncpy(cfg.webPassword,  doc["webPassword"],  sizeof(cfg.webPassword)  - 1);
    if (doc["meterPsuVoltage"]) cfg.meterPsuVoltage = doc["meterPsuVoltage"];
    if (doc["ant1Name"])        strncpy(cfg.ant1Name, doc["ant1Name"], sizeof(cfg.ant1Name) - 1);
    if (doc["ant2Name"])        strncpy(cfg.ant2Name, doc["ant2Name"], sizeof(cfg.ant2Name) - 1);
    if (doc["useStaticIP"].is<bool>())  cfg.useStaticIP = doc["useStaticIP"];
    if (doc["staticIP"].is<const char*>())      strncpy(cfg.staticIP,      doc["staticIP"],      sizeof(cfg.staticIP)      - 1);
    if (doc["staticNetmask"].is<const char*>()) strncpy(cfg.staticNetmask, doc["staticNetmask"], sizeof(cfg.staticNetmask) - 1);
    if (doc["staticGateway"].is<const char*>()) strncpy(cfg.staticGateway, doc["staticGateway"], sizeof(cfg.staticGateway) - 1);
    if (doc["staticDNS"].is<const char*>())     strncpy(cfg.staticDNS,     doc["staticDNS"],     sizeof(cfg.staticDNS)     - 1);
#ifdef WITH_DISPLAY
    if (doc["remoteHost"].is<const char*>())    strncpy(cfg.remoteHost, doc["remoteHost"], sizeof(cfg.remoteHost) - 1);
#endif
    cfg.configured = true;

    if (configManager.update(cfg)) {
        sendJson(res, 200, "{\"result\":\"ok\",\"restart\":true}");
        delay(500);
        ESP.restart();
    } else {
        sendJson(res, 500, "{\"error\":\"save failed\"}");
    }
}

// Each command endpoint as a separate handler — the library's route registration
// takes a function pointer, so we can't bind the cmd parameter.
static void runCommand(hsv::HTTPResponse* res, uint8_t cmd) {
    LdgWebServer* self = s_instance;
    if (!self) { sendJson(res, 500, "{\"error\":\"no instance\"}"); return; }

    bool ok = false;
    if (self->getCmdHandler()) {
        ok = self->getCmdHandler()(cmd);
    } else if (self->getTuner()) {
        switch (cmd) {
            case TUNER_CMD_TOGGLE_ANT:  ok = self->getTuner()->toggleAntenna();   break;
            case TUNER_CMD_MEM_TUNE:    ok = self->getTuner()->memoryTune();      break;
            case TUNER_CMD_FULL_TUNE:   ok = self->getTuner()->fullTune();        break;
            case TUNER_CMD_BYPASS:      ok = self->getTuner()->bypass();          break;
            case TUNER_CMD_AUTO_MODE:   ok = self->getTuner()->setAutoMode();     break;
            case TUNER_CMD_MANUAL_MODE: ok = self->getTuner()->setManualMode();   break;
        }
    }
    JsonDocument doc;
    doc["command"] = cmd;
    doc["result"]  = ok ? "success" : "failed";
    String out;
    serializeJson(doc, out);
    sendJson(res, 200, out);
}

static void h_cmd_toggle      (hsv::HTTPRequest* req, hsv::HTTPResponse* res) { if (!authenticate(req, res)) return; runCommand(res, TUNER_CMD_TOGGLE_ANT); }
static void h_cmd_memory_tune (hsv::HTTPRequest* req, hsv::HTTPResponse* res) { if (!authenticate(req, res)) return; runCommand(res, TUNER_CMD_MEM_TUNE); }
static void h_cmd_full_tune   (hsv::HTTPRequest* req, hsv::HTTPResponse* res) { if (!authenticate(req, res)) return; runCommand(res, TUNER_CMD_FULL_TUNE); }
static void h_cmd_bypass      (hsv::HTTPRequest* req, hsv::HTTPResponse* res) { if (!authenticate(req, res)) return; runCommand(res, TUNER_CMD_BYPASS); }
static void h_cmd_auto        (hsv::HTTPRequest* req, hsv::HTTPResponse* res) { if (!authenticate(req, res)) return; runCommand(res, TUNER_CMD_AUTO_MODE); }
static void h_cmd_manual      (hsv::HTTPRequest* req, hsv::HTTPResponse* res) { if (!authenticate(req, res)) return; runCommand(res, TUNER_CMD_MANUAL_MODE); }

static void h_telemetry(hsv::HTTPRequest* req, hsv::HTTPResponse* res) {
    if (!authenticate(req, res)) return;

    std::string body = readRequestBody(req);
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) { sendJson(res, 400, "{\"error\":\"invalid json\"}"); return; }

    tuner_meter_t meter{};
    meter.forward_power_watts   = doc["fwd_power"] | 0.0;
    meter.reflected_power_watts = doc["ref_power"] | 0.0;
    meter.swr                   = doc["swr"]       | 1.0;
    meter.forward_power_raw     = doc["fwd_raw"]   | 0;
    meter.reflected_power_raw   = doc["ref_raw"]   | 0;

    const char* b = doc["band"];
    if (b) {
        if      (!strcmp(b, "160m")) meter.band = BAND_160M;
        else if (!strcmp(b, "80m"))  meter.band = BAND_80M;
        else if (!strcmp(b, "60m"))  meter.band = BAND_60M;
        else if (!strcmp(b, "40m"))  meter.band = BAND_40M;
        else if (!strcmp(b, "30m"))  meter.band = BAND_30M;
        else if (!strcmp(b, "20m"))  meter.band = BAND_20M;
        else if (!strcmp(b, "17m"))  meter.band = BAND_17M;
        else if (!strcmp(b, "15m"))  meter.band = BAND_15M;
        else if (!strcmp(b, "12m"))  meter.band = BAND_12M;
        else if (!strcmp(b, "10m"))  meter.band = BAND_10M;
        else if (!strcmp(b, "6m"))   meter.band = BAND_6M;
        else                         meter.band = BAND_UNKNOWN;
    }

    if (s_instance) {
        s_instance->pushMeterEvent(&meter);
        if (s_instance->getRemoteTelemetryHandler()) {
            s_instance->getRemoteTelemetryHandler()(&meter);
        }
    }
    sendJson(res, 200, "{\"result\":\"ok\"}");
}

// ---------------------------------------------------------------------------
// SSE handler
//
// The library buffers responses in a ~1400-byte cache to enable Content-Length
// keep-alive responses. We can't disable that from the public API, so we
// write a large initial padding comment to force-overflow the cache, which
// switches the response into pass-through streaming mode for the rest of
// the connection.
// ---------------------------------------------------------------------------

static void h_events(hsv::HTTPRequest* req, hsv::HTTPResponse* res) {
    if (!authenticate(req, res)) return;

    res->setHeader("Content-Type", "text/event-stream");
    res->setHeader("Cache-Control", "no-cache, no-transform");
    res->setHeader("X-Accel-Buffering", "no");
    res->setHeader("Access-Control-Allow-Origin", "*");
    // Streaming response with no Content-Length — there's no keep-alive to do
    // at HTTP level (TCP keepalive is independent). Telling the library "close"
    // routes post-handler cleanup down the simple STATE_BODY_FINISHED →
    // closeConnection() branch instead of the keep-alive reuse path.
    res->setHeader("Connection", "close");

    // Local helper: every res->print() can silently fail because
    // esp_tls_conn_write returns a negative ssize_t on connection-reset, and
    // HTTPResponse::print just returns that as size_t (an enormous number).
    // Compare returned bytes to expected length — the ONLY reliable failure
    // signal. Returns true on a clean write, false on any short/over write.
    auto write_ok = [res](const char* s, size_t n) -> bool {
        return res->print(s) == n;
    };

    // Force the response cache to overflow once so subsequent writes stream
    // direct to TLS. If the client TLS is already torn down, this fails — bail
    // out immediately rather than entering the loop with a dead connection.
    {
        char pad[1500];
        pad[0] = ':';
        pad[1] = ' ';
        memset(pad + 2, '.', sizeof(pad) - 5);
        pad[sizeof(pad) - 3] = '\n';
        pad[sizeof(pad) - 2] = '\n';
        pad[sizeof(pad) - 1] = '\0';
        if (!write_ok(pad, sizeof(pad) - 1)) {
            if (res->_con) res->_con->signalClientClose();
            return;
        }
    }

    // Snapshot current seqs so this subscriber doesn't replay history.
    uint32_t lastMeterSeq, lastStatusSeq;
    char initMeter[sizeof(s_sse.meterJson)];
    portENTER_CRITICAL(&s_sse.mux);
    lastMeterSeq   = s_sse.meterSeq;
    lastStatusSeq  = s_sse.statusSeq;
    strncpy(initMeter, s_sse.meterJson, sizeof(initMeter));
    initMeter[sizeof(initMeter) - 1] = '\0';
    portEXIT_CRITICAL(&s_sse.mux);

    // If a meter value exists, send it once so the client renders immediately
    // instead of waiting for the next push. Bail on write failure here too.
    if (lastMeterSeq > 0 && initMeter[0] != '\0') {
        static const char  HDR[]   = "event: meter\ndata: ";
        static const char  END[]   = "\n\n";
        static const size_t HDR_LEN = sizeof(HDR) - 1;
        static const size_t END_LEN = sizeof(END) - 1;
        size_t mLen = strlen(initMeter);
        if (!write_ok(HDR, HDR_LEN) ||
            !write_ok(initMeter, mLen) ||
            !write_ok(END, END_LEN)) {
            if (res->_con) res->_con->signalClientClose();
            return;
        }
    }

    static const char  KEEPALIVE[]   = ": keepalive\n\n";
    static const size_t KEEPALIVE_LEN = sizeof(KEEPALIVE) - 1;
    const uint32_t KEEPALIVE_INTERVAL_MS = 2000;
    const int      MAX_FAILED_WRITES     = 3;
    // Hard session cap. Even on a healthy stream we exit periodically so the
    // worker slot recycles and any subtle library-side state drift gets reset.
    // Browsers reconnect SSE automatically; the snapshot above keeps the UI
    // continuous across the reconnect.
    const uint32_t SESSION_MAX_MS = 10UL * 60 * 1000;

    uint32_t lastKeepalive = millis();
    uint32_t sessionStart  = millis();
    int failedWrites = 0;

    while (failedWrites < MAX_FAILED_WRITES &&
           millis() - sessionStart < SESSION_MAX_MS) {
        uint32_t mSeq, sSeq;
        char     mBuf[sizeof(s_sse.meterJson)];
        char     sBuf[sizeof(s_sse.statusJson)];

        portENTER_CRITICAL(&s_sse.mux);
        mSeq = s_sse.meterSeq;
        sSeq = s_sse.statusSeq;
        if (mSeq != lastMeterSeq) {
            strncpy(mBuf, s_sse.meterJson, sizeof(mBuf));
            mBuf[sizeof(mBuf) - 1] = '\0';
        }
        if (sSeq != lastStatusSeq) {
            strncpy(sBuf, s_sse.statusJson, sizeof(sBuf));
            sBuf[sizeof(sBuf) - 1] = '\0';
        }
        portEXIT_CRITICAL(&s_sse.mux);

        bool didWrite = false;
        bool writeOk  = true;

        if (mSeq != lastMeterSeq) {
            didWrite = true;
            static const char  HDR[]   = "event: meter\ndata: ";
            static const char  END[]   = "\n\n";
            static const size_t HDR_LEN = sizeof(HDR) - 1;
            static const size_t END_LEN = sizeof(END) - 1;
            size_t mLen = strlen(mBuf);
            if (!write_ok(HDR, HDR_LEN) ||
                !write_ok(mBuf, mLen)   ||
                !write_ok(END, END_LEN)) writeOk = false;
            lastMeterSeq = mSeq;
        }
        if (sSeq != lastStatusSeq) {
            didWrite = true;
            static const char  HDR[]   = "event: status\ndata: ";
            static const char  END[]   = "\n\n";
            static const size_t HDR_LEN = sizeof(HDR) - 1;
            static const size_t END_LEN = sizeof(END) - 1;
            size_t sLen = strlen(sBuf);
            if (!write_ok(HDR, HDR_LEN) ||
                !write_ok(sBuf, sLen)   ||
                !write_ok(END, END_LEN)) writeOk = false;
            lastStatusSeq = sSeq;
        }
        if (millis() - lastKeepalive > KEEPALIVE_INTERVAL_MS) {
            didWrite = true;
            if (!write_ok(KEEPALIVE, KEEPALIVE_LEN)) writeOk = false;
            lastKeepalive = millis();
        }

        if (didWrite) {
            if (writeOk) failedWrites = 0;
            else         failedWrites++;
        }
        delay(50);
    }

    // Explicit teardown signal so the library's connection state machine
    // advances promptly rather than waiting for its idle timeout.
    if (res->_con) res->_con->signalClientClose();
}

// ---------------------------------------------------------------------------
// LdgWebServer methods
// ---------------------------------------------------------------------------

LdgWebServer::LdgWebServer()
    : m_server(nullptr), m_sslCert(nullptr), m_tuner(nullptr),
      m_cmdHandler(nullptr), m_meterGetter(nullptr),
      m_remoteTelemetryHandler(nullptr), m_lastMeterUpdate(0) {
    s_instance = this;
}

LdgWebServer::~LdgWebServer() {
    if (m_server) { m_server->stop(); delete m_server; }
    if (m_sslCert) delete m_sslCert;
    if (s_instance == this) s_instance = nullptr;
}

bool LdgWebServer::begin(TunerProtocol* tuner, command_handler_t cmdHandler,
                         meter_getter_t meterGetter,
                         remote_telemetry_handler_t remoteTelemetryHandler) {
    m_tuner = tuner;
    m_cmdHandler = cmdHandler;
    m_meterGetter = meterGetter;
    m_remoteTelemetryHandler = remoteTelemetryHandler;
    s_instance = this;

    m_sslCert = portalCert_newSSLCert();
    if (!m_sslCert) {
        Serial.println("ERROR: runtime web server: no SSL cert");
        return false;
    }

    // The captive portal HTTPSServer is torn down in setupWiFi() after STA
    // join (with a 3s wait so its last connection drains), so port 443 is
    // free here. Bind to 0.0.0.0 so the runtime is reachable on every
    // interface — including the softAP on display units where the remote
    // unit needs to talk to it.
    m_server = new hsv::HTTPSServer(m_sslCert, 443, 4);

    m_server->registerNode(new hsv::ResourceNode("/",                   "GET",  &h_root));
    m_server->registerNode(new hsv::ResourceNode("/api/status",         "GET",  &h_status));
    m_server->registerNode(new hsv::ResourceNode("/api/config",         "GET",  &h_config_get));
    m_server->registerNode(new hsv::ResourceNode("/api/config",         "POST", &h_config_post));
    m_server->registerNode(new hsv::ResourceNode("/api/events",         "GET",  &h_events));
    m_server->registerNode(new hsv::ResourceNode("/api/telemetry",      "POST", &h_telemetry));
    m_server->registerNode(new hsv::ResourceNode("/api/command/toggle",      "POST", &h_cmd_toggle));
    m_server->registerNode(new hsv::ResourceNode("/api/command/memory_tune", "POST", &h_cmd_memory_tune));
    m_server->registerNode(new hsv::ResourceNode("/api/command/full_tune",   "POST", &h_cmd_full_tune));
    m_server->registerNode(new hsv::ResourceNode("/api/command/bypass",      "POST", &h_cmd_bypass));
    m_server->registerNode(new hsv::ResourceNode("/api/command/auto",        "POST", &h_cmd_auto));
    m_server->registerNode(new hsv::ResourceNode("/api/command/manual",      "POST", &h_cmd_manual));

    m_server->start();
    if (!m_server->isRunning()) {
        Serial.println("ERROR: runtime HTTPS server failed to bind :443");
        return false;
    }
    Serial.println("Runtime HTTPS server started on :443");
    return true;
}

void LdgWebServer::loop() {
    if (m_server) m_server->loop();

    // Mirror the meter into SSE state every 500ms so polling subscribers see
    // updates without us needing an explicit push from every meter source.
    if (millis() - m_lastMeterUpdate > 500) {
        const tuner_meter_t* meter = nullptr;
        if (m_meterGetter)   meter = m_meterGetter();
        else if (m_tuner)    meter = m_tuner->getMeterData();
        if (meter) pushMeterEvent(meter);
        m_lastMeterUpdate = millis();
    }
}

void LdgWebServer::pushMeterEvent(const tuner_meter_t* meter) {
    String payload = serializeMeter(meter, m_tuner);
    portENTER_CRITICAL(&s_sse.mux);
    strncpy(s_sse.meterJson, payload.c_str(), sizeof(s_sse.meterJson));
    s_sse.meterJson[sizeof(s_sse.meterJson) - 1] = '\0';
    s_sse.meterSeq++;
    portEXIT_CRITICAL(&s_sse.mux);
}

void LdgWebServer::pushStatusEvent(const char* status) {
    if (!status) return;
    portENTER_CRITICAL(&s_sse.mux);
    strncpy(s_sse.statusJson, status, sizeof(s_sse.statusJson));
    s_sse.statusJson[sizeof(s_sse.statusJson) - 1] = '\0';
    s_sse.statusSeq++;
    portEXIT_CRITICAL(&s_sse.mux);
}

