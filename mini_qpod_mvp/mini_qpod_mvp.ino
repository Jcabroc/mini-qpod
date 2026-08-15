#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "robot_config.h"
#include "servo_control.h"
#include "gait_balance.h"

enum RobotMode : uint8_t { SAFE_OFF, CALIBRATION, STAND_MODE, LEG_TEST, WALK_MODE };

Adafruit_PWMServoDriver pca(PCA9685_ADDRESS);
ServoController servos(pca);
ImuManager imu;
GaitBalance motion(servos, imu);

RobotMode mode = SAFE_OFF;
bool walkUnlocked = WALK_ENABLED_AT_BOOT;
char inputLine[80];
uint8_t inputLength = 0;

void printHelp();
void processCommand(char *line);

void setup() {
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  Serial.begin(115200);
  Serial.setTimeout(25);
  Wire.begin();
  pca.begin();
  pca.setPWMFreq(SERVO_PWM_HZ);
  delay(10);
  servos.begin();
  servos.disable();
  bool imuOk = imu.begin();
  Serial.println(F("\nMINI Q-POD MVP v0.2"));
  Serial.print(F("Enlace IMU Pico UART: "));
  Serial.println(IMU_ENABLED ? (imuOk ? F("ESPERANDO DATOS") : F("ERROR")) : F("DESACTIVADO"));
  Serial.println(F("Arranque seguro: servos OFF. Escriba HELP."));
}

void loop() {
  // Mantener la telemetria disponible tambien en SAFE_OFF para poder validar
  // la IMU con el comando IMU antes de energizar los servos.
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

  if (mode == LEG_TEST || mode == WALK_MODE || mode == STAND_MODE) {
    if (!motion.update()) {
      mode = SAFE_OFF;
      servos.disable();
      Serial.println(F("[ABORT] IMU o inclinacion insegura. Servos OFF."));
    } else if (mode == LEG_TEST && motion.phase() == GaitBalance::IDLE) {
      mode = STAND_MODE;
      Serial.println(F("[LEG] Prueba terminada."));
    }
  }
  servos.update();
}

