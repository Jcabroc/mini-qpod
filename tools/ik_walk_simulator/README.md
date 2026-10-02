# Simulador Mini Q-POD

Abre `tools\\ik_walk_simulator\\run.bat` para ejecutar la interfaz visible en
Windows o usa `python -m tools.ik_walk_simulator.app` desde la raíz (Python
3.10+ con Tkinter). No abre puertos ni carga firmware.

La geometría, ejes, montaje, calibración y límites se conservan desde el
firmware revisado. El cuerpo muestra centros COXA de 93 × 93 mm; su espesor es
solo visual. El control del ratón rota y acerca la cámara, +Z inicia arriba y
los botones X/Y pueden girar 180°.

## Caminata y controles

«Caminata lenta» es el preset inicial y «Onda secuencial» conserva su misma
secuencia con otro periodo. Ambas usan 85% de apoyo y desfases
`[0, 0.5, 0.75, 0.25]`: cada vuelo dura 15% del ciclo y hay un intervalo de
10% con cuatro apoyos antes del siguiente despegue. Durante esa pausa, el
cuerpo se traslada sobre el próximo trípode; después los tres pies de apoyo
permanecen fijos respecto del suelo mientras el pie en vuelo levanta, avanza y
aterriza con curvas suaves. La reproducción avanza una fase por cuadro en el
mismo motor C++ del Nano, traducido a Python.

W/S avanza o retrocede; A/D gira. Puedes mantener una tecla de cada eje para
combinar avance y giro. La aceleración, los cambios de dirección y la parada
se suavizan. «Detener» termina cuando las cuatro patas vuelven a apoyo; X en el
teclado solicita esa parada y espacio pausa la reproducción. READY, ciclo,
cuadro a cuadro, velocidad y reinicio también están disponibles. «Paso pata» ejecuta un único vuelo coordinado de la pata seleccionada, con pretraslado COM y los otros tres pies apoyados; el Nano ofrece el comando USB `STEP 0..3` con la misma rutina.

Los campos de altura corporal y apertura cambian mientras la fase continúa.
El cuerpo sube o baja conservando los puntos de apoyo en el suelo; la apertura
se incorpora pie a pie en cada aterrizaje. Ambos movimientos existen en el
firmware: Control Lite TRIANGLE/CROSS cambia altura ±4 mm y SQUARE/R3 apertura
±3 mm. La traslación manual lateral/vertical de los botones inferiores sigue
siendo exclusiva del simulador.

WALK y WAVE cargan zancada, elevación, periodo, proporción de apoyo, desfases,
altura/apertura inicial y estimación COM desde `gait_defaults.json`. El trote
diagonal queda experimental, nunca se marca estáticamente estable con dos
apoyos. PACE está retirado. Puedes guardar/cargar presets JSON y exportarlos
para Nano; el generador produce `mini_qpod_ik_walk_mvp/gait_presets.h`.

## Indicadores y modelo

La tabla muestra ángulos mecánicos y comandos eléctricos. Las patas aparecen
como apoyo o vuelo; el trazado muestra la trayectoria de los pies. La interfaz
separa zancada nominal, movimiento mundial y relativo del pie y avance corporal
medido. Durante la primera vuelta el cuerpo se coloca sobre el primer trípode;
las siguientes avanzan ~18 mm/ciclo recto con el preset actual. El COM y el
polígono son una aproximación geométrica configurable; no modelan masa real,
inercia, fuerza, fricción, torque, flexión ni impactos y no validan estabilidad
física.

Los objetivos IK y comandos fuera de límites se rechazan y paran el cuadro; no
se recortan ni se amplían límites. A 50 mm puede rechazarse R1/CH3 cerca de
94,97° frente a mínimo 95°.

## Ejecutar paridad

```powershell
python -m unittest tools.ik_walk_simulator.test_walk tools.ik_walk_simulator.test_parity -v
```

La suite compila el header C++ compartido y compara READY, objetivos articulares
y eléctricos, ciclos completos por cuadro, cambios de eje, altura/apertura,
polígono de apoyo, avance y datos PS2J del transmisor real. Para compilar el
firmware sin cargar, sigue `mini_qpod_ik_walk_mvp/README.md`.
