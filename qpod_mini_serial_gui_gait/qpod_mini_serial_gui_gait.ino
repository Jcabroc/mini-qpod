#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <EEPROM.h>
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

PS2J_Packet rxData = { 0, 0, 0, 0, 0, 1, 0, 0 };
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
  "L1_COXA", "L1_FEMUR", "L1_TIBIA",
  "R1_COXA", "R1_FEMUR", "R1_TIBIA",
  "L2_COXA", "L2_FEMUR", "L2_TIBIA",
  "R2_COXA", "R2_FEMUR", "R2_TIBIA",
  "CUELLO"
};

// =====================================================
// Tabla calibrada base
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
// Parámetros ajustables desde GUI/Serial
// =====================================================
float PARAM_STAND_COXA_GAIN = 0.30f;
float PARAM_STAND_HEIGHT_GAIN = 0.60f;
float PARAM_GAIT_COXA_GAIN = 0.28f;
float PARAM_GAIT_LIFT_GAIN = 0.30f;

uint16_t PARAM_GAIT_LIFT_MS = 180;
uint16_t PARAM_GAIT_SWING_MS = 220;
uint16_t PARAM_GAIT_DROP_MS = 180;
uint16_t PARAM_GAIT_PUSH_MS = 220;

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

int gaitOrder[4] = { 0, 3, 1, 2 };  // L1, R2, R1, L2

// =====================================================
// Estado gait
// =====================================================
bool gaitActive = false;
uint8_t gaitPhase = 0;
uint8_t gaitLegIndex = 0;
uint32_t gaitPhaseStartMs = 0;

float gaitCmdFB = 0.0f;
float gaitCmdTurn = 0.0f;

// =====================================================
// EEPROM MANAGER
// =====================================================
struct EEPROMData {
  uint8_t magic;
  uint8_t angleA[13];
  uint8_t angleCenter[13];
  uint8_t angleB[13];
  float standPose_coxaGain;
  float standPose_heightGain;
  float gait_coxaGain;
  float gait_liftGain;
  uint16_t gait_liftMs;
  uint16_t gait_swingMs;
  uint16_t gait_dropMs;
  uint16_t gait_pushMs;
};

const uint16_t EEPROM_BASE = 0;
const uint8_t EEPROM_MAGIC = 0xAA;
EEPROMData eepromData;

void eeprom_write();

void copyRuntimeToEEPROMData() {
  eepromData.magic = EEPROM_MAGIC;
  for (int i = 0; i < 13; i++) {
    eepromData.angleA[i] = constrain(angleA[i], 0, 180);
    eepromData.angleCenter[i] = constrain(angleCenter[i], 0, 180);
    eepromData.angleB[i] = constrain(angleB[i], 0, 180);
  }
  eepromData.standPose_coxaGain = PARAM_STAND_COXA_GAIN;
  eepromData.standPose_heightGain = PARAM_STAND_HEIGHT_GAIN;
  eepromData.gait_coxaGain = PARAM_GAIT_COXA_GAIN;
  eepromData.gait_liftGain = PARAM_GAIT_LIFT_GAIN;
  eepromData.gait_liftMs = PARAM_GAIT_LIFT_MS;
  eepromData.gait_swingMs = PARAM_GAIT_SWING_MS;
  eepromData.gait_dropMs = PARAM_GAIT_DROP_MS;
  eepromData.gait_pushMs = PARAM_GAIT_PUSH_MS;
}

