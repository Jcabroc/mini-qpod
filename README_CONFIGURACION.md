# 📋 Q-POD MINI - SISTEMA DE CONFIGURACIÓN EN TIEMPO REAL

## 🎯 Descripción General

Este sistema permite:
✅ Parametrizar el robot **sin recompilar código**  
✅ Guardar/cargar configuraciones en **JSON**  
✅ Almacenar parámetros en **EEPROM del Arduino**  
✅ Ver estado en tiempo real (NRF24, PCA9685, ángulos)  
✅ Calibrar servos interactivamente  

---

## 🔧 COMPONENTES

### 1. **GUI Python** (`gui_qpod_config.py`)
Interfaz gráfica multiTab:
- **Tab 1 - Estado:** Monitoreo en vivo de hardware
- **Tab 2 - Calibración:** Edición de ángulos A/Center/B
- **Tab 3 - Posturas:** Ganancias standPose
- **Tab 4 - Gait:** Ganancias y timings de ciclo de marcha
- **Tab 5 - Avanzado:** Parámetros control remoto
- **Tab 6 - Log:** Terminal de debug

### 2. **Protocolo Serial** (`serial_protocol.cpp`)
Comunicación bidireccional Arduino ↔ Python
- Comandos para ajuste en vivo
- Lectura de estado
- Manejo de EEPROM

### 3. **Configuración** (archivos JSON)
Almacenamiento de parámetros portátil

---

## 🚀 INSTALACIÓN

### Paso 1: Instalar Python + dependencias
```bash
# Descargar Python 3.8+
# https://www.python.org/downloads/

# En PowerShell:
pip install pyserial
```

### Paso 2: Copiar archivos Python
```
Tu proyecto/
├── gui_qpod_config.py          ← Interfaz
├── configs/
│   ├── default_config.json      ← Configuración por defecto
│   └── mi_config.json           ← Tus propias configs
```

### Paso 3: Integrar protocolo en Arduino

**Abre `test_gait.ino` y sigue estos pasos:**

#### 3.1 - Añade INCLUDES al principio
```cpp
#include <EEPROM.h>
```

#### 3.2 - Después de definiciones globales, copia la sección EEPROM
Copia TODO el contenido de `serial_protocol.cpp` en tu sketch, justo después de las definiciones de variables globales.

#### 3.3 - En setup(), añade:
```cpp
void setup() {
  // ... código existente ...
  
  Serial.begin(115200);
  delay(600);
  
  // ← AÑADE ESTAS LÍNEAS
  eeprom_init();
  Serial.println("EEPROM inicializado");
  // ← FIN AÑADIDAS
  
  // ... resto del setup ...
}
```

#### 3.4 - En loop(), al final, añade:
```cpp
void loop() {
  // ... código existente de NRF24, control, gait, etc ...
  
  // ← AÑADE ESTA LÍNEA al final del loop()
  serial_process();
  // ← FIN AÑADIDA
}
```

---

## 📊 CÓMO USAR

### 1. Conectar y calibrar

```
1. Abre Python → gui_qpod_config.py
2. Selecciona tu puerto COM (ej: COM3)
3. Haz clic en "Conectar"
4. Ve a Tab "Calibración"
5. Selecciona un servo (ej: L1_COXA)
6. Mueve los sliders A / Center / B
7. Haz clic "Enviar" para aplicar en vivo
```

### 2. Guardar configuración

```
1. Una vez calibrado, haz clic "💾 Guardar JSON"
2. Selecciona nombre: ej "qpod_config_202604231200.json"
3. Se guarda automáticamente
```

### 3. Enviar a EEPROM del robot

```
1. Haz todos tus cambios
2. Haz clic "📤 → Arduino (EEPROM)"
3. Los parámetros se guardan en memoria del Arduino
4. El robot recordará la config **incluso sin alimentación**
```

### 4. Cargar configuración anterior

```
1. Haz clic "📂 Cargar JSON"
2. Selecciona archivo anterior
3. Los valores se actualizan en GUI
4. Haz clic "📤 → Arduino" para enviar al robot
```

---

## 🎛️ PARÁMETROS EXPLICADOS

### **Calibración**
- **Ángulo A:** Extremo 1 del servo (0-180°)
- **Ángulo Center:** Centro calibrado (típicamente 90°)
- **Ángulo B:** Extremo 2 del servo (0-180°)

### **Postura Base (StandPose)**
- **Ganancia Coxa:** Cuánto abren las patas lateralmente (0.0 = juntas, 1.0 = máximo)
  - **Recomendado: 0.45-0.55**
- **Ganancia Altura:** Cuánto se baja el robot (0.0 = normal, 1.0 = mínimo)
  - **Recomendado: 0.65-0.75**

