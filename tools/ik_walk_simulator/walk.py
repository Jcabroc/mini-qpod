"""Motion logic ported from mini_qpod_ik_walk_mvp/ik_walk_core.h and .ino."""
from dataclasses import dataclass
import json
import math
from pathlib import Path

from tools.ik_simulator.kinematics import Angles, MOUNTS, forward, inverse

def _approach(value,target,max_delta):
    return max(value-max_delta,min(value+max_delta,target))

def ready_frame(start,target,progress):
    t=max(0.,min(1.,float(progress)));s=t*t*(3.-2.*t)
    return tuple(a+(b-a)*s for a,b in zip(start,target))

LEGS = ("L1", "R1", "L2", "R2")
ORDER = (0, 3, 1, 2)
_TRANSFER = json.loads((Path(__file__).with_name("gait_defaults.json")).read_text(encoding="utf-8"))
COM_ESTIMATE = _TRANSFER["com_estimate"]
_PRESETS_BY_KEY = _TRANSFER["presets"]
GAIT_PRESETS = {p["label"]:{**{k:v for k,v in p.items() if k!="label"},"phase_offsets":tuple(p["phase_offsets"])} for p in _PRESETS_BY_KEY.values()}
PATTERN_CHOICES = tuple(p["label"] for p in _PRESETS_BY_KEY.values()) + ("Personalizado",)
CAL = ((20,90,115,1),(10,110,180,1),(25,90,180,-1),
       (90,110,180,1),(10,90,180,-1),(0,90,170,-1),
       (70,100,180,1),(10,90,180,1),(0,100,160,-1),
       (30,100,140,-1),(10,105,175,-1),(40,115,180,-1))
READY_ELECTRICAL = (84,110,100,102,90,100,108,90,110,94,105,125)
READY_MECHANICAL = tuple(Angles(*map(math.radians, row)) for row in
                         ((-6,-35,-70),(-8,-35,-70),(8,-35,-70),(6,-35,-70)))
READY_FEET = tuple(forward(leg, angles) for leg, angles in zip(LEGS, READY_MECHANICAL))

def gait_swing(start, touchdown, t, lift):
    t=min(1.,max(0.,float(t))); smooth=t*t*(3.-2.*t)
    z=math.sin(math.pi*t)
    return (start[0]+(touchdown[0]-start[0])*smooth,
            start[1]+(touchdown[1]-start[1])*smooth,
            start[2]+lift*z*z)

def gait_phase_at(phase, offset):
    """Phase offset port of IkWalk::gaitPhaseAt."""
    return math.floor(((phase+offset)%1.)*100000.+.5)/100000.

def gait_in_stance(phase,duty):
    return phase < duty-1e-6

def minimum_gait_support(duty, phase_offsets):
    return gait_support_range(duty, phase_offsets)[0]

def gait_support_range(duty, phase_offsets):
    if len(phase_offsets)!=4: raise ValueError("Se requieren cuatro desfases, uno por pata")
    events={0.,1.}
    for offset in phase_offsets:
        events.add((-offset)%1.)
        events.add((duty-offset)%1.)
    ordered=sorted(events); samples=set(ordered)
    samples.update((a+b)/2 for a,b in zip(ordered,ordered[1:]))
    counts=[4-sum(not gait_in_stance((p+o)%1.,duty) for o in phase_offsets) for p in samples]
    return min(counts),max(counts)

def stride_metrics(stride, duty, direction="forward"):
    """Nominal geometry of one cycle; excludes turns and IK rejection."""
    stride=float(stride); duty=float(duty)
    if direction in ("left", "right"):
        return {"body_advance":0.,"world_swing":stride,"body_relative_swing":None}
    sign=-1. if direction=="backward" else 1.
    return {"body_advance":sign*stride,"world_swing":stride,"body_relative_swing":stride*duty}

def support_hull(points):
    points=sorted(set((float(p[0]),float(p[1])) for p in points))
    if len(points)<=1: return points
    def cross(o,a,b): return (a[0]-o[0])*(b[1]-o[1])-(a[1]-o[1])*(b[0]-o[0])
    lower=[]
    for p in points:
        while len(lower)>=2 and cross(lower[-2],lower[-1],p)<=0: lower.pop()
        lower.append(p)
    upper=[]
    for p in reversed(points):
        while len(upper)>=2 and cross(upper[-2],upper[-1],p)<=0: upper.pop()
        upper.append(p)
    return lower[:-1]+upper[:-1]

