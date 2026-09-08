# Especificación técnica oficial — Mini Q-Pod

| Campo | Valor |
|---|---|
| Documento | Especificación técnica y fuente de verdad del robot |
| Proyecto | Mini Q-Pod |
| Repositorio | `https://github.com/Jcabroc/mini-qpod.git` |
| Rama oficial actual | `main` |
| Versión del documento | `0.1.0` |
| Firmware oficial en desarrollo | `mini_qpod_mvp/mini_qpod_mvp.ino` |
| Estado | MVP compilable, pendiente de validación física |
| Última actualización | 2026-08-12 |

> Este documento es la fuente de verdad técnica del Mini Q-Pod. Toda decisión
> que cambie hardware, conexiones, límites, seguridad, comportamiento, protocolo,
> dependencias o pruebas debe quedar registrada aquí en el mismo cambio de código.

## 1. Objetivo del sistema

Mini Q-Pod es un robot cuadrúpedo de cuatro patas, tres articulaciones por pata y
un servo adicional para el cuello. El objetivo de la versión actual es conseguir
una plataforma segura y calibrable que:

1. Controle 13 servos mediante PCA9685.
2. Respete límites mecánicos configurables por articulación.
3. Limite la velocidad antes de enviar órdenes físicas.
4. Permita calibrar y guardar parámetros sin reescribir todo el firmware.
5. Adopte una postura base reproducible.
6. Desplace el cuerpo antes de levantar una pata.
7. Pruebe individualmente las cuatro patas.
8. Ejecute una marcha lenta con una sola pata elevada.
9. Incorpore posteriormente estabilización con IMU y cinemática inversa.

La cinemática inversa no pertenece al alcance del MVP actual.

### Modelo matemático independiente en PC (2026-09-08)

`tools/ik_simulator/` contiene la IK/FK y sus pruebas independientes del firmware.
Las medidas confirmadas desde el STEP original son: separación entre centros
COXA de 93.0 mm en ancho y largo; COXA=42.294 mm, FEMUR=60.611 mm y
TIBIA=88.714 mm, centro a centro. Origen en el centro geométrico del cuerpo,
+X derecha, +Y frontal, +Z arriba; montaje COXA en Z=0 como supuesto del modelo.
Montajes (X mm, Y mm, yaw): L1=(-46.5,+46.5,+135°),
R1=(+46.5,+46.5,+45°), L2=(-46.5,-46.5,-135°), R2=(+46.5,-46.5,-45°).
El yaw es mecánico/radial alrededor de +Z, desde +X hacia +Y.
La conversión a ángulos eléctricos por canal se implementará en una capa
posterior. Este modelo no cambia EEPROM, límites ni sketches de movimiento.
Convenciones, ecuaciones, selección de ramas y comando de pruebas se documentan
en `tools/ik_simulator/README.md`; no se declara validación física.

La interpretación de referencias CAD y la separación entre geometría ideal,
calibración eléctrica, sobre mecánico y colisiones se registra en
`docs/ik_constraints.md`. `tools/ik_simulator/validation.py` agrega avisos de
fronteras singulares y comprobaciones físicas pendientes; no aplica como topes
las cotas ilustrativas CAD ni habilita movimientos físicos.

## 2. Política de fuente de verdad

### 2.1 Archivos oficiales

El desarrollo nuevo debe realizarse en:

```text
mini_qpod_mvp/
├── mini_qpod_mvp.ino
├── robot_config.h
├── servo_control.h
├── gait_balance.h
└── README.md
```

Responsabilidades:

- `mini_qpod_mvp.ino`: entrada, estados, consola serial y coordinación.
- `robot_config.h`: configuración que normalmente modifica el operador.
- `servo_control.h`: límites, movimiento suave, PCA9685 y EEPROM.
- `gait_balance.h`: postura, desplazamiento, elevación, marcha e IMU.
- `README.md`: instalación y procedimiento práctico de operación.
- `ESPECIFICACION_TECNICA.md`: requisitos, decisiones y estado técnico oficial.

