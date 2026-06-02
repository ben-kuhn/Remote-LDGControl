# LDG Tuner Remote Controller

ESP32-based remote control interface for LDG AG-1000ProII (and compatible) auto-tuners. Provides web UI, MQTT integration, and optional touchscreen display.

## Features

- **Tuner Control**: Full remote control via TTL serial interface
  - Memory Tune, Full Tune, Bypass toggle
  - Auto/Manual mode selection
  - Antenna A/B switching
- **Real-time Metering**: Forward power, reflected power, SWR, band detection
- **Web Interface**: Responsive browser UI with WebSocket live updates
- **MQTT Integration**: Telemetry publishing and command subscription
- **Optional Display**: BTT TFT35-SPI with LVGL touchscreen UI
- **Dual Build Modes**:
  - `esp32-display`: Full unit with touchscreen (shack controller)
  - `esp32-remote`: Headless unit for remote tuner locations
  - `esp32-nodisplay`: Headless without remote link
- **Security**: HTTP Basic Auth, rate limiting, login lockout, CSRF tokens

## Hardware

### Required
- ESP32 WROOM development board
- USB to TTL serial adapter (3.3V or 5V, for initial flash)
- 4-pin mini-DIN connector for tuner interface
  - Kycon KMDLAX-4P available from Mouser
  - **WARNING**: Do NOT use S-Video cables - pins are wired differently and one carries +12V

### Optional (Display Unit)
- BTT TFT35-SPI display (ILI9488 controller)
- XPT2046 touchscreen (built into BTT TFT35)

### Wiring

#### Tuner TTL Interface (4-pin mini-DIN)

| Pin | Signal | ESP32 Pin | Notes |
|-----|--------|-----------|-------|
| 1   | +12V   | -         | Do NOT connect (meter power only) |
| 2   | GND    | GND       | Common ground |
| 3   | TX     | GPIO 16   | Tuner TX -> ESP32 RX |
| 4   | RX     | GPIO 17   | ESP32 TX -> Tuner RX |

**Serial parameters**: 38400 baud, 8N1, TTL levels (5V)

> **WARNING**: The TTL voltage levels are NOT RS-232. Do not connect directly to an RS-232 port without a level shifter.

#### BTT TFT35-SPI (Display Unit)

| Signal | ESP32 Pin | Notes |
|--------|-----------|-------|
| MOSI   | GPIO 23   | SPI data |
| MISO   | GPIO 19   | SPI data |
| SCLK   | GPIO 18   | SPI clock |
| CS     | GPIO 5    | Display chip select |
| DC     | GPIO 15   | Data/command |
| RST    | GPIO 26   | Reset |
| BL     | GPIO 32   | Backlight |
| T_CS   | GPIO 33   | Touch chip select |
| T_IRQ  | GPIO 39   | Touch interrupt |
| T_DIN  | GPIO 32   | Touch MOSI (shared with BL) |
| T_DO   | GPIO 39   | Touch MISO (shared with IRQ) |
| T_CLK  | GPIO 25   | Touch clock |

## Install

### Option 1: Web Flasher (Recommended)

No software installation required. Works in Chrome, Edge, or Opera.