def estimate_static_stability(support_points, com_point):
    """Geometric COM-to-support-polygon estimate; never a physical certification."""
    hull=support_hull(support_points)
    if len(hull)<3:
        return {"state":"indeterminate","margin":None,"hull":hull,
                "text":f"No estable estáticamente estimada: {len(hull)} contactos no forman un polígono de área"}
    distances=[]
    for a,b in zip(hull,hull[1:]+hull[:1]):
        dx,dy=b[0]-a[0],b[1]-a[1]
        length=math.hypot(dx,dy)
        if length<=1e-9: continue
        distances.append((dx*(com_point[1]-a[1])-dy*(com_point[0]-a[0]))/length)
    if not distances:
        return {"state":"indeterminate","margin":None,"hull":hull,"text":"Estabilidad estática estimada: indeterminada"}
    margin=min(distances)
    if margin<0:
        return {"state":"outside","margin":margin,"hull":hull,
                "text":f"COM estimado fuera del polígono de apoyo (margen geométrico {margin:.1f} mm)"}
    return {"state":"inside","margin":margin,"hull":hull,
            "text":f"COM estimado dentro del polígono de apoyo (margen geométrico {margin:.1f} mm)"}

def balance_translation(support_points, com_point, requested_margin=2.):
    """Small planar body correction shared with IkWalk::balanceTranslation."""
    hull=support_hull(support_points)
    if len(hull)<3:return (0.,0.)
    current=estimate_static_stability(hull,com_point)["margin"]
    if current is None or current>=requested_margin:return (0.,0.)
    center=(sum(p[0] for p in hull)/len(hull),sum(p[1] for p in hull)/len(hull))
    center_margin=estimate_static_stability(hull,center)["margin"]
    if center_margin is None or center_margin<=current:return (0.,0.)
    target=min(requested_margin,center_margin)
    alpha=max(0.,min(1.,(target-current)/(center_margin-current)))
    return ((center[0]-com_point[0])*alpha,(center[1]-com_point[1])*alpha)

@dataclass(frozen=True)
class JointResult:
    angles: Angles | None
    electrical: tuple[float, float, float] | None
    reason: str = ""

def map_electrical(leg_index, angles):
    result=[]
    for j, radians in enumerate((angles.yaw, angles.femur, angles.knee)):
        channel=leg_index*3+j; low,center,high,direction=CAL[channel]
        anchor=(READY_MECHANICAL[leg_index].yaw,READY_MECHANICAL[leg_index].femur,READY_MECHANICAL[leg_index].knee)[j]
        value=READY_ELECTRICAL[channel]+direction*(radians-anchor)*180/math.pi
        if value < low+5 or value > high-5:
            raise ValueError(f"CH{channel}: {value:.2f}° fuera del intervalo configurado [{low+5}, {high-5}]°")
        result.append(value)
    return tuple(result)

def solve(leg_index, point):
    try:
        angles=inverse(LEGS[leg_index], point)
        return JointResult(angles, map_electrical(leg_index, angles))
    except (ValueError, KeyError) as exc:
        return JointResult(None, None, str(exc))

