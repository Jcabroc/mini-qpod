# Mini Q-POD — Vista Primitiva ESP32-CAM

## Objetivo

Usar una ESP32-CAM con OV2640 como sistema de visión primitiva del Mini Q-POD.

La cámara no realizará reconocimiento avanzado de objetos. Su función inicial será detectar cambios visuales simples y entregar información compacta al cerebro del robot.

Arquitectura objetivo:

```text
OV2640 → ESP32-CAM → Pico W → Nano → PCA9685 → actuadores
```

La ESP32-CAM actúa como retina. El Pico W integra percepción y sensores. El Nano conserva el control seguro del cuerpo, locomoción y actuadores.

## Principios de diseño

- La ESP32-CAM observa, pero no decide movimientos.
- El Pico W interpreta la información visual junto con IMU y demás sensores.
- El Nano no procesa imágenes.
- El Nano mantiene IK, límites, servos, watchdog y SAFE_OFF.
- La comunicación visual debe ser liviana.
- La reacción local debe funcionar sin depender del PC.
- El PC/HUB podrá utilizarse posteriormente para visión avanzada.

## ETAPA 1 — Vista primitiva aislada

**Estado:** EN IMPLEMENTACIÓN

Objetivo: conseguir que la ESP32-CAM detecte movimiento correctamente estando sobre una mesa, sin conectarla todavía al robot.

Resolución inicial implementada: `160×120` (`FRAMESIZE_QQVGA`) en escala de grises.

Procesamiento inicial:

- Captura de imagen.
- Conversión a escala de grises.
- Comparación con cuadro anterior.
- Eliminación de cambios pequeños.
- Cálculo de región activa.
- Cálculo de centroide.
- Cálculo del área modificada.

Estados iniciales: `QUIET`, `LEFT`, `CENTER`, `RIGHT`, `UNKNOWN`.

Pruebas:

- Mano moviéndose izquierda → derecha.
- Mano moviéndose derecha → izquierda.
- Persona caminando frente a la cámara.
- Escena completamente quieta.
- Cambio de iluminación.
- Encender/apagar una luz.

La implementación actual compara cuadros consecutivos, descarta diferencias menores a un umbral de 18 niveles de gris y exige al menos 180 píxeles activos. También limita cambios excesivamente grandes como `UNKNOWN`, porque suelen indicar una variación global de iluminación.

Diagnóstico serie actual:

```text
MOTION state=LEFT x=22 y=58 area=87 score=36 threshold=18
```

`x` e `y` son el centroide en píxeles, `area` es el número de píxeles que superaron el umbral, `score` es una confianza heurística de 0 a 100 y `threshold` es el umbral de diferencia utilizado.

La salida también incluye la luminancia media del frame en escala de grises y su clasificación:

```text
MOTION state=QUIET x=0 y=0 area=139 score=7 threshold=18 luminance=114 light=NORMAL
```

Umbrales iniciales de luminancia: `DARK < 60`, `NORMAL = 60..190`, `BRIGHT > 190`; un cambio absoluto de al menos 25 niveles entre frames se informa como `LIGHT_CHANGE`. Son valores iniciales y deben calibrarse con pruebas reales de iluminación.

**Criterio:** validar físicamente escena quieta, mano cruzando izquierda-centro-derecha y cambios de iluminación antes de ajustar los umbrales.

Validación con monitor realizada: la aplicación abrió, el parser aceptó líneas reales y la ESP32-CAM respondió en `COM25` a `115200`. La primera lectura reveló que la placa aún tenía el firmware JPEG anterior; se cargó Stage 1. La lectura posterior mostró luminancia y estados, pero también transiciones esporádicas `UNKNOWN/CENTER` en la escena disponible. La etapa queda **EN VALIDACIÓN**, pendiente de una prueba física controlada para calibrar falsos positivos y confirmar `LEFT`, `CENTER` y `RIGHT`.

## ETAPA 2 — Posición del movimiento

**Estado:** PENDIENTE — no iniciar hasta validar la Etapa 1.

Agregar valores normalizados `X`, `Y`, `AREA`, `MOTION` y `CONFIDENCE`.

Rangos sugeridos:

| Variable | Rango |
| --- | --- |
| X | -100 a +100 |
| Y | -100 a +100 |
| AREA | 0 a 100 |
| MOTION | 0 a 100 |
| CONFIDENCE | 0 a 100 |

Ejemplo de salida:

```text
VISION,MOTION,-62,5,24,48,81
```

Un objeto cruzando la cámara horizontalmente debe producir una transición aproximadamente continua: `-80 → -60 → -40 → -20 → 0 → +20 → +40 → +60 → +80`.

## ETAPA 3 — Acercamiento y alejamiento

**Estado:** PENDIENTE

Agregar análisis temporal de `AREA` usando una ventana aproximada de 5–10 frames. No interpretar cambios de un solo cuadro como acercamiento o alejamiento.

Estados nuevos: `APPROACHING` y `RECEDING`.

```text
18 → 23 → 29 → 38 → 47  = APPROACHING
51 → 42 → 34 → 27 → 20  = RECEDING
```

Esto no representa distancia real; únicamente expansión o contracción visual relativa.

## ETAPA 4 — Protocolo de comunicación

**Estado:** PENDIENTE

Comunicación primaria: UART ESP32-CAM ↔ Pico W. No usar JSON como protocolo principal embarcado.

```text
VISION,<STATE>,<X>,<Y>,<AREA>,<MOTION>,<CONF>
VISION,APPROACHING,12,-4,37,61,82
```

Estados: `QUIET`, `MOTION`, `LEFT`, `CENTER`, `RIGHT`, `APPROACHING`, `RECEDING`, `UNKNOWN`.

