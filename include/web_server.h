#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "tuner_protocol.h"
#include "config.h"

typedef bool (*command_handler_t)(uint8_t cmd);
typedef const tuner_meter_t* (*meter_getter_t)();
typedef void (*remote_telemetry_handler_t)(const tuner_meter_t* meter);

class LdgWebServer {
public:
    LdgWebServer();
    ~LdgWebServer();

    bool begin(TunerProtocol* tuner, command_handler_t cmdHandler = nullptr,
               meter_getter_t meterGetter = nullptr,
               remote_telemetry_handler_t remoteTelemetryHandler = nullptr);
    void loop();

    // SSE: push meter event to all connected clients
    void pushMeterEvent(const tuner_meter_t* meter);

    // SSE: push status event
    void pushStatusEvent(const char* status);

    // SSE: push command event to remote unit clients
    void pushCommandEvent(uint8_t cmd);

private:
    AsyncWebServer* m_server;
    TunerProtocol* m_tuner;
    command_handler_t m_cmdHandler;
    meter_getter_t m_meterGetter;
    remote_telemetry_handler_t m_remoteTelemetryHandler;

    void handleRoot(AsyncWebServerRequest* request);
    void handleStatus(AsyncWebServerRequest* request);
    void handleConfig(AsyncWebServerRequest* request);
    void handleCommand(AsyncWebServerRequest* request, uint8_t cmd);
    void handleNotFound(AsyncWebServerRequest* request);
    void handleRemoteTelemetry(AsyncWebServerRequest* request);

    AsyncEventSource* m_events;
    uint32_t m_lastMeterUpdate;
};

#endif // WEB_SERVER_H
