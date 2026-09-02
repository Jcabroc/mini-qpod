/*
  Mini Q-POD — Pico W Sensor Station
  Publishes USB telemetry for POD Station and preserves MPU6050 -> Nano UART.

  Install on Raspberry Pi Pico W using Arduino-Pico (rp2040:rp2040).
  No external libraries required.  Confirm DHT model and active logic levels
  before energizing the robot.
*/
#include <Arduino.h>
#include <Wire.h>
#include <math.h>

constexpr uint8_t MPU_ADDRESS = 0x68;
constexpr uint8_t I2C_SDA_PIN = 0, I2C_SCL_PIN = 1;
constexpr uint8_t SONAR_TRIG_PIN = 2, SONAR_ECHO_PIN = 3;  // ECHO needs 5V->3.3V divider.
constexpr uint8_t UART_TX_PIN = 4, UART_RX_PIN = 5;
constexpr uint8_t DHT_PIN = 6, IMPACT_PIN = 7, TOUCH_PIN = 9, PIR_PIN = 10;
constexpr uint8_t LDR_LEFT_PIN = 26, LDR_RIGHT_PIN = 27;
constexpr uint8_t RGB_R_PIN = 13, RGB_G_PIN = 14, RGB_B_PIN = 15;  // Reserved; not driven.

// Start with DHT11, then alternate with DHT22 automatically if no valid frame
// arrives.  This lets us diagnose an unknown legacy DHT without reflashing.
bool dhtIsDHT22 = false;
const char *dhtModel = "AUTO";
// Many impact boards are LOW on impact. Change only after a safe bench test.
// Verified from live telemetry: the installed module stays HIGH at rest.
constexpr bool IMPACT_ACTIVE_LOW = true;
constexpr bool TOUCH_ACTIVE_HIGH = true;
constexpr bool PIR_ACTIVE_HIGH = true;
constexpr uint16_t TOUCH_DEBOUNCE_MS = 50;
constexpr uint16_t PIR_DEBOUNCE_MS = 150;

constexpr uint32_t UART_BAUD = 38400, IMU_PERIOD_MS = 20, SENSOR_PERIOD_MS = 200, DHT_PERIOD_MS = 2200;
float rollDeg = 0, pitchDeg = 0, gyroZDegPerSec = 0;
uint32_t lastImuUs = 0, lastImuMs = 0, lastSensorMs = 0, lastDhtMs = 0;
uint16_t sequence = 0;
float temperatureC = NAN, humidity = NAN;
bool touchStable = false, touchCandidate = false, pirStable = false, pirCandidate = false;
uint32_t touchCandidateSinceMs = 0, pirCandidateSinceMs = 0;

int16_t read16() { return (int16_t)((Wire.read() << 8) | Wire.read()); }
uint8_t crc8(const char *data) { uint8_t crc = 0; while (*data) crc ^= (uint8_t)*data++; return crc; }

bool beginMpu() {
  Wire.beginTransmission(MPU_ADDRESS); Wire.write(0x6B); Wire.write(0x00);
  return Wire.endTransmission() == 0;
}
bool readMpu() {
  Wire.beginTransmission(MPU_ADDRESS); Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom((int)MPU_ADDRESS, 14, true) != 14) return false;
  int16_t ax = read16(), ay = read16(), az = read16(); read16(); int16_t gx = read16(), gy = read16(), gz = read16();
  uint32_t now = micros(); float dt = constrain((now - lastImuUs) / 1000000.0f, 0.001f, 0.1f); lastImuUs = now;
  float accRoll = atan2((float)ay, (float)az) * 57.29578f;
  float accPitch = atan2(-(float)ax, sqrt((float)ay * ay + (float)az * az)) * 57.29578f;
  rollDeg = .98f * (rollDeg + gx / 131.0f * dt) + .02f * accRoll;
  pitchDeg = .98f * (pitchDeg + gy / 131.0f * dt) + .02f * accPitch;
  gyroZDegPerSec = gz / 131.0f;  // This is rotation rate, not absolute yaw.
  return true;
}
void sendImuToNano() {
  char payload[42]; snprintf(payload, sizeof(payload), "IMU,%u,%.2f,%.2f", sequence++, rollDeg, pitchDeg);
  Serial2.print(payload); Serial2.print('*'); if (crc8(payload) < 16) Serial2.print('0'); Serial2.println(crc8(payload), HEX);
}

