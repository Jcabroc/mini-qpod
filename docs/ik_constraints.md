# Restricciones de IK y referencias mecánicas Mini Q-POD

## Procedencia y alcance

La geometría y la interpretación CAD de este documento fueron confirmadas por
el usuario desde el STEP original. Las diez capturas aportadas se conservan en
`reference/cad/` y se enlazan abajo; se revisó su contenido visual sin efectuar
mediciones adicionales ni una inspección de sólidos. Los STEP históricos del repositorio
no se han registrado ni validado como ensamblaje de colisión del modelo actual.

## Referencias CAD

Estas imágenes son evidencia visual de geometría, recorridos y posibles
colisiones; **no reemplazan la calibración física por canal**. Se conservaron
los PNG originales sin alteraciones y se renombraron según su contenido.
Los números siguientes corresponden al orden de las capturas aportadas.

| Captura | Archivo | Evidencia visual |
|---|---|---|
| 1 | [Chasis: centros COXA](reference/cad/chassis_coxa_spacing.png) | Separación de 93 mm en ancho y largo |
| 2 | [Longitudes de eslabones](reference/cad/leg_link_lengths.png) | COXA 42.294, fémur 60.611 y tibia 88.714 mm |
| 3 | [COXA superior 01](reference/cad/coxa_top_sweep_01.png) | Referencia diagonal X_NEUTRAL, con separación angular anotada de 90° |
| 4 | [COXA superior 02](reference/cad/coxa_top_sweep_02.png) | Barrido hacia patas del mismo lateral; cotas visuales de 56° |
| 5 | [COXA superior 03](reference/cad/coxa_top_sweep_03.png) | Barrido hacia pares frontal y trasero; cotas visuales de 34° |
| 6 | [Recorrido lateral 01](reference/cad/leg_lateral_workspace_01.png) | Pata próxima a extensión horizontal |
| 7 | [Recorrido lateral 02](reference/cad/leg_lateral_workspace_02.png) | Fémur elevado y tibia descendente |
| 8 | [Sobre de posible colisión](reference/cad/leg_collision_envelope.png) | Configuración recogida para estudiar proximidad entre piezas |
| 9 | [Recorrido lateral 03](reference/cad/leg_lateral_workspace_03.png) | Configuración extendida hacia abajo |
| 10 | [Recorrido lateral 04](reference/cad/leg_lateral_workspace_04.png) | Fémur elevado y tibia orientada hacia fuera |

El lote contiene una vista de chasis, tres vistas superiores de barrido/referencia
COXA y seis vistas de una pata; no incluye una cuarta captura de barrido ni una
vista frontal identificada. La descripción previa de cuatro vistas superiores
se mantiene como contexto aportado, sin inventar imágenes faltantes.
Las cotas angulares son referencias del dibujo, no topes rígidos ni ángulos
eléctricos. Las capturas no prueban por sí solas contacto o ausencia de colisión.

## 1. Geometría ideal de IK

Distancia entre centros COXA: 93.0 × 93.0 mm. Origen en el centro geométrico,
+X derecha, +Y frontal, +Z arriba. Se supone el plano de los montajes en Z=0.
Longitudes centro a centro: COXA=42.294 mm, FEMUR=60.611 mm, TIBIA=88.714 mm.

| Pata | X mm | Y mm | Yaw mecánico/radial |
|---|---:|---:|---:|
| L1 | -46.5 | +46.5 | +135° |
| R1 | +46.5 | +46.5 | +45° |
| L2 | -46.5 | -46.5 | -135° |
| R2 | +46.5 | -46.5 | -45° |

El yaw de montaje gira desde +X hacia +Y alrededor de +Z. `X_NEUTRAL`
diagonal es la referencia nominal (yaw local cero), no un preset completo.
La IK usa segmentos ideales sin volumen y devuelve radianes mecánicos.
Extensión total y rodilla plegada son fronteras geométricas singulares;
la existencia de solución no demuestra que el robot pueda ocupar esa posición.

## 2. Límites eléctricos individuales calibrados

La autoridad sigue siendo la calibración física ya registrada de cada canal.
No se infieren ángulos eléctricos absolutos, centros, signos ni topes desde CAD.
La futura capa eléctrica deberá relacionar ángulos mecánicos con canal,
orientación, offset y calibración individual, sin asumir simetría eléctrica.
Hasta entonces esta comprobación está pendiente; no se cargan ni alteran
EEPROM, tablas de calibración ni límites existentes.

## 3. Sobre mecánico del CAD

Las cuatro vistas superiores muestran el barrido COXA y las fronteras de
interferencia entre patas antes de choque. **No son presets de postura.**
Las cotas visuales de 45°, 56° y 34° no se convierten en límites rígidos.
El yaw nominal de montaje de ±45°/±135° es una referencia de coordenadas;
no debe confundirse con esas cotas de barrido.

Las vistas laterales describen recorridos lógicos de cuerpo/COXA, fémur y
tibia de una pata genérica, incluidas proximidades de choque con chasis y
entre piezas. Constituyen evidencia cualitativa de un sobre por validar.
Faltan intervalos mecánicos confirmados, offsets, volúmenes y holguras para
decidir cuantitativamente si una configuración está dentro del sobre.

Por ahora toda solución alcanzable se marca `MECHANICAL_ENVELOPE_UNVALIDATED`.
En las fronteras geométricas se añaden `FULL_EXTENSION_SINGULARITY` o
`FOLDED_KNEE_SINGULARITY`. Son avisos, sin recortar ángulos ni retirar soluciones.
La tolerancia de detección de frontera (1e-7 mm) es numérica, no una holgura
mecánica ni un umbral CAD de proximidad. No se inventa una banda de peligro.

## 4. Futuras restricciones de colisión

Contra chasis: registrar sólidos en el marco del cuerpo, espesores, ejes y
holguras; incluir contacto entre piezas de una misma pata. Una prueba del
pie o de segmentos sin grosor no basta para certificar ausencia de choque.

Entre vecinas: evaluar simultáneamente L1–R1, L1–L2, R1–R2 y L2–R2,
sin excluir pares diagonales si sus volúmenes alcanzan a interferir.
Evaluar también el barrido entre posiciones: extremos libres no garantizan
trayectoria libre. Antes de habilitar restricciones hacen falta poses CAD
conocidas de contacto/no contacto y márgenes contrastados físicamente.

## Capa de validación y estados

`tools/ik_simulator/validation.py` consume la IK sin modificarla:

| Comprobación | Estado actual |
|---|---|
| IK alcanzable | `pass`, `fail` fuera de alcance, `indeterminate` en eje yaw |
| Límites mecánicos de articulación | `pending` |
| Límites eléctricos por canal | `pending` |
| Colisión contra chasis | `pending` |
| Colisión entre patas vecinas | `pending` |

`ValidationReport` contiene solución, comprobaciones y avisos. `ik_reachable`
describe exclusivamente la geometría; no existe un indicador global de
«movimiento seguro». Un estado pendiente nunca se presenta como aprobado.
Objetivos inalcanzables no tienen solución; entradas inválidas siguen generando
errores de API. No hay envío de movimientos ni bloqueo físico en esta capa.

Pruebas actuales: extensión máxima, plegado, alcance interior/exterior,
singularidad yaw, objetivo válido pero mecánicamente pendiente, y ausencia de
bloqueo al cruzar las cotas angulares ilustrativas. Futuras pruebas deberán
verificar cada conversión eléctrica calibrada, contactos, holguras y trayectorias
con evidencia independiente, antes de conectar esta validación al firmware.
