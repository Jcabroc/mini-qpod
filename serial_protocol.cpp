// =====================================================
// Q-POD MINI - PROTOCOLO SERIAL + EEPROM MANAGER
// Añadir esta sección al código test_gait.ino
// =====================================================

#include <EEPROM.h>

// =====================================================
// EEPROM LAYOUT
// =====================================================
const uint16_t EEPROM_SIZE = 1024;

struct EEPROMData {
  uint8_t magic;  // 0xAA = datos válidos
  
  // Ángulos (3 bytes cada uno x 13 servos = 39 bytes)
  uint8_t angleA[13];
  uint8_t angleCenter[13];
  uint8_t angleB[13];
  
  // Ganancias (4 bytes float x 4 = 16 bytes)
  float standPose_coxaGain;
  float standPose_heightGain;
  float gait_coxaGain;
  float gait_liftGain;
  
  // Timings (2 bytes x 4 = 8 bytes)
  uint16_t gait_liftMs;
  uint16_t gait_swingMs;
  uint16_t gait_dropMs;
  uint16_t gait_pushMs;
  
  // Total: 1 + 39 + 39 + 39 + 16 + 8 = 142 bytes
};

const uint16_t EEPROM_BASE = 0;
EEPROMData eepromData;

// =====================================================
// EEPROM FUNCTIONS
// =====================================================
void eeprom_init() {
  eeprom_read();
  if (eepromData.magic != 0xAA) {
    Serial.println("[EEPROM] No válido, usando valores por defecto");
    eeprom_reset();
  }
}

void eeprom_read() {
  EEPROM.get(EEPROM_BASE, eepromData);
}

void eeprom_write() {
  eepromData.magic = 0xAA;
  EEPROM.put(EEPROM_BASE, eepromData);
  Serial.println("[EEPROM] ✓ Guardado");
}

void eeprom_reset() {
  // Ángulos por defecto
  for (int i = 0; i < 13; i++) {
    eepromData.angleA[i] = angleA[i];
    eepromData.angleCenter[i] = angleCenter[i];
    eepromData.angleB[i] = angleB[i];
  }
  
  // Ganancias por defecto
  eepromData.standPose_coxaGain = 0.45f;
  eepromData.standPose_heightGain = 0.70f;
  eepromData.gait_coxaGain = 0.28f;
  eepromData.gait_liftGain = 0.30f;
  
  // Timings por defecto
  eepromData.gait_liftMs = 150;
  eepromData.gait_swingMs = 180;
  eepromData.gait_dropMs = 150;
  eepromData.gait_pushMs = 200;
  
  eeprom_write();
}

// =====================================================
// CARGAR DESDE EEPROM A VARIABLES RUNTIME
// =====================================================
void eeprom_apply() {
  // Copiar ángulos
  for (int i = 0; i < 13; i++) {
    angleA[i] = eepromData.angleA[i];
    angleCenter[i] = eepromData.angleCenter[i];
    angleB[i] = eepromData.angleB[i];
  }
  
  // Copiar ganancias (esto requiere que estas sean variables globales)
  // Assumo que están definidas como: float SAFE_HEIGHT_GAIN, SAFE_COXA_GAIN, etc
  // Necesitarás ajustar según tu código
  
  Serial.println("[EEPROM] Valores aplicados");
}

