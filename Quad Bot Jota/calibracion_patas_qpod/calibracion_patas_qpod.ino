#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// ======= CONFIG =======
Adafruit_PWMServoDriver pca(0x40);

// Ajusta estos valores a tu servo (rango típico 50Hz):
// Si tu servo no llega a 0/180 o hace ruido: ajustamos luego.
static const uint16_t SERVO_MIN = 110;  // pulso ~0°
static const uint16_t SERVO_MAX = 510;  // pulso ~180°

static const uint8_t SERVO_FIRST_CH = 0;
static const uint8_t SERVO_LAST_CH = 15;

static const uint8_t PIN_BUZZER = 3;  // cambia si tu buzzer está en otro pin
static const bool BUZZER_ACTIVE = true;

// ======= ESTADO =======
struct ActiveMove {
  bool active = false;
  uint8_t ch = 0;
  uint32_t offAtMs = 0;
} move;

String line;

// ======= UTILS =======
uint16_t degToPulse(int deg) {
  if (deg < 0) deg = 0;
  if (deg > 180) deg = 180;
  return (uint16_t)map(deg, 0, 180, SERVO_MIN, SERVO_MAX);
}

void allOff() {
  for (uint8_t ch = SERVO_FIRST_CH; ch <= SERVO_LAST_CH; ch++) {
    pca.setPWM(ch, 0, 0);  // OFF
  }
}

void channelOff(uint8_t ch) {
  pca.setPWM(ch, 0, 0);
}

void channelWriteDeg(uint8_t ch, int deg) {
  uint16_t pulse = degToPulse(deg);
  pca.setPWM(ch, 0, pulse);
}

void beep(uint16_t freq = 1800, uint16_t ms = 60) {
  if (!BUZZER_ACTIVE) return;
  tone(PIN_BUZZER, freq, ms);
}

void stopActiveMove() {
  if (move.active) {
    channelOff(move.ch);
    move.active = false;
  }
}

void printHelp() {
  Serial.println(F("\n=== PCA9685 SERVO DEBUG ==="));
  Serial.println(F("Comandos:"));
  Serial.println(F("  000"));
  Serial.println(F("     -> apaga TODOS los canales (sin señal)"));
  Serial.println(F("  CHx angulo segundos"));
  Serial.println(F("     -> ejemplo: CH0 90 10   (mueve CH0 a 90° por 10s y luego OFF)"));
  Serial.println(F("  ALL angulo segundos"));
  Serial.println(F("     -> mueve TODOS por X segundos y luego OFF (opcional)"));
  Serial.println(F("  BEEP freq ms"));
  Serial.println(F("     -> ejemplo: BEEP 2000 80"));
  Serial.println(F("Notas: canales 0..15, angulo 0..180, segundos 1..600"));
  Serial.println(F("===========================\n"));
}

bool startsWithIgnoreCase(const String &s, const char *prefix) {
  String p(prefix);
  String a = s;
  a.toUpperCase();
  p.toUpperCase();
  return a.startsWith(p);
}

// ======= SETUP/LOOP =======
void setup() {
  Serial.begin(115200);
  while (!Serial) { /* nada */
  }

  pinMode(PIN_BUZZER, OUTPUT);
  noTone(PIN_BUZZER);

  Wire.begin();
  pca.begin();
  pca.setPWMFreq(50);
  delay(50);

  allOff();
  beep(1500, 80);

  Serial.println(F("OK: PCA9685 listo. Todos los canales OFF."));
  printHelp();
}

