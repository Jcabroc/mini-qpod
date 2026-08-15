#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// ==========================
// Pines Nano RX
// ==========================
const uint8_t PIN_LED    = 5;   // LED estado/radio
const uint8_t PIN_BUZZER = 4;   // buzzer pasivo

const uint8_t PIN_CE  = 9;
const uint8_t PIN_CSN = 10;

// ==========================
// NRF24
// ==========================
RF24 radio(PIN_CE, PIN_CSN);
const uint8_t ADDR[6] = "JR001";   // Igual al TX

// ==========================
// PS2J v1 (igual al TX)
// ==========================
enum PS2J_Button : uint16_t {
  PS2J_SELECT   = (1u << 0),
  PS2J_START    = (1u << 1),
  PS2J_SQUARE   = (1u << 2),
  PS2J_CROSS    = (1u << 3),
  PS2J_CIRCLE   = (1u << 4),
  PS2J_TRIANGLE = (1u << 5),
  PS2J_R3       = (1u << 6),
};

struct PS2J_Packet {
  int8_t lx;
  int8_t ly;
  int8_t rx;
  int8_t ry;
  uint16_t btn;
  uint8_t mode3;
  uint8_t seq;
  uint8_t flags;
};

PS2J_Packet rxData;

// ==========================
// Estado link
// ==========================
uint32_t lastPacketMs = 0;
bool linkAlive = false;

// ==========================
// Buzzer
// ==========================
void beep(uint16_t freq, uint16_t ms) {
  tone(PIN_BUZZER, freq, ms);
}

bool btnPressed(uint16_t mask) {
  return (rxData.btn & mask) != 0;
}

void printButtons(uint16_t b) {
  if (b == 0) {
    Serial.print("NONE");
    return;
  }

  bool first = true;
  auto printOne = [&](const char* name) {
    if (!first) Serial.print(",");
    Serial.print(name);
    first = false;
  };

  if (b & PS2J_SELECT)   printOne("SELECT");
  if (b & PS2J_START)    printOne("START");
  if (b & PS2J_SQUARE)   printOne("SQUARE");
  if (b & PS2J_CROSS)    printOne("CROSS");
  if (b & PS2J_CIRCLE)   printOne("CIRCLE");
  if (b & PS2J_TRIANGLE) printOne("TRIANGLE");
  if (b & PS2J_R3)       printOne("R3");
}

void setup() {
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  digitalWrite(PIN_LED, LOW);
  noTone(PIN_BUZZER);

  Serial.begin(115200);
  delay(600);

  Serial.println();
  Serial.println("=== Q-POD MINI RX TEST (PS2J) ===");
  Serial.println("Nano + NRF24 + LED + Buzzer");

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

  radio.setChannel(76);                 // Igual al TX
  radio.setDataRate(RF24_250KBPS);      // Igual al TX
  radio.setPALevel(RF24_PA_LOW);        // Igual al TX
  radio.setAutoAck(true);
  radio.openReadingPipe(1, ADDR);
  radio.startListening();

  Serial.println("NRF24 RX listo. Esperando paquetes...");
  beep(1800, 100);
  delay(150);
  beep(2400, 100);
}

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

    // --------- Prueba funcional ----------
    // CROSS    -> LED ON
    // CIRCLE   -> beep corto
    // TRIANGLE -> LED + beep
    // SQUARE   -> LED OFF
    // START    -> beep grave
    // SELECT   -> beep agudo

    if (btnPressed(PS2J_CROSS)) {
      digitalWrite(PIN_LED, HIGH);
    }

    if (btnPressed(PS2J_SQUARE)) {
      digitalWrite(PIN_LED, LOW);
    }

    if (btnPressed(PS2J_CIRCLE)) {
      beep(1800, 80);
    }

    if (btnPressed(PS2J_TRIANGLE)) {
      digitalWrite(PIN_LED, HIGH);
      beep(2400, 120);
    }

    if (btnPressed(PS2J_START)) {
      beep(900, 120);
    }

    if (btnPressed(PS2J_SELECT)) {
      beep(3000, 120);
    }

    if (btnPressed(PS2J_R3)) {
      digitalWrite(PIN_LED, !digitalRead(PIN_LED));
      beep(1400, 60);
      delay(120); // pequeño antirebote bruto
    }

    // Debug serial
    Serial.print("seq=");
    Serial.print(rxData.seq);
    Serial.print(" mode=");
    Serial.print(rxData.mode3);
    Serial.print(" rx=");
    Serial.print(rxData.rx);
    Serial.print(" ry=");
    Serial.print(rxData.ry);
    Serial.print(" btn=0x");
    Serial.print(rxData.btn, HEX);
    Serial.print(" [");
    printButtons(rxData.btn);
    Serial.println("]");
  }

  // Timeout de link
  if (linkAlive && (millis() - lastPacketMs > 800)) {
    linkAlive = false;
    digitalWrite(PIN_LED, LOW);
    Serial.println("LINK LOST");
    beep(700, 140);
  }
}