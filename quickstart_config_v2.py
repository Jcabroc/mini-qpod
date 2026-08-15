#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Q-POD MINI - Quickstart Configuration v2
Versión más tolerante con el reset automático del Arduino Nano.
"""

import json
import serial
import serial.tools.list_ports
import time

BAUD = 115200


def list_ports():
    return [(port.device, port.description) for port in serial.tools.list_ports.comports()]


def read_for(ser, seconds=1.5):
    end = time.time() + seconds
    data = b""
    while time.time() < end:
        waiting = ser.in_waiting
        if waiting:
            data += ser.read(waiting)
        time.sleep(0.05)
    return data.decode(errors="replace")


def open_arduino(port, baud=BAUD):
    # Abrir el puerto reinicia muchos Arduino Nano por DTR.
    # Por eso esperamos más que en la versión original.
    ser = serial.Serial(port, baudrate=baud, timeout=0.2)
    time.sleep(2.5)
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    return ser


def ask(ser, command, wait=1.0, retries=3):
    for _ in range(retries):
        ser.reset_input_buffer()
        ser.write((command.strip() + "\n").encode())
        ser.flush()
        time.sleep(wait)
        response = read_for(ser, 0.8)
        if response.strip():
            return response
    return ""


def test_connection(port, baud=BAUD):
    try:
        ser = open_arduino(port, baud)
        response = ask(ser, "INFO", wait=0.5, retries=4)
        ser.close()

        if response.strip():
            print(f"✓ Arduino responde:\n{response}")
            return True
        print("✗ Arduino no responde a INFO")
        return False
    except Exception as e:
        print(f"✗ Error abriendo puerto: {e}")
        return False


def send_line(ser, line, delay=0.04):
    ser.write((line.strip() + "\n").encode())
    ser.flush()
    time.sleep(delay)


def upload_config(port, config_file, baud=BAUD):
    try:
        with open(config_file, "r", encoding="utf-8") as f:
            config = json.load(f)

        ser = open_arduino(port, baud)
        print(f"\n📤 Cargando configuración desde {config_file}...")

        for i in range(13):
            send_line(ser, f"EEPROM_A {i} {config['angleA'][i]}")
            print(f"  → angleA[{i}] = {config['angleA'][i]}")

        for i in range(13):
            send_line(ser, f"EEPROM_C {i} {config['angleCenter'][i]}")

        for i in range(13):
            send_line(ser, f"EEPROM_B {i} {config['angleB'][i]}")

        # Compatibles con el protocolo actual
        if "standPose_coxaGain" in config:
            send_line(ser, f"EEPROM_STAND_COXA {config['standPose_coxaGain']}")
            print(f"  → Stand Coxa = {config['standPose_coxaGain']}")
        if "standPose_heightGain" in config:
            send_line(ser, f"EEPROM_STAND_HEIGHT {config['standPose_heightGain']}")
            print(f"  → Stand Height = {config['standPose_heightGain']}")
        if "gait_coxaGain" in config:
            send_line(ser, f"EEPROM_GAIT_COXA {config['gait_coxaGain']}")
        if "gait_liftGain" in config:
            send_line(ser, f"EEPROM_GAIT_LIFT {config['gait_liftGain']}")
        if "gait_liftMs" in config:
            send_line(ser, f"EEPROM_LIFT_MS {config['gait_liftMs']}")
            print(f"  → Gait Lift MS = {config['gait_liftMs']}")
        if "gait_swingMs" in config:
            send_line(ser, f"EEPROM_SWING_MS {config['gait_swingMs']}")
        if "gait_dropMs" in config:
            send_line(ser, f"EEPROM_DROP_MS {config['gait_dropMs']}")
        if "gait_pushMs" in config:
            send_line(ser, f"EEPROM_PUSH_MS {config['gait_pushMs']}")

        send_line(ser, "EEPROM_SAVE", delay=0.2)
        response = read_for(ser, 1.5)
        if response.strip():
            print(f"\n✓ Respuesta del Arduino:\n{response}")

        ser.close()
        print("\n✓ Configuración cargada")
        return True
    except Exception as e:
        print(f"✗ Error: {e}")
        return False


def main():
    print("=" * 60)
    print("Q-POD MINI - Quickstart Configuration v2")
    print("=" * 60)

    ports = list_ports()
    if not ports:
        print("✗ No hay puertos COM disponibles")
        return

    print("\nPuertos disponibles:")
    for i, (port, desc) in enumerate(ports):
        print(f"  [{i}] {port} - {desc}")

    idx_text = input("\nSelecciona puerto [0]: ").strip()
    idx = int(idx_text) if idx_text else 0
    if idx < 0 or idx >= len(ports):
        print("✗ Índice inválido")
        return

    port = ports[idx][0]
    print(f"\nConectando a {port}...")

    if not test_connection(port):
        print("\n⚠️ No se pudo conectar al Arduino")
        print("Verifica:")
        print("  1. Que cargaste el .ino optimizado en el Nano")
        print("  2. Que el Serial Monitor de Arduino IDE esté cerrado")
        print("  3. Que el .ino responda al comando INFO")
        print("  4. Que uses 115200 baud")
        return

    config_file = input("\nArchivo JSON [default_config.json]: ").strip() or "default_config.json"

    try:
        with open(config_file, "r", encoding="utf-8") as f:
            json.load(f)
    except FileNotFoundError:
        print(f"✗ Archivo no encontrado: {config_file}")
        return
    except json.JSONDecodeError:
        print(f"✗ JSON inválido: {config_file}")
        return

    confirm = input(f"\n¿Cargar configuración desde {config_file}? [s/N]: ").strip().lower()
    if confirm != "s":
        print("Cancelado")
        return

    if upload_config(port, config_file):
        print("\n" + "=" * 60)
        print("✓ ÉXITO")
        print("=" * 60)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nCancelado")
