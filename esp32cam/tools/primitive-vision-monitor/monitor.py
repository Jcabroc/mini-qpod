"""Q-POD Primitive Vision Monitor: small Tkinter serial diagnostic tool."""
from __future__ import annotations
import queue
import threading
import time
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from typing import Optional

import serial
from serial.tools import list_ports

from parser import MotionSample, parse_line

BAUDRATES = (115200, 921600, 57600, 38400, 19200, 9600)
STATES = ("QUIET", "MOTION", "APPROACHING", "RECEDING", "UNKNOWN")
LIGHT_STATES = ("DARK", "NORMAL", "BRIGHT", "LIGHT_CHANGE")
GRID = ("TOP_LEFT", "TOP_CENTER", "TOP_RIGHT", "MID_LEFT", "CENTER", "MID_RIGHT",
        "BOTTOM_LEFT", "BOTTOM_CENTER", "BOTTOM_RIGHT")

class Monitor(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Q-POD Primitive Vision Monitor")
        self.geometry("980x700")
        self.minsize(760, 560)
        self.serial: Optional[serial.Serial] = None
        self.reader: Optional[threading.Thread] = None
        self.stop_reader = threading.Event()
        self.incoming = queue.Queue()
        self.last_sample: Optional[MotionSample] = None
        self.last_time = None
        self.frame_count = 0
        self.fps_value = 0.0
        self.last_rx = None
        self.cells = {}
        self._build_ui()
        self.refresh_ports()
        self.after(80, self.process_queue)
        self.protocol("WM_DELETE_WINDOW", self.close)

    def _build_ui(self):
        top = ttk.Frame(self, padding=8); top.pack(fill="x")
        ttk.Label(top, text="COM port").pack(side="left")
        self.port_var = tk.StringVar(); self.port_box = ttk.Combobox(top, textvariable=self.port_var, width=12, state="readonly")
        self.port_box.pack(side="left", padx=(5, 12))
        ttk.Button(top, text="Refresh", command=self.refresh_ports).pack(side="left")
        ttk.Label(top, text="Baudrate").pack(side="left", padx=(18, 5))
        self.baud_var = tk.StringVar(value="115200")
        ttk.Combobox(top, textvariable=self.baud_var, values=BAUDRATES, width=10, state="readonly").pack(side="left")
        self.connect_button = ttk.Button(top, text="CONNECT", command=self.toggle_connection); self.connect_button.pack(side="left", padx=18)
        self.status_var = tk.StringVar(value="DISCONNECTED"); ttk.Label(top, textvariable=self.status_var).pack(side="left")

        body = ttk.PanedWindow(self, orient="horizontal"); body.pack(fill="both", expand=True, padx=8, pady=(0, 8))
        left = ttk.Frame(body, padding=5); right = ttk.Frame(body, padding=5); body.add(left, weight=3); body.add(right, weight=2)
        ttk.Label(left, text="Motion centroid", font=("Segoe UI", 13, "bold")).pack(anchor="w")
        grid = tk.Frame(left, bg="#252525"); grid.pack(fill="both", expand=True, pady=6)
        for i, name in enumerate(GRID):
            cell = tk.Frame(grid, bg="#303030", highlightthickness=2, highlightbackground="#505050")
            cell.grid(row=i//3, column=i%3, sticky="nsew", padx=2, pady=2)
            grid.rowconfigure(i//3, weight=1); grid.columnconfigure(i%3, weight=1)
            tk.Label(cell, text=name, bg="#303030", fg="white", font=("Segoe UI", 10, "bold")).pack(pady=(10, 2))
            marker = tk.Label(cell, text="", bg="#303030", fg="#ffd34e", font=("Segoe UI", 28, "bold")); marker.pack(expand=True)
            self.cells[name] = (cell, marker)
        ttk.Label(left, text="The highlighted cell shows the current centroid region.", foreground="#666").pack(anchor="w")

        status_frame = ttk.LabelFrame(right, text="State", padding=6); status_frame.pack(fill="x")
        self.state_labels = {}
        for state in STATES:
            label = tk.Label(status_frame, text=state, width=14, anchor="w", bg="#e8e8e8", padx=5); label.pack(fill="x", pady=1); self.state_labels[state] = label
        ttk.Label(status_frame, text="Light", font=("Segoe UI", 9, "bold")).pack(anchor="w", pady=(7, 1))
        self.light_labels = {}
        for state in LIGHT_STATES:
            label = tk.Label(status_frame, text=state, width=14, anchor="w", bg="#e8e8e8", padx=5); label.pack(fill="x", pady=1); self.light_labels[state] = label
        data = ttk.LabelFrame(right, text="Data", padding=6); data.pack(fill="x", pady=8)
        self.values = {}
        for name in ("X", "Y", "AREA", "MOTION/SCORE", "CONFIDENCE", "FPS", "LIGHT"):
            row = ttk.Frame(data); row.pack(fill="x"); ttk.Label(row, text=name, width=16).pack(side="left")
            value = ttk.Label(row, text="—"); value.pack(side="left"); self.values[name] = value

        log_frame = ttk.LabelFrame(right, text="Serial monitor", padding=5); log_frame.pack(fill="both", expand=True)
        self.log = tk.Text(log_frame, height=12, wrap="none", state="disabled", background="#101010", foreground="#d8d8d8")
        self.log.pack(fill="both", expand=True)
        buttons = ttk.Frame(log_frame); buttons.pack(fill="x", pady=(5, 0))
        ttk.Button(buttons, text="CLEAR", command=self.clear_log).pack(side="left")
        ttk.Button(buttons, text="COPY ALL", command=self.copy_log).pack(side="left", padx=5)
        ttk.Button(buttons, text="SAVE LOG", command=self.save_log).pack(side="left")

    def refresh_ports(self):
        ports = [p.device for p in list_ports.comports()]
        self.port_box["values"] = ports
        if ports and self.port_var.get() not in ports: self.port_var.set(ports[0])

    def toggle_connection(self):
        if self.serial and self.serial.is_open: self.disconnect()
        else: self.connect()

    def connect(self):
        try: self.serial = serial.Serial(self.port_var.get(), int(self.baud_var.get()), timeout=0.2)
        except Exception as exc: messagebox.showerror("Serial connection", str(exc)); return
        # ESP32-CAM-MB reset pulse: lets the monitor see the boot banner and
        # guarantees a fresh stream when the board was already running.
        try:
            self.serial.dtr = False
            self.serial.rts = True
            time.sleep(0.1)
            self.serial.rts = False
        except Exception:
            pass
        self.stop_reader.clear(); self.reader = threading.Thread(target=self.read_serial, daemon=True); self.reader.start()
        self.last_rx = None
        self.connect_button.configure(text="DISCONNECT"); self.status_var.set(f"CONNECTED {self.port_var.get()} — WAITING DATA")

    def disconnect(self):
        self.stop_reader.set()
        if self.serial:
            try: self.serial.close()
            except Exception: pass
        self.serial = None; self.connect_button.configure(text="CONNECT"); self.status_var.set("DISCONNECTED")

    def read_serial(self):
        while not self.stop_reader.is_set() and self.serial:
            try: line = self.serial.readline().decode("utf-8", errors="replace")
            except Exception as exc: self.incoming.put(("__ERROR__", str(exc))); break
            if line: self.incoming.put(("LINE", line.rstrip("\r\n")))

    def process_queue(self):
        try:
            while True:
                kind, value = self.incoming.get_nowait()
                if kind == "LINE": self.handle_line(value)
                else: self.append_log(value)
        except queue.Empty: pass
        self.after(80, self.process_queue)

    def handle_line(self, line):
        self.append_log(line)
        self.last_rx = time.monotonic()
        self.status_var.set(f"CONNECTED {self.port_var.get()} — RX OK")
        sample = parse_line(line)
        if sample: self.update_sample(sample)

    def update_sample(self, sample: MotionSample):
        now = time.monotonic()
        if self.last_time is not None:
            dt = now - self.last_time
            if dt > 0: self.fps_value = 0.8 * self.fps_value + 0.2 / dt
        self.last_time = now; self.last_sample = sample
        self.values["X"].configure(text=str(sample.x)); self.values["Y"].configure(text=str(sample.y))
        self.values["AREA"].configure(text=str(sample.area)); self.values["MOTION/SCORE"].configure(text=str(sample.score))
        self.values["CONFIDENCE"].configure(text=str(sample.confidence) if sample.confidence is not None else "—")
        self.values["FPS"].configure(text=f"{self.fps_value:.1f}"); self.values["LIGHT"].configure(text=str(sample.light) if sample.light is not None else "—")
        for state, label in self.state_labels.items(): label.configure(bg="#ffe08a" if state == sample.state else "#e8e8e8")
        for state, label in self.light_labels.items(): label.configure(bg="#ffe08a" if state == sample.light_state else "#e8e8e8")
        for cell, marker in self.cells.values(): cell.configure(bg="#303030"); marker.configure(bg="#303030", text="")
        region = self.region(sample.x, sample.y)
        cell, marker = self.cells[region]; cell.configure(bg="#216e39"); marker.configure(bg="#216e39", text="X")

    @staticmethod
    def region(x, y):
        col = 0 if x < 53 else 1 if x < 107 else 2
        row = 0 if y < 40 else 1 if y < 80 else 2
        return GRID[row * 3 + col]

    def append_log(self, line):
        self.log.configure(state="normal"); self.log.insert("end", line + "\n"); self.log.see("end"); self.log.configure(state="disabled")
    def clear_log(self): self.log.configure(state="normal"); self.log.delete("1.0", "end"); self.log.configure(state="disabled")
    def copy_log(self): self.clipboard_clear(); self.clipboard_append(self.log.get("1.0", "end-1c"))
    def save_log(self):
        path = filedialog.asksaveasfilename(defaultextension=".log", filetypes=(("Log", "*.log"), ("Text", "*.txt")))
        if path:
            with open(path, "w", encoding="utf-8") as output: output.write(self.log.get("1.0", "end-1c"))
    def close(self): self.disconnect(); self.destroy()

if __name__ == "__main__": Monitor().mainloop()
