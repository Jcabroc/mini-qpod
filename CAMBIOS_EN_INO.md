# 📋 CAMBIOS EN test_gait_INTEGRADO.ino

## 🎯 Resumen

Este archivo es el `test_gait.ino` **COMPLETAMENTE INTEGRADO** con:
✅ Protocolo Serial (comandos desde Python)  
✅ EEPROM Manager (guardado persistente)  
✅ Parámetros ajustables en tiempo real  

**Listo para usar con la GUI de Python sin modificaciones adicionales.**

---

## 🔄 CAMBIOS ESPECÍFICOS (5 pasos de integración)

### ✅ PASO 1: Include EEPROM
**Línea 7**
```cpp
#include <EEPROM.h>    // ← NUEVO
```

---

### ✅ PASO 2: Struct + Funciones EEPROM
**Después de `SERVO_MAX` (línea 58)**

Añadidas:
- `struct EEPROMData` - Estructura de datos EEPROM
- `eeprom_init()` - Inicializar EEPROM
- `eeprom_write()` - Guardar a EEPROM

Esto permite guardar:
```
- 13 ángulos × 3 (A, Center, B)
- 4 ganancias (float)
- 4 timings (uint16)
Total: ~140 bytes de 1024
```

---

### ✅ PASO 3: Variables Dinámicas
**Líneas 175-181**

Cambio importante - Variables ahora son `float` en lugar de `const`:
```cpp
// ANTES:
const uint16_t GAIT_LIFT_MS  = 180;
const float GAIT_COXA_GAIN = 0.28f;

// AHORA:
uint16_t GAIT_LIFT_MS  = 150;        // Variable
float GAIT_COXA_GAIN = 0.28f;        // Variable
```

También:
```cpp
// ANTES:
const float SAFE_HEIGHT_GAIN = 0.55f;
const float SAFE_COXA_GAIN   = 0.45f;

// AHORA:
float SAFE_HEIGHT_GAIN = 0.70f;      // Configurable
float SAFE_COXA_GAIN   = 0.45f;      // Configurable
```

---

### ✅ PASO 4: buildStandPose() mejorada
**Línea 354-368**

Ahora usa ganancias **dinámicas**:
```cpp
// ANTES:
standPose[0] = safeBlendFromCenter(angleCenter[0], coxaForward[0], 0.30f);

// AHORA:
standPose[0] = safeBlendFromCenter(angleCenter[0], coxaForward[0], SAFE_COXA_GAIN);
```

**Ventaja:** Cambias la ganancia desde la GUI y recalcula postura en vivo.

---

### ✅ PASO 5: Función serial_process()
**Línea 657-852**

Nueva función que **procesa comandos desde Python**:

#### 5a - Comandos en VIVO (sin guardar)
```cpp
// Cambia ángulos inmediatamente
A <ch> <angle>
C <ch> <angle>
B <ch> <angle>

// Cambia ganancias inmediatamente
STAND_COXA <valor>
STAND_HEIGHT <valor>
GAIT_COXA <valor>
GAIT_LIFT <valor>

// Cambia timings inmediatamente
GAIT_LIFT_MS <ms>
GAIT_SWING_MS <ms>
GAIT_DROP_MS <ms>
GAIT_PUSH_MS <ms>
```

#### 5b - Comandos EEPROM (guardado permanente)
```cpp
EEPROM_A <ch> <angle>
EEPROM_C <ch> <angle>
EEPROM_B <ch> <angle>
EEPROM_STAND_COXA <valor>
EEPROM_STAND_HEIGHT <valor>
EEPROM_GAIT_COXA <valor>
EEPROM_GAIT_LIFT <valor>
EEPROM_LIFT_MS <ms>
EEPROM_SWING_MS <ms>
EEPROM_DROP_MS <ms>
EEPROM_PUSH_MS <ms>
EEPROM_SAVE  ← Ejecuta guardado
```

#### 5c - Consultas
```cpp
STATE        → NRF/PCA/Modo
ANGLES       → Ángulos actuales
INFO         → Ayuda
```

---

### ✅ PASO 6: setup() modificado
**Línea 854-903**

Añadida línea después de `buildStandPose()`:
```cpp
// NUEVO
eeprom_init();
```

Esto carga los parámetros guardados al iniciar.

---

### ✅ PASO 7: loop() modificado
**Línea 954 (final)**

Última línea del loop:
```cpp
// NUEVO
serial_process();
```

Procesa comandos serial sin bloquear.

---

## 🎯 CÓMO USA LA GUI

### Flujo típico:

