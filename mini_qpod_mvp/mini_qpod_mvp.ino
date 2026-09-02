#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include "robot_config.h"
#include "servo_control.h"
#include "gait_balance.h"

enum RobotMode : uint8_t { SAFE_OFF, CALIBRATION, STAND_MODE, LEG_TEST, WALK_MODE };
enum AbortCause : uint8_t { ABORT_NONE, ABORT_IMU_UNSAFE, ABORT_HOST_TIMEOUT };

Adafruit_PWMServoDriver pca(PCA9685_ADDRESS);
ServoController servos(pca);
ImuManager imu;
GaitBalance motion(servos, imu);

RobotMode mode = SAFE_OFF;
bool walkUnlocked = WALK_ENABLED_AT_BOOT;
int8_t selectedChannel = -1;
AbortCause abortCause = ABORT_NONE;
uint32_t lastCalibrationHostActivityMs = 0;
char inputLine[80];
uint8_t inputLength = 0;
constexpr uint8_t SONAR_PAN_CHANNEL = 12;
constexpr float SONAR_PAN_CENTER_DEG = 90.0f;
constexpr float SONAR_PAN_MIN_DEG = 67.5f;
constexpr float SONAR_PAN_MAX_DEG = 112.5f;

void printHelp();
void processCommand(char *line);
bool parseIntStrict(const char *text, int &value);
bool parseFloatStrict(const char *text, float &value);
bool calibrationImuSafe();
bool calibrationTiltSafe();
void abortToSafeOff(AbortCause cause);
const char *modeName(RobotMode value);
const char *abortName(AbortCause value);
void refreshCalibrationHostActivity();
bool calibrationHostTimedOut();

void setup() {
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  Serial.begin(115200);
  Wire.begin();
  pca.begin();
  pca.setPWMFreq(SERVO_PWM_HZ);
  delay(10);
  servos.begin();
  servos.disable();
  bool imuOk = imu.begin();
  Serial.println(F("\nMINI Q-POD MVP v0.3"));
  Serial.print(F("Enlace IMU Pico UART: "));
  Serial.println(IMU_ENABLED ? (imuOk ? F("ESPERANDO DATOS") : F("ERROR")) : F("DESACTIVADO"));
  Serial.println(F("Arranque seguro: servos OFF. Escriba HELP."));
}

void loop() {
  // La telemetria se mantiene aun en SAFE_OFF para comprobar la IMU sin PWM.
  if (IMU_ENABLED) imu.update();

  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      inputLine[inputLength] = '\0';
      if (inputLength) processCommand(inputLine);
      inputLength = 0;
    } else if (inputLength < sizeof(inputLine) - 1) {
      inputLine[inputLength++] = c;
    }
  }

  if (mode == CALIBRATION && !calibrationImuSafe()) {
    abortToSafeOff(ABORT_IMU_UNSAFE);
  } else if (mode == CALIBRATION && calibrationHostTimedOut()) {
    abortToSafeOff(ABORT_HOST_TIMEOUT);
  } else if (mode == LEG_TEST || mode == WALK_MODE || mode == STAND_MODE) {
    if (!motion.update()) {
      abortToSafeOff(ABORT_IMU_UNSAFE);
    } else if (mode == LEG_TEST && motion.phase() == GaitBalance::IDLE) {
      mode = STAND_MODE;
      Serial.println(F("[LEG] Prueba terminada."));
    }
  }
  servos.update();
}

