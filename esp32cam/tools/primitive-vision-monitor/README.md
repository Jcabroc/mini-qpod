# Q-POD Primitive Vision Monitor

Monitor local para Windows de la salida serie de la ESP32-CAM Stage 1. Usa Tkinter y pyserial; el parser está separado de la interfaz para poder cambiar el protocolo después.

## Ejecución

Con el entorno Python de ESP-IDF preparado:

```powershell
Set-Location tools\primitive-vision-monitor
& 'C:\Espressif\tools\python\v5.4.4\venv\Scripts\python.exe' monitor.py
```

Selecciona `COM25` (o el puerto que Windows asigne), `115200` y pulsa `CONNECT`. Al conectar, la herramienta genera un pulso de reset del ESP32-CAM-MB; el estado debe pasar de `WAITING DATA` a `RX OK` y el monitor serie debe mostrar el arranque y las líneas `MOTION`.

Prueba del parser sin hardware:

```powershell
& 'C:\Espressif\tools\python\v5.4.4\venv\Scripts\python.exe' test_parser.py
```

Las líneas desconocidas se conservan en el log, pero no afectan a la interfaz. `APPROACHING`, `RECEDING`, `CONFIDENCE`, `LIGHT` y bounding boxes quedan previstos para futuras líneas del protocolo.
