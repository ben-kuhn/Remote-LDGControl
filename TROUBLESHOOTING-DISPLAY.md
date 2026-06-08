# Display Troubleshooting — BTT TFT35-SPI V2.1

Working notes for an in-progress debug. Pick this up from a clean session.

## Symptoms

- Hardware: NodeMCU ESP32S v1.1 + BTT TFT35-SPI V2.1, wired per `WIRING.md` / `wiring-diagram.svg`.
- Firmware: `esp32-display` variant (built and flashed successfully).
- On boot: backlight comes on, display flickers briefly, then settles to a gray/white screen. No LVGL UI, no boot artifacts.
- Tuner not connected at the time of test — display unit on the bench only.

## What we know about the hardware

Source: BTT's own hardware files (https://github.com/bigtreetech/TFT35-SPI/tree/master/v2/Hardware), specifically `BTT TFT35-SPI V2.1_IO.pdf` and `BTT TFT35-SPI V2.1_SCH.pdf`.

The BTT TFT35-SPI V2.1 is a **passive** ILI9488 + NS2009 breakout. There is **no onboard MCU** — the J1 connector goes straight to the LCD's SPI bus and the NS2009 touch I2C. This contradicts an earlier guess that it had an STM32 in the path; ignore that theory.

J1 (XH 2.54 mm, 10-pin) pinout, confirmed against `_IO.pdf`:

| J1 pin | Signal | Notes |
|---|---|---|
| 1 | IORQ (NS2009 PENIRQ) | Touch interrupt |
| 2 | SCL (NS2009) | I2C clock |
| 3 | SDA (NS2009) | I2C data |
| 4 | RS (LCD)  | Data/command |
| 5 | NSS (LCD) | Chip select |
| 6 | SCK (LCD) | SPI clock |
| 7 | MOSI (LCD) | |
| 8 | MISO (LCD) | |
| 9 | GND | |
| 10 | +5V | Board has on-board AMS1117-3.3 |

The `WIRING.md` table is correct against this. Don't change it.

**Critically: there is no RST or BL pin on J1.**
- The ILI9488's RESET (U4 pin 15 in the schematic) is pulled high by a 10 kΩ resistor (`R10`) to the on-board +3.3V rail. No external drive line, no RC cap. The LCD relies on the AMS1117's slow output ramp for its power-on reset.
- The LCD backlight (U4 pins 33–36, anode + 3× cathode) is wired straight through a current-limiting resistor — always on whenever +5V is supplied. No backlight control pin.

So the firmware's `TFT_RST=26` and `TFT_BL=32` are **wrong** — those GPIOs aren't connected to anything on the BTT side, and the `README.md` pin table that lists "Display RST → GPIO 26" / "Display BL → GPIO 32" is also wrong. `WIRING.md` and the SVG omit RST/BL correctly.

## Why the symptom is plausible

With current settings:
- TFT_eSPI's init toggles GPIO 26 thinking it's resetting the LCD — but GPIO 26 is wired to nothing, so the LCD never gets a hardware reset.
- The LCD's only reset path is its own power-on reset via R10 + AMS1117 ramp. If that doesn't fire cleanly, the panel sits in an indeterminate state (gray/white is consistent with this).
- TFT_eSPI's ILI9488 init does send a software reset (`0x01`) as part of the command sequence, which *should* recover this — but only if SPI commands are actually getting through.
- `SPI_FREQUENCY=40000000` (40 MHz) is above the ILI9488 datasheet's typical 20–27 MHz max for writes. On breadboard jumpers, 40 MHz routinely produces "gray screen, init didn't take" symptoms because edges are garbled.

## Proposed fix (not yet applied)

Do all of these in one batch because they're cheap and each rules out a likely cause:

1. **`platformio.ini` and `include/User_Setup.h`**: set `TFT_RST=-1` (tell TFT_eSPI there's no reset pin) and remove `TFT_BL=32`. Stops TFT_eSPI from driving phantom pins and makes the config honest.
2. **`platformio.ini` and `include/User_Setup.h`**: lower `SPI_FREQUENCY` from `40000000` to `20000000` to rule out signal integrity. Can step back up after we have a working baseline.
3. **`README.md`**: drop the RST and BL rows from the pin table; add a one-line note that the BTT V2.1 handles both internally. Leave `WIRING.md` and SVG alone (already correct).
4. **`src/display_ui.cpp` (`DisplayUI::begin`)**: right after `m_tft->init()` + `setRotation()`, insert `m_tft->fillScreen(TFT_RED); delay(500);` before LVGL takes over. This isolates "is the LCD accepting commands at all?" from "is LVGL's pipeline OK?" — a red flash means SPI/init is working and the problem is elsewhere; still gray means SPI commands aren't landing.

## How to pick this back up

1. Read this file, `WIRING.md`, and the relevant code (`include/User_Setup.h`, `src/display_ui.cpp:begin`, `platformio.ini` `esp32-display` env).
2. Apply the 4 changes above as one commit.
3. Flash to the display unit and capture serial output: `pio device monitor -b 115200`. Reset the board and look for:
   - Does `setup()` reach `display.begin()`?
   - Any panics, brownouts, or stack traces?
   - Is the red fill from step 4 visible?
4. Interpret:
   - **Red flash, then LVGL UI** → fixed, step `SPI_FREQUENCY` back up to find the ceiling.
   - **Red flash, then still gray** → SPI works but LVGL pipeline is broken. Look at `displayFlush`, draw buffer sizes, or panel rotation.
   - **Still gray, no red flash** → SPI commands aren't reaching the LCD. Move on to wiring continuity: verify each ESP32 GPIO ↔ J1 pin with a multimeter, check JST-XH crimps, check shared ground.
5. If wiring checks pass and SPI still doesn't work, possible follow-ups: explicit pre-init delay (give AMS1117 time to ramp), try a different ESP32 board (rule out a damaged GPIO), try a known-good ILI9488 driver sketch from TFT_eSPI's examples to rule out our config.

## Things to NOT do

- Don't change `WIRING.md` or `wiring-diagram.svg` — the J1 pinout there is correct against the BTT schematic.
- Don't add a "fallback to plain HTTP" or similar shortcut for any other issue you find along the way. Project policy in `CLAUDE.md` stands.
- Don't assume the BTT has an onboard MCU or a Marlin/SPI mode — it doesn't, that was a wrong guess earlier in the debug session.

## Reference files

- `platformio.ini` — `esp32-display` env, `build_flags` for TFT_eSPI.
- `include/User_Setup.h` — TFT_eSPI pin configuration.
- `include/config.h` — display pin constants (`TFT_*`, `TOUCH_*`, `DISPLAY_*`).
- `src/display_ui.cpp` — `DisplayUI::begin()` is where `m_tft->init()` happens.
- `WIRING.md` — confirmed-correct J1 pinout.
- `README.md` lines listing "Display RST | GPIO 26" and "Display BL | GPIO 32" — these are the doc lines to delete.
