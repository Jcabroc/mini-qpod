#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Q-POD MINI - Quickstart Configuration
Script para cargar configuración por defecto y verificar conexión
"""

import json
import serial
import serial.tools.list_ports
import time
import sys

def list_ports():
    """Lista puertos COM disponibles"""
    ports = []
    for port, desc, hwid in serial.tools.list_ports.comports():
        ports.append((port, desc))
    return ports

def test_connection(port, baud=115200):
    """Prueba conexión con Arduino"""
    try:
        ser = serial.Serial(port, baud, timeout=1)
        time.sleep(0.5)
        
        # Enviar INFO
        ser.write(b"INFO\n")
        time.sleep(0.2)
        
        if ser.in_waiting:
            response = ser.read(ser.in_waiting).decode()
            print(f"✓ Arduino responde:\n{response}")
            ser.close()
            return True
        else:
            print("✗ Arduino no responde")
            ser.close()
            return False
    except Exception as e:
        print(f"✗ Error: {e}")
        return False

def upload_config(port, config_file, baud=115200):
    """Carga configuración a Arduino"""
    try:
        with open(config_file, 'r') as f:
            config = json.load(f)
        
        ser = serial.Serial(port, baud, timeout=1)
        time.sleep(0.5)
        
        print(f"\n📤 Uploadando configuración desde {config_file}...")
        
        # Enviar ángulos
        for i in range(13):
            cmd = f"EEPROM_A {i} {config['angleA'][i]}\n"
            ser.write(cmd.encode())
            time.sleep(0.05)
            print(f"  → angleA[{i}] = {config['angleA'][i]}")
        
        for i in range(13):
            cmd = f"EEPROM_C {i} {config['angleCenter'][i]}\n"
            ser.write(cmd.encode())
            time.sleep(0.05)
        
        for i in range(13):
            cmd = f"EEPROM_B {i} {config['angleB'][i]}\n"
            ser.write(cmd.encode())
            time.sleep(0.05)
        
        # Enviar ganancias
        cmd = f"EEPROM_STAND_COXA {config['standPose_coxaGain']}\n"
        ser.write(cmd.encode())
        time.sleep(0.05)
        print(f"  → Stand Coxa = {config['standPose_coxaGain']}")
        
        cmd = f"EEPROM_STAND_HEIGHT {config['standPose_heightGain']}\n"
        ser.write(cmd.encode())
        time.sleep(0.05)
        print(f"  → Stand Height = {config['standPose_heightGain']}")
        
        # Enviar timings
        cmd = f"EEPROM_LIFT_MS {config['gait_liftMs']}\n"
        ser.write(cmd.encode())
        time.sleep(0.05)
        print(f"  → Gait Lift MS = {config['gait_liftMs']}")
        
        cmd = f"EEPROM_SWING_MS {config['gait_swingMs']}\n"
        ser.write(cmd.encode())
        time.sleep(0.05)
        
        cmd = f"EEPROM_DROP_MS {config['gait_dropMs']}\n"
        ser.write(cmd.encode())
        time.sleep(0.05)
        
        cmd = f"EEPROM_PUSH_MS {config['gait_pushMs']}\n"
        ser.write(cmd.encode())
        time.sleep(0.05)
        
        # Guardar en EEPROM
        ser.write(b"EEPROM_SAVE\n")
        time.sleep(0.2)
        
        # Leer respuesta
        time.sleep(0.5)
        if ser.in_waiting:
            response = ser.read(ser.in_waiting).decode()
            print(f"\n✓ Respuesta del Arduino:\n{response}")
        
        ser.close()
        print("\n✓ Configuración cargada exitosamente")
        return True
        
    except Exception as e:
        print(f"✗ Error: {e}")
        return False

def main():
    print("=" * 60)
    print("Q-POD MINI - Quickstart Configuration")
    print("=" * 60)
    
    # Listar puertos
    ports = list_ports()
    if not ports:
        print("✗ No hay puertos COM disponibles")
        return
    
    print("\nPuertos disponibles:")
    for i, (port, desc) in enumerate(ports):
        print(f"  [{i}] {port} - {desc}")
    
    # Seleccionar puerto
    idx = input("\nSelecciona puerto [0]: ").strip()
    if not idx:
        idx = 0
    else:
        idx = int(idx)
    
    if idx < 0 or idx >= len(ports):
        print("✗ Índice inválido")
        return
    
    port = ports[idx][0]
    print(f"\nConectando a {port}...")
    
    # Probar conexión
    if not test_connection(port):
        print("\n⚠️ No se pudo conectar al Arduino")
        print("Verifica:")
        print("  1. El Arduino está enchufado")
        print("  2. El puerto es correcto")
        print("  3. El código test_gait.ino tiene serial_protocol.cpp integrado")
        return
    
    # Cargar configuración
    config_file = input("\nArchivo JSON [default_config.json]: ").strip()
    if not config_file:
        config_file = "default_config.json"
    
    try:
        with open(config_file, 'r') as f:
            json.load(f)
    except FileNotFoundError:
        print(f"✗ Archivo no encontrado: {config_file}")
        return
    except json.JSONDecodeError:
        print(f"✗ JSON inválido: {config_file}")
        return
    
    confirm = input(f"\n¿Cargar configuración desde {config_file}? [s/N]: ").strip().lower()
    if confirm != 's':
        print("Cancelado")
        return
    
    # Upload
    if upload_config(port, config_file):
        print("\n" + "=" * 60)
        print("✓ ÉXITO - Puedes desenchufar y enchufar el Arduino")
        print("  Los parámetros se guardarán en EEPROM")
        print("=" * 60)
    else:
        print("\n✗ Error durante la carga")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nCancelado por usuario")
    except Exception as e:
        print(f"\n✗ Error: {e}")
