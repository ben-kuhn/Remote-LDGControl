#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "tuner_protocol.h"
#include "config.h"

typedef bool (*mqtt_command_handler_t)(uint8_t cmd);
typedef void (*mqtt_telemetry_handler_t)(const tuner_meter_t* meter);
typedef void (*mqtt_status_handler_t)(bool remoteConnected);

class MqttHandler {
public:
    MqttHandler();
    ~MqttHandler();

    bool begin(WiFiClient& wifiClient, mqtt_command_handler_t cmdHandler = nullptr);
    bool connect();
    void disconnect();
    void loop();
    bool isConnected() const;

    // Publish telemetry data
    void publishTelemetry(const tuner_meter_t* meter);

    // Publish remote unit telemetry
    void publishRemoteTelemetry(const tuner_meter_t* meter);

    // Publish status update
    void publishStatus(const char* status);

    // Publish remote unit status
    void publishRemoteStatus(bool connected);

    // Publish command response
    void publishCommandResponse(const char* command, const char* result);

    // Send command to remote unit
    bool sendRemoteCommand(uint8_t cmd);

    // Subscribe to remote unit telemetry
    void subscribeRemoteTelemetry(mqtt_telemetry_handler_t handler);

    // Subscribe to remote unit status
    void subscribeRemoteStatus(mqtt_status_handler_t handler);

    // Check if remote unit is connected (via MQTT last-will or heartbeat)
    bool isRemoteConnected() const;

    void reconnect();

private:
    void setupCallbacks();
    static void messageCallback(char* topic, byte* payload, unsigned int length);

    PubSubClient* m_client;
    mqtt_command_handler_t m_cmdHandler;
    mqtt_telemetry_handler_t m_remoteTelemetryHandler;
    mqtt_status_handler_t m_remoteStatusHandler;
    bool m_connected;
    bool m_remoteConnected;
    uint32_t m_lastReconnectAttempt;
    uint32_t m_lastRemoteHeartbeat;
    static MqttHandler* s_instance;

    void handleCommand(const char* payload);
    void handleRemoteCommand(const char* payload);
    void handleRemoteTelemetry(const char* payload);
};

#endif // MQTT_HANDLER_H
