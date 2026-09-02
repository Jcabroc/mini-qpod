#pragma once

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <math.h>
#include "robot_config.h"
#include "servo_control.h"

class ImuManager {
 public:
  ImuManager() : uart_(IMU_UART_RX_PIN, IMU_UART_UNUSED_TX_PIN) {}

  bool begin() {
    if (!IMU_ENABLED) return false;
    uart_.begin(IMU_UART_BAUD);
    return true;
  }

  bool update() {
    while (uart_.available()) consume((char)uart_.read());
    return healthy();
  }

  bool healthy() const {
    return healthy_ && millis() - lastGoodMs_ < IMU_PACKET_TIMEOUT_MS;
  }
  float roll() const { return roll_; }
  float pitch() const { return pitch_; }

 private:
  bool healthy_ = false;
  float roll_ = 0.0f;
  float pitch_ = 0.0f;
  uint32_t lastUs_ = 0;
  uint32_t lastGoodMs_ = 0;
  SoftwareSerial uart_;
  char line_[48] = {};
  uint8_t length_ = 0;
  uint16_t lastSequence_ = 0;

  static uint8_t crc8(const char *data) {
    uint8_t crc = 0;
    while (*data) crc ^= (uint8_t)*data++;
    return crc;
  }

  void consume(char c) {
    if (c == '\r') return;
    if (c == '\n') {
      line_[length_] = '\0';
      parseLine();
      length_ = 0;
    } else if (length_ < sizeof(line_) - 1) {
      line_[length_++] = c;
    } else {
      length_ = 0;
    }
  }

  void parseLine() {
    // Trama: IMU,<secuencia>,<roll>,<pitch>*<crc XOR hexadecimal>
    char *star = strrchr(line_, '*');
    if (!star || strncmp(line_, "IMU,", 4)) return;
    uint8_t receivedCrc = (uint8_t)strtoul(star + 1, nullptr, 16);
    *star = '\0';
    if (crc8(line_) != receivedCrc) return;
    char *save = nullptr;
    strtok_r(line_, ",", &save);
    char *seqText = strtok_r(nullptr, ",", &save);
    char *rollText = strtok_r(nullptr, ",", &save);
    char *pitchText = strtok_r(nullptr, ",", &save);
    if (!seqText || !rollText || !pitchText || strtok_r(nullptr, ",", &save)) return;
    float newRoll = atof(rollText), newPitch = atof(pitchText);
    if (!isfinite(newRoll) || !isfinite(newPitch) || fabs(newRoll) > 180.0f ||
        fabs(newPitch) > 180.0f) return;
    lastSequence_ = (uint16_t)strtoul(seqText, nullptr, 10);
    roll_ = newRoll;
    pitch_ = newPitch;
    lastGoodMs_ = millis();
    healthy_ = true;
  }
};

class GaitBalance {
 public:
  enum Phase : uint8_t { IDLE, SHIFT, LIFT, SWING, DROP, SETTLE };

  GaitBalance(ServoController &servos, ImuManager &imu) : servos_(servos), imu_(imu) {}

  void stand() {
    for (uint8_t i = 0; i < SERVO_COUNT; ++i) servos_.setTarget(i, servos_.center(i));
    for (uint8_t leg = 0; leg < LEG_COUNT; ++leg) {
      const LegConfig &l = LEGS[leg];
      // Mantiene la semantica comprobada por el firmware anterior.
      servos_.setTarget(l.coxa, coxaForward(leg, DEFAULT_STAND_COXA_GAIN));
      servos_.setTarget(l.femur, lowJoint(l.femur, DEFAULT_STAND_HEIGHT_GAIN));
      servos_.setTarget(l.tibia, lowJoint(l.tibia, DEFAULT_STAND_HEIGHT_GAIN));
    }
    servos_.setTarget(12, servos_.center(12));
    phase_ = IDLE;
  }

  bool startLegTest(uint8_t leg) {
    if (leg >= LEG_COUNT || phase_ != IDLE) return false;
    activeLeg_ = leg;
    singleStep_ = true;
    phase_ = SHIFT;
    phaseStartMs_ = millis();
    return true;
  }

  bool startWalk() {
    if (phase_ != IDLE) return false;
    singleStep_ = false;
    gaitIndex_ = 0;
    activeLeg_ = GAIT_ORDER[gaitIndex_];
    phase_ = SHIFT;
    phaseStartMs_ = millis();
    return true;
  }

  void stop() { stand(); }

  bool update() {
    if (IMU_ENABLED) {
      imu_.update();
      if (!imu_.healthy() || fabs(imu_.roll()) > DEFAULT_MAX_SAFE_TILT_DEG ||
          fabs(imu_.pitch()) > DEFAULT_MAX_SAFE_TILT_DEG) {
        stop();
        return false;
      }
    }
    if (phase_ == IDLE) {
      applyBalance();
      return true;
    }

    uint32_t elapsed = millis() - phaseStartMs_;
    applyStandTargets();
    applyBodyShift(activeLeg_);
    const LegConfig &leg = LEGS[activeLeg_];

    switch (phase_) {
      case SHIFT:
        if (elapsed >= DEFAULT_SHIFT_MS) next(LIFT);
        break;
      case LIFT:
        liftLeg(leg);
        if (elapsed >= DEFAULT_LIFT_MS) next(SWING);
        break;
      case SWING:
        liftLeg(leg);
        servos_.setTarget(leg.coxa, coxaForward(activeLeg_, DEFAULT_STEP_GAIN));
        if (elapsed >= DEFAULT_SWING_MS) next(DROP);
        break;
      case DROP:
        servos_.setTarget(leg.coxa, coxaForward(activeLeg_, DEFAULT_STEP_GAIN));
        if (elapsed >= DEFAULT_DROP_MS) next(SETTLE);
        break;
      case SETTLE:
        if (elapsed >= DEFAULT_SETTLE_MS) finishStep();
        break;
      default: break;
    }
    applyBalance();
    return true;
  }