void copyEEPROMDataToRuntime() {
  for (int i = 0; i < 13; i++) {
    angleA[i] = constrain(eepromData.angleA[i], 0, 180);
    angleCenter[i] = constrain(eepromData.angleCenter[i], 0, 180);
    angleB[i] = constrain(eepromData.angleB[i], 0, 180);
  }
  PARAM_STAND_COXA_GAIN = constrain(eepromData.standPose_coxaGain, 0.0f, 1.0f);
  PARAM_STAND_HEIGHT_GAIN = constrain(eepromData.standPose_heightGain, 0.0f, 1.0f);
  PARAM_GAIT_COXA_GAIN = constrain(eepromData.gait_coxaGain, 0.0f, 1.0f);
  PARAM_GAIT_LIFT_GAIN = constrain(eepromData.gait_liftGain, 0.0f, 1.0f);
  PARAM_GAIT_LIFT_MS = constrain(eepromData.gait_liftMs, 50, 2000);
  PARAM_GAIT_SWING_MS = constrain(eepromData.gait_swingMs, 50, 2000);
  PARAM_GAIT_DROP_MS = constrain(eepromData.gait_dropMs, 50, 2000);
  PARAM_GAIT_PUSH_MS = constrain(eepromData.gait_pushMs, 50, 2000);
}

void eeprom_write() {
  copyRuntimeToEEPROMData();
  EEPROM.put(EEPROM_BASE, eepromData);
  Serial.println(F("[EEPROM] OK GUARDADO"));
}

void eeprom_init() {
  EEPROM.get(EEPROM_BASE, eepromData);
  if (eepromData.magic != EEPROM_MAGIC) {
    Serial.println(F("[EEPROM] No valido, inicializando defaults..."));
    copyRuntimeToEEPROMData();
    EEPROM.put(EEPROM_BASE, eepromData);
  } else {
    copyEEPROMDataToRuntime();
    Serial.println(F("[EEPROM] OK CARGADO"));
  }
}

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

void allOff() {
  for (int ch = FIRST_CH; ch <= LAST_CH; ch++) pca.setPWM(ch, 0, 0);
  Serial.println(F("TODOS LOS CANALES OFF"));
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
  gain = constrain(gain, 0.0f, 1.0f);
  return center + (extreme - center) * gain;
}

// =====================================================
// Tablas semánticas de movimiento
// =====================================================
void buildPoseTables() {
  for (int ch = FIRST_CH; ch <= LAST_CH; ch++) {
    coxaForward[ch] = angleCenter[ch];
    coxaBack[ch] = angleCenter[ch];
    lowPose[ch] = angleCenter[ch];
    highPose[ch] = angleCenter[ch];
  }

  // Coxas corregidas
  coxaForward[0] = angleA[0];
  coxaBack[0] = angleB[0];  // L1
  coxaForward[3] = angleA[3];
  coxaBack[3] = angleB[3];  // R1
  coxaForward[6] = angleA[6];
  coxaBack[6] = angleB[6];  // L2
  coxaForward[9] = angleB[9];
  coxaBack[9] = angleA[9];  // R2

  // Altura corregida
  lowPose[1] = angleA[1];
  highPose[1] = angleB[1];
  lowPose[2] = angleB[2];
  highPose[2] = angleA[2];
  lowPose[4] = angleB[4];
  highPose[4] = angleA[4];
  lowPose[5] = angleB[5];
  highPose[5] = angleA[5];
  lowPose[7] = angleB[7];
  highPose[7] = angleA[7];
  lowPose[8] = angleB[8];
  highPose[8] = angleA[8];
  lowPose[10] = angleB[10];
  highPose[10] = angleA[10];
  lowPose[11] = angleB[11];
  highPose[11] = angleA[11];
}