bool waitLevel(bool level, uint32_t timeoutUs) {
  uint32_t start = micros(); while (digitalRead(DHT_PIN) != level) if (micros() - start > timeoutUs) return false; return true;
}
bool readDht(float &temp, float &hum, bool isDht22) {
  pinMode(DHT_PIN, OUTPUT); digitalWrite(DHT_PIN, LOW); delay(isDht22 ? 2 : 20);
  digitalWrite(DHT_PIN, HIGH); delayMicroseconds(30); pinMode(DHT_PIN, INPUT_PULLUP);
  if (!waitLevel(LOW, 120) || !waitLevel(HIGH, 120) || !waitLevel(LOW, 120)) return false;
  uint8_t data[5] = {};
  for (uint8_t i = 0; i < 40; ++i) {
    if (!waitLevel(HIGH, 90)) return false; uint32_t start = micros();
    if (!waitLevel(LOW, 100)) return false; uint32_t width = micros() - start;
    data[i / 8] = (data[i / 8] << 1) | (width > 50);
  }
  if (((uint8_t)(data[0] + data[1] + data[2] + data[3])) != data[4]) return false;
  if (isDht22) { hum = ((data[0] << 8) | data[1]) * .1f; temp = (((data[2] & 0x7F) << 8) | data[3]) * .1f; if (data[2] & 0x80) temp = -temp; }
  else { hum = data[0]; temp = data[2]; }
  return true;
}
bool filteredDigital(uint8_t pin, bool activeHigh, bool &stable, bool &candidate, uint32_t &candidateSince, uint16_t debounceMs) {
  bool raw = digitalRead(pin) == (activeHigh ? HIGH : LOW);
  uint32_t now = millis();
  if (raw != candidate) { candidate = raw; candidateSince = now; }
  if (candidate != stable && now - candidateSince >= debounceMs) stable = candidate;
  return stable;
}
float readSonarCm() {
  digitalWrite(SONAR_TRIG_PIN, LOW); delayMicroseconds(3); digitalWrite(SONAR_TRIG_PIN, HIGH); delayMicroseconds(10); digitalWrite(SONAR_TRIG_PIN, LOW);
  unsigned long pulse = pulseIn(SONAR_ECHO_PIN, HIGH, 30000); return pulse ? pulse * .0343f / 2.0f : -1;
}
void publishSensor(const char *line) {
  // USB feeds POD Station directly; UART feeds the Nano relay on D2.
  Serial.println(line);
  Serial2.println(line);
}
void sendSensors() {
  float sonar = readSonarCm();
  char line[64];
  snprintf(line, sizeof(line), "SENSOR,imu,%.2f,%.2f,%.2f", rollDeg, pitchDeg, gyroZDegPerSec); publishSensor(line);
  if (sonar >= 0) { snprintf(line, sizeof(line), "SENSOR,sonar,%.1f", sonar); publishSensor(line); }
  snprintf(line, sizeof(line), "SENSOR,impact,%d", (digitalRead(IMPACT_PIN) == (IMPACT_ACTIVE_LOW ? LOW : HIGH)) ? 1 : 0); publishSensor(line);
  snprintf(line, sizeof(line), "SENSOR,touch,%d", filteredDigital(TOUCH_PIN, TOUCH_ACTIVE_HIGH, touchStable, touchCandidate, touchCandidateSinceMs, TOUCH_DEBOUNCE_MS) ? 1 : 0); publishSensor(line);
  snprintf(line, sizeof(line), "SENSOR,pir,%d", filteredDigital(PIR_PIN, PIR_ACTIVE_HIGH, pirStable, pirCandidate, pirCandidateSinceMs, PIR_DEBOUNCE_MS) ? 1 : 0); publishSensor(line);
  snprintf(line, sizeof(line), "SENSOR,ldr,%d,%d", analogRead(LDR_LEFT_PIN), analogRead(LDR_RIGHT_PIN)); publishSensor(line);
  if (isfinite(temperatureC) && isfinite(humidity)) { snprintf(line, sizeof(line), "SENSOR,dht,%.1f,%.1f", temperatureC, humidity); publishSensor(line); }
}
void handleCommands() {
  static String input;
  while (Serial.available()) {
    char ch = (char)Serial.read(); if (ch == '\r') continue;
    if (ch != '\n') { input += ch; continue; }
    input.trim(); input.toUpperCase();
    if (input == "SENSORS" || input == "STATUS") sendSensors();
    else if (input == "IMU") Serial.printf("IMU: OK roll=%.2f pitch=%.2f z_rate=%.2f\n", rollDeg, pitchDeg, gyroZDegPerSec);
    else if (input == "PING") Serial.println("PONG");
    else if (input == "HELP") Serial.println("HELP: STATUS IMU SENSORS PING");
    else Serial.println("[BLOCKED] Command unavailable on sensor station");
    input = "";
  }
}
void setup() {
  Serial.begin(115200); Wire.setSDA(I2C_SDA_PIN); Wire.setSCL(I2C_SCL_PIN); Wire.begin();
  Serial2.setTX(UART_TX_PIN); Serial2.setRX(UART_RX_PIN); Serial2.begin(UART_BAUD);
  pinMode(SONAR_TRIG_PIN, OUTPUT); pinMode(SONAR_ECHO_PIN, INPUT); pinMode(IMPACT_PIN, INPUT); pinMode(TOUCH_PIN, INPUT); pinMode(PIR_PIN, INPUT);
  analogReadResolution(10);  // The station graph uses a 0–1023 scale.
  // RGB pins deliberately remain untouched until common-anode/common-cathode is confirmed.
  if (!beginMpu()) { Serial.println("[FATAL] MPU6050 no responde en 0x68"); while (true) delay(1000); }
  lastImuUs = micros(); Serial.println("[OK] Mini Q-POD Pico Sensor Station ready");
}
void loop() {
  uint32_t now = millis(); handleCommands();
  if (now - lastImuMs >= IMU_PERIOD_MS) { lastImuMs = now; if (readMpu()) sendImuToNano(); }
  if (now - lastDhtMs >= DHT_PERIOD_MS) {
    lastDhtMs = now; float t, h;
    if (readDht(t, h, dhtIsDHT22)) {
      temperatureC = t; humidity = h; dhtModel = dhtIsDHT22 ? "DHT22" : "DHT11";
      Serial.print("[OK] DHT detectado: "); Serial.println(dhtModel);
    } else {
      dhtIsDHT22 = !dhtIsDHT22;
      Serial.print("[WARN] DHT sin lectura; proxima prueba "); Serial.println(dhtIsDHT22 ? "DHT22" : "DHT11");
    }
  }
  if (now - lastSensorMs >= SENSOR_PERIOD_MS) { lastSensorMs = now; sendSensors(); }
}
