# Mini Q-POD Pose Lab

Aplicación de escritorio de simulación, Python 3.10+ con Tkinter (incluido en
el instalador estándar de Python para Windows). Desde la raíz del repositorio:

```powershell
python -B -m tools.ik_pose_lab
```

También se puede ejecutar `python tools/ik_pose_lab/launch.py`.
Tres paneles simultáneos muestran superior X/Y, frontal X/Z y el plano radial
de la pata seleccionada. Cuerpo de 93 × 93 mm, colores azul/verde/violeta para
COXA/fémur/tibia, objetivos ámbar o rojos. Las proyecciones se ajustan al rango.
La altura es la distancia del origen al plano de pies Z=-altura; cuerpo siempre
nivelado (roll/pitch=0). La vista frontal superpone las patas delanteras/traseras.

Los campos numéricos recalculan al editar. Apertura es la componente lateral
nominal desde cada montaje cuando yaw=0: radio = sqrt(2)*apertura.
Avance desplaza todos los pies en +Y después del abanico simétrico COXA.
Yaw positivo abre el abanico hacia los laterales desde X_NEUTRAL; no es un
ángulo eléctrico. Tras añadir avance, el yaw resuelto puede diferir del abanico.
Rama de rodilla: -1 o +1. No se imponen límites CAD ni eléctricos.

READY (altura 80, apertura 75) y X_NEUTRAL (70,85) son ejemplos matemáticos
editables, no posturas físicas aprobadas ni presets extraídos de las vistas CAD.
Los círculos ámbar marcan regiones cualitativas de incertidumbre alrededor
de COXA, con radio COXA+FEMUR solo para visualización. Sus solapamientos no
son detección de colisión: faltan volúmenes, holguras y trayectorias CAD.

Estado por pata: OK IK acompañado de zona mecánica pendiente, inalcanzable
o singularidad. Nunca se indica aprobación física global. Ver
[restricciones](../../docs/ik_constraints.md).

Guardar escribe un JSON versionado con controles; Cargar acepta ese formato
y también exportaciones. Exportar añade objetivos corporales, ángulos mecánicos
en radianes, comprobaciones y avisos calculados mediante la IK compartida.
Al cargar se recalculan resultados, sin confiar en datos derivados exportados.

Se revisó `gui_qpod_config.py`: se reaprovecha el patrón Tk/ttk y diálogos JSON.
Sus clases mezclan calibración y transporte serial; no se importan porque
introducirían dependencias de hardware innecesarias. La IK no se duplica:
se usa `ik_simulator.validation`, y FK para los extremos; los puntos intermedios
de la cadena son únicamente geometría de dibujo.

## Pruebas

```powershell
python -B -m unittest discover -s tools/ik_simulator -p "test_*.py" -v
python -B -m unittest discover -s tools/ik_pose_lab -p "test_*.py" -v
```

## EXE portable Windows (opcional)

En un entorno de construcción con Python y PyInstaller instalado
(`python -m pip install pyinstaller`), ejecutar:

```powershell
powershell -ExecutionPolicy Bypass -File tools/ik_pose_lab/build_exe.ps1
```

Resultado: `tools/ik_pose_lab/dist/Mini-QPOD-Pose-Lab.exe`. Empaqueta Tcl/Tk
y Python en un único archivo, sin consola. Compilar en Windows para Windows.
No requiere archivos CAD al ejecutar. La construcción no instala paquetes
automáticamente; el EXE debe probarse en el equipo de destino.
No existe comunicación serial, PWM, carga a Arduino ni control físico.