### 2.2 Código histórico

Los demás sketches, GUI, modelos, planillas y documentos se consideran material
histórico o de referencia hasta que una sección de este documento los declare
oficiales. No se deben copiar comportamientos históricos al firmware oficial sin
documentar su origen y validarlos nuevamente.

El sketch usado como principal referencia de comportamiento fue:

```text
qpod_mini_serial_gui_gait/qpod_mini_serial_gui_gait.ino
```

Los límites iniciales del MVP proceden de `default_config.json`, pero todavía se
clasifican como **provisionales**.

### 2.3 Regla de actualización

Un cambio de código no está técnicamente completo si afecta este documento y no
lo actualiza. Como mínimo se debe revisar:

- versión y fecha;
- tabla de hardware;
- tabla de servos;
- parámetros predeterminados;
- estados y protocolo serial;
- riesgos o supuestos;
- pruebas ejecutadas;
- historial de decisiones.

## 3. Estado de verificación

Se usan tres estados:

- **CONFIRMADO:** respaldado por código y/o inspección física.
- **PROVISIONAL:** heredado del proyecto anterior, requiere prueba física.
- **PENDIENTE:** falta información o implementación.

| Elemento | Estado | Evidencia/observación |
|---|---|---|
| Arduino Nano ATmega328P | PROVISIONAL | Firmware compilado para Nano old bootloader; confirmar placa física |
| PCA9685 `0x40` | CONFIRMADO EN CÓDIGO | Usado en firmware histórico y MVP |
| 13 servos, canales 0–12 | CONFIRMADO EN CÓDIGO | Doce articulaciones y cuello |
| Fuente externa de servos | PENDIENTE | Documentar tensión, corriente y modelo |
| NRF24L01 | HISTÓRICO | Existe en firmware anterior; fuera del MVP inicial |
| Mando PS2J | HISTÓRICO | Existe en firmware anterior; fuera del MVP inicial |
| Modelo de IMU | CONFIRMADO POR USUARIO | MPU6050 conectada a la Pico W |
| MPU6050 `0x68` | CONFIRMADO POR USUARIO | La Pico realiza la lectura I2C |
| Límites mecánicos | PROVISIONAL | Valores heredados, faltan pruebas servo por servo |
| Postura base | PROVISIONAL | Algoritmo compilado, falta prueba física |
| Elevación individual | PROVISIONAL | Secuencia compilada, falta prueba física |
| Marcha lenta | PROVISIONAL | Secuencia compilada, bloqueada hasta pruebas |
| Balance activo | PROVISIONAL | Transporte Pico-Nano implementado; faltan validar ejes y signos |
| Cinemática inversa | PENDIENTE | Etapa posterior al MVP estable |

## 4. Hardware y conexiones

### 4.1 Controlador principal

| Propiedad | Valor actual |
|---|---|
| Plataforma objetivo | Arduino Nano / ATmega328P |
| Baudrate serial | 115200 |
| Bus de servos | I²C mediante PCA9685 |
| Dirección PCA9685 | `0x40` |
| Frecuencia PWM | 50 Hz |
| Pulso lógico mínimo | 110 cuentas PCA9685 |
| Pulso lógico máximo | 510 cuentas PCA9685 |
| LED | Pin D5 |
| Buzzer | Pin D4 |

La Raspberry Pi Pico W concentra los sensores. La orientación llega al Nano por
UART software a 38400 baud: GP4/TX Pico hacia D2/RX Nano. El retorno D3/TX Nano
hacia GP5/RX Pico queda desconectado en el MVP porque requiere adaptación de nivel
de 5 V a 3.3 V. Ambas placas deben compartir GND.

Los valores de pulso son una conversión global de 0–180°. Los límites de cada
articulación se aplican antes de esta conversión.

### 4.2 Alimentación

