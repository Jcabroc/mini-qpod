# Mini Q-POD Pico WH Locomotion MVP

Implementación de locomoción para la Raspberry Pi Pico WH / RP2040 identificada
por serigrafía. Reutiliza una
sola biblioteca local: `libraries/MiniQpodWalkCore/`. Allí viven IK, geometría,
calibración, presets, límites y protocolo Control Lite, compartidos con el
sketch Nano de referencia. Este firmware no se ha cargado ni probado con robot.

Compilar sin cargar:

```powershell
arduino-cli compile --libraries libraries --fqbn rp2040:rp2040:rpipicow mini_qpod_pico_walk_mvp
```

La Pico inicializa el PCA9685 y el NRF24, pero arranca con los 16 PWM apagados.
READY es la única entrada que puede comenzar a escribir canales de patas;
`X` apaga de inmediato, y la pérdida de radio hace que la marcha termine en
apoyo. Los límites y los rechazos IK no se recortan ni se amplían.

GP8/GP9 quedan reservados para una UART futura con ESP32-S3 SuperMini; no se
inicializan ni implementan funciones de S3 en este firmware.

El cableado, requisitos de 3,3 V y procedimiento de banco están en
[docs/PICO_W_LOCOMOTION_BENCH.md](../docs/PICO_W_LOCOMOTION_BENCH.md).
