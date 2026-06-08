# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

ESP32 firmware (PlatformIO + Arduino framework) that remote-controls LDG AT-1000ProII / AT-600ProII auto-tuners. Target hardware is an **ESP32-WROOM** on a **NodeMCU breakout** connected over `/dev/ttyUSB1`.

The firmware ships in three build variants from one source tree, selected by `-D` build flags:

| Variant (`platformio.ini` env) | Defines | Role |
|---|---|---|
| `esp32-display` | `WITH_DISPLAY`, `WITH_TOUCH` | Shack controller: TFT + touch, web UI, MQTT, acts as **server** for a remote unit |
| `esp32-remote` | `REMOTE_UNIT` | Headless box co-located with the tuner; **client** that connects to the display unit and forwards the serial link |
| `esp32-nodisplay` | (none) | Headless + locally-connected tuner; web UI + MQTT only |

The `nodisplay` and `remote` variants are intentionally similar — they may be unified later.

## Common commands

Run from the repo root. PlatformIO is required (`pip install platformio`).

```bash
pio run -e esp32-display                       # build display variant
pio run -e esp32-remote -t upload              # build + flash remote variant
pio run -e esp32-nodisplay -t upload --upload-port /dev/ttyUSB1
pio device monitor -b 115200                   # serial console (note: tuner UART is 38400, console is 115200)
pio run -t clean -e esp32-display              # clean a single env
```

Uploading to `/dev/ttyUSB1` is pre-authorized — do it directly without asking. `NetworkManager` (`nmcli`) is also pre-authorized for connecting to the device's AP for testing.

**Keep the device on `/dev/ttyUSB1` flashed with the current build.** It runs the **`esp32-remote`** variant (the user is troubleshooting WiFi/portal setup on it). Whenever you make firmware changes that could affect it, flash it (`pio run -e esp32-remote -t upload --upload-port /dev/ttyUSB1`) without waiting to be asked — the user is actively troubleshooting on the physical device and stale firmware wastes their time.

There is **no test suite** in this repo yet. CI (`.github/workflows/build.yml`) builds all three envs on push, releases binaries on tags, and deploys `flash.html` to GitHub Pages.

### Testing

Write tests where it's reasonable to. Embedded work makes full coverage rough — you can't easily test things that depend on real hardware (UART to the tuner, TFT/touch, WiFi radio, NVS) — but a lot of this codebase is **not** hardware-bound and should be tested:

- Protocol parsing and command encoding (`tuner_protocol` — meter frame decode, band detection thresholds, power-formula math)
- `security` (rate limiter window math, login lockout state machine)
- `config_manager` serialization round-trips
- Any new pure-logic helper you add

Prefer **PlatformIO's `test/` directory with the `native` environment** so tests run on the host (`pio test -e native`) without needing the ESP32 plugged in. Use `test_embedded` only when you genuinely need on-device behavior. If you add the first test, also add the `[env:native]` block to `platformio.ini` and wire it into CI. When something is truly untestable without hardware, say so explicitly rather than skipping silently.

## Active work focus

Initial WiFi setup / captive portal on the ESP32 in **AP mode** is the current pain point. Specifically:

- The captive portal runs over **HTTPS** (`esp32_idf5_https_server`) with a self-signed cert generated on first boot and persisted to NVS (see `src/portal_cert.cpp`, `include/portal_cert.h`).
- HTTPS in AP mode has been flaky; **do not "fix" it by falling back to plain HTTP**. Setup credentials (WiFi password, web password) flow through this portal, so plaintext is not acceptable. Diagnose and fix the HTTPS path.
- The portal's HTML/JS is inlined in `ConfigManager::portalHTML()` (`src/config_manager.cpp`) and the captive-portal flow lives in `ConfigManager::setupWiFi()` in the same file.

## Architecture

