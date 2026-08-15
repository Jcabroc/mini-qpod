#include <Arduino.h>
#include <Wire.h>
#include <math.h>

// Raspberry Pi Pico W (core Arduino-Pico / Earle Philhower).
constexpr uint8_t MPU_ADDRESS = 0x68;
constexpr uint8_t I2C_SDA_PIN = 0;  // GP0
constexpr uint8_t I2C_SCL_PIN = 1;  // GP1
constexpr uint8_t UART_TX_PIN = 4;  // GP4 -> Nano D2
constexpr uint8_t UART_RX_PIN = 5;  // GP5 <- Nano D3 (reservado)
constexpr uint32_t UART_BAUD = 38400;
constexpr uint16_t SAMPLE_PERIOD_MS = 20;

float rollDeg = 0.0f;
float pitchDeg = 0.0f;
uint32_t lastUs = 0;
uint32_t lastSampleMs = 0;
uint16_t sequence = 0;

int16_t read16() {
  return (int16_t)((Wire.read() << 8) | Wire.read());
}

uint8_t crc8(const char *data) {
  uint8_t crc = 0;
  while (*data) crc ^= (uint8_t)*data++;
  return crc;
}

bool beginMpu() {
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(0x6B);
  Wire.write(0x00);
  return Wire.endTransmission() == 0;
}

bool readMpu() {
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)MPU_ADDRESS, 14, true) != 14) return false;
  int16_t ax = read16(), ay = read16(), az = read16();
  read16();
  int16_t gx = read16(), gy = read16();
  read16();

  uint32_t now = micros();
  float dt = constrain((now - lastUs) / 1000000.0f, 0.001f, 0.1f);
  lastUs = now;
  float accRoll = atan2((float)ay, (float)az) * 57.29578f;
  float accPitch = atan2(-(float)ax, sqrt((float)ay * ay + (float)az * az)) * 57.29578f;
  rollDeg = 0.98f * (rollDeg + (gx / 131.0f) * dt) + 0.02f * accRoll;
  pitchDeg = 0.98f * (pitchDeg + (gy / 131.0f) * dt) + 0.02f * accPitch;
  return true;
}

void sendImu() {
  char payload[40];
  snprintf(payload, sizeof(payload), "IMU,%u,%.2f,%.2f", sequence++, rollDeg, pitchDeg);
  Serial2.print(payload);
  Serial2.print('*');
  if (crc8(payload) < 16) Serial2.print('0');
  Serial2.println(crc8(payload), HEX);
}

void setup() {
  Serial.begin(115200);
  Wire.setSDA(I2C_SDA_PIN);
  Wire.setSCL(I2C_SCL_PIN);
  Wire.begin();
  Serial2.setTX(UART_TX_PIN);
  Serial2.setRX(UART_RX_PIN);
  Serial2.begin(UART_BAUD);
  if (!beginMpu()) {
    Serial.println("[FATAL] MPU6050 no responde en 0x68");
    while (true) delay(1000);
  }
  lastUs = micros();
  Serial.println("[OK] MPU6050 -> UART Nano");
}

void loop() {
  uint32_t now = millis();
  if (now - lastSampleMs < SAMPLE_PERIOD_MS) return;
  lastSampleMs = now;
  if (readMpu()) sendImu();
}