Agregar posteriormente número de secuencia, timestamp opcional, checksum simple opcional y heartbeat. La pérdida del enlace de cámara nunca debe producir movimientos peligrosos.

## ETAPA 5 — ESP32-CAM → Pico W

**Estado:** PENDIENTE

El Pico deberá recibir y validar los mensajes. Inicialmente no se moverá ningún servo.

```text
[CAM] LEFT x=-64 confidence=78
[CAM] CENTER x=-8 confidence=86
[CAM] APPROACHING area=41 confidence=84
```

Agregar timeout. Si no llegan mensajes válidos durante un periodo determinado, informar `CAM_OFFLINE`. La pérdida de visión no debe afectar SAFE_OFF ni la locomoción básica.

## ETAPA 6 — Compensación por movimiento propio

**Estado:** PENDIENTE

Cuando el robot mueve la cabeza o el cuerpo, toda la imagen cambia. El Pico W deberá combinar visión, IMU, estado de locomoción, movimiento de cabeza y estado del robot.

Durante un giro de cabeza podrá ignorar temporalmente ciertos frames, aumentar el umbral, marcar `SELF_MOTION` y esperar estabilización antes de detectar nuevamente. Esta etapa es fundamental antes de permitir reacciones físicas complejas.

## ETAPA 7 — Primer reflejo visual

**Estado:** PENDIENTE

Primer comportamiento permitido: movimiento sostenido hacia un lado → orientar lentamente la cabeza hacia ese lado.

```text
CAM → LEFT
Pico → LOOK_LEFT
Nano → movimiento limitado del servo de cabeza
```

No caminar, perseguir objetos ni retroceder automáticamente todavía. El objetivo es crear el primer reflejo ojo-cabeza del Mini Q-POD.

## ETAPA 8 — Fusión con sensores existentes

**Estado:** FUTURO

Combinar visión con PIR frontal, ultrasonido, IMU, touch y LDR. Ejemplo: PIR detecta presencia → la ESP32-CAM aumenta la frecuencia de análisis → encuentra movimiento → el Pico orienta la cabeza → el ultrasonido confirma distancia → el Pico decide la reacción.

## ETAPA 9 — Modos de actividad visual

**Estado:** FUTURO

Regulación dinámica posible:

```text
IDLE     → ~3 FPS
ALERT    → ~10 FPS
TRACKING → ~15 FPS
```

El objetivo es ahorrar procesamiento y energía cuando no ocurre nada interesante.

## ETAPA 10 — Visión avanzada mediante PC/HUB

**Estado:** FUTURO

La ESP32-CAM mantiene su percepción local básica y, cuando es necesario, envía información o imágenes al PC. El PC/HUB podrá ejecutar OpenCV, modelos de visión o IA para detección de personas, reconocimiento de objetos, tracking avanzado, reconocimiento de señales, navegación visual e interpretación de escenas, sin comprometer los reflejos locales.

## Hardware previsto

ESP32-CAM con OV2640 y PSRAM confirmadas operativas. Enlace previsto: UART ESP32-CAM ↔ Pico W.

Alimentar la ESP32-CAM desde una línea regulada estable de 5 V. No alimentar la cámara directamente desde un GPIO. Mantener GND común y agregar desacoplo próximo a la placa: `470 µF + 100 nF` entre alimentación y GND cerca de la ESP32-CAM.

## Hito v0.1

La primera versión se considerará funcional cuando la ESP32-CAM, completamente aislada del robot, pueda entregar consistentemente `QUIET`, `LEFT`, `CENTER`, `RIGHT`, `APPROACHING` y `RECEDING`, junto con `X`, `Y`, `AREA`, `MOTION` y `CONFIDENCE`, sin mover todavía ninguna parte del Mini Q-POD.

**Nombre provisional:** `mini-qpod-primitive-vision`  
**Versión inicial:** `v0.1`

## Registro de esta iteración

- Se mantuvo el pinout AI-Thinker confirmado y la inicialización OV2640 existente.
- Se cambió únicamente el firmware aislado de la ESP32-CAM a escala de grises `160×120`.
- Se añadió comparación de cuadros consecutivos, filtro de ruido, región activa, centroide, área y diagnóstico serie.
- Se añadieron únicamente las clases `QUIET`, `LEFT`, `CENTER`, `RIGHT` y `UNKNOWN`.
- No se implementaron `APPROACHING` ni `RECEDING`, UART entre placas, servos, Pico W, Nano ni reconocimiento de objetos.
- La compilación fue validada con ESP-IDF 5.4.4; el binario generado queda en `firmware/stage1/`.
- La herramienta recibió datos reales del firmware por `COM25`; la versión con luminancia queda en `firmware/stage1-luminance/`.
- Se añadió luminancia media de escala de grises y estados `DARK`, `NORMAL`, `BRIGHT` y `LIGHT_CHANGE`. No se implementaron `APPROACHING` ni `RECEDING`.

## Herramienta local de diagnóstico

Se añadió `tools/primitive-vision-monitor/`, una aplicación Windows basada en Tkinter y pyserial. `monitor.py` presenta el centroide en una cuadrícula 3×3, estados, datos numéricos, FPS y un monitor serie con autoscroll, `CLEAR`, `COPY ALL` y `SAVE LOG`. `parser.py` interpreta las líneas `MOTION state=...` y descarta líneas desconocidas sin detener la aplicación.

Ejecutar desde la raíz del proyecto:

```powershell
Set-Location tools\primitive-vision-monitor
& 'C:\Espressif\tools\python\v5.4.4\venv\Scripts\python.exe' monitor.py
```

La prueba del parser se ejecuta con `test_parser.py`. La herramienta no modifica el firmware ni inicia todavía las etapas de acercamiento/alejamiento.
