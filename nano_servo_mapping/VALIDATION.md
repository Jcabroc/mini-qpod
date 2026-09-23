# Validación sin hardware — 2026-09-23

Rama `feature/ik-servo-mapping`, creada desde el estado limpio
`3d00207814a0123cb807f4cdc6271446b9082b00` de `feature/primitive-vision`.
La rama `feature/ik-simulator` apuntaba al mismo commit. No se alteró `main`.

Se revisó el pinout del MVP y el emisor Pico antes de implementar. La nueva
carpeta es independiente; no se modifica MVP, IK, Pose Lab ni configuración
existente. No se abrieron puertos seriales ni se cargó firmware. Sin commit/push.

## Resultados

- Arduino AVR 1.8.6, Adafruit PWM Servo Driver 3.0.2,
  `arduino:avr:nano:cpu=atmega328old`: compilación correcta.
- Flash: **16 276 / 30 720 bytes (52%)**.
- Variables globales SRAM: **942 / 2048 bytes (45%)**; quedan 1106 bytes
  para pila y variables locales. Es el informe estático del compilador, no una
  medición de profundidad máxima de pila ni prueba de ejecución sostenida.
- Núcleo C++ real compilado en PC con Zig 0.16.0: parser estricto, trama/checksum,
  frescura IMU, límites de los 13 canales, activación exclusiva, rechazo atómico,
  rampa, selección sin PWM, OFF/X, pérdida IMU, inclinación, watchdog,
  heartbeat, desbordamiento de millis y bloqueo por fallo de bus.
- IK: 12 pruebas correctas; Pose Lab: 5; calibrador existente: 28.
- Monitor de visión: script de simulación de parser correcto (no es una suite
  unittest; el descubrimiento informa cero casos aunque ejecuta sus asserts).
- `git diff --check`: correcto, incluyendo los archivos nuevos por intención
  de añadir al índice. No hay archivos ajenos incluidos.

## Pendiente antes de aceptar seguridad física

Verificar con alimentación de servos desconectada la identidad del firmware
después de una futura carga autorizada, señales FULL_OFF de los trece canales,
pinout real, cadencia IMU y tiempos de corte medidos. Comprobar que el host usado
mantiene PING y no envía comandos del calibrador anterior.

Los tests usan un PCA simulado; no certifican el estado eléctrico. Ante fallo de
I²C, STATUS indica `UNKNOWN_BUS_ERROR`; se solicita apagado y se bloquea nuevo
armado hasta reinicio. Sin cable OE independiente no se garantiza apagado ante
avería de bus o bloqueo del MCU. El corte de fuente sigue siendo necesario.

El primer ARM no tiene posición inicial conocida y puede producir un salto.
Los límites heredados con margen no prueban ausencia de colisiones. El
procedimiento detallado de CH0 y las referencias a medir están en README.md.
No se implementa una transformación mecánica desde datos todavía no medidos.
