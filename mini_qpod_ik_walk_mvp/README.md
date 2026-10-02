# Firmware Mini Q-POD

Firmware listo para Nano ATmega328P, PCA9685 `0x40`/50 Hz y NRF24. Preserva la
geometría, montaje, calibración y límites revisados en
`328ab9302e3c5cc44ba9fedc3c6c22492ea84f03`. La marcha por cuadro está en
`ik_walk_core.h` y es la misma rutina invocada por Python.

Compila sin cargar para Nano con bootloader antiguo:

```powershell
arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328old mini_qpod_ik_walk_mvp
```

El HEX ya compilado para esa placa queda en
`mini_qpod_ik_walk_mvp/build/mini_qpod_ik_walk_mvp.ino.hex`. La compilación no
lo carga al Nano.

Requiere Adafruit PWM Servo Driver y RF24. Al iniciar, todos los canales PWM
quedan apagados y CH12 (cuello) permanece apagado. READY habilita PWM en los
12 canales de patas. Los movimientos fuera de IK o de los límites calibrados
se rechazan; ante un rechazo de marcha se apaga PWM y se informa el error.

## Marcha

WALK es el preset inicial de Caminata lenta y WAVE es Onda. Ambos usan zancada
18 mm, elevación 12 mm, periodo 4 s para WALK / 2,4 s para WAVE, apoyo 0,85 y
desfases `[0, 0.5, 0.75, 0.25]`. Cada vuelo dura 15% del ciclo y el intervalo
entre vuelos 10%: hay como máximo una pata en vuelo y cuatro apoyos durante la
preparación del siguiente trípode. La rutina traslada primero el COM estimado
hacia el polígono de las tres patas que quedarán apoyadas. Conserva los puntos
de apoyo en coordenadas del suelo y usa curvas suaves para levantar, avanzar y
aterrizar.

En la primera vuelta el robot se centra sobre el primer trípode (en el modelo,
unos 9 mm de desplazamiento longitudinal y lateral); las vueltas posteriores
avanzan cerca de los 18 mm nominales por ciclo recto. El ajuste de COM deja un
margen geométrico de 2 mm al final de cada cuadro corregido en el modelo. Es
una aproximación estática, no una simulación de fuerzas ni una validación de
estabilidad física.

TROT permanece experimental. Cuando vuelan dos patas en diagonal, no se
declara estable con solo dos apoyos. PACE está fuera del selector y de los
presets del Nano.

Avance/retroceso y giro se mezclan desde los dos ejes del stick; se suavizan
aceleración, dirección y parada. Al centrar el stick, pulsar CIRCLE o mandar
`STOP`/`PAUSE`, el Nano termina el cuadro de marcha hasta que las cuatro patas
estén apoyadas. `SELECT` o `X` hacen apagado PWM inmediato. `START`/`READY`
solicitan READY; si la marcha está activa espera a detenerse antes de volver a
READY.

Con READY terminado, `STEP 0` a `STEP 3` realiza un vuelo individual coordinado
de L1, R1, L2 o R2. Usa la marcha compartida, prepara el COM sobre las otras
tres patas, detiene el avance al iniciar el vuelo y conserva sus apoyos. Se
puede ejecutar por USB serial; no es un movimiento solo de la interfaz.

La altura corporal cambia suavemente manteniendo los pies de apoyo fijos en el
suelo. La apertura se aplica progresivamente en cada aterrizaje; no reinicia
la fase ni arrastra las patas plantadas. Control Lite: TRIANGLE/CROSS cambian
altura objetivo ±4 mm; SQUARE/R3 cambian apertura objetivo ±3 mm. Los comandos
USB `HEIGHT mm` (−12..12) y `OPENING mm` (0..12) permiten ajustar los mismos
objetivos durante la marcha. Valores fuera de rango se rechazan, nunca se
recortan. Altura y apertura cambian el objetivo IK y pueden parar la marcha si
una articulación llega a un límite configurado.

## Transferir presets y Control Lite

`gait_presets.h` se genera desde
`tools/ik_walk_simulator/gait_defaults.json` con:

```powershell
python -m tools.ik_walk_simulator.export_firmware_presets
```

Tras exportar, vuelve a compilar. La interfaz muestra por separado zancada
nominal, recorrido mundial del pie y avance corporal medido. No amplíes los
límites para forzar recorrido. Por ejemplo, la configuración anterior de
50 mm pedía R1/CH3 a ~94,97° con mínimo 95° y se detenía sin recortar.

El receptor coincide con `codigo_control_lite_v1`: paquete binario PS2J de 9
bytes, canal 76, 250 kbps, dirección `JR001` y payload fijo RF24 de 32 bytes.
El emisor escribe 9 bytes; la radio rellena el tamaño fijo y `RF24::read`
consume el resto al copiar los 9 campos. No usa comandos de texto por radio ni
payload dinámico. Solo el modo 3 posiciones `mode3=2` habilita los ejes.
El transmisor filtra y centra el stick, y el receptor aplica zona muerta 0,18
sin descartar combinaciones de avance y giro. El failsafe de 500 ms al perder
paquetes centra gradualmente ambos ejes y finaliza en apoyo.

## Prueba suspendida futura

No se ha abierto ningún puerto, cargado firmware ni enviado movimiento.
Para una prueba posterior, asegura el cuerpo a un soporte rígido con patas
libres, deja a mano la desconexión de alimentación y revisa juntas/cableado sin
energía. Compila con `atmega328old`, enciende sin iniciar marcha y solicita
READY de forma deliberada. Prueba WALK primero con una entrada breve; observa
que no se solapen vuelos y que no aparezcan errores IK/límite. Verifica CIRCLE
como parada gradual y SELECT como apagado PWM. Repite WAVE; ensaya retroceso y
altura/apertura en incrementos pequeños porque los límites de R1/CH3 siguen
vigentes. Desenergiza antes de tocar el mecanismo. La estimación COM no valida
el contacto dinámico ni sustituye esta comprobación suspendida.

La paridad ejecutable es:

```powershell
python -m unittest tools.ik_walk_simulator.test_walk tools.ik_walk_simulator.test_parity -v
```

Compara objetivos READY, ángulos mecánicos/comandos eléctricos, ciclos completos
cuadro a cuadro, cambios de dirección, altura/apertura, apoyos, avance, COM y
paquete Control Lite.
