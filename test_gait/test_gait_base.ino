#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <math.h>

// =====================================================
// Pines Nano
// =====================================================
const uint8_t PIN_LED = 5;
const uint8_t PIN_BUZZER = 4;

const uint8_t PIN_CE = 9;
const uint8_t PIN_CSN = 10;

// =====================================================
// NRF24 / PS2J
// =====================================================
RF24 radio(PIN_CE, PIN_CSN);
const uint8_t ADDR[6] = "JR001";

enum PS2J_Button : uint16_t {
  PS2J_SELECT = (1u << 0),
  PS2J_START = (1u << 1),
  PS2J_SQUARE = (1u << 2),
  PS2J_CROSS = (1u << 3),
  PS2J_CIRCLE = (1u << 4),
  PS2J_TRIANGLE = (1u << 5),
  PS2J_R3 = (1u << 6),
};

struct PS2J_Packet {
  int8_t lx;
  int8_t ly;
  int8_t rx;
  int8_t ry;
  uint16_t btn;
  uint8_t mode3;  // 0 CALIB / 1 MANUAL / 2 GAIT
  uint8_t seq;
  uint8_t flags;
};

PS2J_Packet rxData;
uint16_t prevBtn = 0;

uint32_t lastPacketMs = 0;
bool linkAlive = false;

// =====================================================
// PCA9685
// =====================================================
Adafruit_PWMServoDriver pca = Adafruit_PWMServoDriver(0x40);
const float PWM_FREQ = 50.0;
const int SERVO_MIN = 110;
const int SERVO_MAX = 510;

// =====================================================
// Canales / nombres
// =====================================================
const int FIRST_CH = 0;
const int LAST_CH = 12;
int currentCh = 0;

const char* servoName[13] = {
  "L1_COXA",   // CH0
  "L1_FEMUR",  // CH1
  "L1_TIBIA",  // CH2
  "R1_COXA",   // CH3
  "R1_FEMUR",  // CH4
  "R1_TIBIA",  // CH5
  "L2_COXA",   // CH6
  "L2_FEMUR",  // CH7
  "L2_TIBIA",  // CH8
  "R2_COXA",   // CH9
  "R2_FEMUR",  // CH10
  "R2_TIBIA",  // CH11
  "CUELLO"     // CH12
};

// =====================================================
// Tabla calibrada
// =====================================================
int angleA[13] = {
  50, 10, 180,
  30, 0, 0,
  40, 10, 5,
  130, 180, 180,
  45
};

int angleCenter[13] = {
  90, 95, 100,
  90, 90, 90,
  90, 100, 100,
  90, 90, 90,
  90
};

int angleB[13] = {
  150, 180, 0,
  130, 170, 180,
  140, 180, 180,
  50, 15, 0,
  135
};

// =====================================================
// Estado de servos
// =====================================================
bool servosEnabled = false;
float currentAngle[13];
float targetAngle[13];
uint32_t lastMotionMs = 0;
const uint16_t MOTION_PERIOD_MS = 20;

// =====================================================
// Velocidades / seguridad
// =====================================================
const float SPEED_CALIB = 4.0f;
const float SPEED_MANUAL = 2.5f;
const float SPEED_GAIT = 3.0f;

// Estos NO los tocamos todavía
const float SAFE_HEIGHT_GAIN = 0.55f;
const float SAFE_COXA_GAIN = 0.45f;

// =====================================================
// Cuello
// =====================================================
const int NECK_LEFT = 45;
const int NECK_CENTER = 90;
const int NECK_RIGHT = 135;
int neckTarget = NECK_CENTER;

// =====================================================
// Tablas semánticas
// =====================================================
int coxaForward[13];
int coxaBack[13];
int lowPose[13];
int highPose[13];
int standPose[13];

// =====================================================
// Definición de patas
// =====================================================
struct LegDef {
  int coxa;
  int femur;
  int tibia;
  const char* name;
};

LegDef legs[4] = {
  { 0, 1, 2, "L1" },
  { 3, 4, 5, "R1" },
  { 6, 7, 8, "L2" },
  { 9, 10, 11, "R2" }
};

// orden crawl seguro
int gaitOrder[4] = { 0, 3, 1, 2 };  // L1, R2, R1, L2

// =====================================================
// Estado gait
// =====================================================
bool gaitActive = false;
uint8_t gaitPhase = 0;
uint8_t gaitLegIndex = 0;
uint32_t gaitPhaseStartMs = 0;