void buildStandPose() {
  for (int ch = FIRST_CH; ch <= LAST_CH; ch++) standPose[ch] = angleCenter[ch];

  standPose[0] = safeBlendFromCenter(angleCenter[0], coxaForward[0], PARAM_STAND_COXA_GAIN);
  standPose[6] = safeBlendFromCenter(angleCenter[6], coxaForward[6], PARAM_STAND_COXA_GAIN);
  standPose[3] = safeBlendFromCenter(angleCenter[3], coxaForward[3], PARAM_STAND_COXA_GAIN);
  standPose[9] = safeBlendFromCenter(angleCenter[9], coxaForward[9], PARAM_STAND_COXA_GAIN);

  standPose[1] = safeBlendFromCenter(angleCenter[1], lowPose[1], PARAM_STAND_HEIGHT_GAIN);
  standPose[2] = safeBlendFromCenter(angleCenter[2], lowPose[2], PARAM_STAND_HEIGHT_GAIN);
  standPose[4] = safeBlendFromCenter(angleCenter[4], lowPose[4], PARAM_STAND_HEIGHT_GAIN);
  standPose[5] = safeBlendFromCenter(angleCenter[5], lowPose[5], PARAM_STAND_HEIGHT_GAIN);
  standPose[7] = safeBlendFromCenter(angleCenter[7], lowPose[7], PARAM_STAND_HEIGHT_GAIN);
  standPose[8] = safeBlendFromCenter(angleCenter[8], lowPose[8], PARAM_STAND_HEIGHT_GAIN);
  standPose[10] = safeBlendFromCenter(angleCenter[10], lowPose[10], PARAM_STAND_HEIGHT_GAIN);
  standPose[11] = safeBlendFromCenter(angleCenter[11], lowPose[11], PARAM_STAND_HEIGHT_GAIN);

  standPose[12] = NECK_CENTER;
}

void rebuildMotionTables() {
  buildPoseTables();
  buildStandPose();
}

// =====================================================
// Estados generales
// =====================================================
void enableServosAndGoNeutral() {
  servosEnabled = true;
  gaitActive = false;
  neckTarget = NECK_CENTER;
  rebuildMotionTables();
  for (int ch = FIRST_CH; ch <= LAST_CH; ch++) targetAngle[ch] = standPose[ch];
  targetAngle[12] = neckTarget;
  Serial.println(F("SERVOS ENABLED -> postura base tipo arana"));
  beep(1800, 80);
}

void disableAllServos() {
  servosEnabled = false;
  gaitActive = false;
  allOff();
  Serial.println(F("SERVOS DISABLED"));
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
  Serial.print(F("Canal actual: CH"));
  Serial.print(currentCh);
  Serial.print(F(" -> "));
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
  Serial.println(F("=== Q-POD MINI TEST + SERIAL GUI ==="));
  Serial.println(F("SERIAL: INFO, STATE, ANGLES, CONFIG, STAND, OFF"));
  Serial.println(F("A/C/B <ch> <ang>, GAIN <name> <val>, GAITMS <phase> <ms>, EEPROM_SAVE"));
}

// =====================================================
// Protocolo serial para GUI
// =====================================================
int parseFirstInt(String s) {
  s.trim();
  return s.toInt();
}
int parseSecondInt(String s) {
  s.trim();
  int space = s.indexOf(' ');
  if (space < 0) return 0;
  return s.substring(space + 1).toInt();
}

void applyAngleRuntime(char type, int ch, int angle) {
  if (ch < 0 || ch > 12) {
    Serial.println(F("[ERR] Canal invalido"));
    return;
  }
  angle = constrain(angle, 0, 180);
  if (type == 'A') angleA[ch] = angle;
  else if (type == 'C') angleCenter[ch] = angle;
  else if (type == 'B') angleB[ch] = angle;
  targetAngle[ch] = angle;
  rebuildMotionTables();
  Serial.print(F("["));
  Serial.print(type);
  Serial.print(F("] CH"));
  Serial.print(ch);
  Serial.print(F("="));
  Serial.println(angle);
}

void printState() {
  Serial.print(F("STATE: NRF="));
  Serial.print(linkAlive ? F("OK") : F("LOST"));
  Serial.print(F(" PCA=OK MODE="));
  Serial.print(rxData.mode3);
  Serial.print(F(" SERVOS="));
  Serial.print(servosEnabled ? F("ON") : F("OFF"));
  Serial.print(F(" GAIT="));
  Serial.println(gaitActive ? F("ON") : F("OFF"));
}

void printAngles() {
  Serial.print(F("SERVO: "));
  for (int i = 0; i < 13; i++) {
    Serial.print(i);
    Serial.print(F("="));
    Serial.print((int)(currentAngle[i] + 0.5f));
    if (i < 12) Serial.print(F(" "));
  }
  Serial.println();
}

