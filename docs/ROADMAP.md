# Roadmap oficial — Etapa 2 del Mini Q-POD

Este documento formaliza la segunda etapa oficial del proyecto. Ordena el
trabajo posterior a la consolidación del repositorio; no autoriza por sí mismo
carga de firmware, energización de servos ni movimiento del robot.

## Base y reglas de evidencia

- **Base consolidada:** `5c2338134514c52bee800d4ba7257d0096b2595d` en `main`.
- **Estado de la elección de locomoción:** pendiente entre Nano y Pico. No se
  migra hardware ni se amplían funciones hasta cerrar la etapa 2.
- **Implementación** acredita código, documentación o configuración presente en
  el repositorio. **Simulación** acredita pruebas host o modelos matemáticos.
  **Prueba física** exige una observación registrada del robot real. Ninguna de
  las dos primeras sustituye a la tercera.
- Geometría y restricciones mecánicas: [ik_constraints.md](ik_constraints.md).
  Calibración, límites eléctricos, seguridad y pinout: [ESPECIFICACION_TECNICA.md](../ESPECIFICACION_TECNICA.md).
  El roadmap solo los enlaza: no replica sus tablas ni redefine sus valores.

## Registro de pruebas de la base

La información previa de «45 pruebas con paridad aprobada» no tiene en `main`
un log, SHA ni comando reproducible que permita atribuirla a esta base. El
reporte posterior de «40 pruebas» tampoco coincide con la ejecución actual y
omitió grupos de prueba; debe tratarse como un resumen incompleto, no como una
evidencia de regresión ni de validación física.

En la base indicada se ejecutaron sin hardware: 12 pruebas de IK, 13 pruebas
host de caminata y 28 de calibración (`53` en total). Además pasaron 5 pruebas
de Pose Lab y 6 de consola (`64` host en total). Posteriormente se habilitó
Zig como compilador C++ host y pasaron las **9** pruebas que realmente contiene
`test_parity.py` (el conteo previo de 10 era erróneo). Todos esos resultados son
evidencia de simulación/host, no física.

## Etapas

### 1. Base única y ordenada

- **Estado:** completada.
- **Objetivo:** establecer un único repositorio, una sola rama de integración y
  una clasificación explícita de los componentes activos, de soporte e
  históricos.
- **Criterio de cierre:** `main` es la referencia única; no contiene respaldos
  privados ni productos generados; la documentación enlaza las fuentes de
  verdad y el otro PC tiene un procedimiento seguro para sincronizarse.
- **Evidencia de implementación:** `main` consolidó la línea de caminata y el
  simulador; [README.md](../README.md) clasifica los módulos; el respaldo
  ESP32-CAM fue retirado del historial accesible y se ignora localmente.
- **Evidencia de simulación:** las pruebas host registradas arriba pasan; la
  paridad C++ queda explícitamente pendiente.
- **Evidencia de prueba física:** no aplica como requisito de ordenamiento; no
  se reclama ninguna prueba de locomoción.
- **Dependencias y problemas pendientes:** el otro PC puede conservar historia
  antigua y cambios sin publicar; debe sincronizarse con el procedimiento de
  este documento antes de crear trabajo nuevo.

### 2. Decisión Nano o Pico para locomoción

- **Estado:** completada; implementación de la migración pendiente.
- **Objetivo:** escoger y documentar una única plataforma responsable de la
  locomoción, dejando las demás con responsabilidades compatibles.
- **Criterio de cierre:** una decisión registrada en la especificación con
  responsable de PWM, radio, sensores, protocolo entre placas, límites de
  tensión y plan de migración o permanencia; la otra plataforma no recibe una
  migración implícita.
- **Evidencia de implementación:** el Nano compila la marcha con 99% de flash
  y 74% de RAM. La envoltura Pico W, con el núcleo compartido, compila con 16%
  de flash y 27% de RAM; conserva Nano como referencia. Véanse
  [DECISION_LOCOMOCION.md](DECISION_LOCOMOCION.md) y
  [PICO_W_LOCOMOTION_BENCH.md](PICO_W_LOCOMOTION_BENCH.md).
