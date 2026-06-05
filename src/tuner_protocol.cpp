#include "tuner_protocol.h"
#include "voltage_sensor.h"
#include "config.h"
#include <driver/uart.h>

TunerProtocol::TunerProtocol()
    : m_serial(nullptr),
      m_meterCallback(nullptr),
      m_meterIdx(0),
      m_eomCount(0),
      m_state(State::RESPONSE),
      m_mode(MODE_UNKNOWN),
      m_antenna(ANT_UNKNOWN),
      m_connected(false),
      m_meterActive(false),
      m_pendingResponse(0),
      m_waitingForResponse(false)
{
    memset(&m_meterData, 0, sizeof(tuner_meter_t));
    memset(m_meterBuf, 0, sizeof(m_meterBuf));
}

TunerProtocol::~TunerProtocol() {
    if (m_serial) {
        delete m_serial;
    }
}

bool TunerProtocol::begin(uint8_t rxPin, uint8_t txPin, uint32_t baud) {
    m_serial = new HardwareSerial(TUNER_UART_NUM);
    m_serial->begin(baud, SERIAL_8N1, rxPin, txPin);
    m_connected = true;

    // Start in meter mode to receive telemetry
    delay(100);
    startMeter();

    return true;
}

bool TunerProtocol::sendWake() {
    if (!m_serial || !m_connected) return false;

    for (int i = 0; i < 2; i++) {
        if (m_serial->write(TUNER_CMD_WAKE) != 1) {
            return false;
        }
        delay(TUNER_WAKE_DELAY_MS);
    }
    return true;
}

bool TunerProtocol::sendCommand(uint8_t cmd) {
    if (!m_serial || !m_connected) return false;

    if (!sendWake()) return false;

    if (m_serial->write(cmd) != 1) {
        return false;
    }

    // Per documentation: minimum 200ms before next command
    delay(TUNER_CMD_DELAY_MS);

    return true;
}

void TunerProtocol::switchToControlMode() {
    m_state = State::RESPONSE;
    sendCommand(TUNER_CMD_METER_STOP);
}

void TunerProtocol::switchToMeterMode() {
    m_state = State::METER;
    sendCommand(TUNER_CMD_METER_START);
    m_meterActive = true;
}

char TunerProtocol::waitForResponse(uint32_t timeoutMs) {
    uint32_t start = millis();
    m_waitingForResponse = true;
    m_pendingResponse = 0;

    while (millis() - start < timeoutMs) {
        process();
        if (m_pendingResponse != 0) {
            char resp = m_pendingResponse;
            m_pendingResponse = 0;
            m_waitingForResponse = false;
            return resp;
        }
        delay(1);
    }

    m_waitingForResponse = false;
    return TUNER_RESP_ERROR;
}

bool TunerProtocol::toggleAntenna() {
    switchToControlMode();
    char resp = ' ';
    if (sendCommand(TUNER_CMD_TOGGLE_ANT)) {
        resp = waitForResponse();
    }
    switchToMeterMode();

    if (resp == TUNER_RESP_ANT_A) {
        m_antenna = ANT_A;
    } else if (resp == TUNER_RESP_ANT_B) {
        m_antenna = ANT_B;
    }

    return (resp != TUNER_RESP_ERROR);
}

bool TunerProtocol::memoryTune() {
    switchToControlMode();
    char resp = ' ';
    if (sendCommand(TUNER_CMD_MEM_TUNE)) {
        resp = waitForResponse();
    }
    switchToMeterMode();
    return (resp != TUNER_RESP_ERROR);
}

bool TunerProtocol::fullTune() {
    switchToControlMode();
    char resp = ' ';
    if (sendCommand(TUNER_CMD_FULL_TUNE)) {
        resp = waitForResponse();
    }
    switchToMeterMode();
    return (resp != TUNER_RESP_ERROR);
}

bool TunerProtocol::bypass() {
    switchToControlMode();
    char resp = ' ';
    if (sendCommand(TUNER_CMD_BYPASS)) {
        resp = waitForResponse();
    }
    switchToMeterMode();

    if (resp == TUNER_RESP_BYPASS) {
        m_mode = MODE_BYPASS;
    }
    return (resp != TUNER_RESP_ERROR);
}

bool TunerProtocol::setAutoMode() {
    switchToControlMode();
    char resp = ' ';
    if (sendCommand(TUNER_CMD_AUTO_MODE)) {
        resp = waitForResponse();
    }
    switchToMeterMode();

    if (resp == TUNER_RESP_AUTO) {
        m_mode = MODE_AUTO;
    }
    return (resp != TUNER_RESP_ERROR);
}

bool TunerProtocol::setManualMode() {
    switchToControlMode();
    char resp = ' ';
    if (sendCommand(TUNER_CMD_MANUAL_MODE)) {
        resp = waitForResponse();
    }
    switchToMeterMode();

    if (resp == TUNER_RESP_MANUAL) {
        m_mode = MODE_MANUAL;
    }
    return (resp != TUNER_RESP_ERROR);
}

bool TunerProtocol::sync() {
    switchToControlMode();
    char resp = ' ';
    if (sendCommand(TUNER_CMD_SYNC)) {
        resp = waitForResponse();
    }
    switchToMeterMode();
    return (resp != TUNER_RESP_ERROR);
}

