#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

Adafruit_PWMServoDriver pca = Adafruit_PWMServoDriver(0x40);

// ===============================
// CONFIGURACION BASE
// ===============================
const uint8_t SERVO_YAW_CH   = 0;
const uint8_t SERVO_PITCH_CH = 1;

// Rango PWM típico para servos en PCA9685
// Luego si quieres, esto también se puede afinar.
const int SERVO_MIN_PULSE = 110;
const int SERVO_MAX_PULSE = 510;

// Posiciones actuales
int yawAngle = 90;
int pitchAngle = 90;

// Servo seleccionado desde Serial
uint8_t selectedServo = SERVO_YAW_CH;

// Inversión de sentido para prueba
bool invertYaw = false;
bool invertPitch = false;

// ===============================
// FUNCIONES
// ===============================
int angleToPulse(int angle) {
  angle = constrain(angle, 0, 180);
  return map(angle, 0, 180, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
}

void writeServoRaw(uint8_t channel, int angle, bool inverted) {
  angle = constrain(angle, 0, 180);
  if (inverted) {
    angle = 180 - angle;
  }
  int pulse = angleToPulse(angle);
  pca.setPWM(channel, 0, pulse);
}

void writeYaw(int angle) {
  yawAngle = constrain(angle, 0, 180);
  writeServoRaw(SERVO_YAW_CH, yawAngle, invertYaw);
}

void writePitch(int angle) {
  pitchAngle = constrain(angle, 0, 180);
  writeServoRaw(SERVO_PITCH_CH, pitchAngle, invertPitch);
}

void writeBoth(int yaw, int pitch) {
  writeYaw(yaw);
  writePitch(pitch);
}

void printHelp() {
  Serial.println(F("\n========== CALIBRADOR DE SERVOS =========="));
  Serial.println(F("Comandos:"));
  Serial.println(F("  h              -> mostrar ayuda"));
  Serial.println(F("  y              -> seleccionar servo YAW"));
  Serial.println(F("  p              -> seleccionar servo PITCH"));
  Serial.println(F("  c              -> centrar ambos en 90"));
  Serial.println(F("  +              -> mover servo seleccionado +1 grado"));
  Serial.println(F("  -              -> mover servo seleccionado -1 grado"));
  Serial.println(F("  >              -> mover servo seleccionado +5 grados"));
  Serial.println(F("  <              -> mover servo seleccionado -5 grados"));
  Serial.println(F("  0              -> mover servo seleccionado a 0"));
  Serial.println(F("  9              -> mover servo seleccionado a 90"));
  Serial.println(F("  8              -> mover servo seleccionado a 180"));
  Serial.println(F("  i              -> invertir YAW on/off"));
  Serial.println(F("  k              -> invertir PITCH on/off"));
  Serial.println(F("  s              -> mostrar estado actual"));
  Serial.println(F("  aXXX           -> mover servo seleccionado al angulo XXX"));
  Serial.println(F("                   ejemplo: a73"));
  Serial.println(F("==========================================\n"));
}

void printStatus() {
  Serial.println(F("\n--- ESTADO ACTUAL ---"));
  Serial.print(F("Servo seleccionado: "));
  if (selectedServo == SERVO_YAW_CH) Serial.println(F("YAW"));
  else Serial.println(F("PITCH"));

  Serial.print(F("Yaw angle   = "));
  Serial.println(yawAngle);

  Serial.print(F("Pitch angle = "));
  Serial.println(pitchAngle);

  Serial.print(F("Invert YAW  = "));
  Serial.println(invertYaw ? F("SI") : F("NO"));

  Serial.print(F("Invert PITCH= "));
  Serial.println(invertPitch ? F("SI") : F("NO"));
  Serial.println(F("---------------------\n"));
}

void moveSelectedTo(int angle) {
  if (selectedServo == SERVO_YAW_CH) {
    writeYaw(angle);
    Serial.print(F("YAW -> "));
    Serial.println(yawAngle);
  } else {
    writePitch(angle);
    Serial.print(F("PITCH -> "));
    Serial.println(pitchAngle);
  }
}

void moveSelectedBy(int delta) {
  if (selectedServo == SERVO_YAW_CH) {
    writeYaw(yawAngle + delta);
    Serial.print(F("YAW -> "));
    Serial.println(yawAngle);
  } else {
    writePitch(pitchAngle + delta);
    Serial.print(F("PITCH -> "));
    Serial.println(pitchAngle);
  }
}

// ===============================
// SETUP
// ===============================
void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  pca.begin();
  pca.setPWMFreq(50);
  delay(500);

  writeBoth(90, 90);

  Serial.println(F("Calibrador iniciado."));
  printHelp();
  printStatus();
}

// ===============================
// LOOP
// ===============================
void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.length() == 0) return;

    if (cmd == "h") {
      printHelp();
    }
    else if (cmd == "y") {
      selectedServo = SERVO_YAW_CH;
      Serial.println(F("Servo seleccionado: YAW"));
    }
    else if (cmd == "p") {
      selectedServo = SERVO_PITCH_CH;
      Serial.println(F("Servo seleccionado: PITCH"));
    }
    else if (cmd == "c") {
      writeBoth(90, 90);
      Serial.println(F("Ambos servos centrados en 90"));
    }
    else if (cmd == "+") {
      moveSelectedBy(+1);
    }
    else if (cmd == "-") {
      moveSelectedBy(-1);
    }
    else if (cmd == ">") {
      moveSelectedBy(+5);
    }
    else if (cmd == "<") {
      moveSelectedBy(-5);
    }
    else if (cmd == "0") {
      moveSelectedTo(0);
    }
    else if (cmd == "9") {
      moveSelectedTo(90);
    }
    else if (cmd == "8") {
      moveSelectedTo(180);
    }
    else if (cmd == "i") {
      invertYaw = !invertYaw;
      writeYaw(yawAngle);
      Serial.print(F("Invert YAW = "));
      Serial.println(invertYaw ? F("SI") : F("NO"));
    }
    else if (cmd == "k") {
      invertPitch = !invertPitch;
      writePitch(pitchAngle);
      Serial.print(F("Invert PITCH = "));
      Serial.println(invertPitch ? F("SI") : F("NO"));
    }
    else if (cmd == "s") {
      printStatus();
    }
    else if (cmd.startsWith("a")) {
      int angle = cmd.substring(1).toInt();
      moveSelectedTo(angle);
    }
    else {
      Serial.println(F("Comando no reconocido. Escribe h para ayuda."));
    }
  }
}