# 🔌 GUÍA INTEGRACIÓN - PROTOCOLO SERIAL EN test_gait.ino

## ⚠️ ANTES DE EMPEZAR
- Guarda una copia de `test_gait.ino` (backup)
- Lee esta guía COMPLETA antes de empezar
- Sigue exactamente los pasos

---

## 📋 PASO 1: Añadir INCLUDES

Abre tu archivo `test_gait.ino` y busca la sección de includes al inicio (líneas 1-10):

```cpp
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <math.h>
```

**Añade esta línea al final de los includes:**

```cpp
#include <EEPROM.h>
```

Resultado:
```cpp
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <math.h>
#include <EEPROM.h>    // ← NUEVA
```

---

## 📋 PASO 2: Definiciones EEPROM

Encuentra la sección donde están definidas las constantes (PWM_FREQ, SERVO_MIN, etc). 

Busca esta línea:
```cpp
const int SERVO_MAX = 510;
```

Justo **después** de esa línea, inserta TODO esto:

```cpp
// =====================================================
// EEPROM MANAGER
// =====================================================
#include <EEPROM.h>

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
EEPROMData eepromData;

void eeprom_init() {
  EEPROM.get(EEPROM_BASE, eepromData);
  if (eepromData.magic != 0xAA) {
    Serial.println("[EEPROM] No válido, inicializando...");
    for (int i = 0; i < 13; i++) {
      eepromData.angleA[i] = angleA[i];
      eepromData.angleCenter[i] = angleCenter[i];
      eepromData.angleB[i] = angleB[i];
    }
    eepromData.standPose_coxaGain = 0.45f;
    eepromData.standPose_heightGain = 0.70f;
    eepromData.gait_coxaGain = 0.28f;
    eepromData.gait_liftGain = 0.30f;
    eepromData.gait_liftMs = GAIT_LIFT_MS;
    eepromData.gait_swingMs = GAIT_SWING_MS;
    eepromData.gait_dropMs = GAIT_DROP_MS;
    eepromData.gait_pushMs = GAIT_PUSH_MS;
    eeprom_write();
  }
  Serial.println("[EEPROM] ✓ Inicializado");
}

void eeprom_write() {
  eepromData.magic = 0xAA;
  EEPROM.put(EEPROM_BASE, eepromData);
  Serial.println("[EEPROM] ✓ Guardado");
}
```

---

## 📋 PASO 3: Protocolo Serial

Después del código de EEPROM anterior, añade esta nueva función:

```cpp
// =====================================================
// PROTOCOLO SERIAL
// =====================================================
void serial_process() {
  while (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;
    
    int spaceIdx = line.indexOf(' ');
    String cmd = (spaceIdx > 0) ? line.substring(0, spaceIdx) : line;
    String args = (spaceIdx > 0) ? line.substring(spaceIdx + 1) : "";
    
    // Calibración
    if (cmd == "A") {
      int ch = args.toInt();
      int angle = args.substring(args.indexOf(' ') + 1).toInt();
      if (ch >= 0 && ch <= 12) {
        angleA[ch] = angle;
        targetAngle[ch] = angle;
        Serial.print("[A] CH");
        Serial.print(ch);
        Serial.print(" = ");
        Serial.println(angle);
      }
    }
    else if (cmd == "C") {
      int ch = args.toInt();
      int angle = args.substring(args.indexOf(' ') + 1).toInt();
      if (ch >= 0 && ch <= 12) {
        angleCenter[ch] = angle;
        targetAngle[ch] = angle;
        Serial.print("[C] CH");
        Serial.print(ch);
        Serial.print(" = ");
        Serial.println(angle);
      }
    }
    else if (cmd == "B") {
      int ch = args.toInt();
      int angle = args.substring(args.indexOf(' ') + 1).toInt();
      if (ch >= 0 && ch <= 12) {
        angleB[ch] = angle;
        targetAngle[ch] = angle;
        Serial.print("[B] CH");
        Serial.print(ch);
        Serial.print(" = ");
        Serial.println(angle);
      }
    }
    
    // EEPROM
    else if (cmd == "EEPROM_A") {
      int ch = args.toInt();
      int angle = args.substring(args.indexOf(' ') + 1).toInt();
      if (ch >= 0 && ch <= 12) eepromData.angleA[ch] = angle;
    }
    else if (cmd == "EEPROM_C") {
      int ch = args.toInt();
      int angle = args.substring(args.indexOf(' ') + 1).toInt();
      if (ch >= 0 && ch <= 12) eepromData.angleCenter[ch] = angle;
    }
    else if (cmd == "EEPROM_B") {
      int ch = args.toInt();
      int angle = args.substring(args.indexOf(' ') + 1).toInt();
      if (ch >= 0 && ch <= 12) eepromData.angleB[ch] = angle;
    }
    else if (cmd == "EEPROM_SAVE") {
      eeprom_write();
      // Cargar en runtime
      for (int i = 0; i < 13; i++) {
        angleA[i] = eepromData.angleA[i];
        angleCenter[i] = eepromData.angleCenter[i];
        angleB[i] = eepromData.angleB[i];
      }
      Serial.println("[EEPROM] ✓ GUARDADO");
    }
    
    // Consultas
    else if (cmd == "STATE") {
      Serial.print("STATE: NRF=");
      Serial.print(linkAlive ? "OK" : "LOST");
      Serial.print(" PCA=OK MODE=");
      Serial.println(rxData.mode3);
    }
    else if (cmd == "ANGLES") {
      Serial.print("SERVO: ");
      for (int i = 0; i < 13; i++) {
        Serial.print(i);
        Serial.print("=");
        Serial.print((int)currentAngle[i]);
        if (i < 12) Serial.print(" ");
      }
      Serial.println();
    }
    else if (cmd == "INFO") {
      Serial.println("Q-POD MINI - Comandos:");
      Serial.println(" A/C/B <ch> <ang> - Set ángulos");
      Serial.println(" STATE - Estado");
      Serial.println(" ANGLES - Ángulos actuales");
      Serial.println(" EEPROM_SAVE - Guardar");
    }
  }
}
```

