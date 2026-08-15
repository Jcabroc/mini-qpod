#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
QAP MINI Q-POD - Interfaz de Configuración
GUI para calibración y ajuste de parámetros del robot
Comunicación serial con Arduino Nano + PCA9685
"""

import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import serial
import serial.tools.list_ports
import threading
import json
import time
from datetime import datetime
from collections import deque
import os
from PIL import Image, ImageTk

# =====================================================
# CONFIGURACIÓN
# =====================================================
SERIAL_TIMEOUT = 0.1
SERIAL_BAUD = 115200
WINDOW_WIDTH = 1400
WINDOW_HEIGHT = 900
MAX_LOG_LINES = 500

# Nombres de servos
SERVO_NAMES = [
    "L1_COXA", "L1_FEMUR", "L1_TIBIA",
    "R1_COXA", "R1_FEMUR", "R1_TIBIA",
    "L2_COXA", "L2_FEMUR", "L2_TIBIA",
    "R2_COXA", "R2_FEMUR", "R2_TIBIA",
    "CUELLO"
]

LEGS = {
    "L1": [0, 1, 2],
    "R1": [3, 4, 5],
    "L2": [6, 7, 8],
    "R2": [9, 10, 11],
}

# =====================================================
# DATOS PERSISTENTES
# =====================================================
class ConfigData:
    """Almacena todos los parámetros"""
    def __init__(self):
        self.angleA = [0] * 13
        self.angleCenter = [90] * 13
        self.angleB = [180] * 13
        
        self.standPose_coxaGain = 0.45
        self.standPose_heightGain = 0.70
        
        self.gait_coxaGain = 0.28
        self.gait_liftGain = 0.30
        
        self.gait_liftMs = 150
        self.gait_swingMs = 180
        self.gait_dropMs = 150
        self.gait_pushMs = 200
        
        self.filter_strength = 4
        self.deadzone_percent = 6
        self.send_period_ms = 20
        
    def to_dict(self):
        return self.__dict__
    
    def from_dict(self, d):
        for k, v in d.items():
            if hasattr(self, k):
                setattr(self, k, v)
    
    def save_json(self, filename):
        with open(filename, 'w') as f:
            json.dump(self.to_dict(), f, indent=2)
    
    def load_json(self, filename):
        with open(filename, 'r') as f:
            self.from_dict(json.load(f))


# =====================================================
# COMUNICACIÓN SERIAL
# =====================================================
class ArduinoComm:
    """Maneja comunicación con Arduino"""
    def __init__(self, port=None, baud=SERIAL_BAUD, timeout=SERIAL_TIMEOUT):
        self.port = port
        self.baud = baud
        self.timeout = timeout
        self.ser = None
        self.running = False
        self.rx_thread = None
        self.callbacks = []
        
    def connect(self, port):
        try:
            self.ser = serial.Serial(port, self.baud, timeout=self.timeout)
            time.sleep(0.5)
            self.running = True
            self.rx_thread = threading.Thread(target=self._rx_loop, daemon=True)
            self.rx_thread.start()
            return True
        except Exception as e:
            print(f"Error conectando: {e}")
            return False
    
    def disconnect(self):
        self.running = False
        if self.ser and self.ser.is_open:
            self.ser.close()
    
    def _rx_loop(self):
        """Loop de lectura en background"""
        buffer = ""
        while self.running:
            if self.ser and self.ser.is_open:
                try:
                    if self.ser.in_waiting:
                        data = self.ser.read(self.ser.in_waiting).decode('utf-8', errors='ignore')
                        buffer += data
                        
                        # Procesar líneas completas
                        while '\n' in buffer:
                            line, buffer = buffer.split('\n', 1)
                            line = line.strip()
                            if line:
                                self._dispatch_line(line)
                except Exception as e:
                    print(f"Error leyendo: {e}")
            time.sleep(0.01)
    
    def _dispatch_line(self, line):
        """Dispara callbacks para líneas recibidas"""
        for callback in self.callbacks:
            try:
                callback(line)
            except Exception as e:
                print(f"Error en callback: {e}")
    
    def send_cmd(self, cmd):
        """Envía comando al Arduino"""
        if self.ser and self.ser.is_open:
            self.ser.write((cmd + '\n').encode())
    
    def register_callback(self, func):
        """Registra función callback para líneas recibidas"""
        self.callbacks.append(func)
    
    @staticmethod
    def list_ports():
        """Lista puertos COM disponibles"""
        ports = []
        for port, desc, hwid in serial.tools.list_ports.comports():
            ports.append(f"{port} - {desc}")
        return ports


# =====================================================
# CONFIGURACIÓN DE TEMA OSCURO
# =====================================================
DARK_BG = "#1e1e1e"           # Fondo principal muy oscuro
DARK_FRAME = "#2d2d2d"         # Fondo de frames
DARK_LIGHTER = "#3d3d3d"       # Fondo más claro (hover)
TEXT_COLOR = "#e0e0e0"         # Texto principal
TEXT_MUTED = "#a0a0a0"         # Texto secundario
ACCENT_SUCCESS = "#4caf50"     # Verde para éxito
ACCENT_ERROR = "#f44336"       # Rojo para error
ACCENT_INFO = "#2196f3"        # Azul para info


def configure_dark_theme():
    """Configura tema oscuro para ttk"""
    style = ttk.Style()
    
    # Tema base
    style.theme_use('clam')
    
    # Colores principales
    style.configure('TFrame', background=DARK_BG, relief='flat')
    style.configure('TLabel', background=DARK_BG, foreground=TEXT_COLOR)
    style.configure('TLabelFrame', background=DARK_BG, foreground=TEXT_COLOR, relief='flat', borderwidth=1)
    style.configure('TLabelFrame.Label', background=DARK_BG, foreground=TEXT_COLOR)
    
    # Botones
    style.configure('TButton',
                   background=DARK_FRAME,
                   foreground=TEXT_COLOR,
                   relief='solid',
                   borderwidth=1,
                   focuscolor='none',
                   padding=5)
    style.map('TButton',
             background=[('active', DARK_LIGHTER)],
             foreground=[('active', TEXT_COLOR)])
    
    # Combobox
    style.configure('TCombobox',
                   fieldbackground=DARK_FRAME,
                   background=DARK_FRAME,
                   foreground=TEXT_COLOR)
    
    # Spinbox
    style.configure('TSpinbox',
                   fieldbackground=DARK_FRAME,
                   background=DARK_FRAME,
                   foreground=TEXT_COLOR)
    
    # Scale/Slider
    style.configure('TScale', background=DARK_BG)
    
    # Notebook (tabs)
    style.configure('TNotebook', background=DARK_BG, borderwidth=0)
    style.configure('TNotebook.Tab',
                   background=DARK_FRAME,
                   foreground=TEXT_COLOR,
                   padding=[10, 5])
    style.map('TNotebook.Tab',
             background=[('selected', DARK_LIGHTER)],
             foreground=[('selected', TEXT_COLOR)])
    
    # Scrollbar
    style.configure('TScrollbar', background=DARK_FRAME)
    
    return style


# =====================================================
# GUI PRINCIPAL
# =====================================================
class QpodConfigGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Q-POD MINI - Configurador")
        self.root.geometry(f"{WINDOW_WIDTH}x{WINDOW_HEIGHT}")
        self.root.configure(bg=DARK_BG)
        
        # Cargar icono si existe
        try:
            if os.path.exists("qpod_icon.ico"):
                self.root.iconbitmap("qpod_icon.ico")
        except Exception as e:
            print(f"Nota: No se pudo cargar el icono: {e}")
        
        # Aplicar tema oscuro
        self.style = configure_dark_theme()
        
        self.config = ConfigData()
        self.arduino = ArduinoComm()
        self.arduino.register_callback(self._on_serial_line)
        
        self.connected = False
        self.log_buffer = deque(maxlen=MAX_LOG_LINES)
        self.logo_image = None  # Referencia para evitar garbage collection
        
        self._build_ui()
        self._start_refresh()
    
    def _build_ui(self):
        """Construye la interfaz"""
        # ============================================
        # HEADER CON LOGO
        # ============================================
        header_frame = tk.Frame(self.root, bg=DARK_FRAME, height=90)
        header_frame.pack(side=tk.TOP, fill=tk.X)
        header_frame.pack_propagate(False)
        
        # Cargar y mostrar logo si existe
        try:
            if os.path.exists("qpod_logo.png"):
                logo_pil = Image.open("qpod_logo.png")
                # Redimensionar a altura de 80px manteniendo proporción
                ratio = 80 / logo_pil.height
                new_size = (int(logo_pil.width * ratio), 80)
                logo_pil = logo_pil.resize(new_size, Image.Resampling.LANCZOS)
                
                self.logo_image = ImageTk.PhotoImage(logo_pil)
                logo_label = tk.Label(header_frame, image=self.logo_image, bg=DARK_FRAME)
                logo_label.pack(side=tk.LEFT, padx=10, pady=5)
        except Exception as e:
            print(f"Nota: No se pudo cargar el logo: {e}")
        
        # Barra superior: conexión + botones
        top_frame = ttk.Frame(self.root)
        top_frame.pack(side=tk.TOP, fill=tk.X, padx=5, pady=5)
        
        # Selector de puerto
        ttk.Label(top_frame, text="Puerto COM:").pack(side=tk.LEFT, padx=5)
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(top_frame, textvariable=self.port_var, width=20)
        self.port_combo['values'] = ArduinoComm.list_ports()
        self.port_combo.pack(side=tk.LEFT, padx=5)
        
        ttk.Button(top_frame, text="Conectar", command=self._connect).pack(side=tk.LEFT, padx=2)
        ttk.Button(top_frame, text="Desconectar", command=self._disconnect).pack(side=tk.LEFT, padx=2)
        
        # Estado conexión
        self.status_label = ttk.Label(top_frame, text="[Desconectado]", font=("Arial", 10, "bold"))
        self.status_label.pack(side=tk.LEFT, padx=20)
        self._update_status_color()  # Inicial: rojo
        
        # Botones guardar/cargar
        ttk.Button(top_frame, text="💾 Guardar JSON", command=self._save_json).pack(side=tk.LEFT, padx=2)
        ttk.Button(top_frame, text="📂 Cargar JSON", command=self._load_json).pack(side=tk.LEFT, padx=2)
        ttk.Button(top_frame, text="📤 → Arduino (EEPROM)", command=self._send_to_arduino).pack(side=tk.LEFT, padx=2)
        
        # Notebook (tabs)
        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Tabs
        self._build_tab_state()
        self._build_tab_calibration()
        self._build_tab_poses()
        self._build_tab_gait()
        self._build_tab_advanced()
        self._build_tab_log()
    
    def _build_tab_state(self):
        """Tab 1: Estado del robot"""
        frame = ttk.Frame(self.notebook)
        self.notebook.add(frame, text="Estado")
        
        # Panel estados
        state_panel = ttk.LabelFrame(frame, text="Estado Hardware", padding=10)
        state_panel.pack(fill=tk.X, padx=5, pady=5)
        
        self.state_nrf = ttk.Label(state_panel, text="NRF24: [--]", font=("Arial", 10))
        self.state_nrf.pack(anchor=tk.W)
        
        self.state_pca = ttk.Label(state_panel, text="PCA9685: [--]", font=("Arial", 10))
        self.state_pca.pack(anchor=tk.W)
        
        self.state_battery = ttk.Label(state_panel, text="Batería: [--]%", font=("Arial", 10))
        self.state_battery.pack(anchor=tk.W)
        
        self.state_mode = ttk.Label(state_panel, text="Modo: [--]", font=("Arial", 10))
        self.state_mode.pack(anchor=tk.W)
        
        # Panel servos (grid de 13)
        servo_frame = ttk.LabelFrame(frame, text="Ángulos Actuales de Servos", padding=10)
        servo_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        self.servo_labels = []
        for i in range(13):
            row = i // 7
            col = i % 7
            
            label = ttk.Label(servo_frame, text=f"{SERVO_NAMES[i]}: 0°", font=("Arial", 9))
            label.grid(row=row, column=col, sticky=tk.W, padx=10, pady=5)
            self.servo_labels.append(label)
    
    def _build_tab_calibration(self):
        """Tab 2: Calibración de servos"""
        frame = ttk.Frame(self.notebook)
        self.notebook.add(frame, text="Calibración")
        
        # Lista de servos
        list_frame = ttk.Frame(frame)
        list_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        ttk.Label(list_frame, text="Servos:").pack()
        
        self.servo_listbox = tk.Listbox(list_frame, height=20,
                                       bg=DARK_FRAME, fg=TEXT_COLOR,
                                       selectbackground=DARK_LIGHTER,
                                       selectforeground=TEXT_COLOR,
                                       relief=tk.FLAT, borderwidth=1)
        for name in SERVO_NAMES:
            self.servo_listbox.insert(tk.END, name)
        self.servo_listbox.pack(fill=tk.BOTH, expand=True)
        self.servo_listbox.bind('<<ListboxSelect>>', self._on_servo_select)
        
        # Panel de valores
        values_frame = ttk.LabelFrame(frame, text="Ángulos", padding=10)
        values_frame.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        self.calib_servo_name = ttk.Label(values_frame, text="Servo: --", font=("Arial", 12, "bold"))
        self.calib_servo_name.pack(pady=10)
        
        # AngleA
        ttk.Label(values_frame, text="Ángulo A (extremo 1):").pack()
        angle_a_frame = ttk.Frame(values_frame)
        angle_a_frame.pack(fill=tk.X, pady=5)
        self.angle_a_spinbox = ttk.Spinbox(angle_a_frame, from_=0, to=180, width=10)
        self.angle_a_spinbox.pack(side=tk.LEFT, padx=5)
        ttk.Button(angle_a_frame, text="Enviar", command=self._send_angle_a).pack(side=tk.LEFT, padx=2)
        
        # AngleCenter
        ttk.Label(values_frame, text="Ángulo Centro:").pack()
        angle_c_frame = ttk.Frame(values_frame)
        angle_c_frame.pack(fill=tk.X, pady=5)
        self.angle_center_spinbox = ttk.Spinbox(angle_c_frame, from_=0, to=180, width=10)
        self.angle_center_spinbox.pack(side=tk.LEFT, padx=5)
        ttk.Button(angle_c_frame, text="Enviar", command=self._send_angle_center).pack(side=tk.LEFT, padx=2)
        
        # AngleB
        ttk.Label(values_frame, text="Ángulo B (extremo 2):").pack()
        angle_b_frame = ttk.Frame(values_frame)
        angle_b_frame.pack(fill=tk.X, pady=5)
        self.angle_b_spinbox = ttk.Spinbox(angle_b_frame, from_=0, to=180, width=10)
        self.angle_b_spinbox.pack(side=tk.LEFT, padx=5)
        ttk.Button(angle_b_frame, text="Enviar", command=self._send_angle_b).pack(side=tk.LEFT, padx=2)
        
        self.current_servo_idx = 0
    
    def _build_tab_poses(self):
        """Tab 3: Ganancias de postura"""
        frame = ttk.Frame(self.notebook)
        self.notebook.add(frame, text="Posturas")
        
        # StandPose
        stand_frame = ttk.LabelFrame(frame, text="Postura Base (StandPose)", padding=15)
        stand_frame.pack(fill=tk.X, padx=10, pady=10)
        
        ttk.Label(stand_frame, text="Ganancia Coxa (apertura patas):").pack()
        coxa_frame = ttk.Frame(stand_frame)
        coxa_frame.pack(fill=tk.X, pady=5)
        self.stand_coxa_var = tk.DoubleVar(value=self.config.standPose_coxaGain)
        self.stand_coxa_scale = ttk.Scale(coxa_frame, from_=0.0, to=1.0, variable=self.stand_coxa_var, orient=tk.HORIZONTAL)
        self.stand_coxa_scale.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)
        self.stand_coxa_label = ttk.Label(coxa_frame, text=f"{self.config.standPose_coxaGain:.2f}")
        self.stand_coxa_label.pack(side=tk.LEFT, padx=5)
        
        ttk.Label(stand_frame, text="Ganancia Altura (baja el robot):").pack()
        height_frame = ttk.Frame(stand_frame)
        height_frame.pack(fill=tk.X, pady=5)
        self.stand_height_var = tk.DoubleVar(value=self.config.standPose_heightGain)
        self.stand_height_scale = ttk.Scale(height_frame, from_=0.0, to=1.0, variable=self.stand_height_var, orient=tk.HORIZONTAL)
        self.stand_height_scale.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)
        self.stand_height_label = ttk.Label(height_frame, text=f"{self.config.standPose_heightGain:.2f}")
        self.stand_height_label.pack(side=tk.LEFT, padx=5)
        
        self.stand_coxa_var.trace('w', self._on_stand_coxa_change)
        self.stand_height_var.trace('w', self._on_stand_height_change)
    
    def _build_tab_gait(self):
        """Tab 4: Parámetros gait"""
        frame = ttk.Frame(self.notebook)
        self.notebook.add(frame, text="Gait")
        
        # Ganancias
        gain_frame = ttk.LabelFrame(frame, text="Ganancias", padding=15)
        gain_frame.pack(fill=tk.X, padx=10, pady=10)
        
        ttk.Label(gain_frame, text="Ganancia Coxa (amplitud paso):").pack()
        gait_coxa_frame = ttk.Frame(gain_frame)
        gait_coxa_frame.pack(fill=tk.X, pady=5)
        self.gait_coxa_var = tk.DoubleVar(value=self.config.gait_coxaGain)
        ttk.Scale(gait_coxa_frame, from_=0.0, to=1.0, variable=self.gait_coxa_var, orient=tk.HORIZONTAL).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)
        ttk.Label(gait_coxa_frame, text=f"{self.config.gait_coxaGain:.2f}").pack(side=tk.LEFT, padx=5)
        
        ttk.Label(gain_frame, text="Ganancia Lift (altura levantada):").pack()
        gait_lift_frame = ttk.Frame(gain_frame)
        gait_lift_frame.pack(fill=tk.X, pady=5)
        self.gait_lift_var = tk.DoubleVar(value=self.config.gait_liftGain)
        ttk.Scale(gait_lift_frame, from_=0.0, to=1.0, variable=self.gait_lift_var, orient=tk.HORIZONTAL).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)
        ttk.Label(gait_lift_frame, text=f"{self.config.gait_liftGain:.2f}").pack(side=tk.LEFT, padx=5)
        
        # Timings
        timing_frame = ttk.LabelFrame(frame, text="Timings de Fases (ms)", padding=15)
        timing_frame.pack(fill=tk.X, padx=10, pady=10)
        
        # Lift
        ttk.Label(timing_frame, text="LIFT (levantamiento):").pack()
        lift_frame = ttk.Frame(timing_frame)
        lift_frame.pack(fill=tk.X, pady=5)
        self.gait_lift_ms_spinbox = ttk.Spinbox(lift_frame, from_=50, to=500, width=10)
        self.gait_lift_ms_spinbox.set(self.config.gait_liftMs)
        self.gait_lift_ms_spinbox.pack(side=tk.LEFT, padx=5)
        ttk.Label(lift_frame, text="ms").pack(side=tk.LEFT)
        
        # Swing
        ttk.Label(timing_frame, text="SWING (movimiento horizontal):").pack()
        swing_frame = ttk.Frame(timing_frame)
        swing_frame.pack(fill=tk.X, pady=5)
        self.gait_swing_ms_spinbox = ttk.Spinbox(swing_frame, from_=50, to=500, width=10)
        self.gait_swing_ms_spinbox.set(self.config.gait_swingMs)
        self.gait_swing_ms_spinbox.pack(side=tk.LEFT, padx=5)
        ttk.Label(swing_frame, text="ms").pack(side=tk.LEFT)
        
        # Drop
        ttk.Label(timing_frame, text="DROP (apoyo):").pack()
        drop_frame = ttk.Frame(timing_frame)
        drop_frame.pack(fill=tk.X, pady=5)
        self.gait_drop_ms_spinbox = ttk.Spinbox(drop_frame, from_=50, to=500, width=10)
        self.gait_drop_ms_spinbox.set(self.config.gait_dropMs)
        self.gait_drop_ms_spinbox.pack(side=tk.LEFT, padx=5)
        ttk.Label(drop_frame, text="ms").pack(side=tk.LEFT)
        
        # Push
        ttk.Label(timing_frame, text="PUSH (empuje):").pack()
        push_frame = ttk.Frame(timing_frame)
        push_frame.pack(fill=tk.X, pady=5)
        self.gait_push_ms_spinbox = ttk.Spinbox(push_frame, from_=50, to=500, width=10)
        self.gait_push_ms_spinbox.set(self.config.gait_pushMs)
        self.gait_push_ms_spinbox.pack(side=tk.LEFT, padx=5)
        ttk.Label(push_frame, text="ms").pack(side=tk.LEFT)
    
    def _build_tab_advanced(self):
        """Tab 5: Parámetros avanzados"""
        frame = ttk.Frame(self.notebook)
        self.notebook.add(frame, text="Avanzado")
        
        # Control remoto
        remote_frame = ttk.LabelFrame(frame, text="Control Remoto PS2J", padding=15)
        remote_frame.pack(fill=tk.X, padx=10, pady=10)
        
        ttk.Label(remote_frame, text="Filter Strength (2-8, más alto = más suave):").pack()
        filter_frame = ttk.Frame(remote_frame)
        filter_frame.pack(fill=tk.X, pady=5)
        self.filter_strength_spinbox = ttk.Spinbox(filter_frame, from_=2, to=8, width=10)
        self.filter_strength_spinbox.set(self.config.filter_strength)
        self.filter_strength_spinbox.pack(side=tk.LEFT, padx=5)
        
        ttk.Label(remote_frame, text="Deadzone (% del rango):").pack()
        deadzone_frame = ttk.Frame(remote_frame)
        deadzone_frame.pack(fill=tk.X, pady=5)
        self.deadzone_spinbox = ttk.Spinbox(deadzone_frame, from_=1, to=20, width=10)
        self.deadzone_spinbox.set(self.config.deadzone_percent)
        self.deadzone_spinbox.pack(side=tk.LEFT, padx=5)
        
        ttk.Label(remote_frame, text="Send Period (ms):").pack()
        send_frame = ttk.Frame(remote_frame)
        send_frame.pack(fill=tk.X, pady=5)
        self.send_period_spinbox = ttk.Spinbox(send_frame, from_=5, to=100, width=10)
        self.send_period_spinbox.set(self.config.send_period_ms)
        self.send_period_spinbox.pack(side=tk.LEFT, padx=5)
    
    def _build_tab_log(self):
        """Tab 6: Log de comunicación"""
        frame = ttk.Frame(self.notebook)
        self.notebook.add(frame, text="Log")
        
        # Texto con scrollbar
        text_frame = ttk.Frame(frame)
        text_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        scrollbar = ttk.Scrollbar(text_frame)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        self.log_text = tk.Text(text_frame, height=30, wrap=tk.WORD, 
                               yscrollcommand=scrollbar.set,
                               bg=DARK_FRAME, fg=TEXT_COLOR,
                               insertbackground=TEXT_COLOR,
                               selectbackground=DARK_LIGHTER,
                               selectforeground=TEXT_COLOR,
                               relief=tk.FLAT, borderwidth=1)
        self.log_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.config(command=self.log_text.yview)
        
        # Configurar etiquetas de color para el log
        self.log_text.tag_config('info', foreground=ACCENT_INFO)
        self.log_text.tag_config('success', foreground=ACCENT_SUCCESS)
        self.log_text.tag_config('error', foreground=ACCENT_ERROR)
    
    # =====================================================
    # CALLBACKS
    # =====================================================
    def _on_servo_select(self, event):
        """Selecciona servo de lista"""
        sel = self.servo_listbox.curselection()
        if sel:
            self.current_servo_idx = sel[0]
            self.calib_servo_name.config(text=f"Servo: {SERVO_NAMES[self.current_servo_idx]}")
            self.angle_a_spinbox.set(self.config.angleA[self.current_servo_idx])
            self.angle_center_spinbox.set(self.config.angleCenter[self.current_servo_idx])
            self.angle_b_spinbox.set(self.config.angleB[self.current_servo_idx])
    
    def _send_angle_a(self):
        val = int(self.angle_a_spinbox.get())
        self.config.angleA[self.current_servo_idx] = val
        self.arduino.send_cmd(f"A {self.current_servo_idx} {val}")
        self._log(f"→ Ángulo A[{self.current_servo_idx}] = {val}°")
    
    def _send_angle_center(self):
        val = int(self.angle_center_spinbox.get())
        self.config.angleCenter[self.current_servo_idx] = val
        self.arduino.send_cmd(f"C {self.current_servo_idx} {val}")
        self._log(f"→ Ángulo Center[{self.current_servo_idx}] = {val}°")
    
    def _send_angle_b(self):
        val = int(self.angle_b_spinbox.get())
        self.config.angleB[self.current_servo_idx] = val
        self.arduino.send_cmd(f"B {self.current_servo_idx} {val}")
        self._log(f"→ Ángulo B[{self.current_servo_idx}] = {val}°")
    
    def _on_stand_coxa_change(self, *args):
        val = self.stand_coxa_var.get()
        self.stand_coxa_label.config(text=f"{val:.2f}")
        self.config.standPose_coxaGain = val
        self.arduino.send_cmd(f"STAND_COXA {val:.2f}")
    
    def _on_stand_height_change(self, *args):
        val = self.stand_height_var.get()
        self.stand_height_label.config(text=f"{val:.2f}")
        self.config.standPose_heightGain = val
        self.arduino.send_cmd(f"STAND_HEIGHT {val:.2f}")
    
    def _on_serial_line(self, line):
        """Callback de línea serial recibida"""
        self._log(f"← {line}")
        
        # Parsear mensajes especiales
        if line.startswith("STATE:"):
            self._parse_state(line)
        elif line.startswith("SERVO:"):
            self._parse_servo(line)
    
    def _parse_state(self, line):
        """Parsea mensajes de estado"""
        # STATE: NRF=OK PCA=OK BATTERY=85
        try:
            parts = line.split("STATE:")[1].strip().split()
            for part in parts:
                if "NRF=" in part:
                    nrf_state = part.split("=")[1]
                    self.state_nrf.config(text=f"NRF24: [{nrf_state}]")
                elif "PCA=" in part:
                    pca_state = part.split("=")[1]
                    self.state_pca.config(text=f"PCA9685: [{pca_state}]")
                elif "BATTERY=" in part:
                    battery = part.split("=")[1]
                    self.state_battery.config(text=f"Batería: [{battery}]%")
        except:
            pass
    
    def _parse_servo(self, line):
        """Parsea ángulos de servos"""
        # SERVO: 0=45 1=90 2=135 ...
        try:
            parts = line.split("SERVO:")[1].strip().split()
            for part in parts:
                if "=" in part:
                    ch, angle = part.split("=")
                    ch = int(ch)
                    if ch < 13:
                        self.servo_labels[ch].config(text=f"{SERVO_NAMES[ch]}: {angle}°")
        except:
            pass
    
    def _log(self, msg):
        """Añade línea al log"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.log_buffer.append(f"[{timestamp}] {msg}")
        self.log_text.insert(tk.END, f"[{timestamp}] {msg}\n")
        self.log_text.see(tk.END)
    
    def _connect(self):
        """Conecta con Arduino"""
        port = self.port_var.get()
        if not port:
            messagebox.showerror("Error", "Selecciona un puerto")
            return
        
        port_name = port.split(" - ")[0]
        if self.arduino.connect(port_name):
            self.connected = True
            self.status_label.config(text="[✓ Conectado]")
            self.status_label.configure(foreground=ACCENT_SUCCESS)
            self._log(f"Conectado a {port_name}")
            self.arduino.send_cmd("INFO")
        else:
            messagebox.showerror("Error", f"No se pudo conectar a {port_name}")
    
    def _disconnect(self):
        """Desconecta"""
        self.arduino.disconnect()
        self.connected = False
        self.status_label.config(text="[Desconectado]")
        self.status_label.configure(foreground=ACCENT_ERROR)
        self._log("Desconectado")
    
    def _update_status_color(self):
        """Actualiza el color del estado inicial"""
        self.status_label.configure(foreground=ACCENT_ERROR)
    
    def _save_json(self):
        """Guarda configuración a JSON"""
        filename = filedialog.asksaveasfilename(
            defaultextension=".json",
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")],
            initialfile=f"qpod_config_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
        )
        if filename:
            self._update_config_from_ui()
            self.config.save_json(filename)
            self._log(f"Configuración guardada en {filename}")
            messagebox.showinfo("Éxito", "Configuración guardada")
    
    def _load_json(self):
        """Carga configuración de JSON"""
        filename = filedialog.askopenfilename(
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")]
        )
        if filename:
            self.config.load_json(filename)
            self._update_ui_from_config()
            self._log(f"Configuración cargada desde {filename}")
            messagebox.showinfo("Éxito", "Configuración cargada")
    
    def _send_to_arduino(self):
        """Envía configuración a Arduino (EEPROM)"""
        if not self.connected:
            messagebox.showerror("Error", "No conectado")
            return
        
        self._update_config_from_ui()
        
        # Enviar ángulos
        for i in range(13):
            self.arduino.send_cmd(f"EEPROM_A {i} {self.config.angleA[i]}")
            self.arduino.send_cmd(f"EEPROM_C {i} {self.config.angleCenter[i]}")
            self.arduino.send_cmd(f"EEPROM_B {i} {self.config.angleB[i]}")
            time.sleep(0.05)
        
        # Enviar ganancias
        self.arduino.send_cmd(f"EEPROM_STAND_COXA {self.config.standPose_coxaGain:.3f}")
        self.arduino.send_cmd(f"EEPROM_STAND_HEIGHT {self.config.standPose_heightGain:.3f}")
        self.arduino.send_cmd(f"EEPROM_GAIT_COXA {self.config.gait_coxaGain:.3f}")
        self.arduino.send_cmd(f"EEPROM_GAIT_LIFT {self.config.gait_liftGain:.3f}")
        
        # Enviar timings
        self.arduino.send_cmd(f"EEPROM_LIFT_MS {self.config.gait_liftMs}")
        self.arduino.send_cmd(f"EEPROM_SWING_MS {self.config.gait_swingMs}")
        self.arduino.send_cmd(f"EEPROM_DROP_MS {self.config.gait_dropMs}")
        self.arduino.send_cmd(f"EEPROM_PUSH_MS {self.config.gait_pushMs}")
        
        self._log("Configuración enviada a EEPROM del Arduino")
        messagebox.showinfo("Éxito", "Parámetros guardados en EEPROM del robot")
    
    def _update_config_from_ui(self):
        """Actualiza config desde valores UI"""
        self.config.gait_coxaGain = self.gait_coxa_var.get()
        self.config.gait_liftGain = self.gait_lift_var.get()
        self.config.gait_liftMs = int(self.gait_lift_ms_spinbox.get())
        self.config.gait_swingMs = int(self.gait_swing_ms_spinbox.get())
        self.config.gait_dropMs = int(self.gait_drop_ms_spinbox.get())
        self.config.gait_pushMs = int(self.gait_push_ms_spinbox.get())
        self.config.filter_strength = int(self.filter_strength_spinbox.get())
        self.config.deadzone_percent = int(self.deadzone_spinbox.get())
        self.config.send_period_ms = int(self.send_period_spinbox.get())
    
    def _update_ui_from_config(self):
        """Actualiza UI desde config"""
        self.angle_a_spinbox.set(self.config.angleA[0] if self.config.angleA[0] else 0)
        self.angle_center_spinbox.set(self.config.angleCenter[0] if self.config.angleCenter[0] else 90)
        self.angle_b_spinbox.set(self.config.angleB[0] if self.config.angleB[0] else 180)
        
        self.stand_coxa_var.set(self.config.standPose_coxaGain)
        self.stand_height_var.set(self.config.standPose_heightGain)
        
        self.gait_coxa_var.set(self.config.gait_coxaGain)
        self.gait_lift_var.set(self.config.gait_liftGain)
        
        self.gait_lift_ms_spinbox.set(self.config.gait_liftMs)
        self.gait_swing_ms_spinbox.set(self.config.gait_swingMs)
        self.gait_drop_ms_spinbox.set(self.config.gait_dropMs)
        self.gait_push_ms_spinbox.set(self.config.gait_pushMs)
        
        self._log("UI actualizada desde configuración")
    
    def _start_refresh(self):
        """Refresca valores cada 500ms"""
        def refresh():
            if self.connected:
                self.arduino.send_cmd("STATE")
                self.arduino.send_cmd("ANGLES")
        
        self.root.after(500, refresh)
        self.root.after(500, self._start_refresh)


# =====================================================
# MAIN
# =====================================================
if __name__ == "__main__":
    root = tk.Tk()
    app = QpodConfigGUI(root)
    root.mainloop()
