#ifndef VOLTAGE_SENSOR_H
#define VOLTAGE_SENSOR_H

#include <Arduino.h>
#include "config.h"

class VoltageSensor {
public:
    void begin(uint8_t pin = VOLTAGE_SENSE_PIN);
    double readVoltage();
    bool isValid() const { return m_valid; }

private:
    uint8_t m_pin;
    bool m_valid;
};

extern VoltageSensor voltageSensor;

#endif
