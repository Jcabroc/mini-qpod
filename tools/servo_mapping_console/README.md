# Consola Mini Q-POD Servo Mapping

Cliente mínimo para el firmware `nano_servo_mapping`. Usa `pyserial`, siempre
envía líneas terminadas en LF y mantiene `PING` cada 250 ms mediante un bloqueo
de escritura compartido. El registro se guarda por defecto en
`local_backups/servo_mapping_console.log`, fuera del control de versiones.

```powershell
python -m pip install pyserial
python tools/servo_mapping_console/console.py --port COM9 --baud 115200
```

Al conectar espera el arranque y consulta `STATUS`, `IMU` y `CONFIG`. Solo
acepta `HELP`, `STATUS`, `CONFIG`, `IMU`, `SELECT`, `ARM`, `MOVE`, `OFF` y `X`.
`X` es el apagado de emergencia; también se envía al cerrar, con Ctrl+C, ante
una excepción serial y cuando la IMU deja de estar saludable. No incluye EEPROM,
WALK, STAND, límites ni expansión.

En esta fase la consola solo permite confirmar `ARM 0 90`; exige confirmación
mostrando rango seguro, cuentas y microsegundos nominales. Después limita
`MOVE 0` a 89–91°. No reintenta ARM ni MOVE.
