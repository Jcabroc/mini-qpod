# 🚀 Q-POD MINI - QUICK REFERENCE

## 📥 INSTALACIÓN RÁPIDA

```bash
# 1. Instalar Python + pyserial
pip install pyserial

# 2. En tu carpeta del proyecto
python quickstart_config.py

# 3. Cuando pregunte: cargar default_config.json

# 4. Listo ✓
```

---

## 🎯 3 FORMAS DE USAR

### Opción A: Quickstart (Carga rápida)
```bash
python quickstart_config.py
```
✅ Para cargar configuración de una sola vez  
❌ Sin interfaz visual

### Opción B: GUI Completa (Recomendado)
```bash
python gui_qpod_config.py
```
✅ Interfaz bonita y amigable  
✅ Calibración en vivo  
✅ Manejo de configs JSON  
⚠️ Requiere que tengas GUI (no funciona en SSH)

### Opción C: Manual con Monitor Serial
```
Arduino IDE → Tools → Serial Monitor
Baud: 115200
```
✅ Debugging detallado  
❌ Tedioso para muchos cambios

---

## ⚡ COMANDOS SERIAL (Todos)

```
A <ch> <angle>          → Set angleA[ch]
C <ch> <angle>          → Set angleCenter[ch]
B <ch> <angle>          → Set angleB[ch]

EEPROM_A <ch> <angle>   → Guardar angleA en EEPROM
EEPROM_C <ch> <angle>   → Guardar angleCenter en EEPROM
EEPROM_B <ch> <angle>   → Guardar angleB en EEPROM
EEPROM_SAVE             → Confirmar guardado

STATE                   → Ver estado (NRF/PCA)
ANGLES                  → Ver ángulos actuales
INFO                    → Ver este mensaje
```

---

## 📊 SERVOS (Numeración)

```
L1 = pata delantera izquierda    |  R1 = pata delantera derecha
├─ CH0: COXA                     |  ├─ CH3: COXA
├─ CH1: FEMUR                    |  ├─ CH4: FEMUR
└─ CH2: TIBIA                    |  └─ CH5: TIBIA

L2 = pata trasera izquierda      |  R2 = pata trasera derecha
├─ CH6: COXA                     |  ├─ CH9: COXA
├─ CH7: FEMUR                    |  ├─ CH10: FEMUR
└─ CH8: TIBIA                    |  └─ CH11: TIBIA

CH12 = CUELLO
```

---

## 🎚️ PARÁMETROS RECOMENDADOS

### Postura Base (standPose)
```
standPose_coxaGain      = 0.45 ± 0.10     (apertura patas)
standPose_heightGain    = 0.70 ± 0.10     (altura)
```

### Marcha (Gait)
```
gait_coxaGain           = 0.28 ± 0.10     (amplitud pasos)
gait_liftGain           = 0.30 ± 0.10     (altura levantada)

gait_liftMs             = 150              (tiempo lift)
gait_swingMs            = 180              (tiempo swing)
gait_dropMs             = 150              (tiempo drop)
gait_pushMs             = 200              (tiempo push)
```

---

## 📁 ARCHIVOS CLAVE

```
proyecto/
├── test_gait.ino                    ← Código Arduino (con protocolo integrado)
├── gui_qpod_config.py              ← Interfaz gráfica
├── quickstart_config.py            ← Carga rápida
├── default_config.json             ← Configuración por defecto
├── qpod_config_202604231200.json   ← Tus configs guardadas
├── README_CONFIGURACION.md         ← Manual completo
├── INTEGRACION_PASO_A_PASO.md      ← Guía integración Arduino
└── QUICK_REFERENCE.md              ← Este archivo
```

---

## 🔧 TROUBLESHOOTING

| Problema | Solución |
|----------|----------|
| "Puerto COM no aparece" | Drivers CH340/FTDI + reiniciar VS Code |
| "Arduino no responde" | Verifica serial_process() en loop() |
| "Error: módulo serial" | `pip install pyserial` |
| "Valores no persisten" | Usa botón "EEPROM_SAVE" en GUI |
| "GUI no se abre" | Necesitas display (SSH no funciona) |

---

## 💾 WORKFLOW TIPICO

```
1. Conectar Arduino
2. Abrir GUI: python gui_qpod_config.py
3. Tab "Calibración" → Ajustar servos
4. Tab "Posturas" → Aumentar ganancias
5. Tab "Gait" → Optimizar timings
6. Button "💾 Guardar JSON" → backup
7. Button "📤 → Arduino (EEPROM)" → guardar permanente
8. Desenchufar/enchufar Arduino
9. Verificar que se mantienen cambios ✓
```

---

## 🎯 PUNTOS CLAVE

✅ **Siempre respaldos:** Guarda JSON antes de experimentar  
✅ **Cambios incrementales:** Modifica 1 parámetro a la vez  
✅ **Prueba en vivo:** La GUI permite ver cambios sin recompilar  
✅ **EEPROM es segura:** 1024 bytes, usas ~150  
✅ **Baud rate:** SIEMPRE 115200  

---

## 🚀 INICIO RÁPIDO (5 MINUTOS)

1. Abre PowerShell
2. ```powershell
   cd f:\Documentos\Jota\jRobot\mini Q-Pod
   python gui_qpod_config.py
   ```
3. Selecciona puerto COM → Conectar
4. ¡A calibrar!

---

## 📞 COMANDOS PYTHON DESDE TERMINAL

```powershell
# Listar puertos
python -c "import serial.tools.list_ports; print([p[0] for p in serial.tools.list_ports.comports()])"

# Test rápido de conexión
python quickstart_config.py

# GUI completa
python gui_qpod_config.py
```

---

**¡Listo para empezar!** 🤖
