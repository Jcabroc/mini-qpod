#pragma once

// Raspberry Pi Pico WH / RP2040 locomotion wiring. All controller logic is 3.3 V.
constexpr uint8_t PCA9685_ADDRESS = 0x40;
constexpr uint8_t PCA_SDA_PIN = 0;   // GP0, I2C0 SDA
constexpr uint8_t PCA_SCL_PIN = 1;   // GP1, I2C0 SCL
constexpr uint8_t NRF24_MISO_PIN = 16; // GP16, SPI0 RX
constexpr uint8_t NRF24_CSN_PIN = 17;  // GP17, chip select (GPIO)
constexpr uint8_t NRF24_SCK_PIN = 18;  // GP18, SPI0 SCK
constexpr uint8_t NRF24_MOSI_PIN = 19; // GP19, SPI0 TX
constexpr uint8_t NRF24_CE_PIN = 20;   // GP20, radio enable (GPIO)
// Reserved for the later ESP32-S3 SuperMini link; not initialized in this stage.
constexpr uint8_t S3_UART_TX_PIN = 8;  // Pico GP8 -> S3 RX (3.3 V UART)
constexpr uint8_t S3_UART_RX_PIN = 9;  // Pico GP9 <- S3 TX (3.3 V UART)