- **Evidencia de simulación:** las 9 pruebas de paridad Python/C++ pasaron con
  Zig 0.16.0; no miden tiempo de peor caso del microcontrolador.
- **Evidencia de prueba física:** no hay comparación física Nano/Pico ni
  movimiento de robot.
- **Dependencias y problemas pendientes:** confirmar físicamente la revisión
  Pico WH, VCC/pull-ups PCA9685 y cableado NRF24 antes de validar READY.

### 3. Validación física de READY e IK por pata

- **Estado:** pendiente.
- **Objetivo:** confirmar de forma controlada la postura READY y la
  correspondencia entre geometría IK, calibración eléctrica y movimiento de
  cada pata.
- **Criterio de cierre:** las cuatro patas pasan una secuencia documentada de
  READY y movimientos IK individuales dentro de límites aprobados, sin choque,
  saturación, reinicio ni calentamiento anómalo; se registran signos, offsets y
  observaciones reales por canal.
- **Evidencia de implementación:** `mini_qpod_pico_walk_mvp/` contiene READY,
  conversión y rechazo por límites mediante el núcleo compartido;
  `nano_servo_mapping/` permite medir un canal de manera segura.
- **Evidencia de simulación:** IK/FK, límites host y trayectorias de READY
  pasan pruebas; su propia documentación declara que no validan hardware.
- **Evidencia de prueba física:** ninguna. El registro histórico solo llega a
  `SELECT 0` con PWM apagado y sin alimentar servos.
- **Dependencias y problemas pendientes:** cerrar primero la etapa 2; verificar
  fuente, GND, corte accesible, pinout, IMU y calibración física según la
  especificación.

### 4. Primera caminata lenta

- **Estado:** pendiente.
- **Objetivo:** demostrar la primera marcha autónoma lenta en suelo.
- **Criterio de cierre:** tres ciclos consecutivos en suelo con avance neto,
  sin ayuda manual ni caída, y con parada final ordenada; registrar vídeo,
  preset, alimentación, superficie y observaciones.
- **Evidencia de implementación:** el firmware implementa WALK/WAVE, una pata
  en vuelo, precarga estimada de COM y parada solicitada.
- **Evidencia de simulación:** el simulador cubre contactos, margen geométrico,
  avance y parada; no modela masa, fricción, torque ni impactos.
- **Evidencia de prueba física:** ninguna.
- **Dependencias y problemas pendientes:** etapas 2 y 3 cerradas; corte de
  potencia, límites físicos validados y plan de abortar ante comportamiento
  anómalo.

### 5. Caminata desde Control Lite sin PC permanente

- **Estado:** pendiente.
- **Objetivo:** controlar la caminata con Control Lite sin un PC conectado de
  forma permanente y detener de forma segura ante pérdida de radio.
- **Criterio de cierre:** iniciar, sostener y detener la caminata desde el
  mando; al interrumpir radio se centra la orden, se completa la parada en
  apoyo y queda evidencia física reproducible del failsafe.
- **Evidencia de implementación:** `control_lite.h` y `ps2j_packet.h` definen
  el paquete y el firmware contiene un failsafe de radio.
- **Evidencia de simulación:** el simulador y la paridad C++ prevista cubren
  los datos PS2J; la paridad no se ejecutó en esta base por falta de compilador.
- **Evidencia de prueba física:** ninguna.
- **Dependencias y problemas pendientes:** etapas 2–4, prueba de alcance y
  pérdida real de radio, y verificación de que no se depende de consola USB.

### 6. Retroceso, curvas, altura y apertura durante la marcha

- **Estado:** pendiente.
- **Objetivo:** ampliar la marcha validada a retroceso, giro y ajuste gradual
  de altura/apertura.
- **Criterio de cierre:** cada maniobra se demuestra en suelo, con transición
  suave, detención segura y sin exceder límites; registrar combinaciones
  aprobadas y las rechazadas por seguridad.