const uint16_t GAIT_LIFT_MS = 180;
const uint16_t GAIT_SWING_MS = 220;
const uint16_t GAIT_DROP_MS = 180;
const uint16_t GAIT_PUSH_MS = 220;

const float GAIT_COXA_GAIN = 0.28f;
const float GAIT_LIFT_GAIN = 0.30f;

float gaitCmdFB = 0.0f;
float gaitCmdTurn = 0.0f;

// =====================================================
// Helpers
// =====================================================
void beep(uint16_t freq, uint16_t ms) {
  tone(PIN_BUZZER, freq, ms);
}

bool btnEdge(uint16_t mask) {
  bool now = (rxData.btn & mask) != 0;
  bool old = (prevBtn & mask) != 0;
  return now && !old;
}

int angleToPulse(int angle) {
  angle = constrain(angle, 0, 180);
  return map(angle, 0, 180, SERVO_MIN, SERVO_MAX);
}

void servoWriteDeg(int ch, float angle) {
  if (ch < FIRST_CH || ch > LAST_CH) return;
  int a = constrain((int)(angle + 0.5f), 0, 180);
  int pulse = angleToPulse(a);
  pca.setPWM(ch, 0, pulse);
}

void servoOff(int ch) {
  if (ch < FIRST_CH || ch > LAST_CH) return;
  pca.setPWM(ch, 0, 0);
}

void allOff() {
  for (int ch = FIRST_CH; ch <= LAST_CH; ch++) {
    pca.setPWM(ch, 0, 0);
  }
  Serial.println("TODOS LOS CANALES OFF");
}

float moveTowards(float current, float target, float step) {
  if (current < target) {
    current += step;
    if (current > target) current = target;
  } else if (current > target) {
    current -= step;
    if (current < target) current = target;
  }
  return current;
}

float lerpF(float a, float b, float t) {
  return a + (b - a) * t;
}

float safeBlendFromCenter(int center, int extreme, float gain) {
  return center + (extreme - center) * gain;
}

void setAllTargetsToCenter() {
  for (int ch = FIRST_CH; ch <= LAST_CH; ch++) {
    targetAngle[ch] = angleCenter[ch];
  }
  targetAngle[12] = neckTarget;
}

// =====================================================
// Tablas semánticas de movimiento
// AQUI ESTAN LAS INVERSIONES IMPORTANTES
// =====================================================
void buildPoseTables() {
  for (int ch = FIRST_CH; ch <= LAST_CH; ch++) {
    coxaForward[ch] = angleCenter[ch];
    coxaBack[ch] = angleCenter[ch];
    lowPose[ch] = angleCenter[ch];
    highPose[ch] = angleCenter[ch];
  }

  // -------------------------------------------------
  // COXAS
  // -------------------------------------------------
  // L1 OK
  coxaForward[0] = angleA[0];
  coxaBack[0] = angleB[0];

  // R1 INVERTIDA -> corregida
  coxaForward[3] = angleA[3];
  coxaBack[3] = angleB[3];

  // L2 INVERTIDA -> corregida
  coxaForward[6] = angleA[6];
  coxaBack[6] = angleB[6];

  // R2 OK
  coxaForward[9] = angleB[9];
  coxaBack[9] = angleA[9];

  // -------------------------------------------------
  // ALTURA: FEMUR / TIBIA
  // -------------------------------------------------

  // L1 OK
  lowPose[1] = angleA[1];
  highPose[1] = angleB[1];

  lowPose[2] = angleB[2];
  highPose[2] = angleA[2];

  // R1 FEMUR OK / TIBIA INVERTIDA -> corregida
  lowPose[4] = angleB[4];
  highPose[4] = angleA[4];

  lowPose[5] = angleB[5];
  highPose[5] = angleA[5];

  // L2 FEMUR OK / TIBIA INVERTIDA -> corregida
  lowPose[7] = angleB[7];
  highPose[7] = angleA[7];

  lowPose[8] = angleB[8];
  highPose[8] = angleA[8];

  // R2 FEMUR INVERTIDO -> corregido / TIBIA OK
  lowPose[10] = angleB[10];
  highPose[10] = angleA[10];

  lowPose[11] = angleB[11];
  highPose[11] = angleA[11];
}

