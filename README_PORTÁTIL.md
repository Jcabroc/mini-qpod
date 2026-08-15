# 🚀 GUÍA: USAR EL CONFIGURADOR EN OTROS COMPUTADORES

## 📋 ¿QUÉ NECESITAS?

Para usar el Q-POD Mini Configurador en otro PC, necesitas:

1. **Pendrive de al menos 1 GB** (aunque 512 MB es suficiente)
2. **Windows 7 en adelante** (o Linux/Mac si tienes Python)
3. **Acceso USB** del robot (cable serial/USB)

---

## 🔧 PASO 1: PREPARAR EL PENDRIVE

### A. Copiar los archivos necesarios

Copia **ESTA CARPETA COMPLETA** a tu pendrive:

```
Carpeta del Pendrive/
├── gui_qpod_config.py           ← Interfaz principal
├── quickstart_config.py         ← Alternativa rápida
├── default_config.json          ← Configuración por defecto
├── serial_protocol.cpp          ← Referencia del protocolo
├── requirements.txt             ← Dependencias Python
├── setup_portable.bat           ← ⭐ INSTALAR PRIMERO
├── INICIAR_GUI.bat             ← Inicia la GUI
├── INICIAR_GUI_SILENCIOSO.vbs  ← Inicia sin ventana (opcional)
└── README_PORTÁTIL.md          ← Este archivo
```

---

## 💻 PASO 2: EN EL OTRO COMPUTADOR

### Opción A: Primera vez (instalar dependencias)

1. **Enchufa el pendrive**
2. **Abre el Explorador** y ve a la carpeta del pendrive
3. **Doble-clic en `setup_portable.bat`**
   - Si pide confirmación, haz clic en "Ejecutar"
   - Espera a que termine (dirá "¡Instalación completada!")
   - Presiona una tecla para cerrar

### Opción B: Próximas veces (sin instalar, solo abrir)

1. **Enchufa el pendrive**
2. **Doble-clic en `INICIAR_GUI.bat`**
   - O: Doble-clic en `INICIAR_GUI_SILENCIOSO.vbs` (sin ventana de terminal)

---

## ⚠️ REQUISITOS EN OTRO PC

### Opción 1: Python ya instalado (MÁS FÁCIL)

Si en el otro PC **ya tienen Python 3.8+** instalado:
- Solo ejecuta `setup_portable.bat` una vez
- Listo, ya funciona
- Cada vez haz doble-clic en `INICIAR_GUI.bat`

### Opción 2: Python NO instalado

Si en el otro PC **NO tienen Python**:
1. Descarga Python 3.8+ (o 3.11, 3.12):
   - https://www.python.org/downloads/
   - **⭐ IMPORTANTE**: Marca "Add Python to PATH" en la instalación
2. Ejecuta `setup_portable.bat` desde el pendrive
3. Listo

---

## 🎯 ALTERNATIVAS: SI ALGO FALLA

### Si dice "Python no está instalado"

**Solución:**
1. Ve a https://www.python.org/downloads/
2. Descarga "Windows Installer (64-bit)" o (32-bit) 
3. Instala con ✅ "Add Python to PATH" marcado
4. Reinicia y vuelve a ejecutar `setup_portable.bat`

### Si dice "Error: module serial not found"

**Solución:**
1. Abre `PowerShell` o `Símbolo del sistema`
2. Escribe: `pip install pyserial`
3. Presiona Enter
4. Intenta de nuevo

### Si no se abre la GUI

**Prueba esto:**
1. Abre PowerShell en la carpeta del pendrive
2. Escribe: `python gui_qpod_config.py`
3. Presiona Enter
4. Mira qué error aparece

---

## 📊 ESTRUCTURA DE ARCHIVOS GENERADOS

Después de ejecutar `setup_portable.bat`, se crea:

```
Pendrive/
├── (archivos originales)
├── __pycache__/          ← Generado automáticamente (ignorar)
└── pyserial/             ← Biblioteca instalada
```

Esto es normal y necesario.

---

## 🎯 CÓMO COMPARTIR CON OTROS

**Si quieres pasar el pendrive a otro compañero:**

1. Puedes borrar la carpeta `__pycache__/` (opcional, libera espacio)
2. El resto déjalo igual
3. En el otro PC, simplemente haz doble-clic en `INICIAR_GUI.bat`
   - Si Python está instalado, funcionará directamente
   - Si no, ejecuta primero `setup_portable.bat`

---

## 🚀 OPCIÓN AVANZADA: PYTHON EMBEBIDO (Más portátil)

Si quieres que funcione **sin instalar Python** en otro PC:

1. Descarga Python embebido: https://www.python.org/downloads/ (busca "embedded")
2. Descomprime en una carpeta `python_embedded` en el pendrive
3. Modifica `INICIAR_GUI.bat` para usar esa ruta

Línea a agregar al inicio:
```bat
set "PYTHON_CMD=%~dp0python_embedded\python.exe"
set "PATH=%~dp0python_embedded;!PATH!"
```

*(Esto requiere un poquito más de espacio en el pendrive, ~100 MB)*

---

## ✅ VERIFICACIÓN

Para verificar que todo funciona:

1. Ejecuta `INICIAR_GUI.bat`
2. Espera a que se abra la ventana azul de la GUI
3. Debería verse así:
   - **Tab 1: Estado** (monitoreo de hardware)
   - **Tab 2: Calibración** (ángulos de servos)
   - Etc.

---

## 📞 TROUBLESHOOTING

| Problema | Solución |
|----------|----------|
| "Python no encontrado" | Instala Python 3.8+ y marca "Add Python to PATH" |
| "No se abre la GUI" | Ejecuta en PowerShell: `python gui_qpod_config.py` |
| "Error: serial not found" | Ejecuta: `pip install pyserial` |
| "El puerto serial no aparece" | Conecta el Arduino/USB antes de abrir la GUI |

---

## 📝 NOTA IMPORTANTE

- **No copies solo los .py** - necesitas también los archivos JSON de config
- **Mantén el pendrive enchufado** mientras usas la app (o copia a disco duro)
- **Los settings se guardan** en archivos JSON en la misma carpeta

¡Listo! Deberías poder llevar tu Q-POD Configurador a cualquier lado 🚀
