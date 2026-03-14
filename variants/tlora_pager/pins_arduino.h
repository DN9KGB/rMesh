#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID          0x303a
#define USB_PID          0x1001
#define USB_MANUFACTURER "LILYGO"
#define USB_PRODUCT      "T-LoraPager"

// I2C bus (shared: BHI260, PCF85063, BQ25896, BQ27220, DRV2605, XL9555, TCA8418)
static const uint8_t SDA = 3;
static const uint8_t SCL = 2;

// SPI bus (shared: Display, LoRa, NFC, SD)
static const uint8_t MOSI = 34;
static const uint8_t MISO = 33;
static const uint8_t SCK  = 35;

// SD Card
static const uint8_t SS = 21;
#define SD_CS           (21)

// Display ST7796U  (480x222)
#define DISP_WIDTH      (480)
#define DISP_HEIGHT     (222)
#define DISP_MOSI       (MOSI)
#define DISP_MISO       (MISO)
#define DISP_SCK        (SCK)
#define DISP_CS         (38)
#define DISP_DC         (37)
#define DISP_RST        (-1)
#define DISP_BL         (42)

// LoRa SX1262
#define LORA_SCK        (SCK)
#define LORA_MISO       (MISO)
#define LORA_MOSI       (MOSI)
#define LORA_CS         (36)
#define LORA_RST        (47)
#define LORA_BUSY       (48)
#define LORA_IRQ        (14)

// NFC ST25R3916
#define NFC_CS          (39)
#define NFC_INT         (5)

// Keyboard TCA8418
#define KB_INT          (6)
#define KB_BACKLIGHT    (46)

// Rotary Encoder
#define ROTARY_A        (40)
#define ROTARY_B        (41)
#define ROTARY_C        (7)

// RTC PCF85063A
#define RTC_INT         (1)

// IMU BHI260AP
#define SENSOR_INT      (8)

// GNSS MIA-M10Q (GPS_TX = ESP TX -> GNSS RX, GPS_RX = ESP RX <- GNSS TX)
#define GPS_TX          (12)
#define GPS_RX          (4)
#define GPS_PPS         (13)

// Audio Codec ES8311
#define I2S_MCLK        (10)
#define I2S_SCK         (11)
#define I2S_WS          (18)
#define I2S_SDOUT       (45)
#define I2S_SDIN        (17)

// XL9555 I/O Expander GPIO (0-15)
#define EXPANDS_DRV_EN      (0)   // DRV2605 Haptic Enable
#define EXPANDS_AMP_EN      (1)   // Audio Amp Enable
#define EXPANDS_KB_RST      (2)   // Keyboard RESET
#define EXPANDS_LORA_EN     (3)   // LoRa Power Enable
#define EXPANDS_GPS_EN      (4)   // GNSS Power Enable
#define EXPANDS_NFC_EN      (5)   // NFC Power Enable
#define EXPANDS_DISP_RST    (6)   // Display RESET (not connected)
#define EXPANDS_GPS_RST     (7)   // GNSS RESET
#define EXPANDS_KB_EN       (10)  // Keyboard Power Enable
#define EXPANDS_SD_DET      (12)  // SD Insert Detect
#define EXPANDS_SD_PULLEN   (13)  // SD PullUp Enable
#define EXPANDS_SD_EN       (14)  // SD Power Enable

#endif /* Pins_Arduino_h */