- **Evidencia de implementación:** el firmware y simulador incluyen ejes de
  avance/giro y objetivos de altura/apertura con rechazo de límites.
- **Evidencia de simulación:** las pruebas host cubren objetivos alcanzables,
  cambios de dirección y un rechazo de límite; no certifican el robot.
- **Evidencia de prueba física:** ninguna.
- **Dependencias y problemas pendientes:** etapas 2–5 y validación de margen
  mecánico real para cada configuración.

### 7. Posturas, órbitas, trote experimental y recuperación tras vuelco

- **Estado:** pendiente.
- **Objetivo:** investigar funciones avanzadas sin mezclarlas con la marcha
  lenta validada; el estudio de recuperación tras vuelco es una línea separada.
- **Criterio de cierre:** posturas y órbitas cuentan con límites y pruebas
  propias; el trote se mantiene marcado experimental hasta prueba física; la
  recuperación solo se considera tras un análisis específico de energía,
  contacto y riesgos, no como extensión automática del trote.
- **Evidencia de implementación:** existen presets, Pose Lab y un trote
  experimental en simulación; PACE continúa retirado.
- **Evidencia de simulación:** hay trayectorias y métricas geométricas, sin
  dinámica, fuerzas ni detección de choque.
- **Evidencia de prueba física:** ninguna.
- **Dependencias y problemas pendientes:** etapas anteriores completadas y una
  evaluación de seguridad independiente para cualquier maniobra de recuperación.

### 8. Integración posterior de S3, IMU y sensores

- **Estado:** pendiente.
- **Objetivo:** integrar ESP32-S3 SuperMini, IMU y sensores después de estabilizar locomoción,
  definiendo responsabilidades, alimentación, comunicaciones y fallos seguros.
- **Criterio de cierre:** arquitectura y protocolo documentados; telemetría y
  fallos de cada sensor validados sin degradar las protecciones de locomoción;
  cualquier balance activo tiene pruebas físicas separadas.
- **Evidencia de implementación:** GP8/GP9 están reservados en Pico WH para
  ESP32-S3; la imagen del proveedor indica ESP32-S3 FH4R2, pendiente de
  comprobación física. No hay integración S3 ni balance validado.
- **Evidencia de simulación:** hay parser/monitor de visión e interfaces IMU;
  no prueban cableado, tiempos ni control físico.
- **Evidencia de prueba física:** ninguna de integración con locomoción.
- **Dependencias y problemas pendientes:** etapas 2–6, modelo S3 concreto,
  niveles lógicos, presupuesto de potencia, protocolo y pruebas de fallo.

## Sincronización segura del otro PC tras la reescritura

Esta base reescribió historia y eliminó ramas antiguas. **No** hacer `pull` ni
forzar una rama local antigua contra el remoto. Primero preservar el trabajo
local y después adoptar `origin/main`.

1. En el otro PC, dentro de su copia antigua, ejecutar `git status` y guardar
   los cambios rastreados: `git diff --binary > ..\\mini-qpod-local.patch`.
2. Guardar también los no rastreados que importen fuera del repositorio (o
   usar `git stash push --include-untracked -m "rescate antes de main reescrito"`).
   No rescatar cachés, logs, binarios de compilación ni respaldos privados.
3. Como opción más segura, clonar una copia nueva en otra carpeta:
   `git clone https://github.com/Jcabroc/mini-qpod.git mini-qpod-limpio`.
4. Revisar el parche y los archivos rescatados contra la copia nueva; aplicar o
   copiar manualmente solo cambios vigentes, probarlos y crear una rama nueva
   desde `main`. No aplicar a ciegas código de ramas eliminadas.
5. Si se debe reutilizar la carpeta antigua una vez guardado el rescate:
   `git fetch --prune origin`, `git switch main` y
   `git reset --hard origin/main`. Este último comando descarta el árbol local,
   por eso se ejecuta únicamente después de los pasos 1–2.

El primer trabajo concreto después de sincronizar es cerrar la **etapa 2**:
documentar la decisión Nano o Pico para locomoción, sin migrar hardware todavía.
