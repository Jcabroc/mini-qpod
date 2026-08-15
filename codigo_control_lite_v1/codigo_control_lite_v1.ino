#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>

// ================= NRF =================
RF24 radio(9, 10);                // CE, CSN
const uint8_t ADDR[6] = "JR001";  // Igual en RX

// ================= PINS CONTROL LITE =================
// Stick físico (lo tratamos como stick DERECHO)
static const uint8_t PIN_RX = A0;
static const uint8_t PIN_RY = A1;

// Click del stick (R3)
static const uint8_t PIN_R3 = 2;

// 4 botones “colores” del shield -> PS2
static const uint8_t PIN_TRIANGLE = 3;
static const uint8_t PIN_CIRCLE = 4;
static const uint8_t PIN_CROSS = 5;
static const uint8_t PIN_SQUARE = 6;

// Start / Select
static const uint8_t PIN_START = 7;
static const uint8_t PIN_SELECT = 8;

// Switch 3 posiciones (A2/A3 + GND)
static const uint8_t PIN_MODE_A = A2;
static const uint8_t PIN_MODE_B = A3;

// LED estado + buzzer pasivo
static const uint8_t PIN_LED_LINK = A4;  // resistencia 330–470 ohm
static const uint8_t PIN_BUZZER = A5;    // buzzer pasivo

// ================= AJUSTES =================
static const bool BUTTONS_ACTIVE_LOW = false;  // tu shield: suelto=0 presionado=1
static const uint8_t DEADZONE_PERCENT = 6;     // 4..10 típico
static const uint8_t FILTER_STRENGTH = 4;      // 2..8 (más alto = más suave)

static const uint16_t SEND_PERIOD_MS = 20;     // 50 Hz
static const uint16_t LQ_WINDOW_MS = 1000;     // link quality por ACK
static const uint16_t SERIAL_PERIOD_MS = 120;  // debug

// ================= PS2J v1 =================
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
  int8_t lx;      // Lite no tiene -> 0
  int8_t ly;      // Lite no tiene -> 0
  int8_t rx;      // tu stick X
  int8_t ry;      // tu stick Y
  uint16_t btn;   // 7 botones en bits
  uint8_t mode3;  // switch 0/1/2
  uint8_t seq;    // contador
  uint8_t flags;  // 1=Lite (opcional)
};

static const uint8_t PS2J_FLAG_LITE = 1;

// ================= INTERNALS =================
static int16_t filtX = 512, filtY = 512;

static uint32_t lastSendMs = 0, lastLQMs = 0, lastSerialMs = 0;
static uint16_t sent = 0, acked = 0;
static uint8_t lastLQ = 0;
static bool linkOk = false, prevLinkOk = false;

static uint32_t ledBlinkMs = 0;
static bool ledState = false;

static bool buzzerPlaying = false;
static uint32_t buzzerOffMs = 0;

static uint8_t seqCounter = 0;
static uint8_t prevMode = 255;

// ================= HELPERS =================
static inline bool pressed(uint8_t pin) {
  uint8_t v = digitalRead(pin);
  return BUTTONS_ACTIVE_LOW ? (v == LOW) : (v == HIGH);
}

static inline int16_t readFilteredAnalog(uint8_t pin, int16_t &accum) {
  int16_t raw = analogRead(pin);
  accum = accum + (raw - accum) / (int16_t)FILTER_STRENGTH;
  return accum;
}

static inline int8_t mapAxisToInt8(int16_t adc) {
  int16_t centered = adc - 512;
  int16_t dz = (int32_t)1023 * DEADZONE_PERCENT / 100 / 2;
  if (centered > -dz && centered < dz) centered = 0;

  int32_t scaled = (int32_t)centered * 127 / 512;
  if (scaled > 127) scaled = 127;
  if (scaled < -127) scaled = -127;
  return (int8_t)scaled;
}

static inline uint8_t readMode3pos() {
  // INPUT_PULLUP + centro del switch a GND:
  bool a = (digitalRead(PIN_MODE_A) == LOW);
  bool b = (digitalRead(PIN_MODE_B) == LOW);
  if (a && !b) return 0;
  if (!a && b) return 2;
  return 1;
}

static inline bool pressedR3() {
  return digitalRead(PIN_R3) == LOW;  // con INPUT_PULLUP, LOW = apretado
}

static uint16_t readButtonsPS2J() {
  uint16_t b = 0;
  if (pressed(PIN_SELECT)) b |= PS2J_SELECT;
  if (pressed(PIN_START)) b |= PS2J_START;
  if (pressed(PIN_SQUARE)) b |= PS2J_SQUARE;
  if (pressed(PIN_CROSS)) b |= PS2J_CROSS;
  if (pressed(PIN_CIRCLE)) b |= PS2J_CIRCLE;
  if (pressed(PIN_TRIANGLE)) b |= PS2J_TRIANGLE;


  if (pressedR3()) b |= PS2J_R3;  // <- SOLO R3 distinto
  //if (pressed(PIN_R3)) b |= PS2J_R3;
  return b;
}

