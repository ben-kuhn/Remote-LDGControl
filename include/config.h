#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================
// WiFi Configuration
// ============================================================
#define WIFI_STA_SSID           "your_network_ssid"
#define WIFI_STA_PASSWORD       "your_network_password"
#define WIFI_AP_SSID            "LDGControl"
#define WIFI_AP_PASSWORD        "ldgcontrol"
#define WIFI_AP_CHANNEL         6
#define WIFI_CONNECT_TIMEOUT_MS 15000

// ============================================================
// Network Services
// ============================================================
#define HTTP_PORT               80
#define HTTPS_PORT              443
#define MQTT_PORT               1883
#define MQTTS_PORT              8883

// ============================================================
// MQTT Configuration
// ============================================================
#define MQTT_BROKER             "192.168.1.100"
#define MQTT_CLIENT_PREFIX      "ldg-tuner"
#define MQTT_USERNAME           "ldgcontroller"
#define MQTT_PASSWORD           "change_me"
#define MQTT_QOS                1
#define MQTT_RETAIN_TELEMETRY   false
#define MQTT_KEEPALIVE          60

// MQTT Topics (base path)
#define MQTT_TOPIC_BASE         "ldg/tuner"
#define MQTT_TOPIC_FWD_POWER    MQTT_TOPIC_BASE "/telemetry/fwd_power"
#define MQTT_TOPIC_REF_POWER    MQTT_TOPIC_BASE "/telemetry/ref_power"
#define MQTT_TOPIC_SWR          MQTT_TOPIC_BASE "/telemetry/swr"
#define MQTT_TOPIC_BAND         MQTT_TOPIC_BASE "/telemetry/band"
#define MQTT_TOPIC_STATUS       MQTT_TOPIC_BASE "/status"
#define MQTT_TOPIC_CMD          MQTT_TOPIC_BASE "/command"
#define MQTT_TOPIC_CMD_RESPONSE MQTT_TOPIC_BASE "/command/response"

// Remote unit MQTT topics (prefixed with unit ID)
#define MQTT_REMOTE_TOPIC_BASE  "ldg/tuner/remote"
#define MQTT_REMOTE_TELEMETRY   MQTT_REMOTE_TOPIC_BASE "/telemetry"
#define MQTT_REMOTE_STATUS      MQTT_REMOTE_TOPIC_BASE "/status"
#define MQTT_REMOTE_CMD         MQTT_REMOTE_TOPIC_BASE "/command"

// ============================================================
// Serial (Tuner) Configuration
// ============================================================
#define TUNER_SERIAL_BAUD       38400
#define TUNER_RX_PIN            16  // UART2 RX - connect to tuner TX
#define TUNER_TX_PIN            17  // UART2 TX - connect to tuner RX
#define TUNER_UART_NUM          UART_NUM_2
#define TUNER_CMD_DELAY_MS      250
#define TUNER_WAKE_DELAY_MS     1

// ============================================================
// Display Configuration (WITH_DISPLAY builds)
// ============================================================
#ifdef WITH_DISPLAY
#define DISPLAY_WIDTH           480
#define DISPLAY_HEIGHT          320
#define DISPLAY_ROTATION        1

// BTT TFT35-SPI uses ILI9488 or similar
#define TFT_MOSI                23
#define TFT_MISO                19
#define TFT_SCLK                18
#define TFT_CS                  5
#define TFT_DC                  15
#define TFT_RST                 26
#define TFT_BL                  32

// Touch (XPT2046) - shares SPI bus with display (MOSI/MISO/SCLK)
#define TOUCH_CS                33
#define TOUCH_IRQ               39
#endif

// ============================================================
// Security Configuration
// ============================================================
#define WEB_AUTH_ENABLED        true
#define WEB_AUTH_USERNAME       "admin"
#define WEB_AUTH_PASSWORD       "change_me_please"
#define WEB_RATE_LIMIT_REQUESTS 10
#define WEB_RATE_LIMIT_WINDOW_MS 1000
#define MAX_LOGIN_ATTEMPTS      5
#define LOGIN_LOCKOUT_MS        300000  // 5 minutes

// TLS (optional, requires certificates in LittleFS)
#define TLS_ENABLED             false
#define TLS_CA_CERT_PATH        "/certs/ca.pem"
#define TLS_SERVER_CERT_PATH    "/certs/server.pem"
#define TLS_SERVER_KEY_PATH     "/certs/server.key"

// MQTT TLS (optional)
#define MQTT_TLS_ENABLED        false
#define MQTT_TLS_FINGERPRINT    ""  // SHA1 fingerprint of broker cert

// ============================================================
// Remote Unit Configuration (REMOTE_UNIT builds)
#ifdef REMOTE_UNIT
#define REMOTE_UNIT_ID          1
#else
#define REMOTE_UNIT_ID          0
#endif

// ============================================================
// System
// ============================================================
#define HOSTNAME_PREFIX         "ldg-tuner"
#define OTA_PASSWORD            "ota_password_change_me"
#define NTP_SERVER1             "pool.ntp.org"
#define NTP_SERVER2             "time.nist.gov"
#define NTP_TIMEZONE            "EST5EDT,M3.2.0,M11.1.0"
#define HEARTBEAT_INTERVAL_MS   30000

// ============================================================
// Meter Calculation
// ============================================================
#define METER_PSU_VOLTAGE       13.8  // Tuner's power supply voltage (affects power/SWR accuracy)
#define METER_POWER_SCALE       1000  // 1000 for AG-1000ProII, 600 for AG-600ProII
#define METER_LOW_SCALE         100   // Low range scale (100W for both models)
#define METER_POWER_SCALE       1000  // 1000 for AG-1000ProII, 600 for AG-600ProII
#define METER_LOW_SCALE         100   // Low range scale (100W for both models)
#define METER_POWER_SCALE       1000  // 1000 for AG-1000ProII, 600 for AG-600ProII
#define METER_LOW_SCALE         100   // Low range scale (100W for both models)
#define METER_POWER_SCALE       1000  // 1000 for AG-1000ProII, 600 for AG-600ProII
#define METER_LOW_SCALE         100   // Low range scale (100W for both models)
#define METER_POWER_SCALE       1000  // 1000 for AG-1000ProII, 600 for AG-600ProII
#define METER_LOW_SCALE         100   // Low range scale (100W for both models)
#define METER_POWER_SCALE       1000  // 1000 for AG-1000ProII, 600 for AG-600ProII
#define METER_LOW_SCALE         100   // Low range scale (100W for both models)
#define METER_POWER_SCALE       1000  // 1000 for AG-1000ProII, 600 for AG-600ProII
#define METER_LOW_SCALE         100   // Low range scale (100W for both)
#define METER_POWER_SCALE       1000  // 1000 for AG-1000ProII, 600 for AG-600ProII
#define METER_LOW_SCALE         100   // Low range scale (100W for both)
#define METER_POWER_SCALE       1000  // 1000 for AG-1000ProII, 600 for AG-600ProII
#define METER_LOW_SCALE         100   // Low range scale (100W for both)
#define METER_POWER_SCALE       1000  // 1000 for AG-1000ProII, 600 for AG-600ProII
#define METER_LOW_SCALE         100   // Low range scale (100W for both)

#endif // CONFIG_H
