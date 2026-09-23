# Continuidad Mini Q-POD — oficina → casa

El repositorio es Git normal en GitHub y no depende de una sesión virtual:
`https://github.com/Jcabroc/mini-qpod.git`.

## Estado

- Rama: `feature/ik-servo-mapping`.
- Último commit publicado: `51da32f` (`fix: pace console after Nano startup`).
- `main` no se ha fusionado ni modificado.
- En el PC de oficina hay cambios locales pendientes en `tools/servo_mapping_console/console.py` y `ABRIR_CONSOLA_SERVOS.bat`; revisar y publicar antes de usarlos en casa.
- El Nano tiene cargado `nano_servo_mapping`; no se ha vuelto a cargar después de `51da32f`.

Clonar en casa:

```powershell
git clone https://github.com/Jcabroc/mini-qpod.git
cd mini-qpod
git switch --track origin/feature/ik-servo-mapping
git status
```

Para una copia ya clonada:

```powershell
git fetch --all --prune
git switch feature/ik-servo-mapping
git pull --ff-only
```

## Trabajo realizado

La IK Python está en `tools/ik_simulator/` y el Pose Lab en `tools/ik_pose_lab/`.
La geometría oficial es: chasis 93 × 93 mm, COXA 42.294 mm, FÉMUR 60.611 mm,
TIBIA 88.714 mm; origen en el centro geométrico, +X derecha, +Y frontal y +Z
arriba. El yaw es mecánico/radial, no eléctrico. Las restricciones y capturas CAD
están en `docs/ik_constraints.md` y `docs/reference/cad/`; las cotas CAD no son
límites eléctricos.

`nano_servo_mapping/` es firmware independiente de `mini_qpod_mvp/`. No usa
EEPROM y no incluye `SAVE`, `LOAD`, `DEFAULTS`, `EXPAND`, `STAND`, `LEG` ni
`WALK`. Arranca con los 13 canales apagados; `SELECT` no energiza y solo `ARM`
puede iniciar PWM. Mantiene IMU reciente (<250 ms), inclinación ±12°, watchdog
de host de 1 s, apagado `OFF`/`X`, activación exclusiva y rampa `MOVE` de 10°/s.
El primer `ARM` aplica inmediatamente el ángulo.

`tools/servo_mapping_console/` usa `pyserial`, LF, log con timestamps, espera el
banner y las 13 líneas CONFIG antes de `PING` cada 250 ms. Bloquea localmente
`ARM` a CH0/90° y `MOVE` a 89–91°. `X` se envía al cerrar, con Ctrl+C, ante error
serial o IMU no saludable.

Instalación y ejecución:

```powershell
python -m pip install -r tools/servo_mapping_console/requirements.txt
python -m tools.servo_mapping_console.console --port COM9 --baud 115200 --select 0
```

`ABRIR_CONSOLA_SERVOS.bat` abre la misma herramienta en una ventana visible; el
prompt esperado es `qpod>`. `X` y Enter son la emergencia. Los logs van fuera del
repositorio, en `hardware_backups/`.

## Hardware y respaldo

El Nano de oficina está en COM9, ATmega328P (`0x1e950f`), bootloader antiguo;
avrdude usa protocolo `arduino` a 57600. La aplicación usa USB a 115200. El Pico
envía IMU por D2 a 38400; PCA9685 usa A4/A5; LED D5 y buzzer D4.

El respaldo de solo lectura está fuera del repo en:
`C:\Users\José M Caballero\Desktop\jRobot\Codex\hardware_backups\mini-qpod\nano_before_servo_mapping_20260923_140617\`
(`flash.hex`, `eeprom.hex`, log y hashes). No se escribieron EEPROM ni fusibles.

La primera prueba física quedó detenida antes de alimentar servos: L1 suspendida,
sostenida y radial; `SELECT 0`; `pwm=OFF`, `energized=-1`, `imu=1`, `tilt_ok=1`.
No se ha ejecutado `ARM` ni `MOVE` con 6 V conectados.

## Continuación segura en casa

1. Clonar la rama y comprobar un árbol limpio.
2. Revisar los cambios locales pendientes de la consola y publicarlos antes de
   compartir la herramienta.
3. Con Nano solo por USB, comprobar banner, `STATUS`, `IMU`, `CONFIG` y
   `SELECT 0`; no conectar 6 V todavía.
4. Con L1 suspendida y corte accesible, conectar 6 V sin `ARM`; verificar `pwm=OFF`.
5. Solo con autorización explícita, confirmar `ARM 0 90`. No probar CH1/CH2 ni
   ampliar límites. Ante cualquier movimiento, error o pérdida IMU, escribir `X`
   y cortar físicamente los 6 V.

No trabajar en `main`, no cargar otro firmware, no tocar EEPROM y no reinterpretar
los centros históricos en microsegundos como calibración mecánica.
