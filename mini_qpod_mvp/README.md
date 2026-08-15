# Mini Q-Pod MVP

Firmware mínimo, estructurado y seguro para calibrar los 13 servos del Mini
Q-Pod, probar cada pata y ejecutar una primera marcha lenta. Este MVP no incluye
cinemática inversa.

La fuente de verdad técnica del robot es
[`../ESPECIFICACION_TECNICA.md`](../ESPECIFICACION_TECNICA.md). Este README explica
el uso práctico; cualquier cambio de hardware, límites, protocolo, seguridad o
arquitectura también debe registrarse en la especificación técnica.

> **Advertencia:** los límites iniciales provienen del proyecto anterior y no
> sustituyen una calibración física. Las primeras pruebas deben hacerse con el
> robot elevado, sin peso sobre las patas y con acceso inmediato a la energía.

## Archivos

- `mini_qpod_mvp.ino`: programa principal, consola y modos.
- `robot_config.h`: único archivo de configuración habitual.
- `servo_control.h`: límites, EEPROM, movimiento suave y PCA9685.
- `gait_balance.h`: postura, marcha y receptor UART de orientación desde la Pico.
- `../mini_qpod_pico_imu/mini_qpod_pico_imu.ino`: lectura de la MPU6050 en la Pico W.

Toda orden termina en `ServoController`, que recorta ángulos al rango seguro y
limita la velocidad antes de escribir en el PCA9685.

## Hardware y dependencias

- Arduino Nano o compatible (el firmware parte del hardware actual).
- PCA9685 en dirección I²C `0x40`.
- 13 servos en canales 0 a 12.
- Biblioteca Arduino `Adafruit PWM Servo Driver Library`.
- `Wire` y `EEPROM`, incluidas con el core de Arduino AVR.
- Raspberry Pi Pico W conectada por UART al Nano.
- MPU6050 en `0x68`, conectada por I2C a la Pico W.

La fuente de los servos debe ser externa, adecuada a su consumo y compartir GND
con Arduino. No alimente los 13 servos desde el pin de 5 V del Nano.

## Instalación

1. Abra `mini_qpod_mvp/mini_qpod_mvp.ino` en Arduino IDE.
2. Instale **Adafruit PWM Servo Driver Library** desde el gestor de bibliotecas.
3. Seleccione la placa y el procesador correctos del Nano.
4. Revise `robot_config.h`, especialmente canales y límites iniciales.
5. Compile y cargue el sketch.
6. Abra Monitor Serial a **115200 baud**, terminación “Nueva línea”.

El arranque siempre deja los servos apagados. Escriba `HELP` para ver comandos.

## Secuencia obligatoria de puesta en marcha

### 1. Inspección sin energía de servos

- Confirme canal y articulación de cada servo.
- Confirme que las patas pueden moverse sin cables atrapados.
- Eleve el robot para que ninguna pata soporte peso.
- Tenga preparada la desconexión de la fuente de servos.

### 2. Centro inicial

Conecte la alimentación de servos y ejecute:

```text
CALIB
CENTER
```

El movimiento está limitado a 35 grados por segundo. Si una articulación se
acerca a un choque, ejecute `OFF` o corte la alimentación.

### 3. Calibración de cada servo

Mueva un servo a la vez con incrementos pequeños:

```text
SERVO 0 88
SERVO 0 86
```

Después de encontrar valores seguros, registre mínimo, centro y máximo:

```text
LIMITS 0 50 90 150
```

La condición obligatoria es `min < center < max`. El firmware añade un margen
interno de 5 grados a cada extremo. Repita para los canales 0 a 12 y revise:

```text
CONFIG
```

Guarde solamente después de probar todos los canales:

```text
SAVE
OFF
```

`LOAD` recupera EEPROM. `DEFAULTS`, permitido solo en `OFF`, recupera los valores
compilados en `robot_config.h`; después hay que probarlos antes de ejecutar
`SAVE`.

### 4. Postura de pie

Apoye el robot en una superficie despejada y antideslizante:

```text
STAND
```

Verifique que ninguna articulación sature o fuerce la estructura. Si la postura
no es correcta, use `OFF`, vuelva a calibración y ajuste los límites o los valores
`DEFAULT_STAND_*` en `robot_config.h`.

### 5. Prueba individual de patas