---

## 📋 PASO 4: Modificar setup()

Encuentra tu función `void setup()` y busca esta línea:

```cpp
buildPoseTables();
buildStandPose();
```

**Justo DESPUÉS de esas líneas**, añade:

```cpp
  // EEPROM
  eeprom_init();
```

Ejemplo completo (solo mostramos contexto):
```cpp
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
  
  // ← AÑADE ESTA LÍNEA
  eeprom_init();
  // ← FIN AÑADIDA

  for (int ch = FIRST_CH; ch <= LAST_CH; ch++) {
    targetAngle[ch] = standPose[ch];
    currentAngle[ch] = standPose[ch];
  }
  
  // ... resto del setup ...
}
```

---

## 📋 PASO 5: Modificar loop()

Encuentra tu función `void loop()` y ve al **final** de la función, justo antes del cierre `}`.

Busca algo como esto:
```cpp
void loop() {
  if (radio.available()) {
    // ... procesamiento NRF ...
  }

  if (linkAlive && (millis() - lastPacketMs > 800)) {
    // ... timeout ...
  }

  if (millis() - lastMotionMs >= MOTION_PERIOD_MS) {
    // ... actualización de movimiento ...
  }
}  // ← El cierre de la función
```

**Justo ANTES del cierre `}` final**, añade:

```cpp
  // Procesar comandos serial
  serial_process();
```

Ejemplo:
```cpp
void loop() {
  if (radio.available()) {
    while (radio.available()) {
      radio.read(&rxData, sizeof(rxData));
    }
    // ... resto del procesamiento ...
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
      float step = (rxData.mode3 == 0) ? SPEED_CALIB :
                   (rxData.mode3 == 1) ? SPEED_MANUAL :
                                         SPEED_GAIT;
      updateMotion(step);
    }
  }

  // ← AÑADE ESTA LÍNEA
  serial_process();
  // ← FIN AÑADIDA
}
```

---

## ✅ VERIFICACIÓN

Una vez añadido TODO, compila el código:

```
Arduino IDE → Verify (Ctrl+R)
```

Si NO hay errores → ¡ÉXITO! 🎉

Si hay errores:
- Verifica que NO duplicaste `#include <EEPROM.h>`
- Comprueba que las llaves `{}` están correctas
- Asegúrate de que copiaste EXACTAMENTE el código

---

## 🚀 PRUEBA

1. Sube el código a Arduino Nano
2. Abre PowerShell:
```powershell
cd "f:\Documentos\Jota\jRobot\mini Q-Pod"
python quickstart_config.py
```
3. Selecciona tu puerto COM
4. Cuando aparezca el menú, presiona `s` para confirmar
5. Debería ver el progreso de carga

Si todo va bien:
```
✓ ÉXITO - Puedes desenchufar y enchufar el Arduino
  Los parámetros se guardarán en EEPROM
```

---

## 🎛️ PRÓXIMO PASO

Una vez verificado que funciona:

```powershell
python gui_qpod_config.py
```

Esto abre la interfaz gráfica completa para calibración avanzada.

---

## ❓ SI ALGO FALLA

### Error: "struct EEPROMData"
**Causa:** Copiaste el código en lugar equivocado  
**Solución:** Busca exactamente dónde ir en PASO 2

### Error: "eeprom_init not defined"
**Causa:** No copiaste la función completa  
**Solución:** Verifica que toda la función esté presente

### Arduino no responde en Python
**Causa:** Falta `serial_process()` en loop()  
**Solución:** Verifica PASO 5

### Compila pero no ve cambios
**Causa:** Arduino no reinicia después de programación  
**Solución:** Desenchufa/enchúfa manualmente

---

## 💾 GUARDADO

Una vez compilado y funcionando, tu robot **recordará automáticamente** los parámetros incluso sin alimentación.

¡Éxito! 🚀
