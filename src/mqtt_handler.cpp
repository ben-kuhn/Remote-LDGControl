#include "mqtt_handler.h"
#include "config.h"
#include <ArduinoJson.h>

MqttHandler* MqttHandler::s_instance = nullptr;

MqttHandler::MqttHandler()
    : m_client(nullptr), m_cmdHandler(nullptr),
      m_remoteTelemetryHandler(nullptr), m_remoteStatusHandler(nullptr),
      m_connected(false), m_remoteConnected(false),
      m_lastReconnectAttempt(0), m_lastRemoteHeartbeat(0) {
    s_instance = this;
}

MqttHandler::~MqttHandler() {
    if (m_client) {
        delete m_client;
    }
    s_instance = nullptr;
}

bool MqttHandler::begin(WiFiClient& wifiClient, mqtt_command_handler_t cmdHandler) {
    m_client = new PubSubClient(wifiClient);
    m_client->setServer(MQTT_BROKER, MQTT_PORT);
    m_client->setKeepAlive(MQTT_KEEPALIVE);
    m_client->setBufferSize(1024);
    m_cmdHandler = cmdHandler;

    setupCallbacks();
    return true;
}

bool MqttHandler::connect() {
    if (m_connected) return true;

    String clientId = MQTT_CLIENT_PREFIX;
    clientId += "-";
    clientId += WiFi.macAddress();
    clientId.replace(":", "");

#ifdef REMOTE_UNIT
    clientId += "-remote";
#endif

    bool result = m_client->connect(
        clientId.c_str(),
        MQTT_USERNAME,
        MQTT_PASSWORD,
        MQTT_TOPIC_STATUS,
        MQTT_QOS,
        true,
        "offline"
    );

    if (result) {
        m_connected = true;
        m_client->subscribe(MQTT_TOPIC_CMD, MQTT_QOS);

#ifndef REMOTE_UNIT
        // Display unit subscribes to remote telemetry and commands
        m_client->subscribe(MQTT_REMOTE_TELEMETRY, MQTT_QOS);
        m_client->subscribe(MQTT_REMOTE_CMD, MQTT_QOS);
        m_client->subscribe(MQTT_REMOTE_STATUS, MQTT_QOS);
#else
        // Remote unit subscribes to remote commands
        m_client->subscribe(MQTT_REMOTE_CMD, MQTT_QOS);
#endif

        m_client->publish(MQTT_TOPIC_STATUS, "online", true);
    }

    return result;
}

void MqttHandler::disconnect() {
    if (m_connected) {
        m_client->publish(MQTT_TOPIC_STATUS, "offline", true);
        m_client->disconnect();
        m_connected = false;
    }
}

void MqttHandler::reconnect() {
    if (!m_connected) {
        connect();
    }
}

void MqttHandler::loop() {
    if (!m_client) return;

    if (!m_client->connected()) {
        m_connected = false;
        uint32_t now = millis();
        if (now - m_lastReconnectAttempt > 5000) {
            m_lastReconnectAttempt = now;
            reconnect();
        }
    }

    m_client->loop();

    // Remote unit heartbeat timeout
    if (m_remoteConnected && millis() - m_lastRemoteHeartbeat > (HEARTBEAT_INTERVAL_MS * 2)) {
        m_remoteConnected = false;
        if (m_remoteStatusHandler) {
            m_remoteStatusHandler(false);
        }
    }
}

bool MqttHandler::isConnected() const {
    return m_connected && m_client && m_client->connected();
}

bool MqttHandler::isRemoteConnected() const {
    return m_remoteConnected;
}

void MqttHandler::publishTelemetry(const tuner_meter_t* meter) {
    if (!isConnected()) return;

    JsonDocument doc;
    doc["fwd_power"] = meter->forward_power_watts;
    doc["ref_power"] = meter->reflected_power_watts;
    doc["swr"] = meter->swr;
    doc["band"] = TunerProtocol::bandToString(meter->band);
    doc["fwd_raw"] = meter->forward_power_raw;
    doc["ref_raw"] = meter->reflected_power_raw;

    String payload;
    serializeJson(doc, payload);

    m_client->publish(MQTT_TOPIC_FWD_POWER, payload.c_str(), MQTT_RETAIN_TELEMETRY);
}

void MqttHandler::publishRemoteTelemetry(const tuner_meter_t* meter) {
    if (!isConnected()) return;

    JsonDocument doc;
    doc["fwd_power"] = meter->forward_power_watts;
    doc["ref_power"] = meter->reflected_power_watts;
    doc["swr"] = meter->swr;
    doc["band"] = TunerProtocol::bandToString(meter->band);
    doc["fwd_raw"] = meter->forward_power_raw;
    doc["ref_raw"] = meter->reflected_power_raw;
    doc["unit"] = REMOTE_UNIT_ID;

    String payload;
    serializeJson(doc, payload);

    m_client->publish(MQTT_REMOTE_TELEMETRY, payload.c_str(), MQTT_RETAIN_TELEMETRY);
}