// =====================================================
// POSTURA BASE TIPO ARAÑA
// TODAVIA SIN AUMENTAR AMPLITUDES
// =====================================================
void buildStandPose() {
  for (int ch = FIRST_CH; ch <= LAST_CH; ch++) {
    standPose[ch] = angleCenter[ch];
  }

  // Coxas abiertas un poco
  standPose[0] = safeBlendFromCenter(angleCenter[0], coxaForward[0], 0.30f);  // L1
  standPose[6] = safeBlendFromCenter(angleCenter[6], coxaForward[6], 0.30f);  // L2
  standPose[3] = safeBlendFromCenter(angleCenter[3], coxaForward[3], 0.30f);  // R1
  standPose[9] = safeBlendFromCenter(angleCenter[9], coxaForward[9], 0.30f);  // R2

  // Femur + tibia en apoyo, no plano
  standPose[1] = safeBlendFromCenter(angleCenter[1], lowPose[1], 0.60f);
  standPose[2] = safeBlendFromCenter(angleCenter[2], lowPose[2], 0.60f);

  standPose[4] = safeBlendFromCenter(angleCenter[4], lowPose[4], 0.60f);
  standPose[5] = safeBlendFromCenter(angleCenter[5], lowPose[5], 0.60f);

  standPose[7] = safeBlendFromCenter(angleCenter[7], lowPose[7], 0.60f);
  standPose[8] = safeBlendFromCenter(angleCenter[8], lowPose[8], 0.60f);

  standPose[10] = safeBlendFromCenter(angleCenter[10], lowPose[10], 0.60f);
  standPose[11] = safeBlendFromCenter(angleCenter[11], lowPose[11], 0.60f);

  standPose[12] = NECK_CENTER;
}

// =====================================================
// Estados generales
// =====================================================
void enableServosAndGoNeutral() {
  servosEnabled = true;
  gaitActive = false;
  neckTarget = NECK_CENTER;

  for (int ch = FIRST_CH; ch <= LAST_CH; ch++) {
    targetAngle[ch] = standPose[ch];
  }
  targetAngle[12] = neckTarget;

  Serial.println("SERVOS ENABLED -> postura base tipo arana");
  beep(1800, 80);
}

void disableAllServos() {
  servosEnabled = false;
  gaitActive = false;
  allOff();
  Serial.println("SERVOS DISABLED");
  beep(700, 120);
}

void updateMotion(float maxStep) {
  if (!servosEnabled) return;

  for (int ch = FIRST_CH; ch <= LAST_CH; ch++) {
    currentAngle[ch] = moveTowards(currentAngle[ch], targetAngle[ch], maxStep);
    servoWriteDeg(ch, currentAngle[ch]);
  }
}

void printCurrentChannel() {
  Serial.print("Canal actual: CH");
  Serial.print(currentCh);
  Serial.print(" -> ");
  Serial.println(servoName[currentCh]);
}

void nextChannel() {
  currentCh++;
  if (currentCh > LAST_CH) currentCh = FIRST_CH;
  printCurrentChannel();
  beep(1800, 60);
}

void prevChannel() {
  currentCh--;
  if (currentCh < FIRST_CH) currentCh = LAST_CH;
  printCurrentChannel();
  beep(1200, 60);
}

void printHelp() {
  Serial.println();
  Serial.println("=== Q-POD MINI TEST ===");
  Serial.println("GLOBAL:");
  Serial.println(" START  -> activar servos + postura base");
  Serial.println(" SELECT -> apagar todos los servos");
  Serial.println();
  Serial.println("MODO 0 = CALIB");
  Serial.println(" TRIANGLE -> siguiente canal");
  Serial.println(" SQUARE   -> canal anterior");
  Serial.println(" CROSS    -> punto A");
  Serial.println(" CIRCLE   -> centro");
  Serial.println(" R3       -> punto B");
  Serial.println();
  Serial.println("MODO 1 = MANUAL POSTURA");
  Serial.println(" RY -> altura");
  Serial.println(" RX -> sesgo adelante/atras");
  Serial.println(" TRIANGLE/CROSS/SQUARE -> cuello");
  Serial.println(" CIRCLE -> postura base tipo arana");
  Serial.println();
  Serial.println("MODO 2 = GAIT LENTO");
  Serial.println(" RY -> avance / retroceso");
  Serial.println(" RX -> giro");
  Serial.println(" TRIANGLE/CROSS/SQUARE -> cuello");
  Serial.println(" CIRCLE -> postura base tipo arana");
  Serial.println();
}