Desde `STAND`, pruebe una sola pata y espere a que termine:

```text
LEG 0
LEG 1
LEG 2
LEG 3
```

Orden de nombres: `0=L1`, `1=R1`, `2=L2`, `3=R2`. Cada prueba desplaza primero
el cuerpo, levanta la pata, la adelanta, la baja y vuelve a postura estable.

No continúe si el cuerpo vuelca, una pata se arrastra, un servo alcanza el choque
o la fuente se reinicia.

### 6. Primera marcha

Solo después de superar las cuatro pruebas:

```text
STAND
UNLOCK_WALK
WALK
```

`UNLOCK_WALK` dura hasta reiniciar. La marcha usa el orden L1, R2, R1, L2 y una
sola pata elevada. Para detenerla:

```text
OFF
```

## IMU y balance

La MPU6050 no se conecta al Nano. La Pico W la lee por I2C, calcula roll/pitch
y envía una trama cada 20 ms al Nano. El cableado registrado es:

| Señal | Pico W | Nano |
|---|---:|---:|
| I2C SDA MPU6050 | GP0 | -- |
| I2C SCL MPU6050 | GP1 | -- |
| UART Pico TX -> Nano RX | GP4 | D2 |
| UART Nano TX -> Pico RX | GP5 | D3 (desconectar en este MVP) |
| Referencia eléctrica | GND | GND |

Ambos firmwares usan UART a 38400 baud. El formato es
`IMU,<secuencia>,<roll>,<pitch>*<CRC>`, donde CRC es el XOR de los bytes previos
al asterisco. Cargue primero `mini_qpod_pico_imu.ino` en la Pico y luego el MVP
en el Nano. Con el robot inmóvil y nivelado, ejecute `IMU` e incline manualmente
el cuerpo para comprobar ejes y signos antes de probar las patas.

La conexión usada por este MVP es unidireccional: GP4 de la Pico (3.3 V) puede
entrar a D2 del Nano. No conecte D3 del Nano directamente a GP5: la salida de 5 V
del Nano requiere adaptación de nivel antes de entrar a la Pico. El firmware del
Nano deja esa transmisión deshabilitada.

Con la IMU activa, una lectura inválida o inclinación superior a 12 grados aborta
el movimiento. También aborta si el Nano no recibe una trama válida durante 250
ms. La corrección de balance está limitada a 4 grados y también pasa por los
límites mecánicos.

La IMU estima inclinación; no mide directamente el centro de gravedad. El ajuste
real del soporte se realiza mediante `DEFAULT_BODY_SHIFT_GAIN` y debe validarse
físicamente para cada pata.

## Parámetros que se pueden ajustar

Mantenga pocos cambios por prueba:

- `DEFAULT_SERVOS`: límites y centros físicos.
- `DEFAULT_SAFETY_MARGIN_DEG`: margen contra choque.
- `DEFAULT_MAX_SPEED_DEG_S`: velocidad máxima.
- `DEFAULT_STAND_COXA_GAIN` y `DEFAULT_STAND_HEIGHT_GAIN`: postura.
- `DEFAULT_STEP_GAIN`: longitud articular del paso.
- `DEFAULT_LIFT_GAIN`: elevación.
- `DEFAULT_BODY_SHIFT_GAIN`: traslado previo del cuerpo.
- Tiempos `DEFAULT_*_MS`: duración de cada fase.
- Parámetros `DEFAULT_*TILT*` y `DEFAULT_BALANCE_*`: seguridad de IMU.

Cambiar los valores compilados no reemplaza automáticamente una configuración ya
guardada en EEPROM. Use `OFF`, `DEFAULTS`, repita las pruebas y finalmente `SAVE`.

## Criterio de aprobación del MVP

El firmware puede considerarse listo para continuar cuando:

1. Los 13 canales están identificados y calibrados.
2. Reiniciar conserva una configuración válida.
3. `CENTER` y `STAND` no producen choques.
4. Cada `LEG 0..3` completa diez ciclos sin pérdida de estabilidad.
5. `OFF` detiene cualquier prueba.
6. Con IMU activa, inclinación excesiva detiene la prueba.
7. La marcha completa varios ciclos lentos sin alcanzar límites.

El paso siguiente será corregir la geometría del desplazamiento de cuerpo usando
las medidas reales del robot y, una vez estable, introducir cinemática inversa.
