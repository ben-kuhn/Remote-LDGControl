#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

struct DeviceConfig {
    char mqttBroker[64];
    char mqttUsername[32];
    char mqttPassword[64];
    char webUsername[32];
    char webPassword[64];
    float meterPsuVoltage;
    uint16_t mqttPort;
    uint8_t remoteUnitId;
    bool configured;
};

class ConfigManager {
public:
    ConfigManager();

    bool begin();
    const DeviceConfig& get() const;
    bool update(const DeviceConfig& config);
    bool setupWiFi();
    void reset();
    bool isConfigured() const;

    // Captive portal web server
    void startPortalServer();
    void stopPortalServer();
    
    // Portal content
    String portalHTML();
    String scanNetworksJSON();

private:
    Preferences m_prefs;
    DeviceConfig m_config;

    void loadDefaults();
    bool save();
    bool load();
};

// Global instance
extern ConfigManager configManager;

#endif // CONFIG_MANAGER_H
