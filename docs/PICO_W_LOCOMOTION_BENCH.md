# Pico WH locomotion — conexiones y banco

Este documento describe el firmware `mini_qpod_pico_walk_mvp/`. No autoriza
cargarlo, conectar V+ de servos ni mover el robot sin una revisión física
posterior.

## Mapa de conexiones propuesto

```text
Pico WH (3,3 V)                        PCA9685
--------------                         -------
GP0  (I2C0 SDA) ---------------------> SDA
GP1  (I2C0 SCL) ---------------------> SCL
3V3 ----------------------------------> VCC lógico (*)
GND ----------------------------------> GND lógico

Fuente externa de servos (+) ---------> V+          (*)
Fuente externa de servos (GND) -------> GND --------+---- Pico GND

Pico WH (3,3 V)                        NRF24L01
--------------                         -------
GP16 (SPI0 RX/MISO) <----------------- MISO
GP17 (GPIO CSN) ----------------------> CSN
GP18 (SPI0 SCK) ----------------------> SCK
GP19 (SPI0 TX/MOSI) ------------------> MOSI
GP20 (GPIO CE) -----------------------> CE
3V3 ----------------------------------> VCC
GND ----------------------------------> GND
IRQ -----------------------------------> sin conexión por ahora (*)

Pico WH (3,3 V)                        ESP32-S3 SuperMini (posterior)
----------------                        --------------------------------
GP8  (UART TX reservado) -------------> RX
GP9  (UART RX reservado) <------------- TX
GND -----------------------------------> GND
```

`(*)` requiere confirmación física antes de conectar: verificar la serigrafía
del PCA9685, su VCC lógico, sus resistencias pull-up y que ninguna señal I²C
sea elevada a 5 V. Si la placa obliga pull-ups a 5 V, usar adaptación bidireccional
de nivel o una placa configurada a 3,3 V. V+ solo alimenta servos y permanece
separado del 3V3 de la Pico; todas las masas deben ser comunes. Confirmar además
la alimentación estable de 3,3 V del NRF24 y la ruta física de cada SPI/CE/CSN.

La UART GP8/GP9 queda solo reservada: no se inicializa y no hay código S3. GP9
coincide con el pin Touch de la estación de sensores histórica; si ese arnés
sigue conectado debe retirarse antes de usar la UART. Este mapa corresponde al
controlador de locomoción Pico WH.

## Procedimiento de banco previo a validación física

1. Sin USB ni alimentación de servos, inspeccionar continuidad, GND común,
   polaridad de V+, VCC lógico PCA y 3,3 V del NRF24 con documentación de cada
   módulo. No aplicar 5 V a ningún GPIO de la Pico.
2. Con el robot sin alimentación de servos, compilar solamente:
   `arduino-cli compile --libraries libraries --fqbn rp2040:rp2040:rpipicow mini_qpod_pico_walk_mvp`.
   Esta etapa no realiza carga ni abre puertos.
3. Antes de una futura carga autorizada, revisar que el código llama a
   `pwmOff()` inmediatamente después de `pca.begin()` y tras configurar 50 Hz.
   Verificar además `X` y el failsafe de 500 ms por radio en revisión de código.
4. En una futura sesión física aprobada, mantener V+ de servos desconectado y
   medir primero las líneas I²C/SPI. Solo después de registrar voltajes y PWM
   apagado podrá definirse una prueba de READY separada. Nunca usar este
   documento como autorización para energizar o mover.

## Verificación host

```powershell
arduino-cli compile --libraries libraries --fqbn arduino:avr:nano:cpu=atmega328old mini_qpod_ik_walk_mvp
arduino-cli compile --libraries libraries --fqbn rp2040:rp2040:rpipicow mini_qpod_pico_walk_mvp
python -m unittest tools.ik_walk_simulator.test_parity -v
```

La paridad compara el núcleo C++ con el simulador Python. No comprueba señales,
tiempo de cuadro en RP2040, radio real, PCA real ni movimiento físico.
