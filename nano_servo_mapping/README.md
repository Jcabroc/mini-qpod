# Mini Q-POD Servo Mapping 1.0.0

Firmware independiente para medir el mapeo eléctrico → mecánico, sin IK,
movimientos colectivos, balance ni acceso a EEPROM. No sustituye ni modifica
`mini_qpod_mvp`. No se ha cargado ni probado físicamente.

## Cableado revisado antes de implementar

Según `mini_qpod_mvp/robot_config.h`, `gait_balance.h` y
`mini_qpod_pico_imu/mini_qpod_pico_imu.ino`:

| Función | Nano | Conexión |
|---|---|---|
| Host 115200 baud | UART USB D0/D1 | USB; no conectar la Pico aquí |
| IMU 38400 baud | D2 RX SoftwareSerial | Pico GP4 TX y GND común |
| TX ficticio SoftwareSerial | D8 | **Sin conexión física** |
| PCA9685 dirección 0x40 | A4 SDA / A5 SCL | I²C, lógica y GND común |
| LED | D5 | Encendido al enviar PWM individual |
| Buzzer | D4 | LOW, sin tonos |

No usar D3 → GP5 directamente: 5 V del Nano no son adecuados para la Pico.
Esta revisión verifica el pinout registrado, no el cableado físico presente.
No se añade un pin OE: no consta una conexión existente. La alimentación de
servos debe poder desconectarse físicamente, también durante reset/carga.
Mejora futura obligatoria: conectar `OE` del PCA9685 a un GPIO del Nano con
pull-up, de forma que las salidas permanezcan deshabilitadas durante arranque,
reinicio o fallo del Nano.

## Configuración RAM y significado de los ángulos

La siguiente tabla reproduce el `CONFIG` activo comunicado por el usuario,
no los defaults compilados del MVP ni una lectura nueva de EEPROM.
Margen global 5°; extremos seguros inclusivos.

| CH | Articulación | Mín / centro / máx eléctricos | Dirección histórica | Intervalo con margen |
|---|---|---|---|---|
| 0 | L1 COXA | 20 / 90 / 115 | +1 | 25–110 |
| 1 | L1 FÉMUR | 10 / 110 / 180 | +1 | 15–175 |
| 2 | L1 TIBIA | 25 / 90 / 180 | −1 | 30–175 |
| 3 | R1 COXA | 90 / 110 / 180 | +1 | 95–175 |
| 4 | R1 FÉMUR | 10 / 90 / 180 | −1 | 15–175 |
| 5 | R1 TIBIA | 0 / 90 / 170 | −1 | 5–165 |
| 6 | L2 COXA | 70 / 100 / 180 | +1 | 75–175 |
| 7 | L2 FÉMUR | 10 / 90 / 180 | +1 | 15–175 |
| 8 | L2 TIBIA | 0 / 100 / 160 | −1 | 5–155 |
| 9 | R2 COXA | 30 / 100 / 140 | −1 | 35–135 |
| 10 | R2 FÉMUR | 10 / 105 / 175 | −1 | 15–170 |
| 11 | R2 TIBIA | 40 / 115 / 180 | −1 | 45–175 |
| 12 | Cuello | 75 / 95 / 120 | +1 | 80–115 |

`ARM`/`MOVE` reciben grados **eléctricos absolutos**. No aplican dirección,
offset ni centro a la entrada: las direcciones se conservan como evidencia,
pendiente su correspondencia con las convenciones IK. Los intervalos con margen
son límites de software heredados, no una certificación de ausencia de colisiones.

Los centros físicos históricos CH0–CH11 son, por separado:
1505, 1700, 1590, 1645, 1555, 1635, 1690, 1635, 1565, 1590, 1600, 1725 µs.
No se convierten en centros eléctricos nuevos. La READY histórica (COXA
L1 −6°, R1 −8°, L2 +8°, R2 +6°, fémures 0°, tibias −10°) tiene una convención
todavía sin reconciliar con la IK y no se implementa aquí.

Conversión explícita, compatible con el redondeo y mapa del código MVP local:

```
a = trunc(deg + 0.5)                         # entradas válidas positivas
cuentas = 110 + trunc(a * 400 / 180)        # entero largo, no clamp
us_nominal = cuentas * (prescale + 1) / 25  # oscilador nominal 25 MHz
```

Se configura 50 Hz; se informa el prescaler leído del dispositivo, normalmente
121. Con él, 90° → 310 cuentas → 1512.80 µs nominales. No es una medición
del oscilador ni del pulso real. La rampa interna es de 10°/s, pasos máximos
0.2° cada 20 ms; la salida se cuantiza a grados enteros y cuentas. No se
promete velocidad física exacta del eje. Un bucle retrasado no recupera tiempo
con un salto grande. No se calcula trigonometría ni se duplica la IK.