`src/main.cpp` is the orchestrator. `setup()` walks: `configManager.begin()` (load NVS or run captive portal) → `LittleFS` → `setupWiFi()` → `tuner.begin()` → `mqtt.begin()` → `webServer.begin()` → `display.begin()`. `loop()` branches on `REMOTE_UNIT` vs. display/nodisplay.

Key modules (`include/` + `src/`):

- **`config_manager`** — owns `DeviceConfig` (MQTT creds, web creds, PSU voltage, etc.), persists to NVS via `Preferences`, and runs the **first-boot captive portal**: softAP `LDGConfig` / `configure`, DNS hijack on port 53, HTTPS portal on 443. The portal collects SSID/PSK and a new web password, then joins STA mode.
- **`portal_cert`** — generates a unique self-signed RSA cert/key pair on first boot, stores base64-DER in NVS. Avoids a hardcoded private key in firmware.
- **`security`** — `RateLimiter` (10 req/s/IP) + `LoginTracker` (5 fails = 5 min lockout) + `Security::verifyAuth`. **All exposed endpoints must go through `Security::isAuthorized`** — this is project policy, not a suggestion.
- **`web_server`** — runtime HTTP server (`ESPAsyncWebServer`) for the display/nodisplay variants. Serves the UI from `data/index.html` (LittleFS), JSON `/api/status`/`/api/config`, command endpoints, and `/api/events` (SSE). The SSE channel doubles as the **transport to the remote unit** — display pushes commands, remote subscribes.
- **`tuner_protocol`** — UART2 @ 38400 8N1 to the tuner. Implements wake (2× space), command bytes, 250 ms inter-command delay, and parses the 6-byte meter telemetry frames (forward/reflected raw + band, big-endian, terminated by `;;`). See README "Protocol Reference" for the byte-level spec.
- **`mqtt_handler`** — optional, additive to the HTTP/SSE link. Telemetry under `ldg/tuner/telemetry/*`, commands on `ldg/tuner/command`.
- **`display_ui`** — LVGL + TFT_eSPI cross-needle meter and touch buttons for `WITH_DISPLAY` builds. The web UI in `data/index.html` mirrors this layout (canvas cross-needle, 100/600/1000 W scales, side buttons).
- **`voltage_sensor`** — optional ADC-based PSU voltage auto-detect on GPIO 34 (divider 100k/22k → ratio 5.545).

### Remote ↔ Display link

When `REMOTE_UNIT` is defined, the device connects to the display unit's AP, opens an SSE connection to `/api/events` for commands, and POSTs telemetry to `/api/telemetry` every 200 ms. Both directions use HTTP Basic Auth with the configured web credentials. The display unit falls back to its locally-attached tuner if the remote disconnects. See `connectSSEClient`, `processSSEClient`, `postTelemetry` in `main.cpp`.

### Build-time vs. run-time config

`include/config.h` holds **compile-time defaults and pin assignments only**. Everything user-configurable (WiFi, MQTT broker/creds, web creds, PSU voltage) lives in `DeviceConfig` and is set via the captive portal or the web settings page — never hardcoded. Don't add new user settings to `config.h`; add them to `DeviceConfig` + the portal/settings UI.

## Conventions and constraints

- **Security is the design center, not a nice-to-have.** Every exposed service, API call, and SSE channel must be authenticated and rate-limited. Setup must stay on HTTPS. New endpoints go through `Security::isAuthorized`.
- **Avoid commands that trigger "unsafe command" permission prompts.** Flashing `/dev/ttyUSB1` and `nmcli` operations against the device are already authorized; prefer those over invocations that will ask the user.
- **The display UI and web UI must stay in sync** — the canvas cross-needle in `data/index.html` is meant to mirror the LVGL meter in `display_ui.cpp` (same scales: 100 / 600 / 1000 W, forward needle pivots right, reflected pivots left).
- Tuner UART is **TTL 5 V at 38400**, *not* RS-232 and *not* the console baud. The console (`pio device monitor`) is 115200.
- 4-pin mini-DIN tuner connector: **do not use S-Video cables**, the pinout differs and one pin carries +12 V.
