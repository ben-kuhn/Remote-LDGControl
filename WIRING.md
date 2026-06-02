# Wiring Diagram

## ESP32 WROOM Pin Assignment

```
                    ┌─────────────────────────────────────┐
                    │           ESP32 WROOM               │
                    │                                     │
         3V3 ───────┤ 3V3                           GND   ├────── GND (common)
                    │                                     │
    (not used) ─────┤ GPIO 0                        GPIO 1 ├──── (not used, TX0)
                    │                                     │
    (not used) ─────┤ GPIO 2                        GPIO 3 ├──── (not used, RX0)
                    │                                     │
                    │ GPIO 4                        GPIO 5 ├──── TFT CS
    (not used) ─────┤                                     │
                    │ GPIO 6 (flash)                GPIO 7 ├ (flash)
    (not used) ─────┤                                     │
                    │ GPIO 8 (flash)                GPIO 9 ├ (flash)
    (not used) ─────┤                                     │
                    │ GPIO 10 (flash)              GPIO 11 ├ (flash)
    (not used) ─────┤                                     │
                    │ GPIO 12                      GPIO 13 ├──── (not used)
    (not used) ─────┤                                     │
                    │ GPIO 14                      GPIO 15 ├──── TFT DC
    (not used) ─────┤                                     │
                    │ GPIO 16 ──── Tuner RX        GPIO 17 ├──── Tuner TX
    (not used) ─────┤                                     │
                    │ GPIO 18 ──── SPI SCLK        GPIO 19 ├──── SPI MISO
    (not used) ─────┤                                     │
                    │ GPIO 20                      GPIO 21 ├──── (not used)
    (not used) ─────┤                                     │
                    │ GPIO 22                      GPIO 23 ├──── SPI MOSI
    (not used) ─────┤                                     │
                    │ GPIO 25 ──── (reserved)      GPIO 26 ├──── TFT RST
    (not used) ─────┤                                     │
                    │ GPIO 27 ──── (reserved)      GPIO 28 ├ (not populated)
    (not used) ─────┤                                     │
                    │ GPIO 29 (not populated)      GPIO 30 ├ (not populated)
    (not used) ─────┤                                     │
                    │ GPIO 31 (not populated)      GPIO 32 ├──── TFT Backlight
    (not used) ─────┤                                     │
                    │ GPIO 33 ──── Touch CS        GPIO 34 ├ (input only, unused)
    (not used) ─────┤                                     │
                    │ GPIO 35 (input only, unused) GPIO 36 ├ (input only, unused)
    (not used) ─────┤                                     │
                    │ GPIO 37 (not populated)      GPIO 38 ├ (not populated)
    (not used) ─────┤                                     │
                    │ GPIO 39 ──── Touch IRQ       EN     ├──── (reset button)
                    └─────────────────────────────────────┘
                              USB (programming)
```

## Connection 1: Tuner TTL Interface + Power (4-pin mini-DIN)

**CRITICAL**: The mini-DIN connector on the AG-1000ProII is NOT standard S-Video. Pin 1 carries +12V. Do NOT use an S-Video cable.

### Power from Tuner +12V (Linear Regulator)

A linear regulator is preferred over a switching buck converter to avoid RF noise injection into your receiver.

**Thermal calculation:**
- Input: 12V (11-16V per tuner spec)
- Output: 5V (display unit) or 3.3V (remote unit)
- Peak current: ~350mA (ESP32 WiFi TX + display)
- Dissipation at 5V: (12V - 5V) × 0.35A = **2.45W**
- Dissipation at 3.3V: (12V - 3.3V) × 0.2A = **1.74W**

A TO-220 package with a small heatsink handles this easily.

```
Tuner +12V (mini-DIN Pin 1)
    │
    ├──┤ 100nF ceramic ──┤ (close to regulator input)
    │
    ┌─────────────────────────┐
    │   LM2940CT-5.0 (TO-222) │  ← requires heatsink
    │   IN ───────────────────┤
    │   OUT ──────────────────├── 5V ── ESP32 5V / Display VCC
    │   GND ──────────────────┤
    └─────────────────────────┘
    │
    ├──┤ 10µF tantalum ──┤ (close to regulator output)
    │
    └── GND (mini-DIN Pin 2) ─── ESP32 GND / Display GND
```

**Regulator selection:**
- **Display unit**: LM2940CT-5.0 (5V, 1A LDO) + TO-220 heatsink
- **Remote unit (no display)**: LM2940CT-3.3 (3.3V, 1A LDO) + small heatsink
- Alternative: LM7805 / LM78L33 (higher dropout voltage, needs larger heatsink)

### Complete Wiring

```
┌─────────────────────────────────────────────────────────┐
│              LDG AG-1000ProII Rear Panel                │
│                                                         │
│   4-pin mini-DIN (REMOTE connector)                     │
│                                                         │
│   Pin 1 ─── +12V ── LM2940CT-5.0 ── 5V ── ESP32 5V    │
│   Pin 2 ─── GND  ─────────────────────── ESP32 GND     │
│   Pin 3 ─── TX   ─────────────────────── ESP32 GPIO 16 │
│   Pin 4 ─── RX   ─────────────────────── ESP32 GPIO 17 │
│                                                         │
│   Serial: 38400 baud, 8N1, TTL (5V)                    │
│                                                         │
│   NOTE: If your ESP32 is 3.3V logic only, use a        │
│   level shifter (e.g., TXS0108E or 74HCT245) between   │
│   the ESP32 and tuner for reliable communication.      │
│   Many ESP32 dev boards have 5V-tolerant GPIO.         │
└─────────────────────────────────────────────────────────┘
```