  Phase phase() const { return phase_; }
  uint8_t activeLeg() const { return activeLeg_; }

 private:
  ServoController &servos_;
  ImuManager &imu_;
  Phase phase_ = IDLE;
  uint8_t activeLeg_ = 0;
  uint8_t gaitIndex_ = 0;
  bool singleStep_ = true;
  uint32_t phaseStartMs_ = 0;

  void next(Phase p) { phase_ = p; phaseStartMs_ = millis(); }

  void finishStep() {
    if (singleStep_) {
      stand();
      return;
    }
    gaitIndex_ = (gaitIndex_ + 1) % LEG_COUNT;
    activeLeg_ = GAIT_ORDER[gaitIndex_];
    next(SHIFT);
  }

  void applyStandTargets() {
    for (uint8_t leg = 0; leg < LEG_COUNT; ++leg) {
      const LegConfig &l = LEGS[leg];
      servos_.setTarget(l.coxa, coxaForward(leg, DEFAULT_STAND_COXA_GAIN));
      servos_.setTarget(l.femur, lowJoint(l.femur, DEFAULT_STAND_HEIGHT_GAIN));
      servos_.setTarget(l.tibia, lowJoint(l.tibia, DEFAULT_STAND_HEIGHT_GAIN));
    }
  }

  void liftLeg(const LegConfig &leg) {
    servos_.setTarget(leg.femur, highJoint(leg.femur, DEFAULT_LIFT_GAIN));
    servos_.setTarget(leg.tibia, highJoint(leg.tibia, DEFAULT_LIFT_GAIN));
  }

  void applyBodyShift(uint8_t liftedLeg) {
    // Empuja las tres coxas de apoyo en sentido opuesto a la pata elevada.
    for (uint8_t leg = 0; leg < LEG_COUNT; ++leg) {
      if (leg == liftedLeg) continue;
      const uint8_t coxa = LEGS[leg].coxa;
      float base = coxaForward(leg, DEFAULT_STAND_COXA_GAIN);
      float sign = (leg == 0 || leg == 2) ? 1.0f : -1.0f;
      float liftedSide = (liftedLeg == 0 || liftedLeg == 2) ? 1.0f : -1.0f;
      servos_.setTarget(coxa, base + sign * liftedSide * DEFAULT_BODY_SHIFT_GAIN *
                                      (servos_.maxSafe(coxa) - servos_.minSafe(coxa)));
    }
  }

  void applyBalance() {
    if (!IMU_ENABLED || !imu_.healthy()) return;
    float rollCorrection = constrain(-imu_.roll() * DEFAULT_BALANCE_KP,
                                     -DEFAULT_MAX_BALANCE_CORRECTION_DEG,
                                      DEFAULT_MAX_BALANCE_CORRECTION_DEG);
    float pitchCorrection = constrain(-imu_.pitch() * DEFAULT_BALANCE_KP,
                                      -DEFAULT_MAX_BALANCE_CORRECTION_DEG,
                                       DEFAULT_MAX_BALANCE_CORRECTION_DEG);
    for (uint8_t leg = 0; leg < LEG_COUNT; ++leg) {
      const LegConfig &l = LEGS[leg];
      float side = (leg == 0 || leg == 2) ? 1.0f : -1.0f;
      float front = (leg == 0 || leg == 1) ? 1.0f : -1.0f;
      servos_.setTarget(l.femur, servos_.target(l.femur) + side * rollCorrection + front * pitchCorrection);
    }
  }

  float coxaForward(uint8_t leg, float gain) const {
    uint8_t ch = LEGS[leg].coxa;
    // R2 es el unico coxa invertido en la tabla comprobada anterior.
    return (leg == 3) ? servos_.towardMax(ch, gain) : servos_.towardMin(ch, gain);
  }

  float lowJoint(uint8_t ch, float gain) const {
    // Tabla semantica heredada: femur/tibia bajos alternan por montaje.
    bool towardMin = (ch == 1 || ch == 7);
    return towardMin ? servos_.towardMin(ch, gain) : servos_.towardMax(ch, gain);
  }

  float highJoint(uint8_t ch, float gain) const {
    bool lowWasMin = (ch == 1 || ch == 7);
    return lowWasMin ? servos_.towardMax(ch, gain) : servos_.towardMin(ch, gain);
  }
};
    // Pico Sensor Station can share the same one-way UART safely.  These lines
    // are forwarded unchanged to the Nano USB serial port for POD Station;
    // only the CRC-protected IMU frame below affects movement or balance.
    if (!strncmp(line_, "SENSOR,", 7)) {
      Serial.println(line_);
      return;
    }
