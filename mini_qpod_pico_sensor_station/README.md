# Mini Q-POD Pico Sensor Station

Firmware para **Raspberry Pi Pico W** que publica los sensores en el USB serie a
115200 baud para la aplicación `Mini-QPOD-Sensor-Station.exe`; conserva además
la trama IMU y retransmite todos los sensores hacia el Nano por GP4 → D2 a
38400 baud. El MVP actualizado del Nano reenvía estas tramas por su USB a
115200 baud.

## Pinout

| Periférico | Pico W |
|---|---|
| MPU6050 SDA / SCL | GP0 / GP1 |
| Sonar HC-SR04 TRIG / ECHO | GP2 / GP3 |
| DHT | GP6 |
| Impacto | GP7 |
| Touch | GP9 |
| Mini PIR | GP10 |
| LDR izquierda / derecha | GP26 ADC0 / GP27 ADC1 |
| RGB | GP13 / GP14 / GP15 — reservado, no se activa |

## Seguridad y carga

1. El ECHO del HC-SR04 **debe** pasar por divisor 5 V → 3.3 V antes de GP3.
2. El firmware inicia intentando DHT11 y alterna automáticamente DHT11/DHT22
   si no obtiene respuesta. Cuando detecte uno, lo informa por USB. Si no
   detecta ninguno, revise alimentación, GND, cable DATA y el pull-up.
3. El módulo de impacto conectado fue observado activo en alto en reposo, por
   eso `IMPACT_ACTIVE_LOW` está en `true`. Si una prueba física demuestra lo
   contrario, vuelva a cambiarlo.
4. Este sketch no controla el RGB ni el buzzer: el tipo de LED aún no está
   confirmado y el buzzer pertenece al Nano D4.
5. Conecte la aplicación al **COM USB de la Pico**, no al Nano, para ver todos
   estos datos. El Nano MVP actual solo recibe la IMU.

Compile con:

```powershell
arduino-cli compile --fqbn rp2040:rp2040:rpipicow .
```

La telemetría usa líneas `SENSOR,<sensor>,<valor>`; por ejemplo
`SENSOR,dht,23.4,58.0` y `SENSOR,ldr,512,490`.