void processCommand(char *line) {
  char *cmd = strtok(line, " \t");
  if (!cmd) return;
  for (char *p = cmd; *p; ++p) *p = toupper((unsigned char)*p);

  if (!strcmp(cmd, "HELP")) {
    if (strtok(nullptr, " \t")) Serial.println(F("[ERR] HELP no recibe argumentos."));
    else printHelp();
  } else if (!strcmp(cmd, "OFF")) {
    if (strtok(nullptr, " \t")) { Serial.println(F("[ERR] OFF no recibe argumentos.")); return; }
    servos.disable();
    motion.stop();
    selectedChannel = -1;
    mode = SAFE_OFF;
    Serial.println(F("[OFF] ch=-1 PWM=OFF mode=SAFE_OFF"));
  } else if (!strcmp(cmd, "CALIB")) {
    if (strtok(nullptr, " \t")) { Serial.println(F("[ERR] CALIB no recibe argumentos.")); return; }
    if (!calibrationImuSafe()) { Serial.println(F("[ERR] CALIB bloqueado: IMU perdida o inclinacion insegura.")); return; }
    servos.disable();
    motion.stop();
    selectedChannel = -1;
    abortCause = ABORT_NONE;
    mode = CALIBRATION;
    refreshCalibrationHostActivity();
    Serial.println(F("[CALIB] ch=-1 PWM=OFF seleccione un canal con SELECT."));
  } else if (!strcmp(cmd, "SELECT")) {
    char *a = strtok(nullptr, " \t");
    int channel;
    if (!a || strtok(nullptr, " \t") || !parseIntStrict(a, channel) || channel < 0 || channel >= SERVO_COUNT) {
      Serial.println(F("[ERR] Uso: SELECT <0..12>.")); return;
    }
    if (mode != CALIBRATION) { Serial.println(F("[ERR] Use CALIB antes de SELECT.")); return; }
    servos.disable();
    selectedChannel = (int8_t)channel;
    refreshCalibrationHostActivity();
    Serial.print(F("[SELECT] ch=")); Serial.print(channel); Serial.println(F(" PWM=OFF"));
  } else if (!strcmp(cmd, "ENABLE")) {
    char *a = strtok(nullptr, " \t"), *b = strtok(nullptr, " \t");
    int channel; float requested, effective;
    if (!a || !b || strtok(nullptr, " \t") || !parseIntStrict(a, channel) ||
        !parseFloatStrict(b, requested) || channel < 0 || channel >= SERVO_COUNT) {
      Serial.println(F("[ERR] Uso: ENABLE <0..12> <angulo>.")); return;
    }
    if (mode != CALIBRATION || selectedChannel != channel) {
      Serial.println(F("[ERR] Seleccione primero ese canal con SELECT.")); return;
    }
    if (!calibrationImuSafe()) { Serial.println(F("[ERR] ENABLE bloqueado: IMU perdida o inclinacion insegura.")); return; }
    if (!servos.enableOnly((uint8_t)channel, requested, effective)) {
      Serial.println(F("[ERR] Canal no habilitable.")); return;
    }
    refreshCalibrationHostActivity();
    Serial.print(F("[ENABLE] ch=")); Serial.print(channel);
    Serial.print(F(" angle=")); Serial.println(effective, 2);
  } else if (!strcmp(cmd, "CENTER")) {
    char *a = strtok(nullptr, " \t"); int channel; float effective;
    if (!a || strtok(nullptr, " \t") || !parseIntStrict(a, channel) || channel < 0 || channel >= SERVO_COUNT) {
      Serial.println(F("[ERR] Uso: CENTER <0..12>.")); return;
    }
    if (mode != CALIBRATION || selectedChannel != channel || !servos.isOnlyChannelEnabled((uint8_t)channel)) {
      Serial.println(F("[ERR] CENTER exige canal seleccionado y habilitado.")); return;
    }
    if (!servos.setActiveTarget((uint8_t)channel, servos.center((uint8_t)channel), effective)) {
      Serial.println(F("[ERR] CENTER no aplicado.")); return;
    }
    refreshCalibrationHostActivity();
    Serial.print(F("[CENTER] ch=")); Serial.print(channel);
    Serial.print(F(" angle=")); Serial.println(effective, 2);
  } else if (!strcmp(cmd, "SERVO")) {
    char *a = strtok(nullptr, " \t"), *b = strtok(nullptr, " \t");
    int channel; float requested, effective;
    if (!a || !b || strtok(nullptr, " \t") || !parseIntStrict(a, channel) ||
        !parseFloatStrict(b, requested) || channel < 0 || channel >= SERVO_COUNT) {
      Serial.println(F("[ERR] Uso: SERVO <0..12> <angulo>.")); return;
    }
    if (mode != CALIBRATION || selectedChannel != channel || !servos.isOnlyChannelEnabled((uint8_t)channel)) {
      Serial.println(F("[ERR] SERVO exige canal seleccionado y habilitado.")); return;
    }
    if (!servos.setActiveTarget((uint8_t)channel, requested, effective)) {
      Serial.println(F("[ERR] Servo no aplicado.")); return;
    }
    refreshCalibrationHostActivity();
    Serial.print(F("[SERVO] ch=")); Serial.print(channel);
    Serial.print(F(" angle=")); Serial.println(effective, 2);
  } else if (!strcmp(cmd, "LIMITS")) {
    char *a = strtok(nullptr, " \t"), *b = strtok(nullptr, " \t");
    char *c = strtok(nullptr, " \t"), *d = strtok(nullptr, " \t");
    int channel, minAngle, centerAngle, maxAngle;
    if (!a || !b || !c || !d || strtok(nullptr, " \t") || !parseIntStrict(a, channel) ||
        !parseIntStrict(b, minAngle) || !parseIntStrict(c, centerAngle) || !parseIntStrict(d, maxAngle) ||
        channel < 0 || channel >= SERVO_COUNT) {
      Serial.println(F("[ERR] Uso: LIMITS <ch> <min> <center> <max>.")); return;
    }
    if (mode != CALIBRATION || servos.enabled()) {
      Serial.println(F("[ERR] LIMITS exige CALIB con PWM apagado.")); return;
    }
    if (!servos.setCalibration((uint8_t)channel, minAngle, centerAngle, maxAngle)) {
      Serial.println(F("[ERR] Limites incoherentes; no aplicados.")); return;
    }
    refreshCalibrationHostActivity();
    const ServoConfig &s = servos.config((uint8_t)channel);
    Serial.print(F("[LIMITS] ch=")); Serial.print(channel);
    Serial.print(F(" min=")); Serial.print(s.minAngle);
    Serial.print(F(" center=")); Serial.print(s.centerAngle);
    Serial.print(F(" max=")); Serial.print(s.maxAngle);
    Serial.print(F(" safeMin=")); Serial.print(servos.minSafe((uint8_t)channel), 0);
    Serial.print(F(" safeMax=")); Serial.println(servos.maxSafe((uint8_t)channel), 0);
  } else if (!strcmp(cmd, "SAVE")) {
    if (strtok(nullptr, " \t")) { Serial.println(F("[ERR] SAVE no recibe argumentos.")); return; }
    if (mode != SAFE_OFF) { Serial.println(F("[ERR] Ejecute OFF antes de SAVE.")); return; }
    Serial.println(servos.save() ? F("[SAVE] EEPROM OK.") : F("[ERR] Configuracion invalida."));
  } else if (!strcmp(cmd, "LOAD")) {
    if (strtok(nullptr, " \t")) { Serial.println(F("[ERR] LOAD no recibe argumentos.")); return; }
    if (mode != SAFE_OFF) { Serial.println(F("[ERR] Ejecute OFF antes de LOAD.")); return; }
    Serial.println(servos.load() ? F("[LOAD] EEPROM OK.") : F("[ERR] EEPROM invalida; sin cambios."));
  } else if (!strcmp(cmd, "DEFAULTS")) {
    if (mode != SAFE_OFF) Serial.println(F("[ERR] Ejecute OFF antes de DEFAULTS."));
    else if (strtok(nullptr, " \t")) Serial.println(F("[ERR] DEFAULTS no recibe argumentos."));
    else { servos.loadDefaults(); Serial.println(F("[DEFAULTS] En RAM; use CALIB para probar y SAVE para guardar.")); }
  } else if (!strcmp(cmd, "STAND")) {
    servos.enable(); motion.stand(); mode = STAND_MODE;
    Serial.println(F("[STAND] Moviendo lentamente a postura base."));
  } else if (!strcmp(cmd, "LEG")) {
    char *a = strtok(nullptr, " \t"); int leg;
    if (!a || strtok(nullptr, " \t") || !parseIntStrict(a, leg)) Serial.println(F("[ERR] Uso: LEG <0..3>."));
    else if (mode != STAND_MODE) Serial.println(F("[ERR] Primero STAND; uso LEG <0..3>."));
    else if (!motion.startLegTest((uint8_t)leg)) Serial.println(F("[ERR] Pata invalida u otro movimiento activo."));
    else { mode = LEG_TEST; Serial.println(F("[LEG] Prueba iniciada; listo para OFF.")); }
  } else if (!strcmp(cmd, "UNLOCK_WALK")) {
    if (strtok(nullptr, " \t")) Serial.println(F("[ERR] UNLOCK_WALK no recibe argumentos."));
    else { walkUnlocked = true; Serial.println(F("[WALK] Desbloqueado hasta reiniciar. Confirme que probo las 4 patas.")); }
  } else if (!strcmp(cmd, "WALK")) {
    if (strtok(nullptr, " \t")) Serial.println(F("[ERR] WALK no recibe argumentos."));
    else if (!walkUnlocked) Serial.println(F("[ERR] Use UNLOCK_WALK tras probar LEG 0..3."));
    else if (mode != STAND_MODE || !motion.startWalk()) Serial.println(F("[ERR] Primero STAND y espere estabilizacion."));
    else { mode = WALK_MODE; Serial.println(F("[WALK] Marcha lenta iniciada. OFF para detener.")); }
  } else if (!strcmp(cmd, "PING")) {
    if (strtok(nullptr, " \t")) { Serial.println(F("[ERR] PING no recibe argumentos.")); return; }
    refreshCalibrationHostActivity();
    Serial.print(F("PING mode=")); Serial.print(modeName(mode));
    Serial.print(F(" active=")); Serial.println(servos.activeChannel());
  } else if (!strcmp(cmd, "IMU")) {
    if (strtok(nullptr, " \t")) { Serial.println(F("[ERR] IMU no recibe argumentos.")); return; }
    Serial.print(F("IMU enabled=")); Serial.print(IMU_ENABLED);
    Serial.print(F(" healthy=")); Serial.print(imu.healthy());
    Serial.print(F(" roll=")); Serial.print(imu.roll(), 2);
    Serial.print(F(" pitch=")); Serial.print(imu.pitch(), 2);
    Serial.print(F(" calibrationTiltAbort=")); Serial.println(CALIBRATION_ABORT_ON_TILT);
  } else if (!strcmp(cmd, "SENSORS")) {
    if (strtok(nullptr, " \t")) { Serial.println(F("[ERR] SENSORS no recibe argumentos.")); return; }
    Serial.println(F("SENSORS relay=UART_PICO live=enabled"));
  } else if (!strcmp(cmd, "SONAR_ARM")) {
    if (strtok(nullptr, " \t")) { Serial.println(F("[ERR] SONAR_ARM no recibe argumentos.")); return; }
    if (mode != SAFE_OFF || !calibrationImuSafe()) { Serial.println(F("[ERR] SONAR_ARM exige SAFE_OFF e IMU segura.")); return; }
    servos.disable(); motion.stop(); selectedChannel = SONAR_PAN_CHANNEL;
    float effective;
    if (!servos.enableOnly(SONAR_PAN_CHANNEL, SONAR_PAN_CENTER_DEG, effective)) { selectedChannel = -1; Serial.println(F("[ERR] No se pudo habilitar CH12.")); return; }
    mode = CALIBRATION; abortCause = ABORT_NONE; refreshCalibrationHostActivity();
    Serial.print(F("[SONAR] ARM ch=12 angle=")); Serial.println(effective, 1);
  } else if (!strcmp(cmd, "SONAR")) {
    char *angleText = strtok(nullptr, " \t"); float requested, effective;
    if (!angleText || strtok(nullptr, " \t") || !parseFloatStrict(angleText, requested) || requested < SONAR_PAN_MIN_DEG || requested > SONAR_PAN_MAX_DEG) { Serial.println(F("[ERR] Uso: SONAR <67.5..112.5>.")); return; }
    if (mode != CALIBRATION || selectedChannel != SONAR_PAN_CHANNEL || !servos.isOnlyChannelEnabled(SONAR_PAN_CHANNEL)) { Serial.println(F("[ERR] Use SONAR_ARM antes de mover la cabeza.")); return; }
    if (!calibrationImuSafe() || !servos.setActiveTarget(SONAR_PAN_CHANNEL, requested, effective)) { abortToSafeOff(ABORT_IMU_UNSAFE); return; }
    refreshCalibrationHostActivity();
    Serial.print(F("[SONAR] angle=")); Serial.println(effective, 1);
  } else if (!strcmp(cmd, "SONAR_OFF")) {
    if (strtok(nullptr, " \t")) { Serial.println(F("[ERR] SONAR_OFF no recibe argumentos.")); return; }
    servos.disable(); motion.stop(); selectedChannel = -1; mode = SAFE_OFF;
    Serial.println(F("[SONAR] OFF PWM=OFF"));
  } else if (!strcmp(cmd, "SONAR_PING")) {
    if (mode == CALIBRATION && selectedChannel == SONAR_PAN_CHANNEL) refreshCalibrationHostActivity();
  } else if (!strcmp(cmd, "BEEP")) {
    char *frequencyText = strtok(nullptr, " \t"), *durationText = strtok(nullptr, " \t"); int frequency, duration;
    if (!frequencyText || !durationText || strtok(nullptr, " \t") || !parseIntStrict(frequencyText, frequency) || !parseIntStrict(durationText, duration) || frequency < 100 || frequency > 4000 || duration < 20 || duration > 1000) { Serial.println(F("[ERR] Uso: BEEP <100..4000 Hz> <20..1000 ms>.")); return; }
    tone(PIN_BUZZER, (unsigned int)frequency, (unsigned long)duration);
    Serial.print(F("[BEEP] frequency=")); Serial.print(frequency); Serial.print(F(" duration=")); Serial.println(duration);
  } else if (!strcmp(cmd, "STATUS")) {
    if (strtok(nullptr, " \t")) { Serial.println(F("[ERR] STATUS no recibe argumentos.")); return; }
    Serial.print(F("STATUS mode=")); Serial.print(modeName(mode));
    Serial.print(F(" servos=")); Serial.print(servos.enabled());
    Serial.print(F(" selected=")); Serial.print(selectedChannel);
    Serial.print(F(" active=")); Serial.print(servos.activeChannel());
    Serial.print(F(" imu=")); Serial.print(IMU_ENABLED ? imu.healthy() : true);
    Serial.print(F(" abort=")); Serial.print(abortName(abortCause));
    Serial.print(F(" phase=")); Serial.print((int)motion.phase());
    Serial.print(F(" leg=")); Serial.println(motion.activeLeg());
  } else if (!strcmp(cmd, "CONFIG")) {
    if (strtok(nullptr, " \t")) { Serial.println(F("[ERR] CONFIG no recibe argumentos.")); return; }
    Serial.print(F("CONFIG margin=")); Serial.println(servos.safetyMargin());
    for (uint8_t i = 0; i < SERVO_COUNT; ++i) {
      const ServoConfig &s = servos.config(i);
      Serial.print(F("CONFIG ch=")); Serial.print(i);
      Serial.print(F(" channel=")); Serial.print(s.channel);
      Serial.print(F(" min=")); Serial.print(s.minAngle);
      Serial.print(F(" center=")); Serial.print(s.centerAngle);
      Serial.print(F(" max=")); Serial.print(s.maxAngle);
      Serial.print(F(" direction=")); Serial.print(s.direction);
      Serial.print(F(" margin=")); Serial.print(servos.safetyMargin());
      Serial.print(F(" safeMin=")); Serial.print(servos.minSafe(i), 0);
      Serial.print(F(" safeMax=")); Serial.println(servos.maxSafe(i), 0);
    }
  } else {
    Serial.println(F("[ERR] Comando desconocido. Use HELP."));
  }
}

