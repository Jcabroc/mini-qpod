# 📦 Q-POD MINI - SISTEMA DE CONFIGURACIÓN
## 📋 Índice de Archivos Entregados

---

## 🎯 ¿QUÉ ACABAS DE RECIBIR?

Un **sistema completo** para parametrizar tu robot Mini Q-Pod sin recompilar código:

✅ **GUI interactiva** en Python (Tkinter)  
✅ **Comunicación serial** bidireccional con Arduino  
✅ **Almacenamiento en EEPROM** del Nano  
✅ **Guardar/cargar configuraciones** en JSON  
✅ **Monitoreo en tiempo real** de estado  

---

## 📂 ARCHIVOS PYTHON

### 1. `gui_qpod_config.py` ⭐ PRINCIPAL
**La interfaz gráfica completa**

```
Características:
├─ Tab 1: Estado (NRF24, PCA9685, servos)
├─ Tab 2: Calibración (ángulos A/Center/B)
├─ Tab 3: Posturas (ganancias standPose)
├─ Tab 4: Gait (ganancias + timings)
├─ Tab 5: Avanzado (control remoto PS2J)
├─ Tab 6: Log (terminal debug)
└─ Botones: Guardar/Cargar JSON, Enviar a EEPROM
```

**Cómo usar:**
```bash
python gui_qpod_config.py
```

---

### 2. `quickstart_config.py` 🚀 RÁPIDO
**Carga rápida sin GUI**

```
Use case:
- Cargar configuración de una vez
- Sin interfaz gráfica (útil para SSH)
- Prueba rápida de conectividad
```

**Cómo usar:**
```bash
python quickstart_config.py
```

Flujo:
1. Lista puertos COM
2. Prueba conexión
3. Carga default_config.json
4. Guarda en EEPROM

---

## 📂 ARCHIVOS DE CONFIGURACIÓN

### 3. `default_config.json`
**Valores por defecto del robot**

```json
{
  "angleA": [...],           // Ángulo extremo 1 de servos
  "angleCenter": [...],      // Centro calibrado
  "angleB": [...],           // Ángulo extremo 2
  "standPose_coxaGain": 0.45,
  "standPose_heightGain": 0.70,
  "gait_coxaGain": 0.28,
  "gait_liftGain": 0.30,
  "gait_liftMs": 150,
  "gait_swingMs": 180,
  "gait_dropMs": 150,
  "gait_pushMs": 200,
  "filter_strength": 4,
  "deadzone_percent": 6,
  "send_period_ms": 20
}
```

Puedes crear más configuraciones:
- `qpod_spider_pose.json`
- `qpod_aggressive_gait.json`
- etc.

---

## 📂 ARCHIVOS ARDUINO

### 4. `serial_protocol.cpp`
**Protocolo de comunicación serial**

Contiene:
- Estructura EEPROM
- Funciones de lectura/escritura EEPROM
- Parser de comandos serial

⚠️ Necesita ser **integrado en test_gait.ino**

---

## 📚 DOCUMENTACIÓN

### 5. `README_CONFIGURACION.md` 📖 MANUAL COMPLETO
Includes:
- Descripción del sistema
- Instalación paso a paso
- Cómo usar cada componente
- Explicación de parámetros
- Protocolo serial completo
- Estructura EEPROM
- Troubleshooting

**Lectura obligatoria** para entender el sistema

---

### 6. `INTEGRACION_PASO_A_PASO.md` 🔌 GUÍA ARDUINO
**Instrucciones exactas para integrar en test_gait.ino**

Paso a paso:
1. Añadir `#include <EEPROM.h>`
2. Copiar definiciones EEPROM
3. Copiar protocolo serial
4. Modificar `setup()` → añadir `eeprom_init()`
5. Modificar `loop()` → añadir `serial_process()`

**Con ejemplos de código exacto** que debes copiar/pegar

---

### 7. `QUICK_REFERENCE.md` ⚡ CHEAT SHEET
**Referencia rápida**

Incluye:
- Instalación rápida (3 líneas)
- 3 formas de usar
- Comandos serial rápidos
- Numeración de servos
- Parámetros recomendados
- Troubleshooting tabla
- Workflow típico

**Imprímelo o guárdalo en favoritos**

---

### 8. `INDEX.md` (este archivo)
**Descripción de todos los archivos**

---

## 🚀 INICIO RÁPIDO (ELIJE UNO)

### Opción A: 5 minutos sin Arduino (solo Python)
```bash
pip install pyserial
python quickstart_config.py
```

### Opción B: GUI completa (recomendado)
```bash
pip install pyserial
python gui_qpod_config.py
# Se abre interfaz gráfica
```

### Opción C: Integración con Arduino
1. Lee `INTEGRACION_PASO_A_PASO.md` (15 min)
2. Sigue exactamente los 5 pasos
3. Compila `test_gait.ino` con protocolo integrado
4. Luego: `python gui_qpod_config.py`

