// User Setup for BTT TFT35-SPI (ILI9488) with XPT2046 Touch
// Save as TFT_eSPI/User_Setups/Setup208_LDGControl.h

#define ILI9488_DRIVER

// SPI pins for ESP32
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   5
#define TFT_DC   15
#define TFT_RST  -1

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT

#define SPI_FREQUENCY  20000000
#define SPI_READ_FREQUENCY  16000000
#define SPI_TOUCH_FREQUENCY  2500000

#define SUPPORT_TRANSACTIONS
