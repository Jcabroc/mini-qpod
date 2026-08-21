# Calibrador provisional Mini Q-POD

Aplicación local para Windows, hecha con Python, Tkinter y PySerial. Es una
herramienta de calibración puntual: no es POD Station, no abre un navegador, no
usa servidor, base de datos, Docker, React ni Electron.

## Instalar y ejecutar en Windows

Abra PowerShell en esta carpeta y ejecute:

```powershell
py -m pip install -r requirements.txt
py servo_calibrator.py
```

Tkinter suele venir incluido con Python para Windows. Si `py` no existe, instale
Python 3 desde python.org y marque **Add Python to PATH**.

Para revisar la interfaz sin hardware, elija `SIMULATED (sin hardware)` y pulse
**Conectar a 115200**. El simulador vive en memoria: nunca abre un puerto COM ni
controla servos.

## Flujo seguro de la interfaz

1. Conecte a 115200. La aplicación envía únicamente `OFF`, `STATUS`, `IMU` y
   `CONFIG`; no habilita ni mueve servos.
2. Confirme `IMU: OK` y el modo `SAFE_OFF`.
3. Pulse `CALIB (PWM OFF)`.
4. Elija una articulación y pulse `SELECT`. Esto no genera PWM.
5. Revise el primer ángulo. Pulse `ENABLE` solo con el robot elevado, potencia
   de servos bajo control físico y el horn libre de choque. El primer PWM puede
   producir movimiento inmediato.
6. Use `CENTER`, los pasos de ±1°/±5° o un ángulo manual. Solo el canal activo
   puede moverse.
7. Pulse el botón rojo `SERVOS OFF` ante cualquier comportamiento inesperado.
8. Para editar `LIMITS`, el calibrador manda primero `OFF`; pruebe de nuevo los
   límites antes de `SAVE`. `SAVE` y `LOAD` requieren `SAFE_OFF`.

La terminal acepta exclusivamente `OFF`, consultas, los comandos de calibración,
`SAVE`, `LOAD` y `HELP`. Bloquea `STAND`, `LEG`, `UNLOCK_WALK`, `WALK`,
`DEFAULTS` y cualquier comando desconocido.

Mientras está conectada, la aplicación envía `PING` cada 250 ms. Con un canal
activo en calibración, el Nano corta PWM y emite `[ABORT] HOST_TIMEOUT` si no
recibe actividad válida durante 1000 ms. Al cerrar normalmente, la aplicación
intenta enviar `OFF` antes de cerrar el puerto. Si falla una lectura o escritura,
bloquea los controles e indica que el estado físico no puede confirmarse.

## Semántica vigente del MVP

- El centro eléctrico es la referencia física de **90°** usada al montar el horn.
- `centerAngle` del firmware es la posición neutral/inicial configurable.
- `minAngle` y `maxAngle` son límites mecánicos; el firmware informa y aplica
  límites seguros efectivos con el margen global.
- `direction` es metadato existente para semántica articular futura; no invierte
  el ángulo eléctrico manual.

No se amplió la estructura EEPROM en esta etapa.

## Pruebas sin hardware

```powershell
py -m unittest discover -s tests -v
```

Las pruebas utilizan el puerto serial simulado y verifican la lista blanca local,
el apagado, selección sin PWM, activación individual, límites, aborto IMU y el
contrato estático del firmware. No abren puertos COM reales.