bool parseIntStrict(const char *text, int &value) {
  if (!text || !*text) return false;
  char *end = nullptr;
  long parsed = strtol(text, &end, 10);
  if (*end != '\0' || parsed < -32768L || parsed > 32767L) return false;
  value = (int)parsed;
  return true;
}

bool parseFloatStrict(const char *text, float &value) {
  if (!text || !*text) return false;
  const char *p = text;
  bool negative = false;
  if (*p == '+' || *p == '-') {
    negative = *p == '-';
    ++p;
  }
  bool hasDigits = false;
  float parsed = 0.0f;
  while (*p >= '0' && *p <= '9') {
    hasDigits = true;
    parsed = parsed * 10.0f + (*p - '0');
    if (!isfinite(parsed) || parsed > 1000000.0f) return false;
    ++p;
  }
  if (*p == '.') {
    ++p;
    float factor = 0.1f;
    while (*p >= '0' && *p <= '9') {
      hasDigits = true;
      parsed += (*p - '0') * factor;
      factor *= 0.1f;
      ++p;
    }
  }
  if (!hasDigits || *p != '\0' || !isfinite(parsed)) return false;
  value = negative ? -parsed : parsed;
  return true;
}

bool calibrationTiltSafe() {
  return !CALIBRATION_ABORT_ON_TILT ||
         (fabs(imu.roll()) <= DEFAULT_MAX_SAFE_TILT_DEG && fabs(imu.pitch()) <= DEFAULT_MAX_SAFE_TILT_DEG);
}