void printConfig() {
  Serial.print(F("CONFIG: STAND_COXA="));
  Serial.print(PARAM_STAND_COXA_GAIN, 3);
  Serial.print(F(" STAND_HEIGHT="));
  Serial.print(PARAM_STAND_HEIGHT_GAIN, 3);
  Serial.print(F(" GAIT_COXA="));
  Serial.print(PARAM_GAIT_COXA_GAIN, 3);
  Serial.print(F(" GAIT_LIFT="));
  Serial.print(PARAM_GAIT_LIFT_GAIN, 3);
  Serial.print(F(" LIFT_MS="));
  Serial.print(PARAM_GAIT_LIFT_MS);
  Serial.print(F(" SWING_MS="));
  Serial.print(PARAM_GAIT_SWING_MS);
  Serial.print(F(" DROP_MS="));
  Serial.print(PARAM_GAIT_DROP_MS);
  Serial.print(F(" PUSH_MS="));
  Serial.println(PARAM_GAIT_PUSH_MS);
}

void serial_process() {
  while (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;
    int spaceIdx = line.indexOf(' ');
    String cmd = (spaceIdx > 0) ? line.substring(0, spaceIdx) : line;
    String args = (spaceIdx > 0) ? line.substring(spaceIdx + 1) : "";
    cmd.toUpperCase();
    args.trim();

    if (cmd == "A" || cmd == "C" || cmd == "B") applyAngleRuntime(cmd.charAt(0), parseFirstInt(args), parseSecondInt(args));
    else if (cmd == "EEPROM_A" || cmd == "EEPROM_C" || cmd == "EEPROM_B") {
      int ch = parseFirstInt(args);
      int angle = constrain(parseSecondInt(args), 0, 180);
      if (ch >= 0 && ch <= 12) {
        if (cmd == "EEPROM_A") eepromData.angleA[ch] = angle;
        else if (cmd == "EEPROM_C") eepromData.angleCenter[ch] = angle;
        else eepromData.angleB[ch] = angle;
        Serial.println(F("[EEPROM] parametro cargado en buffer"));
      }
    } else if (cmd == "GAIN") {
      int sp = args.indexOf(' ');
      if (sp < 0) {
        Serial.println(F("[ERR] Uso: GAIN STAND_COXA|STAND_HEIGHT|GAIT_COXA|GAIT_LIFT valor"));
        continue;
      }
      String name = args.substring(0, sp);
      float val = constrain(args.substring(sp + 1).toFloat(), 0.0f, 1.0f);
      name.toUpperCase();
      if (name == "STAND_COXA") PARAM_STAND_COXA_GAIN = val;
      else if (name == "STAND_HEIGHT") PARAM_STAND_HEIGHT_GAIN = val;
      else if (name == "GAIT_COXA") PARAM_GAIT_COXA_GAIN = val;
      else if (name == "GAIT_LIFT") PARAM_GAIT_LIFT_GAIN = val;
      else {
        Serial.println(F("[ERR] GAIN desconocido"));
        continue;
      }
      rebuildMotionTables();
      Serial.print(F("[GAIN] "));
      Serial.print(name);
      Serial.print(F("="));
      Serial.println(val, 3);
    } else if (cmd == "GAITMS") {
      int sp = args.indexOf(' ');
      if (sp < 0) {
        Serial.println(F("[ERR] Uso: GAITMS LIFT|SWING|DROP|PUSH ms"));
        continue;
      }
      String name = args.substring(0, sp);
      uint16_t ms = constrain(args.substring(sp + 1).toInt(), 50, 2000);
      name.toUpperCase();
      if (name == "LIFT") PARAM_GAIT_LIFT_MS = ms;
      else if (name == "SWING") PARAM_GAIT_SWING_MS = ms;
      else if (name == "DROP") PARAM_GAIT_DROP_MS = ms;
      else if (name == "PUSH") PARAM_GAIT_PUSH_MS = ms;
      else {
        Serial.println(F("[ERR] GAITMS desconocido"));
        continue;
      }
      Serial.print(F("[GAITMS] "));
      Serial.print(name);
      Serial.print(F("="));
      Serial.println(ms);
    } else if (cmd == "EEPROM_SAVE" || cmd == "SAVE") eeprom_write();
    else if (cmd == "EEPROM_LOAD" || cmd == "LOAD") {
      eeprom_init();
      rebuildMotionTables();
      Serial.println(F("[EEPROM] OK RECARGADO"));
    } else if (cmd == "STATE") printState();
    else if (cmd == "ANGLES") printAngles();
    else if (cmd == "CONFIG") printConfig();
    else if (cmd == "STAND") {
      enableServosAndGoNeutral();
      Serial.println(F("[STAND] OK"));
    } else if (cmd == "OFF") {
      disableAllServos();
      Serial.println(F("[OFF] OK"));
    } else if (cmd == "INFO" || cmd == "HELP") {
      Serial.println(F("Q-POD MINI - Comandos:"));
      Serial.println(F("A/C/B <ch> <ang> | GAIN STAND_COXA|STAND_HEIGHT|GAIT_COXA|GAIT_LIFT <0..1>"));
      Serial.println(F("GAITMS LIFT|SWING|DROP|PUSH <ms> | STATE | ANGLES | CONFIG | STAND | OFF | EEPROM_SAVE"));
    } else {
      Serial.print(F("[ERR] Comando desconocido: "));
      Serial.println(cmd);
    }
  }
}

