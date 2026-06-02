#ifndef TUNER_PROTOCOL_H
#define TUNER_PROTOCOL_H

#include <Arduino.h>
#include <HardwareSerial.h>

// Command bytes (single ASCII characters)
#define TUNER_CMD_WAKE          ' '
#define TUNER_CMD_TOGGLE_ANT    'A'
#define TUNER_CMD_MEM_TUNE      'T'
#define TUNER_CMD_FULL_TUNE     'F'
#define TUNER_CMD_BYPASS        'P'
#define TUNER_CMD_AUTO_MODE     'C'
#define TUNER_CMD_MANUAL_MODE   'M'
#define TUNER_CMD_SYNC          'Z'
#define TUNER_CMD_METER_START   'S'
#define TUNER_CMD_METER_STOP    'X'

// Response codes
#define TUNER_RESP_GOOD         'T'
#define TUNER_RESP_OKAY         'M'
#define TUNER_RESP_FAIL         'F'
#define TUNER_RESP_ERROR        'E'
#define TUNER_RESP_BYPASS       'P'
#define TUNER_RESP_AUTO         'A'
#define TUNER_RESP_MANUAL       'M'
#define TUNER_RESP_ANT_A        'A'
#define TUNER_RESP_ANT_B        'B'

// Meter packet structure
#define TUNER_METER_DATA_LEN    6
#define TUNER_METER_EOM         0x3B3B  // ";;"

// Band detection thresholds (from reverse-engineered "wtf" bytes)
typedef enum {
    BAND_UNKNOWN = 0,
    BAND_160M,
    BAND_80M,
    BAND_60M,
    BAND_40M,
    BAND_30M,
    BAND_20M,
    BAND_17M,
    BAND_15M,
    BAND_12M,
    BAND_10M,
    BAND_6M,
    BAND_COUNT
} tuner_band_t;

// Tuner operating mode
typedef enum {
    MODE_UNKNOWN = 0,
    MODE_AUTO,
    MODE_MANUAL,
    MODE_BYPASS
} tuner_mode_t;

// Antenna selection
typedef enum {
    ANT_UNKNOWN = 0,
    ANT_A,
    ANT_B
} tuner_ant_t;

// Meter data structure
typedef struct {
    uint16_t forward_power_raw;
    uint16_t reflected_power_raw;
    uint16_t band_indicator;
    double forward_power_watts;
    double reflected_power_watts;
    double swr;
    tuner_band_t band;
} tuner_meter_t;

// Callback type for meter updates
typedef void (*meter_callback_t)(const tuner_meter_t* meter);

class TunerProtocol {
public:
    TunerProtocol();
    ~TunerProtocol();

    // Initialize serial communication with tuner
    bool begin(uint8_t rxPin, uint8_t txPin, uint32_t baud);

    // Send commands to tuner
    bool toggleAntenna();
    bool memoryTune();
    bool fullTune();
    bool bypass();
    bool setAutoMode();
    bool setManualMode();

    // Query tuner state
    bool sync();

    // Meter control
    bool startMeter();
    bool stopMeter();

    // Get current meter readings (updated by background task)
    const tuner_meter_t* getMeterData() const;

    // Get tuner mode (derived from responses)
    tuner_mode_t getMode() const;
    tuner_ant_t getAntenna() const;

    // Background processing (call from loop)
    void process();

    // Set callback for meter updates
    void setMeterCallback(meter_callback_t callback);

    // Check if tuner is communicating
    bool isConnected() const;

    // Calculate power from raw meter value
    static double calculatePower(uint16_t raw, double psuVoltage = 13.8);

    // Get band name string
    static const char* bandToString(tuner_band_t band);

private:
    // Internal methods
    bool sendCommand(uint8_t cmd);
    bool sendWake();
    void switchToControlMode();
    void switchToMeterMode();
    char waitForResponse(uint32_t timeoutMs = 500);
    void processMeterByte(uint8_t b);
    void publishMeterData();
    tuner_band_t decodeBand(uint16_t indicator);

    HardwareSerial* m_serial;
    tuner_meter_t m_meterData;
    meter_callback_t m_meterCallback;

    // Meter parsing state
    uint8_t m_meterBuf[TUNER_METER_DATA_LEN];
    uint8_t m_meterIdx;
    uint8_t m_eomCount;

    // Mode tracking
    enum class State {
        METER,
        RESPONSE,
        ERROR
    };
    State m_state;

    tuner_mode_t m_mode;
    tuner_ant_t m_antenna;
    bool m_connected;
    bool m_meterActive;

    // Response handling
    volatile char m_pendingResponse;
    bool m_waitingForResponse;
};

#endif // TUNER_PROTOCOL_H