class Walker:
    """Firmware-matched IK targets with a continuous, phase-offset gait."""
    def __init__(self):
        self.reset()
    def reset(self):
        self.feet=[tuple(p) for p in READY_FEET]; self.targets=list(self.feet)
        self.motion="IDLE"; self.reference=False; self.direction="forward"; self.leg=0; self.phase=0
        self.cycle_index=0; self.progress=0.; self.time=0.; self.body=(0.,0.); self.body_z=0.; self.body_yaw=0.
        self.gait_phase=0.; self.stride=18.; self.lift=12.; self.period=4.; self.duty=.78; self.turn_degrees=4.; self.start_phase=.02
        self.phase_offsets=GAIT_PRESETS["Caminata lenta"]["phase_offsets"]; self.anchors=[]; self.swing_starts=[]; self.swing_targets=[]
        self.swinging=()
        self.limit_rejection=None
        self.com_offset=(0.,0.); self.balance_margin=2.; self.minimum_margin_before=None; self.minimum_margin_after=None
        self.balance_shift_total=0.; self.balance_shift_last=(0.,0.)
        self.forward_speed=self.turn_speed=self.target_forward=self.target_turn=0.
        self.body_height_target=0.; self.spread=self.spread_target=0.; self.foot_spread=[0.]*4
        self.single_flight_only=False;self.single_flight_leg=0;self.saved_phase_offsets=None;self.saved_duty=None
        self.deferred_ready=False
        self.history=[[] for _ in LEGS]
    def ready(self):
        if self.motion=="GAIT":
            self.target_forward=self.target_turn=0.;self.deferred_ready=True;return
        self.limit_rejection=None
        self.targets=list(READY_FEET); self.ready_starts=list(self.feet); self.ready_elapsed=0.
        self.ready_duration=1.2
        self.motion="READYING"
    def set_drive(self, forward, turn):
        self.target_forward=max(-1.,min(1.,float(forward)))
        self.target_turn=max(-1.,min(1.,float(turn)))
    def set_pose_targets(self, height_mm=None, opening_mm=None):
        if height_mm is not None:self.body_height_target=float(height_mm)
        if opening_mm is not None:self.spread_target=max(0.,float(opening_mm))
    def start_leg(self, leg):
        self.limit_rejection=None
        self.leg=leg; self.phase=0; self.motion="LEG_STEP"; self.progress=0.
        self.swinging=()
        self.start=tuple(self.feet[leg]); self.step_elapsed=0.
    def start_single_step(self,leg,**parameters):
        if not 0<=int(leg)<4:raise ValueError("Pata fuera de rango")
        self.saved_phase_offsets=self.phase_offsets;self.saved_duty=self.duty
        offsets=[.25,.5,.75,0.];offsets[leg]=0.
        others=[.25,.5,.75];j=0
        for i in range(4):
            if i!=leg:offsets[i]=others[j];j+=1
        self.start_cycle(direction="forward",duty=.85,phase_offsets=tuple(offsets),start_phase=.77,**parameters)
        self.single_flight_only=True;self.single_flight_leg=leg
    def start_cycle(self, reference=False, direction="forward", *, stride=18., lift=12., turn_degrees=4., period=4., duty=.76, phase_offsets=(0.,.5,.75,.25), start_phase=0., com_offset=(0.,0.), balance_margin=2.,body_height_mm=0.,leg_opening_mm=0.):
        self.limit_rejection=None
        self.reference=reference; self.cycle_index=0
        self.direction=direction
        if direction in ("forward","backward","left","right"):
            self.set_drive(-1. if direction=="backward" else (0. if direction in ("left","right") else 1.),
                           1. if direction=="left" else (-1. if direction=="right" else 0.))
        if reference:
            self.start_leg(ORDER[0]); self.motion="CYCLE_STEP"
            return
        self.stride=max(1.,float(stride)); self.lift=max(0.,float(lift)); self.turn_degrees=float(turn_degrees); self.period=max(.4,float(period)); self.duty=min(.85,max(.5,float(duty)))
        self.phase_offsets=tuple(float(v)%1. for v in phase_offsets)
        minimum=minimum_gait_support(self.duty,self.phase_offsets)
        if minimum<2: raise ValueError(f"Los desfases dejan solo {minimum} patas en apoyo en alguna fase; ajusta las fases")
        self.start_phase=float(start_phase)%1.; self.gait_phase=self.start_phase; self.motion="GAIT"
        self.com_offset=tuple(map(float,com_offset)); self.balance_margin=max(0.,float(balance_margin))
        self.set_pose_targets(body_height_mm,leg_opening_mm)
        self.minimum_margin_before=None; self.minimum_margin_after=None; self.balance_shift_total=0.; self.balance_shift_last=(0.,0.)
        self.cycle_body_start=self.body; self.cycle_yaw_start=self.body_yaw; self.last_cycle_advance=None
        self._reset_world_anchors()
        self.history=[[tuple(p)] for p in self.feet]
    def _to_world(self, p):
        c,s=math.cos(self.body_yaw),math.sin(self.body_yaw)
        return (self.body[0]+c*p[0]-s*p[1], self.body[1]+s*p[0]+c*p[1], self.body_z+p[2])
    def _to_local(self, p):
        c,s=math.cos(self.body_yaw),math.sin(self.body_yaw); x=p[0]-self.body[0]; y=p[1]-self.body[1]
        return (c*x+s*y,-s*x+c*y,p[2]-self.body_z)
    def start_body_motion(self, axis, distance, duration=.7):
        """Simulation-only body translation while maintaining planted world foot anchors."""
        if self.motion!="IDLE": return False
        self.limit_rejection=None
        self._reset_world_anchors(); self.body_start=(*self.body,self.body_z); self.body_target=list(self.body_start)
        if axis=="z": self.body_target[2]+=distance
        elif axis=="lateral":
            c,s=math.cos(self.body_yaw),math.sin(self.body_yaw)
            self.body_target[0]+=-s*distance; self.body_target[1]+=c*distance
        elif axis=="center": self.body_target=[0.,0.,0.]
        self.body_motion_time=0.; self.body_motion_duration=max(.1,float(duration)); self.motion="BODY_SHIFT"; self.swinging=()
        return True
    def _advance_body_motion(self,dt):
        self.body_motion_time=min(self.body_motion_duration,self.body_motion_time+dt)
        t=self.body_motion_time/self.body_motion_duration; s=t*t*(3-2*t)
        start=self.body_start; target=self.body_target
        self.body=(start[0]+(target[0]-start[0])*s,start[1]+(target[1]-start[1])*s)
        self.body_z=start[2]+(target[2]-start[2])*s
        self.feet=[self._to_local(p) for p in self.anchors]
        if t>=1: self.motion="IDLE"
    def _reset_world_anchors(self):
        self.anchors=[self._to_world(p) for p in self.feet]
        self.swing_starts=list(self.anchors); self.swing_targets=list(self.anchors)
    def _advance_gait(self, dt):
        if self.single_flight_only and not gait_in_stance(gait_phase_at(self.gait_phase,self.phase_offsets[self.single_flight_leg]),self.duty):
            self.target_forward=0.
        self.forward_speed=_approach(self.forward_speed,self.target_forward,2.*dt)
        self.turn_speed=_approach(self.turn_speed,self.target_turn,2.*dt)
        self._translate_body(0.,0.,_approach(self.body_z,self.body_height_target,8.*dt)-self.body_z)
        self.spread=_approach(self.spread,self.spread_target,4.*dt)
        old=self.gait_phase; delta=dt/self.period; new=math.floor(((old+delta)%1.)*100000.+.5)/100000.
        # Prepare the COM over the tripod that will remain after the next lift.
        all_stance=all(gait_in_stance(gait_phase_at(old,o),self.duty) for o in self.phase_offsets)
        upcoming=[(self.duty-gait_phase_at(old,o),i) for i,o in enumerate(self.phase_offsets) if gait_in_stance(gait_phase_at(old,o),self.duty)]
        if all_stance and upcoming:
            until,leg=min(upcoming)
            if 0.<until<=.08:
                world=[self._to_world(p) for p in self.feet]
                triangle=support_hull([world[i][:2] for i in range(4) if i!=leg])
                com=self._to_world((*self.com_offset,0.))[:2]
                dx,dy=balance_translation(triangle,com,self.balance_margin)
                factor=min(1.,dt/max(dt,until*self.period))
                self._translate_body(dx*factor,dy*factor,0.)
        old_world=[self._to_world(p) for p in self.feet]
        origin_x,origin_y,origin_yaw=self.body[0],self.body[1],self.body_yaw
        dyaw=self.turn_speed*self.turn_degrees*delta*math.pi/180.
        mid=origin_yaw+dyaw*.5
        move=self.stride*self.forward_speed*delta
        self.body=(self.body[0]-math.sin(mid)*move,self.body[1]+math.cos(mid)*move)
        self.body_yaw+=dyaw
        full_yaw=self.turn_speed*self.turn_degrees*math.pi/180.
        full_mid=origin_yaw+full_yaw*.5; full_move=self.stride*self.forward_speed
        tx=-math.sin(full_mid)*full_move; ty=math.cos(full_mid)*full_move
        cs,ss=math.cos(full_yaw),math.sin(full_yaw)
        for i,offset in enumerate(self.phase_offsets):
            u0=gait_phase_at(old,offset); u1=gait_phase_at(new,offset)
            stance0=gait_in_stance(u0,self.duty); stance1=gait_in_stance(u1,self.duty)
            if stance0 and not stance1:
                start=old_world[i]; self.swing_starts[i]=start
                rx=start[0]-origin_x; ry=start[1]-origin_y
                self.swing_targets[i]=(origin_x+tx+cs*rx-ss*ry,origin_y+ty+ss*rx+cs*ry,start[2])
            elif not stance0 and stance1:
                self.anchors[i]=self.swing_targets[i]
                self.foot_spread[i]=self.spread
            if stance1:
                self.feet[i]=self._to_local(self.anchors[i])
            else:
                s=(u1-self.duty)/(1.-self.duty)
                target=self.swing_targets[i]
                dx=target[0]-self.body[0];dy=target[1]-self.body[1];radius=math.hypot(dx,dy)
                rx,ry=((dx/radius,dy/radius) if radius>1e-6 else (0.,0.))
                goal=(target[0]+rx*(self.spread-self.foot_spread[i]),target[1]+ry*(self.spread-self.foot_spread[i]),target[2])
                world=gait_swing(self.swing_starts[i],goal,s,self.lift)
                self.feet[i]=self._to_local(world)
        self.gait_phase=new
        # Active leg list is used by the drawing and support-polygon display.
        active=[i for i,o in enumerate(self.phase_offsets) if not gait_in_stance(gait_phase_at(new,o),self.duty)]
        self.swinging=tuple(active)
        self._balance_support()
        if self.single_flight_only and not self.swinging and abs(self.forward_speed)<.02 and abs(self.turn_speed)<.02:
            self.phase_offsets=self.saved_phase_offsets;self.duty=self.saved_duty
            self.single_flight_only=False;self.target_forward=self.target_turn=0.;self.motion="IDLE"
            if getattr(self,"deferred_ready",False):self.deferred_ready=False;self.ready()
        if new<old:
            dx=self.body[0]-self.cycle_body_start[0];dy=self.body[1]-self.cycle_body_start[1]
            self.last_cycle_advance=dx*(-math.sin(self.cycle_yaw_start))+dy*math.cos(self.cycle_yaw_start)
            self.cycle_body_start=self.body;self.cycle_yaw_start=self.body_yaw
            if abs(self.target_forward)<.01 and abs(self.target_turn)<.01 and not self.swinging:
                self.motion="IDLE"
    def _translate_body(self,dx,dy,dz):
        world=[self._to_world(p) for p in self.feet]
        self.body=(self.body[0]+dx,self.body[1]+dy);self.body_z+=dz
        self.feet=[self._to_local(p) for p in world]
    def _balance_support(self):
        world=[self._to_world(p) for p in self.feet]
        support=[world[i][:2] for i in range(4) if i not in self.swinging]
        com=self._to_world((self.com_offset[0],self.com_offset[1],0.))[:2]
        before=estimate_static_stability(support,com)["margin"]
        if before is not None:self.minimum_margin_before=before if self.minimum_margin_before is None else min(self.minimum_margin_before,before)
        dx,dy=balance_translation(support,com,self.balance_margin)
        if dx or dy:
            self.body=(self.body[0]+dx,self.body[1]+dy)
            self.feet=[self._to_local(p) for p in world]
            self.balance_shift_last=(dx,dy);self.balance_shift_total+=math.hypot(dx,dy)
        else:self.balance_shift_last=(0.,0.)
        com=self._to_world((self.com_offset[0],self.com_offset[1],0.))[:2]
        after=estimate_static_stability(support,com)["margin"]
        if after is not None:self.minimum_margin_after=after if self.minimum_margin_after is None else min(self.minimum_margin_after,after)
    def target(self):
        x,y,z=self.start
        if self.phase==0: return (x,y,z+10)
        if self.phase==1: return (x,y+(0 if self.direction in ("left","right") else (-10 if self.direction=="backward" else 10)),z+10)
        if self.phase==2: return (x,y+(0 if self.direction in ("left","right") else (-10 if self.direction=="backward" else 10)),z)
        return self.start
    def advance(self, dt=.04):
        was_active=self.motion!="IDLE"
        self.time += dt
        if self.motion=="READYING":
            self.ready_elapsed=min(self.ready_duration,self.ready_elapsed+dt)
            t=self.ready_elapsed/self.ready_duration
            self.feet=[ready_frame(start,dst,t) for start,dst in zip(self.ready_starts,self.targets)]
            if t>=1:self.motion="IDLE"
        elif self.motion in ("LEG_STEP","CYCLE_STEP"):
            dt=min(dt,max(0.,4.-self.step_elapsed))
            dst=self.target(); p=self.feet[self.leg]; d=math.dist(p,dst); step=10*dt
            if d>step and d>0: self.feet[self.leg]=tuple(a+(b-a)*step/d for a,b in zip(p,dst))
            else:
                self.feet[self.leg]=dst; self.phase+=1
                if self.phase==4:
                    if self.motion=="CYCLE_STEP" and self.cycle_index<3:
                        self.cycle_index+=1; self.start_leg(ORDER[self.cycle_index]); self.motion="CYCLE_STEP"
                    else: self.motion="IDLE"
        elif self.motion=="GAIT":
            remaining=dt
            max_substep=max(.004,self.period*.02)
            while remaining>1e-12:
                substep=min(remaining,max_substep)
                self._advance_gait(substep); remaining-=substep
        elif self.motion=="BODY_SHIFT": self._advance_body_motion(dt)
        if was_active:
            for i,foot in enumerate(self.feet):
                result=solve(i,foot)
                if result.electrical is None:
                    self.limit_rejection=(i,result.reason)
                    # Firmware disables PWM and ends the motion on the first rejected frame.
                    self.motion="IDLE"
                    break
        for i,p in enumerate(self.feet):
            self.history[i].append(p)
            if len(self.history[i])>160: self.history[i].pop(0)
