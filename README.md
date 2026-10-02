# Mini Q-Pod

Repositorio único del Mini Q-Pod. La rama `main` es el punto de sincronización
entre los equipos de casa y oficina: no se trabaja en copias paralelas ni se
suben binarios generados o respaldos de dispositivos.

## Estado actual

| Componente | Estado | Uso |
|---|---|---|
| `mini_qpod_ik_walk_mvp/` | Activo, pendiente de validación física | Firmware Nano para IK y caminata lenta. |
| `nano_servo_mapping/` | Activo de seguridad | Mapeo/calibración de un servo a la vez. |
| `tools/ik_simulator/` | Activo de ingeniería | IK/FK matemática y validación no física. |
| `tools/ik_walk_simulator/` | Activo de ingeniería | Simulador y paridad con el firmware de caminata. |
| `esp32cam/` | Diagnóstico experimental | Cámara y visión primitiva; no controla servos. |
| `mini_qpod_mvp/` | Referencia MVP | Base anterior, no punto de partida para cambios nuevos. |
| `test_gait*/`, `codigo_control_lite_v1/`, GUI y material CAD | Histórico o soporte | Consultar solo cuando la especificación lo indique. |

La fuente de verdad técnica, hardware, seguridad y estados de validación es
[ESPECIFICACION_TECNICA.md](ESPECIFICACION_TECNICA.md). Las soluciones IK y las
marchas simuladas **no** constituyen una validación física: seguir siempre los
procedimientos de seguridad de cada firmware.

## Flujo único de trabajo

1. Actualiza antes de empezar: `git pull --ff-only origin main`.
2. Trabaja en una rama `codex/<tema>` desde `main`.
3. Ejecuta las pruebas relevantes y actualiza la especificación si cambia una
   decisión técnica.
4. Integra el resultado en `main` y vuelve a actualizar el otro equipo.

No se guardan imágenes crudas de firmware, puertos COM locales, cachés Python,
ni productos de compilación. Los respaldos privados de dispositivos se
conservan fuera del repositorio.

## Verificación sin hardware

```powershell
python -B -m unittest discover -s tools/ik_simulator -p 'test_*.py' -v
python -m unittest tools.ik_walk_simulator.test_walk tools.ik_walk_simulator.test_parity -v
python -B -m unittest discover -s tools/servo_calibrator/tests -p 'test_*.py' -v
```

Consulta los README dentro de cada componente para las dependencias y las
instrucciones de compilación.