1. **GUI Python** → Envía comando `A 0 50`
   ```
   "Set ángulo A del servo 0 a 50°"
   ```

2. **Arduino recibe** → `serial_process()` lo ejecuta
   ```cpp
   angleA[0] = 50;
   targetAngle[0] = 50;    // Aplica inmediatamente
   ```

3. **Servo se mueve** en vivo

4. **Usuario hace clic** "📤 → Arduino (EEPROM)"
   ```
   GUI envía: EEPROM_A 0 50
   GUI envía: EEPROM_SAVE
   ```

5. **Arduino guarda** en EEPROM permanentemente

6. **Desenchufa/enchúfa** Arduino → ¡Se recuerda! ✓

---

## 📊 PARÁMETROS AHORA AJUSTABLES

### Desde GUI (sin recompilar):

| Parámetro | Tipo | Rango | Default |
|-----------|------|-------|---------|
| angleA[13] | int | 0-180 | * |
| angleCenter[13] | int | 0-180 | * |
| angleB[13] | int | 0-180 | * |
| SAFE_COXA_GAIN | float | 0.0-1.0 | 0.45 |
| SAFE_HEIGHT_GAIN | float | 0.0-1.0 | 0.70 |
| GAIT_COXA_GAIN | float | 0.0-1.0 | 0.28 |
| GAIT_LIFT_GAIN | float | 0.0-1.0 | 0.30 |
| GAIT_LIFT_MS | uint16 | 50-500 | 150 |
| GAIT_SWING_MS | uint16 | 50-500 | 180 |
| GAIT_DROP_MS | uint16 | 50-500 | 150 |
| GAIT_PUSH_MS | uint16 | 50-500 | 200 |

*= Valores por defecto en default_config.json

---

## ⚠️ CAMBIOS IMPORTANTES

1. **Valores nuevos más agresivos:**
   - `GAIT_LIFT_MS`: 180 → **150** (más rápido)
   - `GAIT_SWING_MS`: 220 → **180** (pasos más rápidos)
   - `GAIT_DROP_MS`: 180 → **150** (más estable)
   - `GAIT_PUSH_MS`: 220 → **200** (más natural)
   - `SAFE_HEIGHT_GAIN`: 0.55 → **0.70** (postura más baja)

2. **Variables dinámicas:**
   - Todos los timings ahora son `uint16_t` (no `const`)
   - Todas las ganancias ahora son `float` (no `const`)
   - Se pueden cambiar sin recompilar

3. **buildStandPose() mejorada:**
   - Usa ganancias dinámicas
   - Se puede recalcular con comando desde GUI

---

## 🚀 CÓMO USAR

### Compilar:
```
Arduino IDE → Selecciona Arduino Nano
Sube el código como siempre
```

### Controlar desde Python:
```bash
python gui_qpod_config.py
```

### Probar en terminal:
```bash
python quickstart_config.py
```

---

## 🔍 VERIFICACIÓN RÁPIDA

Para verificar que todo está integrado:

1. **Abre Monitor Serial** (Arduino IDE)
   - Baud: 115200
   - Envía: `INFO`
   - Debe responder con lista de comandos

2. **Desde PowerShell:**
   ```bash
   python quickstart_config.py
   ```
   - Carga configuración automáticamente
   - Debe ver "✓ ÉXITO"

3. **Desenchufa/enchúfa Arduino**
   - Debe recordar los parámetros
   - Postura debe ser igual

---

## 📁 COMPATIBILIDAD

✅ Compatible con GUI: `gui_qpod_config.py`  
✅ Compatible con quickstart: `quickstart_config.py`  
✅ Compatible con JSON: `default_config.json`  
✅ Protocolo serial: Bidireccional  
✅ EEPROM: ~140 bytes usados  

---

## 🎉 ¡LISTO!

Este archivo `.ino` está **completamente listo**:
- Copia/pega a Arduino IDE
- Compila sin cambios
- Sube a Nano
- Abre GUI Python
- ¡A configurar! 🚀

---

## 📞 SI ALGO NO FUNCIONA

### "Arduino no responde"
```
Abre Monitor Serial → Baud 115200 → Envía "INFO"
Si no responde: Comprueba que serial_process() está en loop()
```

### "EEPROM no guarda"
```
Verifica que ejecutaste: EEPROM_SAVE
Revisa Monitor Serial para confirmar guardado
```

### "Postura no cambia"
```
1. Envía: STAND_COXA 0.5
2. Envía: STAND_HEIGHT 0.7
3. Presiona START en control remoto
La postura debe cambiar en vivo
```

---

¡**Éxito con tu Q-POD!** 🤖
