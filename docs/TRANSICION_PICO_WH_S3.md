# Transición Pico WH — ESP32-S3

La Pico WH queda dedicada a IK, marcha, PCA9685 y NRF24/Control Lite. La
ESP32-S3 SuperMini sustituirá sensores/IMU en una etapa posterior; el texto
«ESP32-S3 FH4R2» procede de imágenes del proveedor y debe verificarse en la
placa física antes de usarlo.

## Cableado actual de la Pico: revisión documental

| Acción | Cable o pin | Estado |
|---|---|---|
| Conservar | GND común de la lógica, PCA9685 y fuente de servos | Requisito de seguridad confirmado en documentación; confirmar continuidad física. |
| Conservar | USB de la Pico WH para futura programación/serial | Documentado; no se usa para alimentar servos. |
| Retirar de GP0/GP1 | SDA/SCL del MPU6050 histórico (`0x68`) | Confirmado en código histórico; comprobar que son esos dos cables en el robot antes de retirarlos. GP0/GP1 pasan a PCA9685. |
| Retirar de GP4 | UART Pico TX → Nano D2 | Documentado en firmware IMU; confirmar cable físico. Ya no pertenece a la arquitectura de locomoción. |
| No conectar | GP5 ← Nano D3 | Estaba reservado en código y declarado desconectado; verificar que siga sin cable. |
| Retirar si existe | Arnés Touch en GP9 de la estación de sensores | Documentado en `mini_qpod_pico_sensor_station`; pendiente confirmar si esa estación es la instalada. GP9 se reserva para S3 RX. |
| Conectar después de inspección | PCA9685: GP0/SDA, GP1/SCL, GND y VCC lógico 3,3 V | Mapa de firmware; confirmar VCC/pull-ups de la placa antes de conectar. V+ de servos sigue desconectado. |
| Conectar después de inspección | NRF24: GP16 MISO, GP17 CSN, GP18 SCK, GP19 MOSI, GP20 CE, 3V3 y GND | Mapa de firmware; confirmar pinout y alimentación 3,3 V del módulo. |
| Reservar, no conectar todavía | GP8 TX ↔ RX S3 y GP9 RX ↔ TX S3, con GND común | No hay firmware ni protocolo S3 en esta etapa. |

Los demás pines de la estación histórica (GP2/3, GP6/7, GP10, GP13–15 y
GP26/27) solo constan en su sketch. No deben suponerse conectados ni retirarse
sin inspección física.

El próximo objetivo físico es READY en Pico WH y, solo después de su cierre
registrado, Caminata lenta. Este documento no autoriza cargar firmware,
energizar V+ ni mover servos.