### Mini-DIN Connector Pinout (viewed from front, pins facing you)

```
       ┌─────┐
      /  1   \     Pin 1: +12V (top)
     | 4   2 |    Pin 2: GND (right)
      \  3   /     Pin 3: TX  (bottom)
       └─────┘     Pin 4: RX  (left)
```

## Connection 2: BTT TFT35-SPI Display

The display and touchscreen share the same SPI bus.

```
┌─────────────────────────────────────────────────────────┐
│              BTT TFT35-SPI (3.5" 480x320)               │
│                                                         │
│   Signal     ESP32 Pin    Notes                         │
│   ─────────────────────────────────────────────────     │
│   3V3        3V3          Power (or 5V if module has    │
│                           regulator)                    │
│   GND        GND          Common ground                 │
│   MOSI       GPIO 23      Shared SPI data              │
│   MISO       GPIO 19      Shared SPI data              │
│   SCK        GPIO 18      Shared SPI clock             │
│   CS         GPIO 5       Display chip select          │
│   DC/RS      GPIO 15      Data/Command select          │
│   RESET      GPIO 26      Display reset                │
│   BL/LED     GPIO 32      Backlight control            │
│                                                         │
│   Touch (XPT2046):                                      │
│   T_CS       GPIO 33      Touch chip select            │
│   T_IRQ      GPIO 39      Touch interrupt (input only) │
│   T_DIN      GPIO 23      Shared with MOSI             │
│   T_DO       GPIO 19      Shared with MISO             │
│   T_CLK      GPIO 18      Shared with SCK              │
└─────────────────────────────────────────────────────────┘
```

### SPI Bus Wiring (shared)

```
                    ESP32
                  ┌───────┐
                  │ GPIO18├── SCLK ──────────┬── Display SCK
                  │       │                  │
                  │ GPIO23├── MOSI ──────────┼── Display MOSI
                  │       │                  ├── Touch T_DIN
                  │ GPIO19├── MISO ──────────┼── Display MISO
                  │       │                  ├── Touch T_DO
                  │ GPIO 5├── TFT CS ────────┤   Display CS
                  │       │                  │
                  │ GPIO33├── Touch CS ──────┤   Touch T_CS
                  │       │                  │
                  │ GPIO15├── DC ────────────┤   Display DC
                  │       │                  │
                  │ GPIO26├── RST ───────────┤   Display RESET
                  │       │                  │
                  │ GPIO32├── BL ────────────┤   Display Backlight
                  │       │                  │
                  │ GPIO39├── Touch IRQ ─────┤   Touch T_IRQ
                  └───────┘                  │
                                             │
                                    ┌────────┴────────┐
                                    │   BTT TFT35     │
                                    │   (ILI9488 +    │
                                    │    XPT2046)     │
                                    └─────────────────┘
```

## Connection 3: Remote Unit (No Display)

Only the tuner TTL interface is needed.

```
┌─────────────────────────────────────────────────────────┐
│              Remote Unit (ESP32 only)                   │
│                                                         │
│   ESP32 GND   ─── Tuner Pin 2 (GND)                    │
│   ESP32 GPIO16 ─── Tuner Pin 3 (TX)                    │
│   ESP32 GPIO17 ─── Tuner Pin 4 (RX)                    │
│                                                         │
│   USB power or external 5V supply                      │
└─────────────────────────────────────────────────────────┘
```

## Parts List

| Item | Qty | Notes |
|------|-----|-------|
| ESP32 WROOM dev board | 1-2 | Any variant with USB (DevKitC, NodeMCU-32S) |
| 4-pin mini-DIN connector | 1 | Kycon KMDLAX-4P or equivalent |
| LM2940CT-5.0 | 1 | 5V LDO regulator, 1A (display unit) |
| LM2940CT-3.3 | 1 | 3.3V LDO regulator, 1A (remote unit) |
| TO-220 heatsink | 1-2 | For linear regulator (~2.5W dissipation) |
| 100nF ceramic capacitor | 1 | Regulator input decoupling |
| 10µF tantalum capacitor | 1 | Regulator output stability |
| Jumper wires | ~15 | Male-female for display, male-male for tuner |
| Level shifter (optional) | 1 | TXS0108E or 74HCT245 if 3.3V logic issues |
| BTT TFT35-SPI display | 1 | For display unit only |

## Assembly Notes

1. **Common ground**: All GND connections must be tied together (ESP32, tuner, display, regulator)
2. **Power from tuner**: The tuner's +12V on pin 1 powers the ESP32 via an LM2940CT linear regulator. A heatsink is required (~2.5W dissipation at 350mA).
3. **Regulator capacitors**: Place the 100nF ceramic close to the regulator input, and the 10µF tantalum close to the output. These are required for stability.
4. **Level shifting**: The AG-1000ProII uses 5V TTL. Most ESP32 GPIO are 5V-tolerant, but if you get serial errors, add a level shifter.
5. **SPI bus**: The display and touchscreen share MOSI/MISO/SCLK. Only CS pins are separate.
6. **Backlight**: GPIO 32 controls backlight via PWM. Set to HIGH for full brightness.
7. **Touch IRQ**: GPIO 39 is input-only, which is fine for the touch interrupt line.
8. **RF considerations**: The linear regulator produces zero switching noise. Keep regulator leads short and use the capacitors as shown to prevent oscillation.
