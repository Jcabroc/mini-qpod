"""Tk desktop application; no hardware transport."""
import math
from pathlib import Path
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

from tools.ik_pose_lab.pose import Pose, PRESETS, dumps, loads
from tools.ik_simulator.kinematics import COXA, FEMUR, MOUNTS, forward, leg_to_body
from tools.ik_simulator.validation import validate_target


class PoseLab:
    def __init__(self, root):
        self.root = root
        root.title("Mini Q-POD Pose Lab")
        root.geometry("1280x720")
        root.minsize(1000, 620)
        self.vars = {k: tk.StringVar(value=str(v)) for k, v in vars(PRESETS['READY']).items()}
        bar = ttk.Frame(root, padding=12)
        bar.pack(fill="x")
        ttk.Label(bar, text="Mini Q-POD Pose Lab", font=("Segoe UI", 18, "bold")).pack(side="left")
        for name in PRESETS:
            ttk.Button(bar, text=name, command=lambda n=name: self.set_pose(PRESETS[n])).pack(side="left", padx=6)
        for label, action in (("Guardar", self.save), ("Cargar", self.load), ("Exportar JSON", lambda: self.save(True))):
            ttk.Button(bar, text=label, command=action).pack(side="right", padx=4)
        controls = ttk.Frame(root, padding=12)
        controls.pack(fill="x")
        for i, (key, label) in enumerate((("height", "Altura (mm)"), ("spread", "Apertura (mm)"),
                                        ("advance", "Avance +Y (mm)"), ("yaw", "Abanico COXA (°)"),
                                        ("knee_sign", "Rama (-1 / +1)"))):
            ttk.Label(controls, text=label).grid(row=0, column=i, padx=10, sticky="w")
            ttk.Entry(controls, textvariable=self.vars[key], width=17).grid(row=1, column=i, padx=10)
        self.selected = tk.StringVar(value="R1")
        ttk.Label(controls, text="Pata lateral").grid(row=0, column=5)
        ttk.Combobox(controls, textvariable=self.selected, values=list(MOUNTS), state="readonly", width=8).grid(row=1, column=5)
        self.note = tk.StringVar()
        ttk.Label(root, textvariable=self.note, padding=8).pack(fill="x")
        views = ttk.Frame(root)
        views.pack(fill="both", expand=True, padx=10)
        self.canvases = []
        for title in ("SUPERIOR · X / Y", "FRONTAL · X / Z", "LATERAL · plano radial / Z"):
            panel = ttk.LabelFrame(views, text=title, padding=4)
            panel.pack(side="left", fill="both", expand=True)
            canvas = tk.Canvas(panel, background="#17212e", highlightthickness=0, width=350)
            canvas.pack(fill="both", expand=True)
            canvas.bind("<Configure>", lambda e: self.refresh())
            self.canvases.append(canvas)
        self.status = tk.StringVar()
        ttk.Label(root, textvariable=self.status, padding=12, font=("Segoe UI", 10)).pack(fill="x")
        ttk.Label(root, text="Ámbar: sobre mecánico incierto. Proyecciones sin sólidos ni detección de contacto. Solo simulación.", padding=8).pack(fill="x")
        for var in (*self.vars.values(), self.selected):
            var.trace_add("write", lambda *_: self.refresh())
        self.refresh()

    def pose(self):
        return Pose(**{k: float(v.get()) for k, v in self.vars.items()})

    def set_pose(self, pose):
        for k, v in vars(pose).items():
            self.vars[k].set(str(v))

    def save(self, export=False):
        try:
            text = dumps(self.pose(), export)
            path = filedialog.asksaveasfilename(defaultextension=".json", filetypes=[("Pose JSON", "*.json")])
            if path:
                Path(path).write_text(text, encoding="utf-8")
        except (ValueError, OSError) as exc:
            messagebox.showerror("Pose", str(exc))

    def load(self):
        path = filedialog.askopenfilename(filetypes=[("Pose JSON", "*.json")])
        if path:
            try:
                self.set_pose(loads(Path(path).read_text(encoding="utf-8")))
            except (ValueError, OSError) as exc:
                messagebox.showerror("Pose", str(exc))

    def refresh(self):
        try:
            pose = self.pose()
        except ValueError as exc:
            self.note.set(f"Entrada inválida: {exc}. Vistas borradas hasta corregir.")
            for c in self.canvases:
                c.delete("all")
            self.status.set("")
            return
        targets = pose.targets()
        reports = {leg: validate_target(leg, p, pose.knee_sign) for leg, p in targets.items()}
        self.note.set("Origen corporal · +X derecha · +Y frontal · +Z arriba | Cuerpo nivelado (roll/pitch = 0)")
        lines = []
        chains = {}
        for leg, report in reports.items():
            state = "OK IK · zona mecánica pendiente"
            if report.checks[0].status == "fail":
                state = "inalcanzable"
            elif report.checks[0].status == "indeterminate" or any("SINGULARITY" in w for w in report.warnings):
                state = "singularidad · zona mecánica pendiente"
            lines.append(f"{leg}: {state}")
            if report.angles:
                a = report.angles
                # Joint positions for rendering only; endpoint uses shared FK.
                def joint(radius, z):
                    return leg_to_body(leg, (radius*math.cos(a.yaw), radius*math.sin(a.yaw), z))
                m = MOUNTS[leg]
                chains[leg] = [(m.x, m.y, 0), joint(COXA, 0),
                               joint(COXA+FEMUR*math.cos(a.femur), FEMUR*math.sin(a.femur)),
                               forward(leg, a)]
        self.status.set("\n".join(lines))
        for view, c in enumerate(self.canvases):
            c.delete("all")
            w, h = max(c.winfo_width(), 300), max(c.winfo_height(), 250)
            extent = max(220, *(abs(v) + 30 for p in targets.values() for v in p))
            scale = min(w, h) / (2*extent)
            def xy(x, y):
                return w/2+x*scale, h/2-y*scale
            def line(points, **opts):
                c.create_line(*[v for p in points for v in xy(*p)], **opts)
            line([(-extent, 0), (extent, 0)], fill="#546273", dash=(3, 4))
            line([(0, -extent), (0, extent)], fill="#546273", dash=(3, 4))
            if view == 0:
                c.create_rectangle(*xy(-46.5, 46.5), *xy(46.5, -46.5), outline="#e2e8f0", width=2)
                # Qualitative uncertainty bands, not collision geometry.
                for m in MOUNTS.values():
                    x, y = xy(m.x, m.y)
                    r = (COXA+FEMUR)*scale
                    c.create_oval(x-r,y-r,x+r,y+r,outline="#b88935",dash=(3,5))
                    c.create_text(x,y-10,text=next(k for k,v in MOUNTS.items() if v==m),fill="white")
                c.create_text(8,16,anchor="w",text="Ámbar: incertidumbre mecánica",fill="#edb85f")
            elif view == 1:
                line([(-46.5,0),(46.5,0)],fill="white",width=5)
                line([(-extent,-pose.height),(extent,-pose.height)],fill="#7fa890")
                c.create_text(8,16,anchor="w",text=f"Altura {pose.height:g} mm · pies Z = {-pose.height:g}",fill="white")
            else:
                c.create_text(8,16,anchor="w",text=f"{self.selected.get()} · COXA / FÉMUR / TIBIA",fill="white")
            for leg, target in targets.items():
                if view == 2 and leg != self.selected.get():
                    continue
                m = MOUNTS[leg]
                a = reports[leg].angles
                theta = m.yaw + (a.yaw if a else 0)
                def project(p):
                    if view == 0: return p[0],p[1]
                    if view == 1: return p[0],p[2]
                    return (p[0]-m.x)*math.cos(theta)+(p[1]-m.y)*math.sin(theta),p[2]
                pts = chains.get(leg, [])
                for i in range(len(pts)-1):
                    line([project(pts[i]),project(pts[i+1])],fill=("#63c6ed","#73d9a5","#d6a5ed")[i],width=3)
                x,y = xy(*project(target))
                c.create_oval(x-4,y-4,x+4,y+4,fill="#edb85f" if a else "#ff6d78",outline="")


def main():
    root = tk.Tk()
    PoseLab(root)
    root.mainloop()


if __name__ == "__main__":
    main()