// =====================================================
// MODO 0 - CALIB
// =====================================================
void processCalibMode() {
  if (!servosEnabled) return;

  if (btnEdge(PS2J_TRIANGLE)) nextChannel();
  if (btnEdge(PS2J_SQUARE)) prevChannel();

  if (btnEdge(PS2J_CROSS)) {
    targetAngle[currentCh] = angleA[currentCh];
    Serial.print("CH");
    Serial.print(currentCh);
    Serial.print(" -> A = ");
    Serial.println(angleA[currentCh]);
    beep(1000, 50);
  }

  if (btnEdge(PS2J_CIRCLE)) {
    targetAngle[currentCh] = angleCenter[currentCh];
    Serial.print("CH");
    Serial.print(currentCh);
    Serial.print(" -> CENTER = ");
    Serial.println(angleCenter[currentCh]);
    beep(1600, 50);
  }

  if (btnEdge(PS2J_R3)) {
    targetAngle[currentCh] = angleB[currentCh];
    Serial.print("CH");
    Serial.print(currentCh);
    Serial.print(" -> B = ");
    Serial.println(angleB[currentCh]);
    beep(2200, 50);
  }
}

// =====================================================
// MODO 1 - MANUAL POSTURA
// =====================================================
void processManualMode() {
  if (!servosEnabled) return;

  gaitActive = false;

  if (btnEdge(PS2J_TRIANGLE)) {
    neckTarget = NECK_LEFT;
    beep(2100, 50);
  }
  if (btnEdge(PS2J_CROSS)) {
    neckTarget = NECK_CENTER;
    beep(1500, 50);
  }
  if (btnEdge(PS2J_SQUARE)) {
    neckTarget = NECK_RIGHT;
    beep(900, 50);
  }

  if (btnEdge(PS2J_CIRCLE)) {
    neckTarget = NECK_CENTER;
    for (int ch = FIRST_CH; ch <= LAST_CH; ch++) {
      targetAngle[ch] = standPose[ch];
    }
    targetAngle[12] = neckTarget;
    Serial.println("Postura base tipo arana");
    beep(1800, 80);
  }

  float ry = (float)(-rxData.ry) / 127.0f;
  ry = constrain(ry, -1.0f, 1.0f);
  float tHeight = (ry + 1.0f) * 0.5f;

  float rx = (float)(rxData.rx) / 127.0f;
  rx = constrain(rx, -1.0f, 1.0f);
  float tCoxa = (rx + 1.0f) * 0.5f;

  for (int ch = FIRST_CH; ch <= LAST_CH; ch++) {
    targetAngle[ch] = standPose[ch];
  }

  for (int ch = 0; ch <= 12; ch++) {
    if (lowPose[ch] != highPose[ch]) {
      float lowSafe = safeBlendFromCenter(standPose[ch], lowPose[ch], 0.35f);
      float highSafe = safeBlendFromCenter(standPose[ch], highPose[ch], 0.35f);
      targetAngle[ch] = lerpF(lowSafe, highSafe, tHeight);
    }
  }

  int coxaChannels[4] = { 0, 3, 6, 9 };
  for (int i = 0; i < 4; i++) {
    int ch = coxaChannels[i];
    float backSafe = safeBlendFromCenter(standPose[ch], coxaBack[ch], 0.22f);
    float forwardSafe = safeBlendFromCenter(standPose[ch], coxaForward[ch], 0.22f);
    targetAngle[ch] = lerpF(backSafe, forwardSafe, tCoxa);
  }

  targetAngle[12] = neckTarget;
}

// =====================================================
// Helpers gait
// =====================================================
float getLegForwardSafe(int legIndex, float extraTurn = 0.0f) {
  int ch = legs[legIndex].coxa;
  float gain = GAIT_COXA_GAIN + extraTurn;
  if (gain < 0.05f) gain = 0.05f;
  float fwd = safeBlendFromCenter(standPose[ch], coxaForward[ch], gain);
  return constrain(fwd, 0, 180);
}

float getLegBackSafe(int legIndex, float extraTurn = 0.0f) {
  int ch = legs[legIndex].coxa;
  float gain = GAIT_COXA_GAIN + extraTurn;
  if (gain < 0.05f) gain = 0.05f;
  float back = safeBlendFromCenter(standPose[ch], coxaBack[ch], gain);
  return constrain(back, 0, 180);
}

float getFemurLiftSafe(int ch) {
  return safeBlendFromCenter(standPose[ch], highPose[ch], GAIT_LIFT_GAIN);
}