void MqttHandler::publishStatus(const char* status) {
    if (!isConnected()) return;
    m_client->publish(MQTT_TOPIC_STATUS, status, true);
}

void MqttHandler::publishRemoteStatus(bool connected) {
    if (!isConnected()) return;

    JsonDocument doc;
    doc["connected"] = connected;
    doc["unit"] = REMOTE_UNIT_ID;

    String payload;
    serializeJson(doc, payload);

    m_client->publish(MQTT_REMOTE_STATUS, payload.c_str(), true);
}

void MqttHandler::publishCommandResponse(const char* command, const char* result) {
    if (!isConnected()) return;

    JsonDocument doc;
    doc["command"] = command;
    doc["result"] = result;

    String payload;
    serializeJson(doc, payload);

    m_client->publish(MQTT_TOPIC_CMD_RESPONSE, payload.c_str(), false);
}

bool MqttHandler::sendRemoteCommand(uint8_t cmd) {
    if (!isConnected()) return false;

    JsonDocument doc;
    doc["command"] = cmd;
    doc["unit"] = REMOTE_UNIT_ID;

    String payload;
    serializeJson(doc, payload);

    return m_client->publish(MQTT_REMOTE_CMD, payload.c_str(), false);
}

void MqttHandler::subscribeRemoteTelemetry(mqtt_telemetry_handler_t handler) {
    m_remoteTelemetryHandler = handler;
}

void MqttHandler::subscribeRemoteStatus(mqtt_status_handler_t handler) {
    m_remoteStatusHandler = handler;
}

void MqttHandler::setupCallbacks() {
    m_client->setCallback([](char* topic, byte* payload, unsigned int length) {
        if (s_instance) {
            s_instance->messageCallback(topic, payload, length);
        }
    });
}

void MqttHandler::messageCallback(char* topic, byte* payload, unsigned int length) {
    char msg[length + 1];
    memcpy(msg, payload, length);
    msg[length] = '\0';

    if (strcmp(topic, MQTT_TOPIC_CMD) == 0) {
        s_instance->handleCommand(msg);
    } else if (strcmp(topic, MQTT_REMOTE_CMD) == 0) {
        s_instance->handleRemoteCommand(msg);
    } else if (strcmp(topic, MQTT_REMOTE_TELEMETRY) == 0) {
        s_instance->handleRemoteTelemetry(msg);
    } else if (strcmp(topic, MQTT_REMOTE_STATUS) == 0) {
        JsonDocument doc;
        if (!deserializeJson(doc, msg)) {
            bool connected = doc["connected"];
            if (s_instance->m_remoteStatusHandler) {
                s_instance->m_remoteStatusHandler(connected);
            }
            s_instance->m_remoteConnected = connected;
        }
    }
}

static uint8_t commandStringToByte(const char* cmd) {
    if (strcmp(cmd, "toggle") == 0)      return TUNER_CMD_TOGGLE_ANT;
    if (strcmp(cmd, "memory_tune") == 0)  return TUNER_CMD_MEM_TUNE;
    if (strcmp(cmd, "full_tune") == 0)    return TUNER_CMD_FULL_TUNE;
    if (strcmp(cmd, "bypass") == 0)       return TUNER_CMD_BYPASS;
    if (strcmp(cmd, "auto") == 0)         return TUNER_CMD_AUTO_MODE;
    if (strcmp(cmd, "manual") == 0)       return TUNER_CMD_MANUAL_MODE;
    return 0;
}

void MqttHandler::handleCommand(const char* payload) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
        publishCommandResponse(payload, "invalid_json");
        return;
    }

    const char* cmd = doc["command"];
    if (!cmd) {
        publishCommandResponse(payload, "missing_command");
        return;
    }

    uint8_t cmdByte = commandStringToByte(cmd);
    if (cmdByte == 0) {
        publishCommandResponse(cmd, "unknown_command");
        return;
    }

    bool result = false;
    if (m_cmdHandler) {
        result = m_cmdHandler(cmdByte);
    }

    publishCommandResponse(cmd, result ? "success" : "failed");
}

void MqttHandler::handleRemoteCommand(const char* payload) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) return;

    uint8_t cmdByte = doc["command"];
    if (cmdByte == 0) return;

    bool result = false;
    if (m_cmdHandler) {
        result = m_cmdHandler(cmdByte);
    }

    publishCommandResponse(doc["command"] ? doc["command"].as<const char*>() : "unknown",
                          result ? "success" : "failed");
}

void MqttHandler::handleRemoteTelemetry(const char* payload) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) return;

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

    m_lastRemoteHeartbeat = millis();
    m_remoteConnected = true;

    if (m_remoteTelemetryHandler) {
        m_remoteTelemetryHandler(&meter);
    }
}