Los servos deben usar una fuente externa dimensionada para la corriente conjunta.
Arduino, PCA9685 y fuente de servos deben compartir GND. Está prohibido alimentar
los 13 servos desde el pin de 5 V del Nano.

Datos pendientes de registrar tras inspección física:

- modelo y tensión nominal de cada tipo de servo;
- tensión real de la fuente;
- corriente continua y máxima disponible;
- protecciones, fusible y método de desconexión;
- modelo de PCA9685 y disposición de alimentación.

### 4.3 IMU

La MPU6050 en `0x68` está conectada por I2C a la Pico W (SDA GP0, SCL GP1).
La Pico calcula roll y pitch mediante filtro complementario y envía al Nano la
trama `IMU,<secuencia>,<roll>,<pitch>*<CRC>` cada 20 ms. El CRC es un XOR de los
bytes anteriores al asterisco. El Nano considera inseguro un enlace sin trama
válida durante 250 ms.

Antes de habilitar pruebas con movimiento debe confirmarse:

- tensión lógica y de alimentación;
- orientación física de los ejes;
- signos de roll y pitch;
- offsets en reposo.

La IMU estima inclinación y movimiento; no mide directamente el centro de gravedad.

## 5. Mapa oficial de articulaciones

Convención provisional de patas:

- `L1`: izquierda delantera.
- `R1`: derecha delantera.
- `L2`: izquierda trasera.
- `R2`: derecha trasera.

Esta orientación debe verificarse observando físicamente el robot.

| Índice | Canal | Nombre | Pata | Articulación | Mín. | Centro | Máx. | Dirección | Estado |
|---:|---:|---|---|---|---:|---:|---:|---:|---|
| 0 | 0 | L1_COXA | L1 | Coxa | 50 | 90 | 150 | +1 | PROVISIONAL |
| 1 | 1 | L1_FEMUR | L1 | Fémur | 10 | 95 | 180 | +1 | PROVISIONAL |
| 2 | 2 | L1_TIBIA | L1 | Tibia | 0 | 100 | 180 | -1 | PROVISIONAL |
| 3 | 3 | R1_COXA | R1 | Coxa | 30 | 90 | 130 | +1 | PROVISIONAL |
| 4 | 4 | R1_FEMUR | R1 | Fémur | 0 | 90 | 170 | -1 | PROVISIONAL |
| 5 | 5 | R1_TIBIA | R1 | Tibia | 0 | 90 | 180 | -1 | PROVISIONAL |
| 6 | 6 | L2_COXA | L2 | Coxa | 40 | 90 | 140 | +1 | PROVISIONAL |
| 7 | 7 | L2_FEMUR | L2 | Fémur | 10 | 100 | 180 | +1 | PROVISIONAL |
| 8 | 8 | L2_TIBIA | L2 | Tibia | 5 | 100 | 180 | -1 | PROVISIONAL |
| 9 | 9 | R2_COXA | R2 | Coxa | 50 | 90 | 130 | -1 | PROVISIONAL |
| 10 | 10 | R2_FEMUR | R2 | Fémur | 15 | 90 | 180 | -1 | PROVISIONAL |
| 11 | 11 | R2_TIBIA | R2 | Tibia | 0 | 90 | 180 | -1 | PROVISIONAL |
| 12 | 12 | CUELLO | — | Cuello | 45 | 90 | 135 | +1 | PROVISIONAL |

La columna `Dirección` está reservada para semántica articular y futura IK. El
MVP actual envía ángulos físicos calibrados y no invierte automáticamente el valor.

## 6. Configuración oficial del MVP