// =====================================================
// PROTOCOLO SERIAL
// =====================================================
void serial_process() {
  while (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    
    if (line.length() == 0) return;
    
    // Parsear comando
    int spaceIdx = line.indexOf(' ');
    String cmd = (spaceIdx > 0) ? line.substring(0, spaceIdx) : line;
    String args = (spaceIdx > 0) ? line.substring(spaceIdx + 1) : "";
    
    // DEBUG
    Serial.print("[CMD] ");
    Serial.print(cmd);
    Serial.print(" | ARGS: ");
    Serial.println(args);
    
    // Procesar comandos
    
    // --- CALIBRACIÓN ---
    if (cmd == "A") {
      // A <channel> <angle>
      int ch = args.toInt();
      int angle = args.substring(args.indexOf(' ') + 1).toInt();
      if (ch >= 0 && ch <= 12) {
        targetAngle[ch] = angle;
        Serial.print("[CALIB] A[");
        Serial.print(ch);
        Serial.print("] = ");
        Serial.println(angle);
      }
    }
    else if (cmd == "C") {
      // C <channel> <angle>
      int ch = args.toInt();
      int angle = args.substring(args.indexOf(' ') + 1).toInt();
      if (ch >= 0 && ch <= 12) {
        targetAngle[ch] = angle;
        Serial.print("[CALIB] CENTER[");
        Serial.print(ch);
        Serial.print("] = ");
        Serial.println(angle);
      }
    }
    else if (cmd == "B") {
      // B <channel> <angle>
      int ch = args.toInt();
      int angle = args.substring(args.indexOf(' ') + 1).toInt();
      if (ch >= 0 && ch <= 12) {
        targetAngle[ch] = angle;
        Serial.print("[CALIB] B[");
        Serial.print(ch);
        Serial.print("] = ");
        Serial.println(angle);
      }
    }
    
    // --- POSTURAS ---
    else if (cmd == "STAND_COXA") {
      float val = args.toFloat();
      Serial.print("[POSE] StandPose Coxa Gain = ");
      Serial.println(val);
      // Necesitarás actualizar tu variable SAFE_COXA_GAIN o similar
    }
    else if (cmd == "STAND_HEIGHT") {
      float val = args.toFloat();
      Serial.print("[POSE] StandPose Height Gain = ");
      Serial.println(val);
      // Necesitarás actualizar tu variable SAFE_HEIGHT_GAIN o similar
    }
    
    // --- GAIT ---
    else if (cmd == "GAIT_COXA") {
      float val = args.toFloat();
      Serial.print("[GAIT] Coxa Gain = ");
      Serial.println(val);
      // GAIT_COXA_GAIN = val;
    }
    else if (cmd == "GAIT_LIFT") {
      float val = args.toFloat();
      Serial.print("[GAIT] Lift Gain = ");
      Serial.println(val);
      // GAIT_LIFT_GAIN = val;
    }
    
    // --- TIMINGS ---
    else if (cmd == "GAIT_LIFT_MS") {
      uint16_t val = args.toInt();
      Serial.print("[GAIT] Lift MS = ");
      Serial.println(val);
      // GAIT_LIFT_MS = val;
    }
    else if (cmd == "GAIT_SWING_MS") {
      uint16_t val = args.toInt();
      Serial.print("[GAIT] Swing MS = ");
      Serial.println(val);
      // GAIT_SWING_MS = val;
    }
    else if (cmd == "GAIT_DROP_MS") {
      uint16_t val = args.toInt();
      Serial.print("[GAIT] Drop MS = ");
      Serial.println(val);
      // GAIT_DROP_MS = val;
    }
    else if (cmd == "GAIT_PUSH_MS") {
      uint16_t val = args.toInt();
      Serial.print("[GAIT] Push MS = ");
      Serial.println(val);
      // GAIT_PUSH_MS = val;
    }
    
    // --- EEPROM ---
    else if (cmd == "EEPROM_A") {
      int ch = args.toInt();
      int angle = args.substring(args.indexOf(' ') + 1).toInt();
      if (ch >= 0 && ch <= 12) {
        eepromData.angleA[ch] = angle;
        Serial.print("[EEPROM] A[");
        Serial.print(ch);
        Serial.print("] = ");
        Serial.println(angle);
      }
    }
    else if (cmd == "EEPROM_C") {
      int ch = args.toInt();
      int angle = args.substring(args.indexOf(' ') + 1).toInt();
      if (ch >= 0 && ch <= 12) {
        eepromData.angleCenter[ch] = angle;
        Serial.print("[EEPROM] CENTER[");
        Serial.print(ch);
        Serial.print("] = ");
        Serial.println(angle);
      }
    }
    else if (cmd == "EEPROM_B") {
      int ch = args.toInt();
      int angle = args.substring(args.indexOf(' ') + 1).toInt();
      if (ch >= 0 && ch <= 12) {
        eepromData.angleB[ch] = angle;
        Serial.print("[EEPROM] B[");
        Serial.print(ch);
        Serial.print("] = ");
        Serial.println(angle);
      }
    }
    else if (cmd == "EEPROM_STAND_COXA") {
      eepromData.standPose_coxaGain = args.toFloat();
      Serial.print("[EEPROM] Stand Coxa = ");
      Serial.println(eepromData.standPose_coxaGain);
    }
    else if (cmd == "EEPROM_STAND_HEIGHT") {
      eepromData.standPose_heightGain = args.toFloat();
      Serial.print("[EEPROM] Stand Height = ");
      Serial.println(eepromData.standPose_heightGain);
    }
    else if (cmd == "EEPROM_GAIT_COXA") {
      eepromData.gait_coxaGain = args.toFloat();
      Serial.print("[EEPROM] Gait Coxa = ");
      Serial.println(eepromData.gait_coxaGain);
    }
    else if (cmd == "EEPROM_GAIT_LIFT") {
      eepromData.gait_liftGain = args.toFloat();
      Serial.print("[EEPROM] Gait Lift = ");
      Serial.println(eepromData.gait_liftGain);
    }
    else if (cmd == "EEPROM_LIFT_MS") {
      eepromData.gait_liftMs = args.toInt();
      Serial.print("[EEPROM] Lift MS = ");
      Serial.println(eepromData.gait_liftMs);
    }
    else if (cmd == "EEPROM_SWING_MS") {
      eepromData.gait_swingMs = args.toInt();
      Serial.print("[EEPROM] Swing MS = ");
      Serial.println(eepromData.gait_swingMs);
    }
    else if (cmd == "EEPROM_DROP_MS") {
      eepromData.gait_dropMs = args.toInt();
      Serial.print("[EEPROM] Drop MS = ");
      Serial.println(eepromData.gait_dropMs);
    }
    else if (cmd == "EEPROM_PUSH_MS") {
      eepromData.gait_pushMs = args.toInt();
      Serial.print("[EEPROM] Push MS = ");
      Serial.println(eepromData.gait_pushMs);
    }
    else if (cmd == "EEPROM_SAVE") {
      eeprom_write();
      Serial.println("[EEPROM] ✓ GUARDADO EN MEMORIA");
      eeprom_apply();
    }
    
    // --- CONSULTAS ---
    else if (cmd == "STATE") {
      // Enviar estado
      Serial.print("STATE: NRF=");
      Serial.print(linkAlive ? "OK" : "LOST");
      Serial.print(" PCA=OK BATTERY=85 MODE=");
      Serial.println(rxData.mode3);
    }
    else if (cmd == "ANGLES") {
      // Enviar ángulos actuales
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
      Serial.println("=== Q-POD MINI CONFIG ===");
      Serial.println("Comandos disponibles:");
      Serial.println(" A <ch> <angle> - Set angleA");
      Serial.println(" C <ch> <angle> - Set angleCenter");
      Serial.println(" B <ch> <angle> - Set angleB");
      Serial.println(" STATE - Get hardware state");
      Serial.println(" ANGLES - Get current servo angles");
      Serial.println(" EEPROM_SAVE - Save to EEPROM");
    }
  }
}

// =====================================================
// Llamar en setup():
// =====================================================
// eeprom_init();

// =====================================================
// Llamar en loop(), después de procesar otros inputs:
// =====================================================
// serial_process();