## Protocolo

ASCII, mayúsculas, 115200 baud, líneas terminadas en LF (CRLF admitido).
Máximo 79 caracteres; líneas largas o corruptas se descartan completas.
Decimales con punto, sin exponentes, NaN ni infinito; número exacto de argumentos.
`X` es además un byte de emergencia inmediato, aun dentro de una línea incompleta.
No enviar varios comandos de movimiento en lote.

| Orden | Efecto |
|---|---|
| HELP | Enumera exclusivamente este protocolo y códigos |
| STATUS | Estado PWM, selección, canal activo, objetivo/salida, IMU, watchdog, último aborto |
| CONFIG | Tabla RAM con centro, conversión nominal, dirección y margen |
| IMU | Frescura, roll/pitch, edad y umbrales |
| SELECT ch | Apaga los 13 canales y selecciona uno; **no envía PWM activo** |
| ARM ch deg | Primera activación explícita: exige selección, IMU saludable y rango seguro |
| MOVE ch deg | Cambia el objetivo del único canal ya armado; rampa 10°/s |
| PING | Renueva heartbeat; nunca activa ni mueve |
| OFF | FULL_OFF en los 13 canales, elimina selección y armado |
| X | Igual apagado; no necesita LF |

`ARM` repetido se rechaza. `MOVE` antes de `ARM` se rechaza. Errores de sintaxis,
canal o rango conservan el objetivo y la salida anteriores, salvo que venza una
protección independiente: entonces se apaga. No hay recorte silencioso.
Después de OFF/aborto se requieren nueva selección y nuevo ARM; PING no rearma.

Cada respuesta tiene un sobre:
`REPLY cmd=... ch=... result=... accepted=... counts=... us_nominal=...`.
`ch=-1` y `NA` significan que no existe canal/ángulo aplicable; en un rechazo
`accepted=NA` significa que no se aceptó ningún ángulo nuevo. STATUS identifica
la salida conservada. CONFIG y STATUS/IMU/HELP añaden líneas de detalle al sobre.
CONFIG informa centros de referencia, **no** una aceptación para energizarlos.
El arranque emite nombre, versión y `build=local-fecha-hora` de compilación.

Resultados: 0 OK; 1 sintaxis; 2 canal; 3 no seleccionado; 4 ya armado;
5 no armado; 6 rango; 7 IMU; 8 bus I²C.
Abortos: 0 ninguno; 1 pérdida IMU; 2 inclinación; 3 watchdog host; 4 bus I²C.
La causa del último aborto se conserva incluso tras OFF o un nuevo ARM.
El fallo de bus queda bloqueado hasta reinicio; consultas y OFF siguen disponibles.

## Protecciones y límites de esta fase

Al arrancar se solicita FULL_OFF en los 13 canales antes y después de configurar
la frecuencia. SELECT también apaga todos; ARM vuelve a hacerlo antes del primer
pulso individual. Los canales restantes no sostienen ninguna postura.

IMU: `IMU,secuencia,roll,pitch*XX`, XOR hexadecimal del texto anterior al asterisco.
Solo tramas completas con checksum correcto, valores finitos y secuencia distinta
renuevan frescura. Se admite vuelta del contador de 16 bits; duplicados no renuevan.
Se ignora telemetría SENSOR. Edad **menor de 250 ms** y roll/pitch dentro de
±12° son obligatorios. A 250 ms sin trama o inclinación excesiva: apagado.
El watchdog de host apaga a **1000 ms** desde ARM o el último PING; MOVE y las
consultas no lo renuevan. El host debe enviar PING cada 200 ms mientras esté armado.
Las comparaciones de tiempo admiten el desbordamiento de millis().

No hay esperas de movimiento; las UART se atienden con lotes limitados. Apagado
significa envío de FULL_OFF en el mismo servicio de seguridad/comando, sujeto a
latencia UART/I²C. Wire tiene timeout de 3 ms por transacción. No es un circuito
de parada independiente: un bus averiado, MCU bloqueado o PCA que conserve estado
durante un reset puede impedir el apagado físico. Ante fallo I²C se intenta
apagar todo y se bloquea ARM, pero el estado físico no puede certificarse.