// ---- buzzer pasivo ----
static void beep(uint16_t freq, uint16_t ms) {
  tone(PIN_BUZZER, freq);
  buzzerPlaying = true;
  buzzerOffMs = millis() + ms;
}

static void buzzerUpdate() {
  if (buzzerPlaying && (int32_t)(millis() - buzzerOffMs) >= 0) {
    noTone(PIN_BUZZER);
    buzzerPlaying = false;
  }
}

// ---- LED link real (ACK) ----
static void ledLinkUpdate(uint8_t lq, bool ok) {
  uint32_t now = millis();

  if (ok && lq >= 90) {  // link sólido
    digitalWrite(PIN_LED_LINK, HIGH);
    return;
  }

  uint16_t period = !ok ? 120 : (lq >= 70 ? 500 : (lq >= 40 ? 250 : 150));
  if (now - ledBlinkMs >= period) {
    ledBlinkMs = now;
    ledState = !ledState;
    digitalWrite(PIN_LED_LINK, ledState ? HIGH : LOW);
  }
}

void setup() {
  Serial.begin(115200);

  // Botones (ACTIVE HIGH)
  pinMode(PIN_R3, INPUT_PULLUP);
  pinMode(PIN_TRIANGLE, INPUT);
  pinMode(PIN_CIRCLE, INPUT);
  pinMode(PIN_CROSS, INPUT);
  pinMode(PIN_SQUARE, INPUT);
  pinMode(PIN_START, INPUT);
  pinMode(PIN_SELECT, INPUT);

  // Switch
  pinMode(PIN_MODE_A, INPUT_PULLUP);
  pinMode(PIN_MODE_B, INPUT_PULLUP);

  // LED + buzzer
  pinMode(PIN_LED_LINK, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_LED_LINK, LOW);
  noTone(PIN_BUZZER);

  // Filtros
  filtX = analogRead(PIN_RX);
  filtY = analogRead(PIN_RY);

  // NRF
  radio.begin();
  radio.setChannel(76);
  radio.setDataRate(RF24_250KBPS);
  radio.setPALevel(RF24_PA_LOW);
  radio.setRetries(5, 10);
  radio.openWritingPipe(ADDR);
  radio.stopListening();

  beep(1800, 80);
  Serial.println(F("=== CONTROL LITE TX PS2J v1 ==="));
}

void loop() {
  uint32_t now = millis();

  // Envío
  if (now - lastSendMs >= SEND_PERIOD_MS) {
    lastSendMs = now;

    PS2J_Packet p;
    p.lx = 0;
    p.ly = 0;
    p.rx = mapAxisToInt8(readFilteredAnalog(PIN_RX, filtX));
    p.ry = mapAxisToInt8(readFilteredAnalog(PIN_RY, filtY));
    p.btn = readButtonsPS2J();
    p.mode3 = readMode3pos();
    p.seq = seqCounter++;
    p.flags = PS2J_FLAG_LITE;

    bool ok = radio.write(&p, sizeof(p));
    sent++;
    if (ok) acked++;

    // beep cambio de modo
    if (p.mode3 != prevMode) {
      prevMode = p.mode3;
      if (p.mode3 == 0) beep(900, 60);
      else if (p.mode3 == 1) beep(1400, 60);
      else beep(2000, 60);
    }
  }

  // Link quality
  if (now - lastLQMs >= LQ_WINDOW_MS) {
    lastLQMs = now;
    lastLQ = (sent > 0) ? (uint32_t)acked * 100 / sent : 0;
    linkOk = (acked > 0);

    // beep perdió/recuperó link
    if (linkOk != prevLinkOk) {
      if (linkOk) beep(1600, 70);
      else beep(800, 120);
      prevLinkOk = linkOk;
    }

    sent = 0;
    acked = 0;
  }

  ledLinkUpdate(lastLQ, linkOk);

  // Serial debug (opcional)
  if (now - lastSerialMs >= SERIAL_PERIOD_MS) {
    lastSerialMs = now;
    Serial.print(F("LQ="));
    Serial.print(lastLQ);
    Serial.print(F("% link="));
    Serial.print(linkOk ? F("OK") : F("NO"));
    Serial.print(F(" rx="));
    Serial.print(mapAxisToInt8(filtX));
    Serial.print(F(" ry="));
    Serial.print(mapAxisToInt8(filtY));
    Serial.print(F(" mode="));
    Serial.print(prevMode);
    Serial.print(F(" btn=0x"));
    Serial.println(readButtonsPS2J(), HEX);
  }

  buzzerUpdate();
}
