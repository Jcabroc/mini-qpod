#pragma once

#include <Arduino.h>
#include <Adafruit_PWMServoDriver.h>
#include <EEPROM.h>
#include "robot_config.h"

class ServoController {
 public:
  struct StoredConfig {
    uint16_t magic;
    uint8_t version;
    uint8_t safetyMarginDeg;
    ServoConfig servos[SERVO_COUNT];
    uint16_t checksum;
  };

  static constexpr uint16_t STORAGE_MAGIC = 0x514D; // "QM"
  static constexpr uint8_t STORAGE_VERSION = 1;
  static constexpr int8_t NO_ACTIVE_CHANNEL = -1;

  explicit ServoController(Adafruit_PWMServoDriver &driver) : pca_(driver) {}

  void begin() {
    loadDefaults();
    load();
    for (uint8_t i = 0; i < SERVO_COUNT; ++i) {
      current_[i] = config_.servos[i].centerAngle;
      target_[i] = current_[i];
    }
    lastUpdateMs_ = millis();
  }

  void loadDefaults() {
    config_.magic = STORAGE_MAGIC;
    config_.version = STORAGE_VERSION;
    config_.safetyMarginDeg = DEFAULT_SAFETY_MARGIN_DEG;
    for (uint8_t i = 0; i < SERVO_COUNT; ++i) config_.servos[i] = DEFAULT_SERVOS[i];
    config_.checksum = calculateChecksum(config_);
  }

  bool validate() const {
    if (config_.magic != STORAGE_MAGIC || config_.version != STORAGE_VERSION) return false;
    if (config_.safetyMarginDeg > 20) return false;
    for (uint8_t i = 0; i < SERVO_COUNT; ++i) {
      const ServoConfig &s = config_.servos[i];
      if (s.channel > 15 || s.minAngle < 0 || s.maxAngle > 180) return false;
      if (!(s.minAngle < s.centerAngle && s.centerAngle < s.maxAngle)) return false;
      if (s.direction != 1 && s.direction != -1) return false;
      if ((s.maxAngle - s.minAngle) <= (2 * config_.safetyMarginDeg)) return false;
    }
    return true;
  }

  bool save() {
    if (!validate()) return false;
    config_.checksum = calculateChecksum(config_);
    EEPROM.put(0, config_);
    return true;
  }

  bool load() {
    StoredConfig candidate;
    EEPROM.get(0, candidate);
    if (candidate.magic != STORAGE_MAGIC || candidate.version != STORAGE_VERSION) return false;
    if (candidate.checksum != calculateChecksum(candidate)) return false;
    StoredConfig old = config_;
    config_ = candidate;
    if (!validate()) {
      config_ = old;
      return false;
    }
    return true;
  }

  bool setCalibration(uint8_t index, int minAngle, int centerAngle, int maxAngle) {
    if (index >= SERVO_COUNT) return false;
    ServoConfig old = config_.servos[index];
    config_.servos[index].minAngle = minAngle;
    config_.servos[index].centerAngle = centerAngle;
    config_.servos[index].maxAngle = maxAngle;
    if (!validate()) {
      config_.servos[index] = old;
      return false;
    }
    return true;
  }

  bool setTarget(uint8_t index, float angle) {
    if (index >= SERVO_COUNT || !config_.servos[index].enabled) return false;
    target_[index] = clampSafe(index, angle);
    return true;
  }

  void setAllCenters() {
    for (uint8_t i = 0; i < SERVO_COUNT; ++i) setTarget(i, center(i));
  }

  void enable() {
    enabled_ = true;
    activeChannel_ = NO_ACTIVE_CHANNEL;
    lastUpdateMs_ = millis();
  }

  void disable() {
    enabled_ = false;
    activeChannel_ = NO_ACTIVE_CHANNEL;
    for (uint8_t i = 0; i < SERVO_COUNT; ++i) pca_.setPWM(config_.servos[i].channel, 0, 0);
  }