1. Visit the [Web Flasher](https://yourusername.github.io/Remote-LDGControl/)
2. Connect your ESP32 via USB
3. Select your firmware variant and click **Install**
4. After flashing, connect to the `LDGConfig` WiFi AP to configure network settings

### Option 2: Download Pre-built Firmware

Download `.bin` files from the [latest release](https://github.com/ben-kuhn/Remote-LDGControl/releases), then flash with `esptool.py`:

```bash
esptool.py --chip esp32 --port /dev/ttyUSB0 write_flash 0x0 ldg-tuner-display.bin
```

### Option 3: Build from Source

Requires PlatformIO (`pip install platformio`).

```bash
# Display unit (shack controller with touchscreen)
pio run -e esp32-display -t upload

# Remote unit (headless, for remote tuner location)
pio run -e esp32-remote -t upload

# Basic headless unit
pio run -e esp32-nodisplay -t upload

# Monitor serial output
pio device monitor
```

## CI/CD

This project uses GitHub Actions for automated builds:
- Every push to `main` builds all three firmware variants
- Tags (e.g., `v1.0.0`) create a GitHub Release with pre-built binaries
- The `main` branch deploys the web flasher to GitHub Pages

## Configuration

Edit `include/config.h` before building. Key settings:

### WiFi
```c
#define WIFI_STA_SSID       "your_network"
#define WIFI_STA_PASSWORD   "your_password"
#define WIFI_AP_SSID        "LDGControl"     // AP mode SSID (display unit)
#define WIFI_AP_PASSWORD    "ldgcontrol"      // AP mode password
```

### MQTT
```c
#define MQTT_BROKER         "192.168.1.100"
#define MQTT_USERNAME       "ldgcontroller"
#define MQTT_PASSWORD       "change_me"
```

### Web Auth
```c
#define WEB_AUTH_USERNAME   "admin"
#define WEB_AUTH_PASSWORD   "change_me_please"
```

### Tuner
```c
#define TUNER_RX_PIN        16
#define TUNER_TX_PIN        17
#define METER_PSU_VOLTAGE   13.8  // Adjust for accurate power readings
```

## Web Interface

Access at `http://<esp32-ip>/` or `http://ldg-tuner-<mac>.local/`

### API Endpoints

All endpoints require HTTP Basic Auth.

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/` | Web UI |
| GET | `/api/status` | JSON status (power, SWR, band, mode) |
| POST | `/api/command/toggle` | Toggle antenna A/B |
| POST | `/api/command/memory_tune` | Memory tune |
| POST | `/api/command/full_tune` | Full tune |
| POST | `/api/command/bypass` | Toggle bypass |
| POST | `/api/command/auto` | Set auto mode |
| POST | `/api/command/manual` | Set manual mode |

### WebSocket

Connect to `ws://<esp32-ip>/ws` for real-time meter updates.

## MQTT Topics

### Telemetry (published)

| Topic | Description |
|-------|-------------|
| `ldg/tuner/telemetry/fwd_power` | Forward power (Watts), JSON |
| `ldg/tuner/telemetry/ref_power` | Reflected power (Watts), JSON |
| `ldg/tuner/telemetry/swr` | SWR ratio |
| `ldg/tuner/telemetry/band` | Current band |
| `ldg/tuner/status` | Online/offline status (retained) |

### Commands (subscribed)

| Topic | Description |
|-------|-------------|
| `ldg/tuner/command` | Command JSON: `{"command": "full_tune"}` |

Valid commands: `toggle`, `memory_tune`, `full_tune`, `bypass`, `auto`, `manual`

Response published to `ldg/tuner/command/response`.

## Protocol Reference

### Serial Communication

- **Baud**: 38400
- **Format**: 8N1
- **Levels**: TTL (5V)

### Command Sequence

1. Send wake: two space characters (`0x20`) with 1ms delay
2. Send command byte
3. Wait 250ms minimum before next command
4. Read response (control mode) or meter telemetry (meter mode)

### Commands

| Byte | Function | Response |
|------|----------|----------|
| ` ` (0x20) | Wake (send 2x) | None |
| `A` | Toggle antenna | `A` or `B` |
| `T` | Memory tune | `T` (good), `M` (okay), `F` (fail) |
| `F` | Full tune | `T`, `M`, `F` |
| `P` | Toggle bypass | `P` |
| `C` | Auto mode | `A` |
| `M` | Manual mode | `M` |
| `Z` | Sync | Status byte |
| `S` | Start meter telemetry | None |
| `X` | Stop meter telemetry | None |

### Meter Telemetry

6 bytes + `;;` (0x3B3B) end-of-message marker:

| Bytes | Description | Byte Order |
|-------|-------------|------------|
| 0-1 | Forward power (raw) | Big-endian |
| 2-3 | Reflected power (raw) | Big-endian |
| 4-5 | Band indicator | Big-endian |

### Power Calculation

```
P = ((1000 * V_psu * raw) / (65536 * 0.707))^2 / 50
```

Where `V_psu` is your power supply voltage (default 13.8V).

### Band Detection

| Band | Indicator Range |
|------|----------------|
| 160m | 8230 - 9145 |
| 80m  | 4110 - 4710 |
| 60m  | 3060 - 3080 |
| 40m  | 2250 - 2355 |
| 30m  | 1600 - 1630 |
| 20m  | 1140 - 1180 |
| 17m  | 900 - 915 |
| 15m  | 766 - 782 |
| 12m  | 655 - 662 |
| 10m  | 550 - 590 |
| 6m   | 320 - 360 |

## Remote Unit Link

When the tuner is located near the antenna (e.g., in a shack attic or external enclosure), use a remote unit configuration.

### Architecture

```
[Display Unit] <---WiFi AP---> [Remote Unit] <---TTL---> [LDG Tuner]
  (in shack)       HTTP/SSE       (at antenna)
  - Web UI
  - SSE server                  - SSE client (receives commands)
  - Touchscreen                 - HTTP POST (sends telemetry)
  - HTTP POST receiver          - Executes commands
```

No MQTT broker required. The remote unit connects to the display unit's WiFi AP and communicates over HTTP/SSE.

### How It Works

**Remote Unit** (`esp32-remote`):
- Connects to display unit's WiFi AP (`LDGControl`)
- Opens SSE connection to `http://192.168.4.1/api/events` for commands
- POSTs telemetry to `http://192.168.4.1/api/telemetry` every 200ms
- Uses HTTP Basic Auth (same credentials as web UI)

**Display Unit** (`esp32-display`):
- Creates WiFi AP for remote unit
- Accepts telemetry POSTs, pushes to SSE clients (browser UI)
- Pushes commands to SSE when web UI/MQTT commands arrive
- Falls back to local tuner when remote disconnects

### Configuration

```c
// Both units (config.h)
#define WIFI_AP_SSID        "LDGControl"
#define WIFI_AP_PASSWORD    "ldgcontrol"
#define WEB_AUTH_USERNAME   "admin"
#define WEB_AUTH_PASSWORD   "change_me_please"
```

### MQTT (Optional)

If you have an MQTT broker and want Home Assistant integration, both units can also connect to it. The remote unit publishes to `ldg/tuner/remote/telemetry` and the display unit subscribes. This is additive to the HTTP/SSE link, not a replacement.

## Security

### Threat Model

This device will inevitably be exposed to networks by people who shouldn't. Defense in depth:

1. **HTTP Basic Auth** on all web endpoints
2. **Rate limiting** (10 requests/second per IP)
3. **Login lockout** (5 failed attempts = 5 minute ban)
4. **CSRF tokens** for state-changing operations
5. **MQTT authentication** (username/password)
6. **No open relays** - device doesn't proxy or forward

### Hardening Recommendations

- **Never** port-forward the HTTP port to the internet
- Use MQTT TLS if your broker supports it (`MQTT_TLS_ENABLED`)
- Change all default passwords in `config.h`
- Consider client certificate authentication for MQTT (see `config.h` TLS section)
- Place on an isolated IoT VLAN if possible
- Use the AP-only mode (`REMOTE_UNIT`) for air-gapped operation

### TLS Setup (Optional)

1. Generate certificates:
```bash
openssl req -x509 -newkey rsa:2048 -keyout server.key -out server.pem -days 365 -nodes
```

2. Upload to LittleFS:
```bash
mkdir -p data/certs
cp server.key data/certs/
cp server.pem data/certs/
pio run -t uploadfs
```

3. Enable in `config.h`:
```c
#define TLS_ENABLED true
```

## License

This project is provided as-is for amateur radio use. Protocol information derived from reverse engineering of the LDG AG-1000ProII and the [LDGControl](https://github.com/Efpophis/LDGControl) project.

## Credits

- Protocol reverse engineering: Efpophis and contributors (LDGControl project)
- Meter power formula: Community-contributed (see LDGControl source)
- Band detection thresholds: Ardugnome (LDGControl, November 2022)
