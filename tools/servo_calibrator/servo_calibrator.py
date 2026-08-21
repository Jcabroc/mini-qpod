"""Provisional, local-only Mini Q-POD servo calibrator for Windows."""

from __future__ import annotations

import queue
import threading
import tkinter as tk
from tkinter import messagebox, ttk
from pathlib import Path

from backup import ActiveConfigCollector, BackupGate, build_backup, compare_channels, write_backup
from protocol import BAUDRATE, ELECTRICAL_CENTER_DEG, SERVO_NAMES, attempt_safe_off, command_is_allowed, command_name, parse_float, parse_int, parse_key_values
from simulated_serial import SimulatedSerial

try:
    import serial
    import serial.tools.list_ports
except ImportError:  # Lets SIMULATED run before PySerial is installed.
    serial = None


SIMULATED_PORT = "SIMULATED (sin hardware)"


class ServoCalibratorApp(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("Mini Q-POD — Calibrador provisional")
        self.minsize(1040, 690)
        self.protocol("WM_DELETE_WINDOW", self._on_close)

        self.port = None
        self.reader: threading.Thread | None = None
        self.stop_reader = threading.Event()
        self.incoming: queue.Queue[tuple[str, str]] = queue.Queue()
        self.connected = False
        self.motion_locked = True
        self.status_seen = False
        self.imu_seen = False
        self.config_seen = False
        self.imu_healthy = False
        self.current_mode = "UNKNOWN"
        self.active_channel = -1
        self.selected_channel = -1
        self.configs: dict[int, dict[str, str]] = {}
        self.active_rows: list[dict[str, int | str]] = []
        self.live_collector = ActiveConfigCollector()
        self.backup_collector = ActiveConfigCollector()
        self.backup_capture_pending = False
        self.backup_snapshot: dict[str, object] | None = None
        self.backup_gate = BackupGate()
        self.firmware_identification = "UNKNOWN"

        self.port_var = tk.StringVar()
        self.connection_var = tk.StringVar(value="DESCONECTADO")
        self.mode_var = tk.StringVar(value="Modo: —")
        self.imu_var = tk.StringVar(value="IMU: —")
        self.servos_var = tk.StringVar(value="Servos: OFF")
        self.selection_var = tk.StringVar()
        self.first_angle_var = tk.StringVar(value="90")
        self.manual_angle_var = tk.StringVar(value="90")
        self.electrical_var = tk.StringVar(value="90° (referencia física)")
        self.mechanical_var = tk.StringVar(value="No separado en MVP")
        self.center_var = tk.StringVar(value="—")
        self.initial_var = tk.StringVar(value="—")
        self.minimum_var = tk.StringVar(value="")
        self.maximum_var = tk.StringVar(value="")
        self.safe_min_var = tk.StringVar(value="—")
        self.safe_max_var = tk.StringVar(value="—")
        self.direction_var = tk.StringVar(value="—")
        self.raw_command_var = tk.StringVar()
        self.backup_var = tk.StringVar(value="Respaldo obligatorio antes de LIMITS/SAVE.")
        self.comparison_var = tk.StringVar(value="Comparación: sin respaldo confirmado.")

        self._build_ui()
        self.refresh_ports()
        self.after(40, self._drain_incoming)
        self.after(250, self._heartbeat_tick)

    def _build_ui(self) -> None:
        root = ttk.Frame(self, padding=10)
        root.grid(sticky="nsew")
        self.columnconfigure(0, weight=1)
        self.rowconfigure(0, weight=1)
        root.columnconfigure(0, weight=1)
        root.rowconfigure(4, weight=1)

        connection = ttk.LabelFrame(root, text="Conexión", padding=8)
        connection.grid(row=0, column=0, sticky="ew")
        connection.columnconfigure(1, weight=1)
        ttk.Label(connection, text="Puerto:").grid(row=0, column=0, padx=(0, 5))
        self.port_box = ttk.Combobox(connection, textvariable=self.port_var, state="readonly", width=42)
        self.port_box.grid(row=0, column=1, sticky="ew")
        ttk.Button(connection, text="Actualizar", command=self.refresh_ports).grid(row=0, column=2, padx=5)
        self.connect_button = ttk.Button(connection, text="Conectar a 115200", command=self.connect)
        self.connect_button.grid(row=0, column=3, padx=3)
        self.disconnect_button = ttk.Button(connection, text="Desconectar", command=self.disconnect, state="disabled")
        self.disconnect_button.grid(row=0, column=4, padx=3)
        self.connection_label = ttk.Label(connection, textvariable=self.connection_var)
        self.connection_label.grid(row=1, column=0, columnspan=5, sticky="w", pady=(6, 0))

        state = ttk.LabelFrame(root, text="Estado del Nano", padding=8)
        state.grid(row=1, column=0, sticky="ew", pady=(8, 0))
        for column, variable in enumerate((self.mode_var, self.imu_var, self.servos_var)):
            ttk.Label(state, textvariable=variable, width=30).grid(row=0, column=column, sticky="w", padx=(0, 14))

        work = ttk.Frame(root)
        work.grid(row=2, column=0, sticky="nsew", pady=(8, 0))
        work.columnconfigure(0, weight=1)
        work.columnconfigure(1, weight=2)

        select = ttk.LabelFrame(work, text="1. Seleccionar canal (no energiza)", padding=8)
        select.grid(row=0, column=0, sticky="nsew", padx=(0, 8))
        select.columnconfigure(0, weight=1)
        self.servo_box = ttk.Combobox(select, textvariable=self.selection_var, state="readonly", values=self._servo_choices())
        self.servo_box.grid(row=0, column=0, sticky="ew")
        self.servo_box.bind("<<ComboboxSelected>>", lambda _event: self._display_selected_config())
        self.calib_button = ttk.Button(select, text="CALIB (PWM OFF)", command=lambda: self.send_command("CALIB"))
        self.calib_button.grid(row=1, column=0, sticky="ew", pady=(6, 0))
        self.select_button = ttk.Button(select, text="SELECT", command=self.select_channel)
        self.select_button.grid(row=2, column=0, sticky="ew", pady=(6, 0))
        ttk.Label(select, text="CALIB no energiza. Seleccionar otro canal apaga el anterior.", wraplength=245).grid(row=3, column=0, sticky="w", pady=(8, 0))

        controls = ttk.LabelFrame(work, text="2. Habilitar y mover un solo servo", padding=8)
        controls.grid(row=0, column=1, sticky="nsew")
        controls.columnconfigure(1, weight=1)
        ttk.Label(controls, text="Primer ángulo:").grid(row=0, column=0, sticky="w")
        self.first_entry = ttk.Entry(controls, textvariable=self.first_angle_var, width=12)
        self.first_entry.grid(row=0, column=1, sticky="w")
        self.enable_button = ttk.Button(controls, text="ENABLE seleccionado", command=self.enable_selected)
        self.enable_button.grid(row=0, column=2, padx=(8, 0))
        ttk.Label(controls, text="Advertencia: el primer PWM puede mover el servo de inmediato.", foreground="#a00000").grid(row=1, column=0, columnspan=3, sticky="w", pady=(6, 8))
        self.center_button = ttk.Button(controls, text="CENTER seleccionado", command=self.center_selected)
        self.center_button.grid(row=2, column=0, sticky="w")
        for index, (text, delta) in enumerate((("−5°", -5), ("−1°", -1), ("+1°", 1), ("+5°", 5))):
            button = ttk.Button(controls, text=text, command=lambda d=delta: self.move_delta(d))
            button.grid(row=2, column=index + 1, padx=(6, 0), sticky="w")
            setattr(self, f"delta_{index}", button)
        ttk.Label(controls, text="Ángulo manual:").grid(row=3, column=0, sticky="w", pady=(10, 0))
        self.manual_entry = ttk.Entry(controls, textvariable=self.manual_angle_var, width=12)
        self.manual_entry.grid(row=3, column=1, sticky="w", pady=(10, 0))
        self.manual_button = ttk.Button(controls, text="SERVO seleccionado", command=self.move_manual)
        self.manual_button.grid(row=3, column=2, padx=(8, 0), pady=(10, 0))

        configuration = ttk.LabelFrame(root, text="Configuración del canal seleccionado", padding=8)
        configuration.grid(row=3, column=0, sticky="ew", pady=(8, 0))
        fields = (
            ("Centro eléctrico", self.electrical_var, "readonly"),
            ("Centro mecánico", self.mechanical_var, "readonly"),
            ("Neutral / inicial", self.center_var, "normal"),
            ("Posición inicial", self.initial_var, "readonly"),
            ("Mínimo mecánico", self.minimum_var, "normal"),
            ("Máximo mecánico", self.maximum_var, "normal"),
            ("Mínimo seguro", self.safe_min_var, "readonly"),
            ("Máximo seguro", self.safe_max_var, "readonly"),
            ("Dirección", self.direction_var, "readonly"),
        )
        self.config_input_widgets: list[ttk.Entry] = []
        for column, (label, variable, state) in enumerate(fields):
            ttk.Label(configuration, text=label).grid(row=0, column=column, padx=4, sticky="w")
            entry = ttk.Entry(configuration, textvariable=variable, width=16, state=state)
            entry.grid(row=1, column=column, padx=4, sticky="ew")
            if state == "normal":
                self.config_input_widgets.append(entry)
        self.apply_limits_button = ttk.Button(configuration, text="Aplicar LIMITS (PWM OFF)", command=self.apply_limits)
        self.apply_limits_button.grid(row=2, column=0, columnspan=3, sticky="w", padx=4, pady=(7, 0))
        self.read_button = ttk.Button(configuration, text="READ / STATUS", command=self.request_status)
        self.read_button.grid(row=2, column=3, columnspan=2, sticky="w", padx=4, pady=(7, 0))
        self.save_button = ttk.Button(configuration, text="SAVE (solo OFF)", command=lambda: self.send_command("SAVE"))
        self.save_button.grid(row=2, column=5, sticky="w", padx=4, pady=(7, 0))
        self.load_button = ttk.Button(configuration, text="LOAD (solo OFF)", command=lambda: self.send_command("LOAD"))
        self.load_button.grid(row=2, column=6, sticky="w", padx=4, pady=(7, 0))
        self.backup_button = ttk.Button(configuration, text="Leer y respaldar configuración activa", command=self.read_and_backup)
        self.backup_button.grid(row=2, column=0, columnspan=3, sticky="w", padx=4, pady=(34, 0))
        self.confirm_backup_button = ttk.Button(configuration, text="Confirmar respaldo", command=self.confirm_backup)
        self.confirm_backup_button.grid(row=2, column=3, columnspan=2, sticky="w", padx=4, pady=(34, 0))
        self.compare_button = ttk.Button(configuration, text="Comparar respaldo vs. activa", command=self.compare_backup)
        self.compare_button.grid(row=2, column=5, columnspan=2, sticky="w", padx=4, pady=(34, 0))
        ttk.Label(configuration, textvariable=self.backup_var).grid(row=3, column=0, columnspan=9, sticky="w", padx=4, pady=(7, 0))
        ttk.Label(configuration, textvariable=self.comparison_var, wraplength=980).grid(row=4, column=0, columnspan=9, sticky="w", padx=4, pady=(3, 0))
        ttk.Label(configuration, text="En este MVP, centerAngle es la posición neutral/inicial; el centro mecánico todavía no se persiste por separado. No existe restauración desde respaldo.", wraplength=980).grid(row=5, column=0, columnspan=9, sticky="w", padx=4, pady=(3, 0))

        terminal = ttk.LabelFrame(root, text="Terminal de diagnóstico (lista blanca)", padding=8)
        terminal.grid(row=4, column=0, sticky="nsew", pady=(8, 0))
        terminal.columnconfigure(0, weight=1)
        terminal.rowconfigure(0, weight=1)
        self.terminal = tk.Text(terminal, height=10, wrap="word", state="disabled", font=("Consolas", 9))
        self.terminal.grid(row=0, column=0, columnspan=3, sticky="nsew")
        ttk.Entry(terminal, textvariable=self.raw_command_var).grid(row=1, column=0, sticky="ew", pady=(7, 0))
        self.raw_button = ttk.Button(terminal, text="Enviar permitido", command=self.send_raw)
        self.raw_button.grid(row=1, column=1, padx=5, pady=(7, 0))
        ttk.Label(terminal, text="Bloquea STAND, LEG, WALK, UNLOCK_WALK, DEFAULTS y cualquier otro comando.").grid(row=1, column=2, sticky="w", pady=(7, 0))

        self.off_button = tk.Button(root, text="SERVOS OFF", bg="#b00020", fg="white", activebackground="#d00028", activeforeground="white", font=("Segoe UI", 15, "bold"), command=self.servos_off, height=2)
        self.off_button.grid(row=5, column=0, sticky="ew", pady=(10, 0))
        self._set_controls()

    def _servo_choices(self) -> list[str]:
        return [f"{index:02d} — {name}" for index, name in enumerate(SERVO_NAMES)]

    def refresh_ports(self) -> None:
        ports = [SIMULATED_PORT]
        if serial is not None:
            ports.extend(f"{item.device} — {item.description}" for item in serial.tools.list_ports.comports())
        self.port_box["values"] = ports
        if self.port_var.get() not in ports:
            self.port_var.set(ports[0])

    def connect(self) -> None:
        if self.connected:
            return
        selected = self.port_var.get()
        if not selected:
            return
        try:
            if selected == SIMULATED_PORT:
                self.port = SimulatedSerial(timeout=0.1)
            else:
                if serial is None:
                    raise RuntimeError("PySerial no está instalado")
                device = selected.split(" — ", 1)[0]
                self.port = serial.Serial(device, baudrate=BAUDRATE, timeout=0.1, write_timeout=1)
        except Exception as exc:
            self._log(f"[LOCAL ERROR] No se pudo conectar: {exc}")
            return
        self.connected = True
        self.motion_locked = True
        self.status_seen = self.imu_seen = self.config_seen = False
        self.configs.clear()
        self.active_rows = []
        self.live_collector.reset()
        self.backup_collector.reset()
        self.backup_capture_pending = False
        self.backup_snapshot = None
        self.backup_gate.reset()
        self.backup_var.set("Respaldo obligatorio antes de LIMITS/SAVE.")
        self.comparison_var.set("Comparación: sin respaldo confirmado.")
        self.stop_reader.clear()
        self.reader = threading.Thread(target=self._reader_loop, daemon=True)
        self.reader.start()
        self.connection_var.set(f"CONECTADO: {selected} @ {BAUDRATE}")
        self._set_controls()
        # OFF is intentionally the only automatic state-changing command; it is
        # an assertion of the documented safe state and never enables PWM.
        self.send_command("OFF", internal=True)
        self.request_status()

    def disconnect(self, reason: str = "Puerto desconectado", *, attempt_off: bool = True) -> None:
        if self.port is not None:
            self.stop_reader.set()
            if attempt_off and not attempt_safe_off(self.port):
                self._log("[LOCAL] OFF no pudo transmitirse; el estado físico no puede confirmarse.")
            try:
                self.port.close()
            except Exception:
                pass
        self.port = None
        self.connected = False
        self.motion_locked = True
        self.connection_var.set(reason.upper())
        self._set_controls()

    def _reader_loop(self) -> None:
        try:
            while not self.stop_reader.is_set() and self.port is not None:
                raw = self.port.readline()
                if raw:
                    self.incoming.put(("line", raw.decode("ascii", errors="replace").strip()))
        except Exception as exc:
            self.incoming.put(("disconnect", str(exc)))

    def _heartbeat_tick(self) -> None:
        if self.connected:
            self.send_command("PING", internal=True)
        self.after(250, self._heartbeat_tick)

    def _drain_incoming(self) -> None:
        while True:
            try:
                kind, payload = self.incoming.get_nowait()
            except queue.Empty:
                break
            if kind == "disconnect":
                self.disconnect(f"BLOQUEADO: lectura falló; estado físico no confirmable ({payload})", attempt_off=False)
            else:
                self._process_line(payload)
        self.after(40, self._drain_incoming)

    def _process_line(self, line: str) -> None:
        if not line:
            return
        self._log(f"< {line}")
        if line.startswith("[ABORT]"):
            self._emergency_lock("ABORT recibido del Nano")
            return
        try:
            if line.startswith("MINI Q-POD MVP"):
                self.firmware_identification = line
            if line.startswith("STATUS "):
                data = parse_key_values(line)
                required = {"mode", "servos", "selected", "active", "imu", "abort"}
                if not required <= data.keys():
                    raise ValueError("STATUS incompleto")
                self.current_mode = data["mode"]
                self.selected_channel = parse_int(data["selected"], low=-1, high=12)
                self.active_channel = parse_int(data["active"], low=-1, high=12)
                self.imu_healthy = bool(parse_int(data["imu"], low=0, high=1))
                self.status_seen = True
                self.mode_var.set(f"Modo: {self.current_mode}")
                self.servos_var.set("Servos: ON (un canal)" if self.active_channel >= 0 else "Servos: OFF")
                if not self.imu_healthy:
                    self._emergency_lock("IMU no saludable")
                    return
                if data["abort"] != "NONE":
                    self._emergency_lock(f"Aborto enclavado: {data['abort']}")
                    return
            elif line.startswith("IMU "):
                data = parse_key_values(line)
                required = {"enabled", "healthy", "roll", "pitch", "calibrationTiltAbort"}
                if not required <= data.keys():
                    raise ValueError("IMU incompleta")
                healthy = bool(parse_int(data["healthy"], low=0, high=1))
                roll = parse_float(data["roll"])
                pitch = parse_float(data["pitch"])
                self.imu_healthy = healthy
                self.imu_seen = True
                self.imu_var.set(f"IMU: {'OK' if healthy else 'PERDIDA'}  roll={roll:.2f}° pitch={pitch:.2f}°")
                if not healthy:
                    self._emergency_lock("IMU perdida")
                    return
            elif line.startswith("CONFIG margin="):
                self.live_collector.reset()
                self.live_collector.accept_margin(line)
                self.configs.clear()
                if self.backup_capture_pending:
                    self.backup_collector.accept_margin(line)
            elif line.startswith("CONFIG ch="):
                self.live_collector.accept_channel(line)
                data = parse_key_values(line)
                channel = parse_int(data["ch"], low=0, high=12)
                self.configs[channel] = data
                if self.live_collector.complete:
                    self.active_rows = self.live_collector.rows()
                    self.config_seen = True
                    self._display_selected_config()
                if self.backup_capture_pending:
                    self.backup_collector.accept_channel(line)
                    if self.backup_collector.complete:
                        self._finalize_backup()
            elif line.startswith("[SELECT]"):
                data = parse_key_values(line.replace("[SELECT]", "SELECT", 1))
                self.selected_channel = parse_int(data["ch"], low=0, high=12)
                self.active_channel = -1
                self._display_selected_config()
            elif line.startswith("[ENABLE]"):
                data = parse_key_values(line.replace("[ENABLE]", "ENABLE", 1))
                self.active_channel = parse_int(data["ch"], low=0, high=12)
                self.manual_angle_var.set(data["angle"])
            elif line.startswith(("[CENTER]", "[SERVO]")):
                data = parse_key_values(line.replace("[", "", 1).replace("]", "", 1))
                self.manual_angle_var.set(data["angle"])
            elif line.startswith("[LIMITS]"):
                data = parse_key_values(line.replace("[LIMITS]", "LIMITS", 1))
                channel = parse_int(data["ch"], low=0, high=12)
                if channel in self.configs:
                    self.configs[channel].update({"min": data["min"], "center": data["center"], "max": data["max"], "safeMin": data["safeMin"], "safeMax": data["safeMax"]})
                self._display_selected_config()
            elif line.startswith("PING "):
                data = parse_key_values(line)
                if {"mode", "active"} - data.keys():
                    raise ValueError("PING incompleto")
                parse_int(data["active"], low=-1, high=12)
        except (KeyError, ValueError) as exc:
            self._emergency_lock(f"Respuesta inválida: {exc}")
            return
        self._unlock_if_ready()
        self._set_controls()

    def _unlock_if_ready(self) -> None:
        if self.status_seen and self.imu_seen and self.config_seen and self.imu_healthy and self.current_mode in {"SAFE_OFF", "CALIBRATION"}:
            self.motion_locked = False

    def _emergency_lock(self, reason: str) -> None:
        self.motion_locked = True
        self.connection_var.set(f"BLOQUEADO: {reason}")
        self._set_controls()

    def _selected_index(self) -> int | None:
        value = self.selection_var.get()
        if not value:
            return None
        try:
            return int(value.split(" ", 1)[0])
        except ValueError:
            return None

    def select_channel(self) -> None:
        channel = self._selected_index()
        if channel is None:
            messagebox.showwarning("Seleccione un servo", "Seleccione un canal antes de enviar SELECT.")
            return
        self.send_command(f"SELECT {channel}")

    def enable_selected(self) -> None:
        channel = self._selected_index()
        if channel is None:
            messagebox.showwarning("Seleccione un servo", "Seleccione un canal antes de habilitar.")
            return
        try:
            angle = parse_float(self.first_angle_var.get())
        except ValueError as exc:
            messagebox.showerror("Ángulo inválido", str(exc))
            return
        if not messagebox.askyesno("Habilitar servo", f"Canal {channel} ({SERVO_NAMES[channel]}) recibirá su primer PWM en {angle:.2f}°.\n\nPuede moverse inmediatamente. ¿Continuar?"):
            return
        self.send_command(f"ENABLE {channel} {angle:g}")

    def center_selected(self) -> None:
        channel = self._selected_index()
        if channel is not None:
            self.send_command(f"CENTER {channel}")

    def move_delta(self, delta: int) -> None:
        try:
            target = parse_float(self.manual_angle_var.get()) + delta
        except ValueError:
            messagebox.showerror("Ángulo inválido", "Ingrese primero un ángulo manual válido.")
            return
        self.manual_angle_var.set(f"{target:g}")
        self.move_manual()

    def move_manual(self) -> None:
        channel = self._selected_index()
        if channel is None:
            return
        try:
            angle = parse_float(self.manual_angle_var.get())
        except ValueError as exc:
            messagebox.showerror("Ángulo inválido", str(exc))
            return
        self.send_command(f"SERVO {channel} {angle:g}")

    def apply_limits(self) -> None:
        if not self.backup_gate.allows_modifications:
            self._log("[BLOQUEADO] LIMITS requiere respaldo guardado y confirmado.")
            return
        channel = self._selected_index()
        if channel is None:
            return
        try:
            minimum = parse_int(self.minimum_var.get(), low=0, high=180)
            center = parse_int(self.center_var.get(), low=0, high=180)
            maximum = parse_int(self.maximum_var.get(), low=0, high=180)
        except ValueError as exc:
            messagebox.showerror("Límites inválidos", str(exc))
            return
        self.send_command("OFF")
        self.send_command("CALIB")
        self.send_command(f"LIMITS {channel} {minimum} {center} {maximum}")

    def request_status(self) -> None:
        self.send_command("STATUS", internal=True)
        self.send_command("IMU", internal=True)
        self.send_command("CONFIG", internal=True)

    def read_and_backup(self) -> None:
        if not self.connected:
            self._log("[LOCAL] No se puede respaldar sin conexión.")
            return
        self.backup_gate.reset()
        self.backup_snapshot = None
        self.backup_collector.reset()
        self.backup_capture_pending = True
        self.backup_var.set("Leyendo CONFIG: se requieren los 13 canales válidos.")
        self.comparison_var.set("Comparación: esperando respaldo.")
        self._set_controls()
        self.send_command("CONFIG", internal=True)

    def _finalize_backup(self) -> None:
        try:
            rows = self.backup_collector.rows()
            snapshot = build_backup(
                active_rows=rows,
                global_margin=self.backup_collector.global_margin or 0,
                port=self.port_var.get(),
                firmware=self.firmware_identification,
            )
            destination = write_backup(snapshot, Path(__file__).resolve().parent / "backups")
        except (OSError, ValueError) as exc:
            self._backup_failed(f"No se pudo guardar respaldo: {exc}")
            return
        self.backup_capture_pending = False
        self.backup_snapshot = snapshot
        self.backup_gate.mark_saved(destination)
        self.backup_var.set(f"Respaldo guardado: {destination.name}. Confírmelo para habilitar LIMITS/SAVE.")
        self.comparison_var.set("Comparación: respaldo y configuración activa coinciden.")
        self._log(f"[BACKUP] {destination}")
        self._set_controls()

    def _backup_failed(self, reason: str) -> None:
        self.backup_capture_pending = False
        self.backup_snapshot = None
        self.backup_gate.reset()
        self.backup_var.set(f"BLOQUEADO: {reason}")
        self._log(f"[BACKUP ERROR] {reason}")
        self._set_controls()

    def confirm_backup(self) -> None:
        try:
            self.backup_gate.confirm()
        except ValueError as exc:
            self._log(f"[BLOQUEADO] {exc}")
            return
        self.backup_var.set(f"Respaldo confirmado: {self.backup_gate.saved_path.name}")
        self._set_controls()

    def compare_backup(self) -> None:
        if self.backup_snapshot is None:
            self.comparison_var.set("Comparación: primero lea y respalde una configuración completa.")
            return
        try:
            if not self.live_collector.complete:
                raise ValueError("configuración activa incompleta")
            changes = compare_channels(self.backup_snapshot["channels"], self.live_collector.rows())  # type: ignore[arg-type]
        except (KeyError, ValueError) as exc:
            self.comparison_var.set(f"Comparación no disponible: {exc}")
            return
        if not changes:
            self.comparison_var.set("Comparación: sin cambios.")
            return
        summary = "; ".join(f"ch {channel} ({SERVO_NAMES[channel]}): {', '.join(fields)}" for channel, fields in changes.items())
        self.comparison_var.set(f"Cambios: {summary}")

    def servos_off(self) -> None:
        if self.connected:
            self.send_command("OFF", internal=True)
        else:
            self._log("[LOCAL] No hay enlace para enviar OFF; corte la fuente externa si estuviera energizada.")

    def send_raw(self) -> None:
        line = self.raw_command_var.get().strip()
        if not command_is_allowed(line):
            self._log(f"[BLOQUEADO] {command_name(line) or '(vacío)'} no está permitido por este calibrador.")
            return
        self.raw_command_var.set("")
        self.send_command(line)

    def send_command(self, line: str, *, internal: bool = False) -> None:
        if not self.connected or self.port is None:
            self._log("[LOCAL] Comando no enviado: sin conexión.")
            return
        if not command_is_allowed(line):
            self._log(f"[BLOQUEADO] {command_name(line)} no está permitido.")
            return
        movement = command_name(line) in {"CALIB", "SELECT", "ENABLE", "CENTER", "SERVO", "LIMITS"}
        if command_name(line) in {"LIMITS", "SAVE"} and not self.backup_gate.allows_modifications and not internal:
            self._log(f"[BLOQUEADO] {command_name(line)} requiere respaldo guardado y confirmado.")
            return
        if movement and self.motion_locked and not internal:
            self._log("[LOCAL] Movimiento bloqueado por estado de seguridad.")
            return
        try:
            self.port.write((line.strip() + "\n").encode("ascii"))
            self._log(f"> {line.strip()}")
        except Exception as exc:
            self.disconnect(f"BLOQUEADO: escritura falló; estado físico no confirmable ({exc})", attempt_off=False)

    def _display_selected_config(self) -> None:
        channel = self._selected_index()
        if channel is None or channel not in self.configs:
            return
        data = self.configs[channel]
        self.center_var.set(data["center"])
        self.initial_var.set(f"{data['center']}° (centerAngle)")
        self.minimum_var.set(data["min"])
        self.maximum_var.set(data["max"])
        self.safe_min_var.set(f"{data['safeMin']}°")
        self.safe_max_var.set(f"{data['safeMax']}°")
        self.direction_var.set(data["direction"])
        self.first_angle_var.set(data["center"])

    def _set_controls(self) -> None:
        safe_state = "normal" if self.connected and not self.motion_locked else "disabled"
        modification_state = safe_state if self.backup_gate.allows_modifications else "disabled"
        for widget in (self.calib_button, self.select_button, self.enable_button, self.center_button, self.manual_button, self.read_button, self.load_button, self.raw_button, self.first_entry, self.manual_entry, self.servo_box):
            widget.configure(state=safe_state)
        for widget in (self.apply_limits_button, self.save_button):
            widget.configure(state=modification_state)
        self.backup_button.configure(state="normal" if self.connected else "disabled")
        self.confirm_backup_button.configure(state="normal" if self.connected and self.backup_gate.saved_path is not None and not self.backup_gate.confirmed else "disabled")
        self.compare_button.configure(state="normal" if self.connected and self.backup_snapshot is not None else "disabled")
        for index in range(4):
            getattr(self, f"delta_{index}").configure(state=safe_state)
        for widget in self.config_input_widgets:
            widget.configure(state=modification_state)
        self.off_button.configure(state="normal" if self.connected else "disabled")
        self.connect_button.configure(state="disabled" if self.connected else "normal")
        self.disconnect_button.configure(state="normal" if self.connected else "disabled")

    def _log(self, text: str) -> None:
        self.terminal.configure(state="normal")
        self.terminal.insert("end", text + "\n")
        self.terminal.see("end")
        self.terminal.configure(state="disabled")

    def _on_close(self) -> None:
        self.disconnect("Cerrado", attempt_off=True)
        self.destroy()


if __name__ == "__main__":
    ServoCalibratorApp().mainloop()