float getTibiaLiftSafe(int ch) {
  return safeBlendFromCenter(standPose[ch], highPose[ch], GAIT_LIFT_GAIN);
}

float getFemurStandSafe(int ch) {
  return standPose[ch];
}

float getTibiaStandSafe(int ch) {
  return standPose[ch];
}

void setNeutralStandTargets() {
  for (int ch = FIRST_CH; ch <= LAST_CH; ch++) {
    targetAngle[ch] = standPose[ch];
  }
  targetAngle[12] = neckTarget;
}

void startGaitIfNeeded() {
  if (!gaitActive) {
    gaitActive = true;
    gaitPhase = 0;
    gaitLegIndex = 0;
    gaitPhaseStartMs = millis();
    Serial.println("GAIT START");
    beep(1800, 60);
  }
}

void stopGaitToNeutral() {
  if (gaitActive) {
    gaitActive = false;
    setNeutralStandTargets();
    Serial.println("GAIT STOP -> neutral");
    beep(1200, 60);
  }
}

void applyGaitTargets() {
  if (!servosEnabled) return;

  float fb = (float)(-rxData.ry) / 127.0f;
  float turn = (float)(rxData.rx) / 127.0f;

  fb = constrain(fb, -1.0f, 1.0f);
  turn = constrain(turn, -1.0f, 1.0f);

  gaitCmdFB = fb;
  gaitCmdTurn = turn;

  if (fabs(gaitCmdFB) < 0.18f && fabs(gaitCmdTurn) < 0.18f) {
    stopGaitToNeutral();
    return;
  }

  startGaitIfNeeded();

  setNeutralStandTargets();

  int activeLeg = gaitOrder[gaitLegIndex];

  float turnLeft = 0.0f;
  float turnRight = 0.0f;
  if (gaitCmdTurn > 0.0f) {
    turnLeft = 0.10f * gaitCmdTurn;
    turnRight = -0.08f * gaitCmdTurn;
  } else if (gaitCmdTurn < 0.0f) {
    turnLeft = 0.08f * gaitCmdTurn;
    turnRight = -0.10f * gaitCmdTurn;
  }

  auto legTurnGain = [&](int legIndex) {
    bool isLeft = (legIndex == 0 || legIndex == 2);
    return isLeft ? turnLeft : turnRight;
  };

  bool forwardMotion = (gaitCmdFB >= 0.0f);
  float mag = max(fabs(gaitCmdFB), fabs(gaitCmdTurn));
  mag = constrain(mag, 0.2f, 1.0f);

  for (int leg = 0; leg < 4; leg++) {
    int c = legs[leg].coxa;
    if (leg == activeLeg) continue;

    float tg = legTurnGain(leg);
    if (forwardMotion) {
      targetAngle[c] = safeBlendFromCenter(standPose[c], coxaBack[c], 0.12f * mag + tg);
    } else {
      targetAngle[c] = safeBlendFromCenter(standPose[c], coxaForward[c], 0.12f * mag + tg);
    }
  }

  int coxa = legs[activeLeg].coxa;
  int femur = legs[activeLeg].femur;
  int tibia = legs[activeLeg].tibia;

  uint32_t now = millis();
  uint32_t elapsed = now - gaitPhaseStartMs;

  switch (gaitPhase) {
    case 0:
      targetAngle[femur] = getFemurLiftSafe(femur);
      targetAngle[tibia] = getTibiaLiftSafe(tibia);
      targetAngle[coxa] = standPose[coxa];
      if (elapsed >= GAIT_LIFT_MS) {
        gaitPhase = 1;
        gaitPhaseStartMs = now;
      }
      break;

    case 1:
      targetAngle[femur] = getFemurLiftSafe(femur);
      targetAngle[tibia] = getTibiaLiftSafe(tibia);
      if (forwardMotion) {
        targetAngle[coxa] = getLegForwardSafe(activeLeg, legTurnGain(activeLeg));
      } else {
        targetAngle[coxa] = getLegBackSafe(activeLeg, legTurnGain(activeLeg));
      }
      if (elapsed >= GAIT_SWING_MS) {
        gaitPhase = 2;
        gaitPhaseStartMs = now;
      }
      break;

    case 2:
      targetAngle[femur] = getFemurStandSafe(femur);
      targetAngle[tibia] = getTibiaStandSafe(tibia);
      if (elapsed >= GAIT_DROP_MS) {
        gaitPhase = 3;
        gaitPhaseStartMs = now;
      }
      break;

    case 3:
      if (elapsed >= GAIT_PUSH_MS) {
        gaitLegIndex++;
        if (gaitLegIndex >= 4) gaitLegIndex = 0;
        gaitPhase = 0;
        gaitPhaseStartMs = now;
      }
      break;
  }

  targetAngle[12] = neckTarget;
}