// =====================================================
// Modos
// =====================================================
void processCalibMode() {
  if (!servosEnabled) return;
  if (btnEdge(PS2J_TRIANGLE)) nextChannel();
  if (btnEdge(PS2J_SQUARE)) prevChannel();
  if (btnEdge(PS2J_CROSS)) {
    targetAngle[currentCh] = angleA[currentCh];
    beep(1000, 50);
  }
  if (btnEdge(PS2J_CIRCLE)) {
    targetAngle[currentCh] = angleCenter[currentCh];
    beep(1600, 50);
  }
  if (btnEdge(PS2J_R3)) {
    targetAngle[currentCh] = angleB[currentCh];
    beep(2200, 50);
  }
}

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
    rebuildMotionTables();
    for (int ch = FIRST_CH; ch <= LAST_CH; ch++) targetAngle[ch] = standPose[ch];
    targetAngle[12] = neckTarget;
    Serial.println(F("Postura base tipo arana"));
    beep(1800, 80);
  }

  float ry = constrain((float)(-rxData.ry) / 127.0f, -1.0f, 1.0f);
  float tHeight = (ry + 1.0f) * 0.5f;
  float rx = constrain((float)(rxData.rx) / 127.0f, -1.0f, 1.0f);
  float tCoxa = (rx + 1.0f) * 0.5f;

  for (int ch = FIRST_CH; ch <= LAST_CH; ch++) targetAngle[ch] = standPose[ch];

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

float getLegForwardSafe(int legIndex, float extraTurn = 0.0f) {
  int ch = legs[legIndex].coxa;
  float gain = PARAM_GAIT_COXA_GAIN + extraTurn;
  if (gain < 0.05f) gain = 0.05f;
  return constrain(safeBlendFromCenter(standPose[ch], coxaForward[ch], gain), 0, 180);
}

float getLegBackSafe(int legIndex, float extraTurn = 0.0f) {
  int ch = legs[legIndex].coxa;
  float gain = PARAM_GAIT_COXA_GAIN + extraTurn;
  if (gain < 0.05f) gain = 0.05f;
  return constrain(safeBlendFromCenter(standPose[ch], coxaBack[ch], gain), 0, 180);
}

float getFemurLiftSafe(int ch) {
  return safeBlendFromCenter(standPose[ch], highPose[ch], PARAM_GAIT_LIFT_GAIN);
}
float getTibiaLiftSafe(int ch) {
  return safeBlendFromCenter(standPose[ch], highPose[ch], PARAM_GAIT_LIFT_GAIN);
}