| Parámetro | Valor | Unidad | Estado |
|---|---:|---|---|
| Margen mecánico | 5 | grados por extremo | PROVISIONAL |
| Velocidad máxima | 35 | grados/s | PROVISIONAL |
| Periodo de actualización | 20 | ms | CONFIRMADO EN CÓDIGO |
| Ganancia coxa en postura | 0.30 | 0–1 | PROVISIONAL |
| Ganancia altura en postura | 0.60 | 0–1 | PROVISIONAL |
| Ganancia de paso | 0.20 | 0–1 | PROVISIONAL |
| Ganancia de elevación | 0.25 | 0–1 | PROVISIONAL |
| Desplazamiento del cuerpo | 0.08 | 0–1 | PROVISIONAL |
| Fase SHIFT | 500 | ms | PROVISIONAL |
| Fase LIFT | 350 | ms | PROVISIONAL |
| Fase SWING | 450 | ms | PROVISIONAL |
| Fase DROP | 350 | ms | PROVISIONAL |
| Fase SETTLE | 400 | ms | PROVISIONAL |
| Inclinación máxima | 12 | grados | PROVISIONAL |
| Ganancia balance proporcional | 0.35 | grados/grado | PROVISIONAL |
| Corrección máxima de balance | 4 | grados | PROVISIONAL |

El límite operativo efectivo es:

```text
[mínimo mecánico + margen, máximo mecánico - margen]
```

Toda orden debe pasar por `ServoController::setTarget()`. Ningún módulo nuevo
puede escribir directamente en el PCA9685 durante operación normal.

## 7. Persistencia de configuración

La EEPROM guarda:

- identificador mágico `0x514D`;
- versión de estructura `1`;
- margen de seguridad;
- configuración de los 13 servos;
- checksum de integridad.

Si la EEPROM es inválida, el firmware conserva los valores compilados. Los cambios
realizados con `LIMITS` viven en RAM hasta ejecutar `SAVE`.

Reglas:

1. No guardar límites sin probar el movimiento con el robot elevado.
2. Incrementar la versión de EEPROM cuando cambie la estructura almacenada.
3. Documentar toda migración o incompatibilidad.
4. `DEFAULTS` solo se admite con los servos apagados.

## 8. Máquina de estados

| Estado | Propósito | Entrada típica | Salida segura |
|---|---|---|---|
| `SAFE_OFF` | Servos sin PWM | Arranque, `OFF`, aborto | Permanece apagado |
| `CALIBRATION` | Ajuste individual | `CALIB` | `OFF` |
| `STAND_MODE` | Postura base | `STAND` | `OFF` |
| `LEG_TEST` | Ciclo de una pata | `LEG 0..3` | Vuelve a `STAND_MODE` |
| `WALK_MODE` | Marcha lenta | `UNLOCK_WALK`, `WALK` | `OFF` o aborto |

La marcha permanece bloqueada después de cada reinicio. `UNLOCK_WALK` solo la
habilita temporalmente y debe ejecutarse después de probar las cuatro patas.

## 9. Secuencia actual de movimiento

La marcha utiliza el orden:

```text
L1 → R2 → R1 → L2
```

Cada paso se divide en:

```text
SHIFT → LIFT → SWING → DROP → SETTLE
```

- `SHIFT`: desplaza el cuerpo hacia las tres patas de apoyo.
- `LIFT`: eleva fémur y tibia de la pata activa.
- `SWING`: adelanta la coxa manteniendo la pata elevada.
- `DROP`: baja la pata.
- `SETTLE`: espera transferencia de carga antes de continuar.

La geometría del desplazamiento es todavía heurística. Debe ajustarse mediante
pruebas físicas antes de clasificarse como confirmada.

## 10. Protocolo serial oficial

Configuración: **115200 baud**, comandos terminados en nueva línea.

