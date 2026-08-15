#pragma once

#include <Arduino.h>

// ============================================================
// MINI Q-POD MVP - UNICO ARCHIVO DE AJUSTE HABITUAL
// ============================================================

constexpr uint8_t SERVO_COUNT = 13;
constexpr uint8_t LEG_COUNT = 4;

constexpr uint8_t PCA9685_ADDRESS = 0x40;
constexpr float SERVO_PWM_HZ = 50.0f;
constexpr uint16_t SERVO_PULSE_MIN = 110;
constexpr uint16_t SERVO_PULSE_MAX = 510;

constexpr uint8_t PIN_LED = 5;
constexpr uint8_t PIN_BUZZER = 4;

// La MPU6050 pertenece a la Pico W. El Nano recibe la orientacion por UART.
// Cableado oficial: Pico GP4 (TX) -> Nano D2 (RX), GND comun.
constexpr bool IMU_ENABLED = true;
constexpr uint8_t IMU_UART_RX_PIN = 2;
// SoftwareSerial exige un TX, pero el MVP es unidireccional. D8 no se conecta.
// No usar D3 -> GP5 directamente: el Nano entrega 5 V y la Pico trabaja a 3.3 V.
constexpr uint8_t IMU_UART_UNUSED_TX_PIN = 8;
constexpr uint32_t IMU_UART_BAUD = 38400;
constexpr uint16_t IMU_PACKET_TIMEOUT_MS = 250;

struct ServoConfig {
  uint8_t channel;
  int16_t minAngle;
  int16_t centerAngle;
  int16_t maxAngle;
  int8_t direction;  // Reservado para la futura IK: +1 o -1.
  bool enabled;
};

// Valores iniciales tomados de default_config.json. Son provisionales:
// deben verificarse con el robot elevado antes de apoyar peso.
constexpr ServoConfig DEFAULT_SERVOS[SERVO_COUNT] = {
  { 0,  50,  90, 150,  1, true }, // L1_COXA
  { 1,  10,  95, 180,  1, true }, // L1_FEMUR
  { 2,   0, 100, 180, -1, true }, // L1_TIBIA
  { 3,  30,  90, 130,  1, true }, // R1_COXA
  { 4,   0,  90, 170, -1, true }, // R1_FEMUR
  { 5,   0,  90, 180, -1, true }, // R1_TIBIA
  { 6,  40,  90, 140,  1, true }, // L2_COXA
  { 7,  10, 100, 180,  1, true }, // L2_FEMUR
  { 8,   5, 100, 180, -1, true }, // L2_TIBIA
  { 9,  50,  90, 130, -1, true }, // R2_COXA
  {10,  15,  90, 180, -1, true }, // R2_FEMUR
  {11,   0,  90, 180, -1, true }, // R2_TIBIA
  {12,  45,  90, 135,  1, true }  // CUELLO
};

struct LegConfig {
  uint8_t coxa;
  uint8_t femur;
  uint8_t tibia;
};

constexpr LegConfig LEGS[LEG_COUNT] = {
  {0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {9, 10, 11}
};

// Orden lento y cuasiestatico: L1, R2, R1, L2.
constexpr uint8_t GAIT_ORDER[LEG_COUNT] = {0, 3, 1, 2};

// Seguridad y movimiento.
constexpr uint8_t DEFAULT_SAFETY_MARGIN_DEG = 5;
constexpr float DEFAULT_MAX_SPEED_DEG_S = 35.0f;
constexpr uint16_t MOTION_UPDATE_MS = 20;

// Postura y marcha (ganancias 0..1 dentro del rango calibrado).
constexpr float DEFAULT_STAND_COXA_GAIN = 0.30f;
constexpr float DEFAULT_STAND_HEIGHT_GAIN = 0.60f;
constexpr float DEFAULT_STEP_GAIN = 0.20f;
constexpr float DEFAULT_LIFT_GAIN = 0.25f;
constexpr float DEFAULT_BODY_SHIFT_GAIN = 0.08f;

constexpr uint16_t DEFAULT_SHIFT_MS = 500;
constexpr uint16_t DEFAULT_LIFT_MS = 350;
constexpr uint16_t DEFAULT_SWING_MS = 450;
constexpr uint16_t DEFAULT_DROP_MS = 350;
constexpr uint16_t DEFAULT_SETTLE_MS = 400;

// Balance: correccion pequena y deliberadamente limitada.
constexpr float DEFAULT_MAX_SAFE_TILT_DEG = 12.0f;
constexpr float DEFAULT_BALANCE_KP = 0.35f;
constexpr float DEFAULT_MAX_BALANCE_CORRECTION_DEG = 4.0f;

// El usuario debe habilitar la marcha explicitamente despues de las pruebas.
constexpr bool WALK_ENABLED_AT_BOOT = false;