void processCommand(char *line) {
  char *cmd = strtok(line, " ");
  if (!cmd) return;
  for (char *p = cmd; *p; ++p) *p = toupper(*p);

  if (!strcmp(cmd, "HELP")) printHelp();
  else if (!strcmp(cmd, "OFF")) {
    motion.stop(); servos.disable(); mode = SAFE_OFF;
    Serial.println(F("[OFF] Servos desenergizados."));
  } else if (!strcmp(cmd, "CENTER")) {
    if (mode != CALIBRATION) Serial.println(F("[ERR] Use CALIB antes de CENTER."));
    else { servos.setAllCenters(); Serial.println(F("[CENTER] Objetivos centrados.")); }
  } else if (!strcmp(cmd, "CALIB")) {
    servos.enable(); servos.setAllCenters(); mode = CALIBRATION;
    Serial.println(F("[CALIB] Robot elevado: use SERVO o LIMITS. OFF para abortar."));
  } else if (!strcmp(cmd, "SERVO")) {
    char *a = strtok(nullptr, " "), *b = strtok(nullptr, " ");
    if (mode != CALIBRATION || !a || !b) Serial.println(F("[ERR] Uso en CALIB: SERVO <0..12> <angulo>."));
    else if (!servos.setTarget(atoi(a), atof(b))) Serial.println(F("[ERR] Servo o valor invalido."));
    else Serial.println(F("[SERVO] Objetivo aceptado y limitado."));
  } else if (!strcmp(cmd, "LIMITS")) {
    char *a = strtok(nullptr, " "), *b = strtok(nullptr, " ");
    char *c = strtok(nullptr, " "), *d = strtok(nullptr, " ");
    if (mode != CALIBRATION || !a || !b || !c || !d) Serial.println(F("[ERR] Uso: LIMITS <ch> <min> <center> <max>."));
    else if (!servos.setCalibration(atoi(a), atoi(b), atoi(c), atoi(d))) Serial.println(F("[ERR] Limites incoherentes; no aplicados."));
    else Serial.println(F("[LIMITS] Aplicados en RAM. Use SAVE para persistir."));
  } else if (!strcmp(cmd, "SAVE")) {
    Serial.println(servos.save() ? F("[SAVE] EEPROM OK.") : F("[ERR] Configuracion invalida."));
  } else if (!strcmp(cmd, "LOAD")) {
    Serial.println(servos.load() ? F("[LOAD] EEPROM OK.") : F("[ERR] EEPROM invalida; sin cambios."));
  } else if (!strcmp(cmd, "DEFAULTS")) {
    if (mode != SAFE_OFF) Serial.println(F("[ERR] Ejecute OFF antes de DEFAULTS."));
    else { servos.loadDefaults(); Serial.println(F("[DEFAULTS] En RAM; use CALIB para probar y SAVE para guardar.")); }
  } else if (!strcmp(cmd, "STAND")) {
    servos.enable(); motion.stand(); mode = STAND_MODE;
    Serial.println(F("[STAND] Moviendo lentamente a postura base."));
  } else if (!strcmp(cmd, "LEG")) {
    char *a = strtok(nullptr, " ");
    if (mode != STAND_MODE || !a) Serial.println(F("[ERR] Primero STAND; uso LEG <0..3>."));
    else if (!motion.startLegTest(atoi(a))) Serial.println(F("[ERR] Pata invalida u otro movimiento activo."));
    else { mode = LEG_TEST; Serial.println(F("[LEG] Prueba iniciada; listo para OFF.")); }
  } else if (!strcmp(cmd, "UNLOCK_WALK")) {
    walkUnlocked = true;
    Serial.println(F("[WALK] Desbloqueado hasta reiniciar. Confirme que probo las 4 patas."));
  } else if (!strcmp(cmd, "WALK")) {
    if (!walkUnlocked) Serial.println(F("[ERR] Use UNLOCK_WALK tras probar LEG 0..3."));
    else if (mode != STAND_MODE || !motion.startWalk()) Serial.println(F("[ERR] Primero STAND y espere estabilizacion."));
    else { mode = WALK_MODE; Serial.println(F("[WALK] Marcha lenta iniciada. OFF para detener.")); }
  } else if (!strcmp(cmd, "IMU")) {
    Serial.print(F("IMU enabled=")); Serial.print(IMU_ENABLED);
    Serial.print(F(" healthy=")); Serial.print(imu.healthy());
    Serial.print(F(" roll=")); Serial.print(imu.roll(), 2);
    Serial.print(F(" pitch=")); Serial.println(imu.pitch(), 2);
  } else if (!strcmp(cmd, "STATUS")) {
    Serial.print(F("mode=")); Serial.print((int)mode);
    Serial.print(F(" servos=")); Serial.print(servos.enabled());
    Serial.print(F(" phase=")); Serial.print((int)motion.phase());
    Serial.print(F(" leg=")); Serial.println(motion.activeLeg());
  } else if (!strcmp(cmd, "CONFIG")) {
    for (uint8_t i = 0; i < SERVO_COUNT; ++i) {
      const ServoConfig &s = servos.config(i);
      Serial.print(i); Serial.print(':'); Serial.print(s.minAngle); Serial.print(',');
      Serial.print(s.centerAngle); Serial.print(','); Serial.println(s.maxAngle);
    }
  } else Serial.println(F("[ERR] Comando desconocido. Use HELP."));
}

void printHelp() {
  Serial.println(F("OFF | STATUS | CONFIG | IMU"));
  Serial.println(F("CALIB | CENTER | SERVO <ch> <deg> | LIMITS <ch> <min> <center> <max>"));
  Serial.println(F("SAVE | LOAD | DEFAULTS"));
  Serial.println(F("STAND | LEG <0..3> | UNLOCK_WALK | WALK"));
  Serial.println(F("Patas: 0=L1, 1=R1, 2=L2, 3=R2"));
}