| Comando | Condición | Acción |
|---|---|---|
| `HELP` | Cualquiera | Lista comandos |
| `STATUS` | Cualquiera | Modo, PWM, canal seleccionado/activo, IMU, fase y pata |
| `CONFIG` | Cualquiera | Mínimo, centro, máximo, dirección, margen y límites efectivos |
| `IMU` | Cualquiera | Salud, roll y pitch |
| `PING` | Cualquiera | Renueva la comunicación de calibración; nunca habilita ni mueve servos |
| `OFF` | Cualquiera | Detiene y desenergiza servos |
| `CALIB` | IMU segura | Entra a calibración con todos los PWM apagados |
| `SELECT <ch>` | `CALIBRATION` | Selecciona canal y apaga antes el anterior; no energiza |
| `ENABLE <ch> <deg>` | Canal seleccionado e IMU segura | Activa solo ese canal con el primer PWM explícito |
| `CENTER <ch>` | Canal seleccionado y habilitado | Lleva solo ese canal al centro efectivo |
| `SERVO <ch> <deg>` | Canal seleccionado y habilitado | Mueve solo ese canal dentro de límites seguros |
| `LIMITS <ch> <min> <center> <max>` | `CALIBRATION` | Cambia límites en RAM |
| `SAVE` | `SAFE_OFF`, configuración válida | Guarda EEPROM |
| `LOAD` | `SAFE_OFF` | Recupera EEPROM válida |
| `DEFAULTS` | `SAFE_OFF` | Restaura valores compilados en RAM |
| `STAND` | Zona despejada | Activa postura base |
| `LEG <0..3>` | `STAND_MODE` | Prueba una pata |
| `UNLOCK_WALK` | Tras pruebas | Desbloquea marcha hasta reiniciar |
| `WALK` | `STAND_MODE` y desbloqueado | Inicia marcha lenta |

## 11. Seguridad obligatoria

1. Primera calibración con el robot elevado y sin carga en las patas.
2. Acceso inmediato al corte de alimentación de servos.
3. Fuente externa de servos con GND común.
4. Un solo servo por vez durante búsqueda de límites.
5. Incrementos pequeños de ángulo cerca de extremos.
6. Nunca asumir que los valores históricos son físicamente seguros.
7. Probar `CENTER`, luego `STAND`, luego `LEG 0..3`, y solo después `WALK`.
8. No activar balance hasta validar modelo, ejes y signos de la IMU.
9. Toda salida motriz nueva debe pasar por la capa de límites.
10. Ante ruido, calentamiento, vibración, reinicio o bloqueo: cortar energía.

Con IMU activa, una lectura inválida o inclinación mayor al umbral detiene el
movimiento y apaga los servos, incluida la calibración. La inclinación durante
calibración se controla con `CALIBRATION_ABORT_ON_TILT`, que parte en `true` por
seguridad aunque el robot esté elevado. Esta protección todavía requiere
validación física.

Con un canal habilitado en `CALIBRATION`, el Nano exige comunicación válida de la
aplicación: `PING` o un comando de calibración válido deben llegar en menos de
1000 ms. Si vence ese plazo, el Nano apaga todos los PWM, limpia selección y
canal activo, entra a `SAFE_OFF` y deja `abort=HOST_TIMEOUT` enclavado en
`STATUS`. `PING` no puede habilitar ni mover un servo.

## 12. Procedimiento de validación

### 12.1 Puerta A — Electrónica

- [ ] Confirmar placa y bootloader.
- [ ] Confirmar PCA9685 y dirección.
- [ ] Documentar fuente de servos.
- [ ] Confirmar masa común.
- [ ] Confirmar canal físico de los 13 servos.

### 12.2 Puerta B — Calibración

- [ ] Calibrar mínimo, centro y máximo del canal 0.
- [ ] Calibrar canales 1–12.
- [ ] Verificar margen de 5°.
- [ ] Guardar EEPROM.
- [ ] Reiniciar y confirmar persistencia.

### 12.3 Puerta C — Postura

- [ ] `CENTER` sin choque.
- [ ] `STAND` sin choque.
- [ ] Ningún servo permanece saturado.
- [ ] Alimentación estable y sin reinicios.

### 12.4 Puerta D — Patas

- [ ] `LEG 0`: 10 ciclos.
- [ ] `LEG 1`: 10 ciclos.
- [ ] `LEG 2`: 10 ciclos.
- [ ] `LEG 3`: 10 ciclos.
- [ ] El cuerpo permanece dentro del polígono de soporte.