---

## 📊 WORKFLOW RECOMENDADO

```
Semana 1: Familiarización
├─ Lee QUICK_REFERENCE.md (5 min)
├─ Ejecuta quickstart_config.py (5 min)
└─ Explora GUI sin Arduino conectado (5 min)

Semana 2: Integración
├─ Lee INTEGRACION_PASO_A_PASO.md (15 min)
├─ Integra protocolo en Arduino (15 min)
├─ Compila y prueba conexión (5 min)
└─ Verifica con Monitor Serial (10 min)

Semana 3: Calibración
├─ Conecta GUI a Arduino
├─ Calibra servos uno por uno
├─ Ajusta standPose (ganancias)
├─ Guarda configuración JSON
└─ Verifica persistencia en EEPROM

Semana 4+: Optimización Gait
├─ Abre Tab "Gait" en GUI
├─ Modifica ganancias en vivo
├─ Prueba diferentes timings
├─ Perfecciona marcha del robot
└─ Guarda versiones: gait_v1.json, gait_v2.json, etc
```

---

## 🔑 PUNTOS CLAVE

### Capacidades
✅ **Sin recompilar:** Cambios en tiempo real  
✅ **Persistencia:** Los parámetros se guardan en EEPROM  
✅ **Portabilidad:** Múltiples configuraciones JSON  
✅ **Debugging:** Terminal serial en vivo  
✅ **Seguridad:** Validación de comandos  

### Limitaciones
⚠️ GUI necesita display (no SSH)  
⚠️ Baud rate: DEBE ser 115200  
⚠️ EEPROM tiene límite (~1024 bytes)  
⚠️ Sin sensores de realimentación aún  

---

## 📝 RESUMEN DE ARCHIVOS

| Archivo | Tipo | Propósito |
|---------|------|----------|
| gui_qpod_config.py | Python | Interfaz gráfica completa ⭐ |
| quickstart_config.py | Python | Carga rápida sin GUI |
| serial_protocol.cpp | Arduino | Protocolo serial (integrar) |
| default_config.json | JSON | Configuración por defecto |
| README_CONFIGURACION.md | Doc | Manual completo 📖 |
| INTEGRACION_PASO_A_PASO.md | Doc | Guía Arduino 🔌 |
| QUICK_REFERENCE.md | Doc | Cheat sheet ⚡ |
| INDEX.md | Doc | Este archivo |

---

## 🎓 ÓRDENES DE LECTURA RECOMENDADO

**Para empezar rápido:**
1. QUICK_REFERENCE.md (5 min)
2. Ejecuta quickstart_config.py

**Para usar la GUI:**
1. QUICK_REFERENCE.md
2. README_CONFIGURACION.md
3. Ejecuta gui_qpod_config.py

**Para integrar con Arduino:**
1. README_CONFIGURACION.md (opcional, para contexto)
2. INTEGRACION_PASO_A_PASO.md (OBLIGATORIO)
3. Integra en test_gait.ino
4. Usa GUI o quickstart

---

## 🔗 DEPENDENCIES

Python:
```
pyserial          (comunicación serial)
tkinter           (incluido en Python)
json              (incluido en Python)
threading         (incluido en Python)
```

Arduino:
```
Wire.h            (I2C, incluido)
Adafruit_PWMServoDriver.h
RF24.h            (NRF24)
EEPROM.h          (incluido)
```

---

## ✨ PRÓXIMOS PASOS SUGERIDOS

1. **Mejora de marcha:**
   - Implementar easing en movimientos
   - Detectar obstáculos
   - Adaptive gait basado en terreno

2. **Sensorización:**
   - Lectura de batería
   - Estado de PCA9685
   - Inclinómetro del robot

3. **Grabación de datos:**
   - Log de consumo
   - Historial de configuraciones
   - Análisis de rendimiento

4. **Integración Pico:**
   - Enviar datos desde Pico a Nano
   - Controlar parámetros remotamente

---

## 📞 SOPORTE

Si tienes problemas:

1. **Python no instala pyserial:**
   ```powershell
   python -m pip install pyserial
   ```

2. **Arduino no responde:**
   - Abre Monitor Serial de Arduino IDE
   - Baud rate: 115200
   - Envía comando "INFO"

3. **GUI no se abre:**
   - Necesitas display (Windows/Linux con GUI)
   - SSH no funciona (sin display)

4. **Parámetros no persisten:**
   - Verifica que usaste botón "📤 → Arduino (EEPROM)"
   - Espera a que aparezca el mensaje "✓ GUARDADO"

---

## 🎉 ¡ÉXITO!

Ahora tienes:
✅ Configuración sin recompilar  
✅ GUI amigable  
✅ Almacenamiento persistente  
✅ Sistema escalable  

**¡A calibrar el Q-POD!** 🤖

---

*Última actualización: 23 de abril de 2026*