### **Gait (Marcha)**
- **Ganancia Coxa:** Amplitud de pasos adelante/atrás (0.1-0.4)
- **Ganancia Lift:** Altura levantada de pata (0.2-0.5)
- **Timings:**
  - **LIFT (ms):** Tiempo levantamiento → **150-180**
  - **SWING (ms):** Tiempo movimiento horizontal → **180-220**
  - **DROP (ms):** Tiempo apoyo → **150-180**
  - **PUSH (ms):** Tiempo empuje → **180-220**

---

## 🔄 PROTOCOLO SERIAL

### Formato comandos:
```
[CMD] [ARG1] [ARG2] ...
```

### Ejemplos:

```
A 0 50              → Set angleA[servo0] = 50°
C 0 90              → Set angleCenter[servo0] = 90°
B 0 150             → Set angleB[servo0] = 150°

STAND_COXA 0.45     → StandPose coxa gain = 0.45
STAND_HEIGHT 0.70   → StandPose height gain = 0.70

GAIT_COXA 0.28      → Gait coxa gain = 0.28
GAIT_LIFT 0.30      → Gait lift gain = 0.30

GAIT_LIFT_MS 150    → Fase lift = 150ms
GAIT_SWING_MS 180   → Fase swing = 180ms

EEPROM_A 0 50       → Guardar en EEPROM: angleA[0] = 50
EEPROM_SAVE         → Confirmar y guardar TODO

STATE               → Obtener estado NRF/PCA/Batería
ANGLES              → Obtener ángulos actuales
INFO                → Listar comandos disponibles
```

---

## 📁 ESTRUCTURA EEPROM

El Arduino Nano tiene **1024 bytes** de EEPROM.  
Tu sistema usa ~142 bytes:

```
Offset  | Tamaño | Contenido
--------|--------|--------------------
0       | 1      | Magic (0xAA)
1-39    | 39     | angleA[13] (3 bytes c/u)
40-78   | 39     | angleCenter[13]
79-117  | 39     | angleB[13]
118-133 | 16     | Ganancias (4 float)
134-141 | 8      | Timings (4 uint16)
--------|--------|
Total   | ~142   | Uso
```

Sobran **~880 bytes** para futuros datos.

---

## 🐛 TROUBLESHOOTING

### Q: "Puerto COM no aparece"
**A:** 
1. Instala drivers CH340 o FTDI según tu Arduino
2. Reinicia VS Code
3. Desenchufa/enchúfa Arduino

### Q: "Error: módulo serial no encontrado"
**A:** Ejecuta en PowerShell:
```powershell
pip install pyserial
```

### Q: "Envío a Arduino sin respuesta"
**A:**
1. Abre Monitor Serial de Arduino IDE → verifica baud rate **115200**
2. Comprueba que `serial_process()` está en loop()
3. Envía comando "INFO" para verificar conectividad

### Q: "Valores no se guardan en EEPROM"
**A:**
1. Verifica que llamaste `eeprom_init()` en setup()
2. Usa botón "📤 → Arduino" al final
3. Verifica con Monitor Serial que dice "✓ GUARDADO EN MEMORIA"

---

## 📝 EJEMPLO DE FLUJO COMPLETO

```
1. Arduino subido con serial_protocol.cpp integrado ✓
2. Abre GUI Python → conecta a COM3
3. Calibra L1_COXA: A=50, C=90, B=150
4. Ajusta StandPose: Coxa=0.50, Height=0.72
5. Prueba movimiento con control PS2J
6. Si se ve bien: "💾 Guardar JSON" → config_20260423.json
7. "📤 → Arduino" → parámetros en EEPROM ✓
8. Desenchufa/enchúfa Arduino
9. Verifica que sigue con misma postura ✓
```

---

## 🔗 PRÓXIMOS PASOS

1. **Mejora de easing:** Implementar suavizado en interpolación
2. **Sensores:** Integrar lecturas de batería y estado PCA9685
3. **Graficación:** Mostrar trayectorias en tiempo real
4. **Exportación:** Guardar historiales de pruebas

---

## ⚠️ NOTAS IMPORTANTES

- **No toques EEPROM sin saber qué haces** → riesgo de perder datos
- **Siempre respaldos:** Guarda configs JSON antes de experimentar
- **Baud rate:** DEBE ser 115200 tanto en Arduino como Python
- **Delays mínimos:** El protocolo no bloquea, es seguro para tiempo real

---

## 📞 SOPORTE

Si tienes dudas:
1. Revisa el tab "Log" en GUI para debug
2. Abre Monitor Serial de Arduino IDE para ver mensajes raw
3. Comprueba que los comandos siguen el formato exacto

¡Éxito con tu Q-POD! 🚀