void processGaitMode() {
  if (!servosEnabled) return;

  if (btnEdge(PS2J_TRIANGLE)) {
    neckTarget = NECK_LEFT;
    beep(2100, 50);
  }
  if (btnEdge(PS2J_CROSS)) {
    neckTarget = NECK_CENTER;
    beep(1500, 50);
  }
  if (btnEdge(PS2J_SQUARE)) {
    neckTarget = NECK_RIGHT;
    beep(900, 50);
  }

  if (btnEdge(PS2J_CIRCLE)) {
    neckTarget = NECK_CENTER;
    stopGaitToNeutral();
    beep(1800, 80);
  }

  applyGaitTargets();
}

// =====================================================
// Setup
// =====================================================
void setup() {
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  digitalWrite(PIN_LED, LOW);
  noTone(PIN_BUZZER);

  Serial.begin(115200);
  delay(600);

  Serial.println();
  Serial.println("=== Q-POD MINI RX SAFE GAIT TEST ===");

  Wire.begin();
  pca.begin();
  pca.setPWMFreq(PWM_FREQ);
  delay(100);
  allOff();
  Serial.println("PCA9685 lista.");

  buildPoseTables();
  buildStandPose();

  for (int ch = FIRST_CH; ch <= LAST_CH; ch++) {
    targetAngle[ch] = standPose[ch];
    currentAngle[ch] = standPose[ch];
  }

  if (!radio.begin()) {
    Serial.println("ERROR: NRF24 no responde.");
    while (1) {
      digitalWrite(PIN_LED, !digitalRead(PIN_LED));
      tone(PIN_BUZZER, 800);
      delay(80);
      noTone(PIN_BUZZER);
      delay(80);
    }
  }

  radio.setChannel(76);
  radio.setDataRate(RF24_250KBPS);
  radio.setPALevel(RF24_PA_LOW);
  radio.setAutoAck(true);
  radio.openReadingPipe(1, ADDR);
  radio.startListening();

  printHelp();
  printCurrentChannel();

  beep(1800, 100);
  delay(120);
  beep(2400, 100);
}

// =====================================================
// Loop
// =====================================================
void loop() {
  if (radio.available()) {
    while (radio.available()) {
      radio.read(&rxData, sizeof(rxData));
    }

    lastPacketMs = millis();

    if (!linkAlive) {
      linkAlive = true;
      Serial.println("LINK OK");
      beep(1600, 70);
    }

    digitalWrite(PIN_LED, HIGH);

    if (btnEdge(PS2J_START)) {
      enableServosAndGoNeutral();
    }

    if (btnEdge(PS2J_SELECT)) {
      disableAllServos();
    }

    if (rxData.mode3 == 0) {
      processCalibMode();
    } else if (rxData.mode3 == 1) {
      processManualMode();
    } else {
      processGaitMode();
    }

    prevBtn = rxData.btn;

    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 220) {
      lastPrint = millis();
      Serial.print("mode=");
      Serial.print(rxData.mode3);
      Serial.print(" rx=");
      Serial.print(rxData.rx);
      Serial.print(" ry=");
      Serial.print(rxData.ry);
      Serial.print(" servos=");
      Serial.print(servosEnabled ? "ON" : "OFF");
      Serial.print(" gait=");
      Serial.println(gaitActive ? "ON" : "OFF");
    }
  }

  if (linkAlive && (millis() - lastPacketMs > 800)) {
    linkAlive = false;
    digitalWrite(PIN_LED, LOW);
    prevBtn = 0;
    gaitActive = false;
    Serial.println("LINK LOST");
    beep(700, 140);
  }

  if (millis() - lastMotionMs >= MOTION_PERIOD_MS) {
    lastMotionMs = millis();

    if (servosEnabled) {
      float step = (rxData.mode3 == 0) ? SPEED_CALIB : (rxData.mode3 == 1) ? SPEED_MANUAL
                                                                           : SPEED_GAIT;
      updateMotion(step);
    }
  }
}