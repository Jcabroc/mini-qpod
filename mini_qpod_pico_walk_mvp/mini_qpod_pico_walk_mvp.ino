#include <Wire.h>
#include <SPI.h>
#include <RF24.h>
#include <Adafruit_PWMServoDriver.h>
#include "pico_w_pins.h"

// Pico W / RP2040 wrapper. IK, gait, calibration and Control Lite protocol
// remain in the shared runtime; this file only binds them to Pico peripherals.
Adafruit_PWMServoDriver pca(PCA9685_ADDRESS);
RF24 radio(NRF24_CE_PIN, NRF24_CSN_PIN);

#include <MiniQpodWalkCore.h>

void setup() {
  // PCA9685 controller VCC and I2C pull-ups must be at 3.3 V (V+ is separate).
  Wire.setSDA(PCA_SDA_PIN);
  Wire.setSCL(PCA_SCL_PIN);
  Wire.begin();

  // NRF24L01 is a 3.3 V device. SPI0 is explicitly bound before radio.begin().
  SPI.setRX(NRF24_MISO_PIN);
  SPI.setCS(NRF24_CSN_PIN);
  SPI.setSCK(NRF24_SCK_PIN);
  SPI.setTX(NRF24_MOSI_PIN);
  SPI.begin();

  setupWalkRuntime();
}

void loop() {
  loopWalkRuntime();
}