**ARM aplica inmediatamente el ángulo eléctrico solicitado.** Sin realimentación
de posición no existe punto inicial conocido para una rampa desde reposo. Suspender
el robot no elimina choques entre piezas. Confirmar físicamente un ángulo inicial
antes de ARM, disponer de corte de alimentación y no recorrer extremos a ciegas.
La rampa conservadora solo se aplica a MOVE después del primer pulso.

## Compilar y probar sin hardware

Dependencias: Arduino AVR Boards 1.8.6, Adafruit PWM Servo Driver Library 3.0.2
y Adafruit BusIO. Host: Python y `ziglang` (validado con 0.16.0).

```powershell
python -m pip install ziglang
& ./nano_servo_mapping/tests/run.ps1
arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328old nano_servo_mapping
python -B -m unittest discover -s tools/ik_simulator -p 'test_*.py' -v
python -B -m unittest discover -s tools/ik_pose_lab -p 'test_*.py' -v
python -B -m unittest discover -s tools/servo_calibrator/tests -p 'test_*.py' -v
python -B -m unittest discover -s esp32cam/tools/primitive-vision-monitor -p 'test_*.py' -v
git diff --check
```

Las pruebas C++ ejecutan el mismo parser, recepción IMU, límites y controlador
usado en AVR con un PCA simulado; no abren puertos. El ejecutable temporal se
crea fuera del repositorio. No prueban señales, cableado ni latencia física.

## Procedimiento futuro de carga y primera medición (no ejecutado)

1. Cortar físicamente la fuente de servos; robot suspendido y articulaciones
   apoyadas para que los doce canales apagados no provoquen una caída.
   Verificar el pinout anterior, masas, orientación IMU y corte accesible.
2. Tras revisar y autorizar esta versión, compilar y cargar por bootloader USB
   **solo el sketch nano_servo_mapping**, con placa Nano / ATmega328P Old Bootloader.
   Comando de referencia, sustituyendo el puerto confirmado:
   `arduino-cli upload -p COM_CONFIRMADO --fqbn arduino:avr:nano:cpu=atmega328old nano_servo_mapping`.
   No usar programador ISP, borrado de chip ni modificar fuses. Este sketch no
   incluye EEPROM.h ni accede a EEPROM. La conservación al cargar depende también
   del método de carga; el procedimiento usa bootloader, no borrado por ISP.
3. Con potencia aún desconectada, abrir 115200, verificar nombre/versión/build,
   HELP/CONFIG/STATUS/IMU. Debe indicar OFF, selected=-1, energized=-1,
   sin FATAL; verificar IMU saludable repetidamente. No continuar si falla.
4. Iniciar heartbeat PING cada 200 ms con un host que no envíe nada más
   automáticamente. El calibrador antiguo no se presume compatible con este protocolo.
   Enviar SELECT 0 y comprobar OFF y selected=0. Con instrumentos, confirmar que
   los otros doce canales carecen de pulsos antes de alimentar servos. Detenerse aquí
   y solicitar autorización explícita antes de enviar cualquier `ARM`.
5. Para CH0, 90° es el centro eléctrico registrado, no un centro mecánico aprobado.
   Confirmar que es una primera posición físicamente admisible; si no puede
   asegurarse, desacoplar la carga mecánica y establecer una referencia antes de
   energizar. Solo entonces conectar potencia y enviar ARM 0 90 explícitamente.
   Comparar respuesta (310 cuentas, µs según prescaler) y STATUS; verificar que
   solo CH0 está energizado. Cortar potencia ante cualquier anomalía.
6. Registrar yaw mecánico respecto de la referencia radial nominal de L1
   (+135° global, +X derecha, +Y frontal), junto a grados eléctricos, cuentas,
   pulso medido si disponible y sentido observado. No asumir que dirección +1
   prueba el signo de IK. Con espacio comprobado, MOVE 0 91, esperar llegada
   manteniendo PING, medir; MOVE 0 90 y medir retorno. OFF y verificar apagado.
   La dirección opuesta solo tras comprobar separación física. No buscar topes.
7. Antes de ampliar mediciones, comprobar corte por ausencia de PING, pérdida de
   IMU y OFF/X con carga desacoplada o potencia desconectada e instrumento sobre
   PWM. Comprobar todos los canales inactivos tras cada aborto. No inclinar el
   robot suspendido con servos cargados para ensayar el umbral.
8. CH1 y CH2 requieren referencias de fémur horizontal y ángulo relativo de
   tibia, medidas individualmente mediante SELECT y nuevo ARM. No energizar la
   pata completa. Guardar mediciones externamente; este firmware no las persiste.

El resultado pendiente es una tabla medida por canal que permita reconciliar
centro, offset y signo con la IK; no se implementa aún ese puente ni una marcha.