bool TunerProtocol::startMeter() {
    bool result = sendCommand(TUNER_CMD_METER_START);
    if (result) {
        m_meterActive = true;
        m_state = State::METER;
    }
    return result;
}

bool TunerProtocol::stopMeter() {
    bool result = sendCommand(TUNER_CMD_METER_STOP);
    if (result) {
        m_meterActive = false;
        m_state = State::RESPONSE;
    }
    return result;
}

const tuner_meter_t* TunerProtocol::getMeterData() const {
    return &m_meterData;
}

tuner_mode_t TunerProtocol::getMode() const {
    return m_mode;
}

tuner_ant_t TunerProtocol::getAntenna() const {
    return m_antenna;
}

void TunerProtocol::setMeterCallback(meter_callback_t callback) {
    m_meterCallback = callback;
}

bool TunerProtocol::isConnected() const {
    return m_connected;
}

double TunerProtocol::calculatePower(uint16_t raw, double psuVoltage) {
    double tosquare = (1000.0 * psuVoltage * raw) / (65536.0 * 0.707);
    return (tosquare * tosquare) / 50.0;
}

const char* TunerProtocol::bandToString(tuner_band_t band) {
    switch (band) {
        case BAND_160M: return "160m";
        case BAND_80M:  return "80m";
        case BAND_60M:  return "60m";
        case BAND_40M:  return "40m";
        case BAND_30M:  return "30m";
        case BAND_20M:  return "20m";
        case BAND_17M:  return "17m";
        case BAND_15M:  return "15m";
        case BAND_12M:  return "12m";
        case BAND_10M:  return "10m";
        case BAND_6M:   return "6m";
        default:        return "Unknown";
    }
}

tuner_band_t TunerProtocol::decodeBand(uint16_t indicator) {
    // Band thresholds from reverse-engineered data
    if (indicator >= 8230 && indicator <= 9145) return BAND_160M;
    if (indicator >= 4110 && indicator <= 4710) return BAND_80M;
    if (indicator >= 3060 && indicator <= 3080) return BAND_60M;
    if (indicator >= 2250 && indicator <= 2355) return BAND_40M;
    if (indicator >= 1600 && indicator <= 1630) return BAND_30M;
    if (indicator >= 1140 && indicator <= 1180) return BAND_20M;
    if (indicator >= 900  && indicator <= 915)  return BAND_17M;
    if (indicator >= 766  && indicator <= 782)  return BAND_15M;
    if (indicator >= 655  && indicator <= 662)  return BAND_12M;
    if (indicator >= 550  && indicator <= 590)  return BAND_10M;
    if (indicator >= 320  && indicator <= 360)  return BAND_6M;
    return BAND_UNKNOWN;
}

void TunerProtocol::processMeterByte(uint8_t b) {
    if (m_meterIdx < TUNER_METER_DATA_LEN) {
        m_meterBuf[m_meterIdx++] = b;
    } else if (b == 0x3B) {
        m_eomCount++;
        if (m_eomCount == 2) {
            if (m_meterIdx == TUNER_METER_DATA_LEN) {
                publishMeterData();
            }
            m_meterIdx = 0;
            m_eomCount = 0;
        }
    } else {
        // Protocol error
        m_meterIdx = 0;
        m_eomCount = 0;
        m_state = State::ERROR;
    }
}

void TunerProtocol::publishMeterData() {
    // Data arrives in network byte order (big-endian)
    uint16_t fwd_raw = (m_meterBuf[0] << 8) | m_meterBuf[1];
    uint16_t ref_raw = (m_meterBuf[2] << 8) | m_meterBuf[3];
    uint16_t band_raw = (m_meterBuf[4] << 8) | m_meterBuf[5];

    m_meterData.forward_power_raw = fwd_raw;
    m_meterData.reflected_power_raw = ref_raw;
    m_meterData.band_indicator = band_raw;

    m_meterData.forward_power_watts = calculatePower(fwd_raw, voltageSensor.readVoltage());
    m_meterData.reflected_power_watts = calculatePower(ref_raw, voltageSensor.readVoltage());

    // Calculate SWR
    if (m_meterData.reflected_power_watts > 0.0) {
        double ratio = sqrt(m_meterData.forward_power_watts / m_meterData.reflected_power_watts);
        m_meterData.swr = abs((1.0 + ratio) / (1.0 - ratio));
    } else {
        m_meterData.swr = 1.0;
    }

    m_meterData.band = decodeBand(band_raw);

    if (m_meterCallback) {
        m_meterCallback(&m_meterData);
    }
}

void TunerProtocol::process() {
    if (!m_serial || !m_connected) return;

    while (m_serial->available()) {
        uint8_t b = m_serial->read();

        switch (m_state) {
            case State::RESPONSE:
                if (b >= 0x30) {
                    if (m_waitingForResponse) {
                        m_pendingResponse = (char)b;
                    }
                } else {
                    if (m_waitingForResponse) {
                        m_pendingResponse = TUNER_RESP_ERROR;
                    }
                }
                break;

            case State::METER:
                processMeterByte(b);
                break;

            case State::ERROR:
                if (b == 0x3B) {
                    m_eomCount++;
                    if (m_eomCount >= 2) {
                        m_state = State::METER;
                        m_eomCount = 0;
                        m_meterIdx = 0;
                    }
                }
                break;
        }
    }
}