### 12.5 Puerta E — IMU

- [ ] Identificar modelo y orientación.
- [ ] Verificar dirección I²C.
- [ ] Calibrar offsets.
- [ ] Confirmar signos de roll y pitch.
- [ ] Confirmar parada por sensor inválido.
- [ ] Confirmar parada por inclinación excesiva.

### 12.6 Puerta F — Marcha

- [ ] Completar un ciclo lento.
- [ ] Completar 10 ciclos sin choque.
- [ ] Detener correctamente con `OFF`.
- [ ] Registrar límites alcanzados y comportamiento.
- [ ] Repetir sobre superficie real de operación.

## 13. Compilación verificada

El MVP `v0.1` fue compilado localmente para:

```text
arduino:avr:nano:cpu=atmega328old
```

Resultado registrado:

| Recurso | Uso | Disponible |
|---|---:|---:|
| Flash | 14.946 bytes (48%) | 30.720 bytes |
| RAM global | 1.007 bytes (49%) | 2.048 bytes |
| RAM restante estimada | 1.041 bytes | — |

La compilación correcta no equivale a validación física.

## 14. Requisitos antes de cinemática inversa

No se iniciará IK hasta cumplir:

1. Mapa físico confirmado.
2. Límites mecánicos confirmados.
3. Postura repetible.
4. Elevación estable de las cuatro patas.
5. IMU identificada y calibrada.
6. Convención cartesiana documentada.
7. Medidas reales de coxa, fémur y tibia.
8. Origen y orientación del sistema de coordenadas definidos.

La futura cadena de control será:

```text
objetivo de pie (x,y,z)
→ IK
→ ángulos articulares
→ validación geométrica
→ ServoController
→ límites físicos
→ PCA9685
```

## 15. Datos pendientes del robot físico

Registrar durante las próximas pruebas:

| Dato | Valor |
|---|---|
| Modelo exacto del Arduino | PENDIENTE |
| Modelo de los servos | PENDIENTE |
| Modelo de la IMU | PENDIENTE |
| Tensión/corriente de fuente | PENDIENTE |
| Longitud de coxa | PENDIENTE |
| Longitud de fémur | PENDIENTE |
| Longitud de tibia | PENDIENTE |
| Masa total | PENDIENTE |
| Posición aproximada del centro de masa | PENDIENTE |
| Orientación física de ejes IMU | PENDIENTE |

## 16. Convención de versiones

- Firmware MVP: `0.x.y` mientras no supere todas las puertas de validación.
- Firmware `1.0.0`: primera marcha estable, segura y reproducible.
- Cambios incompatibles de EEPROM deben incrementar su versión interna.
- Cada ensayo físico debe registrar fecha, firmware, configuración y resultado.

## 17. Historial de decisiones

| Fecha | Decisión | Motivo |
|---|---|---|
| 2026-08-12 | Separar el MVP en cuatro archivos de código | Mantener claridad sin fragmentar en exceso |
| 2026-08-12 | Usar `robot_config.h` como configuración habitual | Reducir puntos de ajuste |
| 2026-08-12 | Centralizar órdenes en `ServoController` | Impedir que módulos evadan límites |
| 2026-08-12 | Mantener IMU desactivada | El modelo físico no está confirmado |
| 2026-08-12 | Bloquear marcha hasta `UNLOCK_WALK` | Evitar activación accidental |
| 2026-08-12 | Posponer IK | Primero validar mecánica, postura y balance |
| 2026-08-12 | Crear esta especificación | Establecer una fuente de verdad mantenible |

## 18. Registro de pruebas

Añadir una fila por sesión física. No reemplazar resultados anteriores.

| Fecha | Firmware | Configuración | Prueba | Resultado | Observaciones |
|---|---|---|---|---|---|
| — | — | — | — | PENDIENTE | Aún no hay pruebas físicas registradas en este documento |
