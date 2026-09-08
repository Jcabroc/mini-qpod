# IK matemática en PC

La capa separada `validation.py` ofrece `validate_target(leg, body_point,
knee_sign=-1)`: devuelve solución, estados por comprobación y avisos sin aplicar
límites CAD. Ver [restricciones y referencias CAD](../../docs/ik_constraints.md).
Los estados mecánicos, eléctricos y de colisión permanecen pendientes;
`ik_reachable` no equivale a aprobación física.

Modelo independiente en Python (biblioteca estándar), sin acceso serial ni
dependencias de Arduino. Medidas confirmadas por el usuario desde el STEP original:
chasis de 93.0 × 93.0 mm entre centros COXA; COXA=42.294 mm,
FEMUR=60.611 mm, TIBIA=88.714 mm, todas centro a centro.

Origen del cuerpo: centro geométrico; +X derecha, +Y frontal, +Z arriba.
Se asumen los cuatro ejes COXA en el plano Z=0.

| Pata | X (mm) | Y (mm) | Yaw de montaje |
|---|---:|---:|---:|
| L1 | -46.5 | +46.5 | +135° |
| R1 | +46.5 | +46.5 | +45° |
| L2 | -46.5 | -46.5 | -135° |
| R2 | +46.5 | -46.5 | -45° |

El yaw es **mecánico/radial**, medido desde +X hacia +Y alrededor de +Z.
No es un ángulo eléctrico de servo. En el marco local de cada pata, +X apunta
radialmente hacia fuera, +Y es tangencial y +Z sigue arriba.
El yaw devuelto por IK es relativo al yaw de montaje de esa pata.

La API usa mm y radianes. Femur=0 significa horizontal hacia fuera; su signo
positivo eleva la pata. Knee=0 alinea tibia y fémur; knee es el ángulo relativo
de la tibia respecto del fémur, con el mismo signo de elevación.
Se supone COXA horizontal y articulaciones fémur/tibia coplanares, sin offsets
adicionales. Estas convenciones matemáticas requieren validación antes de hardware.

Para un objetivo local (x,y,z): r=hypot(x,y), h=r-COXA,
cos(knee)=(h²+z²-FEMUR²-TIBIA²)/(2 FEMUR TIBIA),
femur=atan2(z,h)-atan2(TIBIA sin(knee), FEMUR+TIBIA cos(knee)).
Yaw=atan2(y,x). Se elige la orientación COXA hacia el objetivo; no se enumera
la solución alternativa con COXA girada 180° y alcance radial firmado negativo.

`knee_sign=-1` (predeterminado) o `+1` selecciona las dos ramas planares;
ninguna se declara físicamente segura. Se rechazan objetivos fuera del anillo
de alcance y el eje vertical COXA, donde yaw no queda determinado.
Las fronteras totalmente plegada/extendida se aceptan aunque son singulares;
no se calculan velocidades ni Jacobianos. Tolerancia de frontera: 1e-9 mm,
con recorte del coseno únicamente para redondeo numérico.

La conversión a **ángulos eléctricos por canal se hará en una capa posterior**:
centros, signos, offsets, calibración y límites no pertenecen a esta IK.
Alcanzable matemáticamente no implica ausencia de colisiones ni viabilidad física.
No se modifican sketches, EEPROM ni límites de servos.

Desde la raíz del repositorio:

```console
python -B -m unittest discover -s tools/ik_simulator -p "test_*.py" -v
```

Ejemplo en Python:

```python
from tools.ik_simulator.kinematics import inverse, forward
angles = inverse("R1", (120, 120, -80))
print(angles)  # radianes mecánicos
print(forward("R1", angles))  # recupera el objetivo en el marco del cuerpo
```

Las pruebas cubren dimensiones, ejes radiales, una solución analítica,
fronteras, errores y reconstrucción FK(IK(p)) de puntos aleatorios reproducibles
para ambas ramas y las cuatro patas. No constituyen validación física.