void loop() {
  // 1) terminar movimiento cuando expire
  if (move.active && (int32_t)(millis() - move.offAtMs) >= 0) {
    channelOff(move.ch);
    move.active = false;
    beep(1200, 50);
    Serial.print(F("DONE: CH"));
    Serial.print(move.ch);
    Serial.println(F(" OFF"));
  }

  // 2) leer línea completa
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      line.trim();
      if (line.length() > 0) {
        // Procesar comando
        if (line == "000") {
          stopActiveMove();
          allOff();
          beep(900, 80);
          Serial.println(F("ALL OFF (sin señal)."));
        } else if (startsWithIgnoreCase(line, "CH")) {
          // Formato: CHx angulo segundos
          // Ej: CH12 90 10
          int sp1 = line.indexOf(' ');
          if (sp1 < 0) {
            Serial.println(F("ERR: falta angulo/segundos"));
            line = "";
            break;
          }

          String chStr = line.substring(2, sp1);
          chStr.trim();
          int ch = chStr.toInt();

          int sp2 = line.indexOf(' ', sp1 + 1);
          if (sp2 < 0) {
            Serial.println(F("ERR: falta segundos"));
            line = "";
            break;
          }

          int ang = line.substring(sp1 + 1, sp2).toInt();
          int sec = line.substring(sp2 + 1).toInt();

          if (ch < 0 || ch > 15) {
            Serial.println(F("ERR: canal fuera de 0..15"));
            line = "";
            break;
          }
          if (ang < 0 || ang > 180) {
            Serial.println(F("ERR: angulo fuera de 0..180"));
            line = "";
            break;
          }
          if (sec < 1) sec = 1;
          if (sec > 600) sec = 600;

          // Apagar todo SIEMPRE, luego activar solo ese canal
          stopActiveMove();
          allOff();

          channelWriteDeg((uint8_t)ch, ang);
          move.active = true;
          move.ch = (uint8_t)ch;
          move.offAtMs = millis() + (uint32_t)sec * 1000UL;

          beep(2000, 60);
          Serial.print(F("MOVE: CH"));
          Serial.print(ch);
          Serial.print(F(" -> "));
          Serial.print(ang);
          Serial.print(F(" deg por "));
          Serial.print(sec);
          Serial.println(F(" s (luego OFF)"));
        } else if (startsWithIgnoreCase(line, "ALL")) {
          // Formato: ALL angulo segundos
          int sp1 = line.indexOf(' ');
          int sp2 = line.indexOf(' ', sp1 + 1);
          if (sp1 < 0 || sp2 < 0) {
            Serial.println(F("ERR: ALL angulo segundos"));
            line = "";
            break;
          }
          int ang = line.substring(sp1 + 1, sp2).toInt();
          int sec = line.substring(sp2 + 1).toInt();
          if (ang < 0 || ang > 180) {
            Serial.println(F("ERR: angulo fuera de 0..180"));
            line = "";
            break;
          }
          if (sec < 1) sec = 1;
          if (sec > 60) sec = 60;  // ALL limitado por seguridad

          stopActiveMove();
          // mover todos
          for (uint8_t ch = 0; ch <= 15; ch++) channelWriteDeg(ch, ang);
          beep(1800, 80);
          Serial.print(F("ALL -> "));
          Serial.print(ang);
          Serial.print(F(" deg por "));
          Serial.print(sec);
          Serial.println(F(" s"));

          delay((uint32_t)sec * 1000UL);
          allOff();
          beep(1200, 60);
          Serial.println(F("ALL OFF"));
        } else if (startsWithIgnoreCase(line, "BEEP")) {
          // Formato: BEEP freq ms
          int sp1 = line.indexOf(' ');
          int sp2 = line.indexOf(' ', sp1 + 1);
          if (sp1 < 0 || sp2 < 0) {
            Serial.println(F("ERR: BEEP freq ms"));
            line = "";
            break;
          }
          int f = line.substring(sp1 + 1, sp2).toInt();
          int ms = line.substring(sp2 + 1).toInt();
          if (f < 50) f = 50;
          if (f > 8000) f = 8000;
          if (ms < 10) ms = 10;
          if (ms > 1000) ms = 1000;
          beep((uint16_t)f, (uint16_t)ms);
          Serial.println(F("OK BEEP"));
        } else if (line == "HELP" || line == "help" || line == "?") {
          printHelp();
        } else {
          Serial.println(F("ERR: comando no reconocido. Escribe HELP."));
        }
      }
      line = "";
    } else {
      line += c;
      // evita que crezca infinito
      if (line.length() > 80) line.remove(0, 40);
    }
  }
}