  // Activa unicamente un canal para calibracion. El primer PWM se escribe de
  // forma explicita al angulo solicitado: el operador debe advertir el posible
  // salto desde la posicion sin energia antes de llamar a este metodo.
  bool enableOnly(uint8_t index, float firstAngle, float &effectiveAngle) {
    if (index >= SERVO_COUNT || !config_.servos[index].enabled) return false;
    disable();
    effectiveAngle = clampSafe(index, firstAngle);
    current_[index] = effectiveAngle;
    target_[index] = effectiveAngle;
    enabled_ = true;
    activeChannel_ = (int8_t)index;
    lastUpdateMs_ = millis();
    writePhysical(index, effectiveAngle);
    return true;
  }

  bool setActiveTarget(uint8_t index, float angle, float &effectiveAngle) {
    if (!enabled_ || activeChannel_ != (int8_t)index) return false;
    effectiveAngle = clampSafe(index, angle);
    target_[index] = effectiveAngle;
    return true;
  }

  void update() {
    if (!enabled_) return;
    uint32_t now = millis();
    uint32_t elapsed = now - lastUpdateMs_;
    if (elapsed < MOTION_UPDATE_MS) return;
    lastUpdateMs_ = now;
    float maxStep = DEFAULT_MAX_SPEED_DEG_S * (elapsed / 1000.0f);
    uint8_t first = 0;
    uint8_t last = SERVO_COUNT;
    if (activeChannel_ != NO_ACTIVE_CHANNEL) {
      first = (uint8_t)activeChannel_;
      last = first + 1;
    }
    for (uint8_t i = first; i < last; ++i) {
      if (!config_.servos[i].enabled) continue;
      float delta = target_[i] - current_[i];
      if (delta > maxStep) delta = maxStep;
      if (delta < -maxStep) delta = -maxStep;
      current_[i] += delta;
      writePhysical(i, current_[i]);
    }
  }

  float minSafe(uint8_t i) const { return config_.servos[i].minAngle + config_.safetyMarginDeg; }
  float maxSafe(uint8_t i) const { return config_.servos[i].maxAngle - config_.safetyMarginDeg; }
  uint8_t safetyMargin() const { return config_.safetyMarginDeg; }
  float center(uint8_t i) const { return config_.servos[i].centerAngle; }
  float current(uint8_t i) const { return current_[i]; }
  float target(uint8_t i) const { return target_[i]; }
  const ServoConfig &config(uint8_t i) const { return config_.servos[i]; }
  bool enabled() const { return enabled_; }
  int8_t activeChannel() const { return activeChannel_; }
  bool isOnlyChannelEnabled(uint8_t index) const {
    return enabled_ && activeChannel_ == (int8_t)index;
  }

  float towardMin(uint8_t i, float gain) const {
    return center(i) + (minSafe(i) - center(i)) * constrain(gain, 0.0f, 1.0f);
  }

  float towardMax(uint8_t i, float gain) const {
    return center(i) + (maxSafe(i) - center(i)) * constrain(gain, 0.0f, 1.0f);
  }

 private:
  Adafruit_PWMServoDriver &pca_;
  StoredConfig config_;
  float current_[SERVO_COUNT] = {};
  float target_[SERVO_COUNT] = {};
  uint32_t lastUpdateMs_ = 0;
  bool enabled_ = false;
  int8_t activeChannel_ = NO_ACTIVE_CHANNEL;

  float clampSafe(uint8_t index, float angle) const {
    return constrain(angle, minSafe(index), maxSafe(index));
  }

  void writePhysical(uint8_t index, float angle) {
    int a = constrain((int)(angle + 0.5f), 0, 180);
    uint16_t pulse = map(a, 0, 180, SERVO_PULSE_MIN, SERVO_PULSE_MAX);
    pca_.setPWM(config_.servos[index].channel, 0, pulse);
  }

  static uint16_t calculateChecksum(const StoredConfig &data) {
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&data);
    const size_t length = sizeof(StoredConfig) - sizeof(data.checksum);
    uint16_t sum = 0xA5A5;
    for (size_t i = 0; i < length; ++i) sum = (uint16_t)((sum << 5) | (sum >> 11)) ^ bytes[i];
    return sum;
  }
};