bool calibrationImuSafe() {
  return !IMU_ENABLED || (imu.healthy() && calibrationTiltSafe());
}

void refreshCalibrationHostActivity() {
  if (mode == CALIBRATION) lastCalibrationHostActivityMs = millis();
}

bool calibrationHostTimedOut() {
  return servos.activeChannel() >= 0 &&
         millis() - lastCalibrationHostActivityMs > CALIBRATION_HOST_TIMEOUT_MS;
}

void abortToSafeOff(AbortCause cause) {
  servos.disable();
  motion.stop();
  selectedChannel = -1;
  mode = SAFE_OFF;
  abortCause = cause;
  Serial.print(F("[ABORT] "));
  Serial.println(abortName(cause));
}

const char *modeName(RobotMode value) {
  switch (value) {
    case SAFE_OFF: return "SAFE_OFF";
    case CALIBRATION: return "CALIBRATION";
    case STAND_MODE: return "STAND_MODE";
    case LEG_TEST: return "LEG_TEST";
    case WALK_MODE: return "WALK_MODE";
    default: return "UNKNOWN";
  }
}

const char *abortName(AbortCause value) {
  switch (value) {
    case ABORT_NONE: return "NONE";
    case ABORT_IMU_UNSAFE: return "IMU_UNSAFE";
    case ABORT_HOST_TIMEOUT: return "HOST_TIMEOUT";
    default: return "UNKNOWN";
  }
}

