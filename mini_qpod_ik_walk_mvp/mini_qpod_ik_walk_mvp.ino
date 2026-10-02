#include <Wire.h>
#include <SPI.h>
#include <RF24.h>
#include <Adafruit_PWMServoDriver.h>

// Reference implementation: Nano ATmega328P, PCA9685 0x40/50 Hz,
// NRF24 CE=D9 and CSN=D10. The runtime is shared with the Pico W sketch.
Adafruit_PWMServoDriver pca(0x40);
RF24 radio(9, 10);

#include <MiniQpodWalkCore.h>

void setup() {
  Wire.begin();
  setupWalkRuntime();
}

void loop() {
  loopWalkRuntime();
}
