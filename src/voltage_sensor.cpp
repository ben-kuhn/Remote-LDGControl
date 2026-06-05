#include "voltage_sensor.h"

VoltageSensor voltageSensor;

void VoltageSensor::begin(uint8_t pin) {
    m_pin = pin;
    pinMode(m_pin, INPUT);
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    m_valid = true;
}

double VoltageSensor::readVoltage() {
    if (!m_valid) return METER_PSU_VOLTAGE;

    uint32_t sum = 0;
    for (int i = 0; i < VOLTAGE_SAMPLES; i++) {
        sum += analogRead(m_pin);
    }
    double avg = (double)sum / VOLTAGE_SAMPLES;

    // Convert ADC reading to voltage
    // ADC: 0-4095 maps to 0-3.3V (with 11dB attenuation)
    double adcVoltage = (avg / 4095.0) * 3.3;

    // Apply voltage divider ratio
    double supplyVoltage = adcVoltage * VOLTAGE_DIVIDER_RATIO;

    // Sanity check: if reading is unreasonable, fall back to default
    if (supplyVoltage < 8.0 || supplyVoltage > 18.0) {
        return METER_PSU_VOLTAGE;
    }

    return supplyVoltage;
}