void setNeutralStandTargets() {
  rebuildMotionTables();
  for (int ch = FIRST_CH; ch <= LAST_CH; ch++) targetAngle[ch] = standPose[ch];
  targetAngle[12] = neckTarget;
}

void startGaitIfNeeded() {
  if (!gaitActive) {
    gaitActive = true;
    gaitPhase = 0;
    gaitLegIndex = 0;
    gaitPhaseStartMs = millis();
    Serial.println(F("GAIT START"));
    beep(1800, 60);
  }
}

void stopGaitToNeutral() {
  if (gaitActive) {
    gaitActive = false;
    setNeutralStandTargets();
    Serial.println(F("GAIT STOP -> neutral"));
    beep(1200, 60);
  }
}

void applyGaitTargets() {
  if (!servosEnabled) return;

  float fb = constrain((float)(-rxData.ry) / 127.0f, -1.0f, 1.0f);
  float turn = constrain((float)(rxData.rx) / 127.0f, -1.0f, 1.0f);
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
    if (forwardMotion) targetAngle[c] = safeBlendFromCenter(standPose[c], coxaBack[c], 0.12f * mag + tg);
    else targetAngle[c] = safeBlendFromCenter(standPose[c], coxaForward[c], 0.12f * mag + tg);
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
      if (elapsed >= PARAM_GAIT_LIFT_MS) {
        gaitPhase = 1;
        gaitPhaseStartMs = now;
      }
      break;
    case 1:
      targetAngle[femur] = getFemurLiftSafe(femur);
      targetAngle[tibia] = getTibiaLiftSafe(tibia);
      targetAngle[coxa] = forwardMotion ? getLegForwardSafe(activeLeg, legTurnGain(activeLeg)) : getLegBackSafe(activeLeg, legTurnGain(activeLeg));
      if (elapsed >= PARAM_GAIT_SWING_MS) {
        gaitPhase = 2;
        gaitPhaseStartMs = now;
      }
      break;
    case 2:
      targetAngle[femur] = standPose[femur];
      targetAngle[tibia] = standPose[tibia];
      if (elapsed >= PARAM_GAIT_DROP_MS) {
        gaitPhase = 3;
        gaitPhaseStartMs = now;
      }
      break;
    case 3:
      if (elapsed >= PARAM_GAIT_PUSH_MS) {
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
  Serial.println(F("=== Q-POD MINI RX SAFE GAIT TEST + SERIAL GUI ==="));

  Wire.begin();
  pca.begin();
  pca.setPWMFreq(PWM_FREQ);
  delay(100);
  allOff();
  Serial.println(F("PCA9685 lista."));

  eeprom_init();
  rebuildMotionTables();

  for (int ch = FIRST_CH; ch <= LAST_CH; ch++) {
    targetAngle[ch] = standPose[ch];
    currentAngle[ch] = standPose[ch];
  }

  if (!radio.begin()) {
    Serial.println(F("ERROR: NRF24 no responde."));
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
  serial_process();

  if (radio.available()) {
    while (radio.available()) radio.read(&rxData, sizeof(rxData));
    lastPacketMs = millis();

    if (!linkAlive) {
      linkAlive = true;
      Serial.println(F("LINK OK"));
      beep(1600, 70);
    }

    digitalWrite(PIN_LED, HIGH);

    if (btnEdge(PS2J_START)) enableServosAndGoNeutral();
    if (btnEdge(PS2J_SELECT)) disableAllServos();

    if (rxData.mode3 == 0) processCalibMode();
    else if (rxData.mode3 == 1) processManualMode();
    else processGaitMode();

    prevBtn = rxData.btn;

    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 500) {
      lastPrint = millis();
      printState();
    }
  }

  if (linkAlive && (millis() - lastPacketMs > 800)) {
    linkAlive = false;
    digitalWrite(PIN_LED, LOW);
    prevBtn = 0;
    gaitActive = false;
    Serial.println(F("LINK LOST"));
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