void printHelp() {
  Serial.println(F("OFF | STATUS | CONFIG | IMU | SENSORS | PING | BEEP <Hz> <ms>"));
  Serial.println(F("SONAR_ARM | SONAR <67.5..112.5> | SONAR_OFF"));
  Serial.println(F("CALIB | SELECT <ch> | ENABLE <ch> <deg>"));
  Serial.println(F("CENTER <ch> | SERVO <ch> <deg> | LIMITS <ch> <min> <center> <max>"));
  Serial.println(F("SAVE | LOAD | DEFAULTS"));
  Serial.println(F("STAND | LEG <0..3> | UNLOCK_WALK | WALK"));
  Serial.println(F("Patas: 0=L1, 1=R1, 2=L2, 3=R2"));
}
#if 0  // Historical misplaced command block; superseded above.
constexpr uint8_t SONAR_PAN_CHANNEL = 12;  // Neck servo carrying the sonar head.
constexpr float SONAR_PAN_CENTER_DEG = 90.0f;
constexpr float SONAR_PAN_MIN_DEG = 67.5f;
constexpr float SONAR_PAN_MAX_DEG = 112.5f;
  } else if (!strcmp(cmd, "SENSORS")) {
    if (strtok(nullptr, " \t")) { Serial.println(F("[ERR] SENSORS no recibe argumentos.")); return; }
    Serial.println(F("SENSORS relay=UART_PICO live=enabled"));
  } else if (!strcmp(cmd, "SONAR_ARM")) {
    if (strtok(nullptr, " \t")) { Serial.println(F("[ERR] SONAR_ARM no recibe argumentos.")); return; }
    if (mode != SAFE_OFF || !calibrationImuSafe()) {
      Serial.println(F("[ERR] SONAR_ARM exige SAFE_OFF e IMU segura.")); return;
    }
    servos.disable(); motion.stop(); selectedChannel = SONAR_PAN_CHANNEL;
    float effective;
    if (!servos.enableOnly(SONAR_PAN_CHANNEL, SONAR_PAN_CENTER_DEG, effective)) {
      selectedChannel = -1; Serial.println(F("[ERR] No se pudo habilitar CH12.")); return;
    }
    mode = CALIBRATION; abortCause = ABORT_NONE; refreshCalibrationHostActivity();
    Serial.print(F("[SONAR] ARM ch=12 angle=")); Serial.println(effective, 1);
  } else if (!strcmp(cmd, "SONAR")) {
    char *angleText = strtok(nullptr, " \t"); float requested, effective;
    if (!angleText || strtok(nullptr, " \t") || !parseFloatStrict(angleText, requested) ||
        requested < SONAR_PAN_MIN_DEG || requested > SONAR_PAN_MAX_DEG) {
      Serial.println(F("[ERR] Uso: SONAR <67.5..112.5>.")); return;
    }
    if (mode != CALIBRATION || selectedChannel != SONAR_PAN_CHANNEL ||
        !servos.isOnlyChannelEnabled(SONAR_PAN_CHANNEL)) {
      Serial.println(F("[ERR] Use SONAR_ARM antes de mover la cabeza.")); return;
    }
    if (!calibrationImuSafe() || !servos.setActiveTarget(SONAR_PAN_CHANNEL, requested, effective)) {
      abortToSafeOff(ABORT_IMU_UNSAFE); return;
    }
    refreshCalibrationHostActivity();
    Serial.print(F("[SONAR] angle=")); Serial.println(effective, 1);
  } else if (!strcmp(cmd, "SONAR_OFF")) {
    if (strtok(nullptr, " \t")) { Serial.println(F("[ERR] SONAR_OFF no recibe argumentos.")); return; }
    servos.disable(); motion.stop(); selectedChannel = -1; mode = SAFE_OFF;
    Serial.println(F("[SONAR] OFF PWM=OFF"));
  } else if (!strcmp(cmd, "SONAR_PING")) {
    // Silent watchdog refresh emitted only by the station while SONAR is armed.
    if (mode == CALIBRATION && selectedChannel == SONAR_PAN_CHANNEL) refreshCalibrationHostActivity();
  } else if (!strcmp(cmd, "BEEP")) {
    char *frequencyText = strtok(nullptr, " \t"), *durationText = strtok(nullptr, " \t");
    int frequency, duration;
    if (!frequencyText || !durationText || strtok(nullptr, " \t") ||
        !parseIntStrict(frequencyText, frequency) || !parseIntStrict(durationText, duration) ||
        frequency < 100 || frequency > 4000 || duration < 20 || duration > 1000) {
      Serial.println(F("[ERR] Uso: BEEP <100..4000 Hz> <20..1000 ms>.")); return;
    }
    tone(PIN_BUZZER, (unsigned int)frequency, (unsigned long)duration);
    Serial.print(F("[BEEP] frequency=")); Serial.print(frequency);
    Serial.print(F(" duration=")); Serial.println(duration);
#endif
