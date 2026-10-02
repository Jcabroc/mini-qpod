"""Tkinter viewer for the firmware gait; drawing only, no physical dynamics."""
import math
import json
from pathlib import Path
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

from .walk import LEGS, READY_FEET, GAIT_PRESETS, PATTERN_CHOICES, COM_ESTIMATE, Walker, solve, estimate_static_stability, stride_metrics
from tools.ik_simulator.kinematics import MOUNTS, COXA, FEMUR, leg_to_body

class App:
    def __init__(self, root):
        self.root=root; root.title("Mini Q-POD · simulador de marcha IK")
        root.geometry("1500x900"); root.minsize(1180,760)
        self.walker=Walker(); self.scale=2.6; self.az=0.72; self.el=0.52; self.drag=None
        self.floor_z=sum(p[2] for p in READY_FEET)/len(READY_FEET)
        self.speed=tk.DoubleVar(value=1.0); self.thickness=tk.DoubleVar(value=14.0)
        self.comx=tk.DoubleVar(value=COM_ESTIMATE["x_mm"]); self.comy=tk.DoubleVar(value=COM_ESTIMATE["y_mm"]); self.comz=tk.DoubleVar(value=0)
        self.balance_margin=tk.DoubleVar(value=COM_ESTIMATE["balance_margin_mm"])
        self.body_height=tk.DoubleVar(value=GAIT_PRESETS["Caminata lenta"].get("body_height_mm",0.))
        self.leg_opening=tk.DoubleVar(value=GAIT_PRESETS["Caminata lenta"].get("leg_opening_mm",0.))
        self.pressed_keys=set(); self.keyboard_drive_active=False
        self.paused=False; self.step_once=False; self.mode=tk.StringVar(value="Marcha coordinada · Nano")
        self.notice=""
        slow=GAIT_PRESETS["Caminata lenta"]
        self.pattern=tk.StringVar(value="Caminata lenta")
        self.stride=tk.DoubleVar(value=slow["stride"]); self.lift=tk.DoubleVar(value=slow["lift"]); self.turn_degrees=tk.DoubleVar(value=slow["turn_degrees"]); self.period=tk.DoubleVar(value=slow["period"]); self.duty=tk.DoubleVar(value=slow["duty"]); self.start_phase=tk.DoubleVar(value=slow["start_phase"])
        self.stride_info=tk.StringVar()
        self.phase_vars=[tk.DoubleVar(value=v) for v in slow["phase_offsets"]]
        bar=ttk.Frame(root,padding=6); bar.pack(fill="x")
        for label,cmd in (("READY",self.ready),("Paso pata",self.single),("Ciclo",self.cycle),
                          ("Pausa",self.pause),("Detener",self.stop),("Reiniciar",self.reset)):
            ttk.Button(bar,text=label,command=cmd).pack(side="left",padx=3)
        ttk.Label(bar,text="Marcha").pack(side="left",padx=(16,3))
        ttk.Combobox(bar,textvariable=self.mode,state="readonly",width=26,
                     values=("Marcha coordinada · Nano",)).pack(side="left")
        ttk.Label(bar,text="Velocidad").pack(side="left",padx=(14,3))
        ttk.Scale(bar,from_=0.25,to=3,variable=self.speed,length=120).pack(side="left")
        ttk.Button(bar,text="Cuadro ▶",command=self.frame).pack(side="left",padx=7)
        ttk.Button(bar,text="Vista +Z arriba",command=self.view_top).pack(side="left",padx=5)
        ttk.Button(bar,text="X 180°",command=lambda:self.rotate_camera(0,math.pi)).pack(side="left",padx=2)
        ttk.Button(bar,text="Y 180°",command=lambda:self.rotate_camera(math.pi,0)).pack(side="left",padx=2)
        ttk.Label(bar,text="Espesor visual (mm)").pack(side="left",padx=(12,3))
        ttk.Scale(bar,from_=5,to=30,variable=self.thickness,length=100).pack(side="left")
        ttk.Label(bar,text="COM Δ X/Y/Z mm").pack(side="left",padx=(8,2))
        for v in (self.comx,self.comy,self.comz): ttk.Spinbox(bar,from_=-40,to=40,increment=1,textvariable=v,width=4).pack(side="left",padx=1)
        cfg=ttk.Frame(root,padding=(8,0,8,5)); cfg.pack(fill="x")
        ttk.Label(cfg,text="Patrón").pack(side="left")
        pat=ttk.Combobox(cfg,textvariable=self.pattern,state="readonly",width=27,values=PATTERN_CHOICES); pat.pack(side="left",padx=4)
        pat.bind("<<ComboboxSelected>>",self.set_pattern)
        for label,var,lo,hi,inc,width in (("Avance/ciclo mm",self.stride,10,80,2,5),("Elevación mm",self.lift,0,50,1,5),("Giro °/ciclo",self.turn_degrees,0,20,1,4),("Periodo s",self.period,.8,6,.1,5),("Apoyo 0-1",self.duty,.5,.85,.01,5)):
            ttk.Label(cfg,text=label).pack(side="left",padx=(8,2)); ttk.Spinbox(cfg,from_=lo,to=hi,increment=inc,textvariable=var,width=width).pack(side="left")
        ttk.Label(cfg,text="Desfases/ciclo").pack(side="left",padx=(12,3))
        for name,var in zip(LEGS,self.phase_vars):
            ttk.Label(cfg,text=name).pack(side="left",padx=(3,0)); ttk.Spinbox(cfg,from_=0,to=.99,increment=.05,textvariable=var,width=4).pack(side="left",padx=1)
        ttk.Label(cfg,text="Inicio").pack(side="left",padx=(5,1)); ttk.Spinbox(cfg,from_=0,to=.99,increment=.01,textvariable=self.start_phase,width=4).pack(side="left")
        ttk.Label(cfg,text="Margen COM mm").pack(side="left",padx=(6,1)); ttk.Spinbox(cfg,from_=0,to=10,increment=.5,textvariable=self.balance_margin,width=4).pack(side="left")
        ttk.Label(cfg,text="Altura cuerpo mm").pack(side="left",padx=(6,1)); ttk.Spinbox(cfg,from_=-12,to=12,increment=1,textvariable=self.body_height,width=4).pack(side="left")
        ttk.Label(cfg,text="Apertura patas mm").pack(side="left",padx=(6,1)); ttk.Spinbox(cfg,from_=0,to=12,increment=1,textvariable=self.leg_opening,width=4).pack(side="left")
        ttk.Label(root,textvariable=self.stride_info,anchor="w").pack(fill="x",padx=12,pady=(0,4))
        files=ttk.Frame(root,padding=(8,0,8,5)); files.pack(fill="x")
        ttk.Button(files,text="Guardar preset…",command=self.save_preset).pack(side="left",padx=3)
        ttk.Button(files,text="Cargar preset…",command=self.load_preset).pack(side="left",padx=3)
        ttk.Button(files,text="Exportar parámetros Nano…",command=self.export_preset).pack(side="left",padx=3)
        ttk.Label(files,text="Balance COM + altura/apertura suave: simulador y firmware · traslado manual: solo simulación").pack(side="left",padx=12)
        bodybar=ttk.Frame(root,padding=(8,0,8,5)); bodybar.pack(fill="x")
        ttk.Label(bodybar,text="Cuerpo con pies plantados · simulación").pack(side="left",padx=3)
        for label,axis,d in (("Subir","z",6),("Bajar","z",-6),("Izq.","lateral",-6),("Der.","lateral",6),("Centrar","center",0)):
            ttk.Button(bodybar,text=label,command=lambda a=axis,v=d:self.body_motion(a,v)).pack(side="left",padx=3)
        body=ttk.Panedwindow(root,orient="horizontal"); body.pack(fill="both",expand=True)
        left=ttk.Frame(body); right=ttk.Frame(body,width=350); body.add(left,weight=4); body.add(right,weight=2)
        self.canvas=tk.Canvas(left,bg="#f5f7fa",highlightthickness=0); self.canvas.pack(fill="both",expand=True)
        self.canvas.bind("<Configure>",lambda e:self.draw()); self.canvas.bind("<ButtonPress-1>",self.mouse_down)
        self.canvas.bind("<B1-Motion>",self.mouse_move); self.canvas.bind("<MouseWheel>",self.zoom)
        self.canvas.bind("<Button-4>",lambda e:self.zoom_delta(1.12)); self.canvas.bind("<Button-5>",lambda e:self.zoom_delta(.89))
        ttk.Label(right,text="Estado y articulaciones",font=("Segoe UI",12,"bold")).pack(anchor="w",padx=10,pady=(8,4))
        self.status=tk.StringVar(); ttk.Label(right,textvariable=self.status,wraplength=330).pack(anchor="w",padx=10)
        self.table=ttk.Treeview(right,columns=("state","mech","elec"),show="headings",height=9)
        for c,t,w in (("state","Pata / fase",110),("mech","Mecánicos °",115),("elec","Eléctricos °",115)):
            self.table.heading(c,text=t); self.table.column(c,width=w,anchor="center")
        self.table.pack(fill="x",padx=7,pady=8)
        ttk.Label(right,text="Trayectoria de los pies (Y-Z, mm)",font=("Segoe UI",10,"bold")).pack(anchor="w",padx=10)
        self.trace=tk.Canvas(right,height=185,bg="white",highlightthickness=1,highlightbackground="#ccd3dc")
        self.trace.pack(fill="x",padx=9,pady=5)
        self.support=tk.StringVar(); ttk.Label(right,textvariable=self.support,wraplength=330).pack(anchor="w",padx=10,pady=5)
        self.stability=tk.StringVar(); ttk.Label(right,textvariable=self.stability,wraplength=330,font=("Segoe UI",9,"bold")).pack(anchor="w",padx=10,pady=3)
        ttk.Label(right,text="Ratón: arrastrar rota 360° · rueda acerca. X detener, espacio pausa; W/S avance-retroceso y A/D giro. Selecciona una fila y pulsa Paso pata para probarla. La cámara no mueve el robot.",wraplength=330).pack(anchor="w",padx=10,pady=6)
        root.bind("<KeyPress>",self.key); root.bind("<KeyRelease>",self.key_release); root.after(40,self.tick); self.draw()
    def ready(self): self.walker.ready(); self.paused=False
    def single(self):
        if self.walker.motion=="IDLE":
            selected=self.table.selection(); leg=int(selected[0]) if selected else 0
            if self.mode.get().startswith("Referencia"):
                self.walker.direction="forward"; self.walker.start_leg(leg)
            else:
                try:
                    self.walker.start_single_step(leg,stride=self.stride.get(),lift=self.lift.get(),turn_degrees=self.turn_degrees.get(),period=self.period.get(),com_offset=(self.comx.get(),self.comy.get()),balance_margin=self.balance_margin.get(),body_height_mm=self.body_height.get(),leg_opening_mm=self.leg_opening.get())
                except (ValueError,tk.TclError) as exc:
                    self.notice=str(exc) or "Revisa los parámetros de marcha"; return
            self.notice=""; self.paused=False
    def cycle(self):
        if self.walker.motion!="IDLE": return
        self.keyboard_drive_active=False;self.pressed_keys.clear()
        self.start_gait("forward",False)
    def start_gait(self,direction,reference=False):
        try:
            self.walker.start_cycle(reference,direction,stride=self.stride.get(),lift=self.lift.get(),turn_degrees=self.turn_degrees.get(),period=self.period.get(),duty=self.duty.get(),phase_offsets=tuple(v.get() for v in self.phase_vars),start_phase=self.start_phase.get(),com_offset=(self.comx.get(),self.comy.get()),balance_margin=self.balance_margin.get(),body_height_mm=self.body_height.get(),leg_opening_mm=self.leg_opening.get())
            self.notice=""; self.paused=False
        except (ValueError,tk.TclError) as exc:
            self.notice=str(exc) or "Revisa los parámetros de marcha"
    def pause(self): self.paused=not self.paused
    def stop(self):
        if self.walker.motion=="GAIT":self.walker.set_drive(0,0)
        else:self.walker.motion="IDLE"
        self.keyboard_drive_active=False;self.pressed_keys.clear();self.paused=False
    def reset(self): self.walker.reset(); self.paused=False
    def set_pattern(self,_event=None):
        preset=GAIT_PRESETS.get(self.pattern.get())
        if not preset:return
        for key,var in (("stride",self.stride),("lift",self.lift),("turn_degrees",self.turn_degrees),("period",self.period),("duty",self.duty),("start_phase",self.start_phase)): var.set(preset[key])
        self.body_height.set(preset.get("body_height_mm",0.));self.leg_opening.set(preset.get("leg_opening_mm",0.))
        for var,value in zip(self.phase_vars,preset["phase_offsets"]): var.set(value)
    def current_preset(self):
        return {"schema":"mini-qpod-gait","version":1,"name":self.pattern.get(),"stride":self.stride.get(),"lift":self.lift.get(),"turn_degrees":self.turn_degrees.get(),"period":self.period.get(),"duty":self.duty.get(),"phase_offsets":[v.get() for v in self.phase_vars],"start_phase":self.start_phase.get(),"com_x_mm":self.comx.get(),"com_y_mm":self.comy.get(),"balance_margin_mm":self.balance_margin.get(),"body_height_mm":self.body_height.get(),"leg_opening_mm":self.leg_opening.get()}
    def save_preset(self):
        path=filedialog.asksaveasfilename(defaultextension=".json",filetypes=(("Preset Q-POD","*.json"),))
        if path:
            try: Path(path).write_text(json.dumps(self.current_preset(),indent=2,ensure_ascii=False),encoding="utf-8")
            except (OSError,ValueError) as exc: messagebox.showerror("Preset",str(exc))
    def load_preset(self):
        path=filedialog.askopenfilename(filetypes=(("Preset Q-POD","*.json"),))
        if not path:return
        try:
            p=json.loads(Path(path).read_text(encoding="utf-8"))
            if p.get("schema")!="mini-qpod-gait" or p.get("version")!=1 or len(p["phase_offsets"])!=4: raise ValueError("Formato de preset no válido")
            vals=[float(p[k]) for k in ("stride","lift","turn_degrees","period","duty","start_phase")]+[float(v) for v in p["phase_offsets"]]
            comx=float(p.get("com_x_mm",COM_ESTIMATE["x_mm"]));comy=float(p.get("com_y_mm",COM_ESTIMATE["y_mm"]));margin=float(p.get("balance_margin_mm",COM_ESTIMATE["balance_margin_mm"]))
            height=float(p.get("body_height_mm",0));opening=float(p.get("leg_opening_mm",0))
            if not -12<=height<=12 or not 0<=opening<=12:raise ValueError("Altura/apertura fuera de rango")
            vals.extend((comx,comy,margin))
            if not all(math.isfinite(v) for v in vals) or vals[0]<=0 or vals[1]<0 or vals[3]<=0 or not .5<=vals[4]<1: raise ValueError("Parámetros fuera de rango")
            self.stride.set(vals[0]);self.lift.set(vals[1]);self.turn_degrees.set(vals[2]);self.period.set(vals[3]);self.duty.set(vals[4]);self.start_phase.set(vals[5])
            self.comx.set(comx);self.comy.set(comy);self.balance_margin.set(margin)
            self.body_height.set(height);self.leg_opening.set(opening)
            for var,value in zip(self.phase_vars,vals[6:]):var.set(value%1)
            # Legacy Pace JSON files remain loadable, but new runs use an explicit choice.
            self.pattern.set("Personalizado")
        except (OSError,KeyError,TypeError,ValueError,json.JSONDecodeError) as exc: messagebox.showerror("Preset",str(exc))
    def export_preset(self):
        path=filedialog.asksaveasfilename(defaultextension=".h",filetypes=(("Header C++ Nano","*.h"),("Texto","*.txt")))
        if not path:return
        p=self.current_preset(); phases=", ".join(f"{v:.4f}f" for v in p["phase_offsets"])
        body=("// Parámetros de marcha exportados desde Mini Q-POD.\n// Copiar las asignaciones dentro de setup() del sketch Nano.\n// Revisar límites y validar con el robot elevado antes de apoyar.\n"
              "// Los movimientos vertical/lateral del cuerpo del simulador son SOLO SIMULACIÓN.\n"
              f"GAIT_STRIDE_MM = {p['stride']:.3f}f;\nGAIT_LIFT_MM = {p['lift']:.3f}f;\n"
              f"GAIT_TURN_DEG = {p['turn_degrees']:.3f}f;\nGAIT_PERIOD_S = {p['period']:.3f}f;\n"
              f"GAIT_DUTY = {p['duty']:.4f}f;\nGAIT_START_PHASE = {p['start_phase']:.4f}f;\n"
              f"GAIT_COM_X_MM = {p['com_x_mm']:.3f}f;\nGAIT_COM_Y_MM = {p['com_y_mm']:.3f}f;\nGAIT_COM_MARGIN_MM = {p['balance_margin_mm']:.3f}f;\n"
              f"targetBodyZ = {p['body_height_mm']:.3f}f;\ntargetSpread = {p['leg_opening_mm']:.3f}f;\n"
              f"float exportedOffsets[4] = {{{phases}}};\nmemcpy(GAIT_OFFSETS, exportedOffsets, sizeof(GAIT_OFFSETS));\n")
        try:Path(path).write_text(body,encoding="utf-8")
        except OSError as exc:messagebox.showerror("Exportar",str(exc))
    def body_motion(self,axis,distance):
        if self.walker.start_body_motion(axis,distance): self.notice="Movimiento de cuerpo solo en simulación"; self.paused=False
    def frame(self): self.walker.advance(.04)
    def tick(self):
        self.walker.set_pose_targets(self.body_height.get(),self.leg_opening.get())
        if not self.paused and self.walker.motion!="IDLE":
            remaining=.04*self.speed.get()
            while remaining>1e-9 and self.walker.motion!="IDLE":
                dt=min(.04,remaining); self.walker.advance(dt); remaining-=dt
        self.draw(); self.root.after(40,self.tick)
    def key(self,e):
        k=e.keysym.lower()
        if k in ("space",): self.pause()
        elif k in ("w","s","a","d"):
            self.pressed_keys.add(k);self.keyboard_drive_active=True
            if self.walker.motion=="IDLE":
                self.mode.set("Marcha coordinada · Nano")
                self.start_gait("forward")
            self._apply_keyboard_drive()
        elif k=="x": self.stop()
        elif k=="right": self.az += .12
        elif k=="left": self.az -= .12
        elif k in ("up","down"): self.frame()
    def key_release(self,e):
        k=e.keysym.lower()
        if k in ("w","s","a","d"):
            self.pressed_keys.discard(k);self._apply_keyboard_drive()
    def _apply_keyboard_drive(self):
        if not self.keyboard_drive_active:return
        forward=(1 if "w" in self.pressed_keys else 0)-(1 if "s" in self.pressed_keys else 0)
        turn=(1 if "a" in self.pressed_keys else 0)-(1 if "d" in self.pressed_keys else 0)
        self.walker.set_drive(forward,turn)
    def mouse_down(self,e): self.drag=(e.x,e.y,self.az,self.el)
    def mouse_move(self,e):
        if self.drag:
            x,y,a,b=self.drag; self.az=a+(e.x-x)*.009; self.el=(b+(e.y-y)*.009)%(2*math.pi); self.draw()
    def view_top(self): self.az=.72; self.el=.52; self.draw()
    def rotate_camera(self,dx,dy): self.az=(self.az+dx)%(2*math.pi); self.el=(self.el+dy)%(2*math.pi); self.draw()
    def zoom(self,e): self.zoom_delta(1.12 if e.delta>0 else .89)
    def zoom_delta(self,f): self.scale=max(.65,min(5,self.scale*f)); self.draw()
    def project(self,p):
        x,y,z=p; a=self.az; b=self.el
        u=x*math.cos(a)-y*math.sin(a); v=x*math.sin(a)+y*math.cos(a)
        # Screen-up is positive: +Z therefore always points upward at the home view.
        return (u, v*math.sin(b)+z*math.cos(b))
    def draw(self):
        c=self.canvas; c.delete("all"); w=max(c.winfo_width(),500); h=max(c.winfo_height(),400)
        ox,oy=w*.49,h*.49; sc=self.scale
        bX,bY=self.walker.body
        def xy(p, attached=True):
            px,py,pz=p
            if attached:
                a=self.walker.body_yaw; c0,s0=math.cos(a),math.sin(a)
                dx,dy=px-bX,py-bY
                px,py=bX+dx*c0-dy*s0,bY+dx*s0+dy*c0
            x,y=self.project((px,py,pz)); return ox+x*sc,oy-y*sc
        # Ground grid
        for n in range(-8,9):
            for p,q in [((-300,n*40,self.floor_z),(300,n*40,self.floor_z)),((n*40,-300,self.floor_z),(n*40,300,self.floor_z))]:
                c.create_line(*xy(p,False),*xy(q,False),fill="#e4e9ef",width=1)
        c.create_text(70,h-54,text="suelo · referencia READY",fill="#718092",anchor="w",font=("Segoe UI",8))
        ax,ay=62,58
        for axis,vec,col in (("+X",(1,0,0),"#c64b4b"),("+Y",(0,1,0),"#248d75"),("+Z",(0,0,1),"#4776bf")):
            dx,dy=self.project(vec); norm=max(math.hypot(dx,dy),1e-6); ex=ax+27*dx/norm; ey=ay-27*dy/norm
            c.create_line(ax,ay,ex,ey,fill=col,width=2,arrow="last"); c.create_text(ex+7,ey,text=axis,fill=col,anchor="w",font=("Segoe UI",8,"bold"))
        bz=self.walker.body_z
        corners=[(bX-46.5,bY-46.5,bz),(bX+46.5,bY-46.5,bz),(bX+46.5,bY+46.5,bz),(bX-46.5,bY+46.5,bz)]
        top=[(x,y,z+self.thickness.get()/2) for x,y,z in corners]
        bottom=[(x,y,z-self.thickness.get()/2) for x,y,z in corners]
        poly=[coord for p in top for coord in xy(p)]; c.create_polygon(*poly,fill="#34465a",outline="#172535",width=2)
        for i in range(4): c.create_line(*xy(top[i]),*xy(bottom[i]),fill="#64778b",width=2)
        for p,q in zip(bottom,bottom[1:]+bottom[:1]): c.create_line(*xy(p),*xy(q),fill="#64778b",width=2)
        # approximate COM, configurable at body center and visual body slab
        com=self.walker._to_world((self.comx.get(),self.comy.get(),self.comz.get())); cx,cy=xy(com,False); c.create_oval(cx-5,cy-5,cx+5,cy+5,fill="#dc3945",outline="white",width=2)
        support=[]
        if self.walker.motion=="GAIT" or (self.walker.motion=="IDLE" and self.walker.swinging):
            swing=set(self.walker.swinging)
        elif self.walker.motion=="SINGLE_PATH":
            swing={self.walker.leg}
        elif self.walker.motion in ("LEG_STEP","CYCLE_STEP") and self.walker.phase in (0,1):
            swing={self.walker.leg}
        else: swing=set()
        for i,name in enumerate(LEGS):
            mount=MOUNTS[name]
            hip=(mount.x+bX,mount.y+bY,bz); foot=self.walker._to_world(self.walker.feet[i])
            sol=solve(i,self.walker.feet[i]); color=("#e49b24" if i in swing else "#248d75")
            hx,hy=xy(hip); fx,fy=xy(foot,False)
            if sol.angles:
                a=sol.angles; local_coxa=(COXA*math.cos(a.yaw),COXA*math.sin(a.yaw),0)
                local_femur=(COXA*math.cos(a.yaw)+FEMUR*math.cos(a.yaw)*math.cos(a.femur),
                             COXA*math.sin(a.yaw)+FEMUR*math.sin(a.yaw)*math.cos(a.femur),FEMUR*math.sin(a.femur))
                p1=leg_to_body(name,local_coxa); p2=leg_to_body(name,local_femur)
                j1=xy((p1[0]+bX,p1[1]+bY,p1[2]+bz)); j2=xy((p2[0]+bX,p2[1]+bY,p2[2]+bz))
                c.create_line(hx,hy,*j1,fill="#4776bf",width=6,capstyle="round")
                c.create_line(*j1,*j2,fill="#248d75",width=6,capstyle="round")
                c.create_line(*j2,fx,fy,fill="#7856a8",width=6,capstyle="round")
                c.create_oval(j1[0]-3,j1[1]-3,j1[0]+3,j1[1]+3,fill="white",outline="#4776bf")
                c.create_oval(j2[0]-3,j2[1]-3,j2[0]+3,j2[1]+3,fill="white",outline="#7856a8")
            else:
                c.create_line(hx,hy,fx,fy,fill="#c74b4b",width=3,dash=(4,3))
            c.create_oval(fx-5,fy-5,fx+5,fy+5,fill=color,outline="white",width=1)
            c.create_text(fx+9,fy-8,text=name,fill="#263746",anchor="w",font=("Segoe UI",9,"bold"))
            if i not in swing: support.append(self.walker._to_world(self.walker.feet[i])[:2])
            phase=("vuelo" if i in swing else "apoyo")
            mech="—" if not sol.angles else ", ".join(f"{math.degrees(v):.1f}" for v in (sol.angles.yaw,sol.angles.femur,sol.angles.knee))
            elec="RECHAZADO · "+sol.reason if not sol.electrical else ", ".join(f"{v:.1f}" for v in sol.electrical)
            iid=str(i)
            if self.table.exists(iid): self.table.item(iid,values=(f"{name} · {phase}",mech,elec))
            else:self.table.insert("","end",iid=iid,values=(f"{name} · {phase}",mech,elec))
            hist=self.walker.history[i]
            if len(hist)>1:
                pts=[]
                for p in hist[::max(1,len(hist)//60)]: pts.extend((p[1]+bY,-p[2]))
                # Small path inset in world coords converted to canvas separately below.
                for j in range(0,len(pts)-2,2):
                    c.create_line(*xy((self.walker.feet[i][0],pts[j],-pts[j+1])),*xy((self.walker.feet[i][0],pts[j+2],-pts[j+3])),fill="#9ba8b7",dash=(2,3))
        if len(support)>=3:
            # triangle is a display of geometric support points only
            hull=__import__("tools.ik_walk_simulator.walk",fromlist=["support_hull"]).support_hull(support)
            if len(hull)>=3:
                screen=[xy((x,y,self.floor_z),False) for x,y in hull]
                c.create_polygon(*[v for p in screen for v in p],outline="#4aa180",fill="",dash=(4,3),width=2)
        com_world=self.walker._to_world((self.comx.get(),self.comy.get(),self.comz.get())); com_xy=com_world[:2]
        stability=estimate_static_stability(support,com_xy)
        text=f"{self.walker.motion} · t={self.walker.time:.1f} s · cuerpo=({bX:.1f},{bY:.1f},{bz:.1f}) mm · yaw={math.degrees(self.walker.body_yaw):.1f}° · modo: {self.mode.get()}"
        metrics=stride_metrics(self.stride.get(),self.duty.get(),self.walker.direction)
        if self.walker.direction in ("left","right"):
            yaw=self.turn_degrees.get()*(1 if self.walker.direction=="left" else -1)
            stride_text=f"Giro: traslación nominal 0 mm/ciclo · rotación {yaw:+.1f}°/ciclo · vuelo del pie {metrics['world_swing']:.1f} mm en mundo."
        else:
            actual="aún sin ciclo completo" if self.walker.last_cycle_advance is None else f"{self.walker.last_cycle_advance:+.1f} mm/ciclo medidos tras balance"
            stride_text=f"{self.pattern.get()}: avance nominal {metrics['body_advance']:+.1f} mm/ciclo; vuelo del pie {metrics['world_swing']:.1f} mm mundo / {metrics['body_relative_swing']:.1f} mm relativo al cuerpo; avance real del cuerpo: {actual}."
        if self.walker.limit_rejection:
            leg,reason=self.walker.limit_rejection
            stride_text+=f" Nano/simulador detenidos por límite en {LEGS[leg]}: {reason}. Objetivo sin recortar."
        shift=self.walker.balance_shift_last
        if self.walker.motion=="GAIT" and (shift[0] or shift[1]):
            stride_text+=f" Ajuste corporal por COM en este cuadro: ({shift[0]:+.2f}, {shift[1]:+.2f}) mm."
        self.stride_info.set(stride_text)
        self.status.set(f"{text} · {self.notice}" if self.notice else text)
        self.support.set(f"Apoyo: {len(support)} ({', '.join(LEGS[i] for i in range(4) if i not in swing) or 'ninguna'}) · vuelo: {len(swing)} ({', '.join(LEGS[i] for i in swing) or 'ninguna'}). COM aproximado Δ=({self.comx.get():.0f},{self.comy.get():.0f},{self.comz.get():.0f}) mm; espesor visual {self.thickness.get():.0f} mm.")
        minimum=self.walker.minimum_margin_after
        before=self.walker.minimum_margin_before
        observed="sin muestra de ciclo" if minimum is None else f"margen mínimo hasta ahora: {minimum:.1f} mm después del traslado ({before:.1f} mm antes)"
        self.stability.set("Estabilidad estática estimada: "+stability["text"]+f". COM corregido con margen solicitado {self.balance_margin.get():.1f} mm; {observed}. Aproximación geométrica, sin validación física.")
        self.draw_trace()
    def draw_trace(self):
        c=self.trace;c.delete("all");w=max(c.winfo_width(),200);h=max(c.winfo_height(),150); pad=18
        c.create_line(pad,h-pad,w-pad,h-pad,fill="#667",arrow="last");c.create_line(pad,h-pad,pad,pad,fill="#667",arrow="last")
        c.create_text(w-25,h-8,text="Y",fill="#556");c.create_text(9,pad,text="Z",fill="#556")
        points=[p for history in self.walker.history for p in history]
        points += self.walker.feet
        ys=[p[1] for p in points]; zs=[p[2] for p in points]
        yr=max(80.,max(ys)-min(ys)); zr=max(45.,max(zs)-min(zs))
        y0=(max(ys)+min(ys)-yr)/2; z0=(max(zs)+min(zs)-zr)/2
        def chart(p): return (pad+(p[1]-y0)/yr*(w-2*pad), h-pad-(p[2]-z0)/zr*(h-2*pad))
        for i,hist in enumerate(self.walker.history):
            if len(hist)<2: continue
            sampled=hist[::max(1,len(hist)//120)]
            vals=[coord for p in sampled for coord in chart(p)]
            c.create_line(*vals,fill=("#e49b24" if i in self.walker.swinging else ("#4776bf" if i%2==0 else "#248d75")),width=2,smooth=True)

def main():
    root=tk.Tk(); App(root); root.mainloop()
if __name__=="__main__":main()
