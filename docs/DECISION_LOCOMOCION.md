# Decisión de locomoción — Pico W/RP2040

**Estado:** adoptada en firmware; carga, cableado y validación física
deliberadamente fuera de este cambio.

## Decisión

La **Raspberry Pi Pico W con RP2040** queda seleccionada como plataforma de
locomoción. `mini_qpod_pico_walk_mvp/` implementa su envoltura usando el núcleo
compartido; el Nano ATmega328P conserva una envoltura de referencia. Ninguno se
carga ni se modifica físicamente como parte de esta decisión.

La identificación está respaldada por los dos sketches de Pico y el FQBN
`rp2040:rp2040:rpipicow`; por tanto el modelo documentado es Pico W con RP2040.
No hay lectura de serial, fotografía de serigrafía ni revisión física registrada:
la variante/revisión de la placa instalada sigue **sin confirmación física**.

## Evidencia de capacidad

| Medida compilada en esta base | Resultado | Lectura |
|---|---:|---|
| Nano, `mini_qpod_ik_walk_mvp` | 30.656/30.720 B flash (99%); 1.518/2.048 B RAM (74%) | Restan 64 B de flash y 530 B de RAM: no queda margen responsable para extender locomoción. |
| Pico W, `mini_qpod_pico_sensor_station` | 324.764/2.093.056 B flash (15%); 70.312/262.144 B RAM (26%) | Es una referencia de capacidad de la plataforma, no una compilación de locomoción. |

El RP2040 dispone de dos Cortex-M0+ hasta 133 MHz y 264 kB de SRAM; el Nano
ATmega328P opera a 16 MHz con 2 kB de SRAM y 32 kB de flash (2 kB reservados
para bootloader). Véanse las especificaciones oficiales de
[RP2040](https://www.raspberrypi.com/documentation/microcontrollers/microcontroller-chips.html)
y [Arduino Nano](https://store.arduino.cc/products/arduino-nano).

La marcha actual llama a la actualización a 25 Hz (presupuesto de 40 ms) y, en
cada cuadro activo, calcula transformaciones e IK de las cuatro patas, usa
trigonometría y raíz cuadrada, construye la trayectoria y escribe los 12
canales del PCA9685. No hay medición de tiempo de peor caso sobre el Nano ni
sobre la Pico. La diferencia de frecuencia y memoria favorece a la Pico, pero
el RP2040 sigue siendo Cortex-M0+ y no debe asumirse una FPU de hardware:
cualquier implementación futura debe medir el tiempo de cuadro, I²C y radio
antes de validar movimiento.

## Adaptación que será necesaria después

No se realiza aquí ninguna migración física. La futura validación deberá conservar, sin reinterpretar,
la geometría y la calibración definidas en
[ik_constraints.md](ik_constraints.md) y
[ESPECIFICACION_TECNICA.md](../ESPECIFICACION_TECNICA.md).

| Interfaz | Estado actual Nano | Trabajo futuro para Pico W |
|---|---|---|
| PCA9685 | I²C en A4/A5, dirección `0x40`, PWM a 50 Hz | Mover I²C a dos GPIO libres, conservar dirección/frecuencia/tabla; comprobar VCC y pull-ups para que SDA/SCL nunca superen 3,3 V o añadir adaptación de nivel. V+ de servos continúa separado. |
| NRF24L01 | RF24 con CE D9, CSN D10 y SPI AVR; canal 76, 250 kbps, dirección y paquete PS2J existentes | Asignar SPI, CE y CSN compatibles con la Pico; portar y comprobar la biblioteca RF24, modo de payload fijo y failsafe sin cambiar el contrato PS2J. El módulo es de lógica 3,3 V; verificar alimentación y cableado real. |
| Sensores/IMU | Pico W usa GP0/GP1 para I²C y GP4/GP5 para UART; estación de sensores ya ocupa más GPIO | Congelar un mapa de pines completo antes de escribir firmware de locomoción; decidir qué telemetría sigue local y eliminar el UART Nano↔Pico solo cuando la migración esté validada. |

## Paridad C++ resuelta

Se instaló `ziglang` 0.16.0 y el arnés
`tools/ik_walk_simulator/test_parity.py` ahora detecta Zig cuando no hay
`g++`/`clang++`. Las **9** pruebas que realmente existen en `main` pasaron:
READY, trayectoria, fases, margen de soporte, paquete Control Lite, presets y
ciclos completos cuadro a cuadro. El conteo anterior de 10 era incorrecto.
Esta evidencia prueba equivalencia Python/C++ host, no temporización de MCU ni
comportamiento físico.

## Siguiente paso concreto

Antes de implementar la migración, cerrar la preparación de la etapa 3:
confirmar físicamente la revisión de la Pico W, el nivel lógico de la placa
PCA9685, el cableado NRF24 y el corte de energía. Después crear un diseño de
pines y un plan de pruebas de banco para Pico, sin energizar servos hasta que
ese diseño sea revisado.
