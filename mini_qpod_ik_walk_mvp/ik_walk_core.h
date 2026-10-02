#pragma once

// Port of tools/ik_simulator/kinematics.py. Units: mm, radians, mechanical angles.
#include <math.h>
#include <stdint.h>

namespace IkWalk {
const float COXA=42.294f, FEMUR=60.611f, TIBIA=88.714f;
const float PI_F=3.14159265358979323846f;
const uint8_t LEG_COUNT=4, JOINTS=3, CHANNELS=12;

struct Vec3 { float x,y,z; };
struct Angles { float yaw,femur,knee; };
struct Mount { float x,y,yaw; };
struct Calibration { int16_t low,center,high; int8_t direction; };
static const Mount mounts[LEG_COUNT]={{-46.5f,46.5f,135*PI_F/180},{46.5f,46.5f,45*PI_F/180},{-46.5f,-46.5f,-135*PI_F/180},{46.5f,-46.5f,-45*PI_F/180}};
static const Calibration calibration[CHANNELS]={{20,90,115,1},{10,110,180,1},{25,90,180,-1},{90,110,180,1},{10,90,180,-1},{0,90,170,-1},{70,100,180,1},{10,90,180,1},{0,100,160,-1},{30,100,140,-1},{10,105,175,-1},{40,115,180,-1}};
static const float readyElectrical[CHANNELS]={84,110,100,102,90,100,108,90,110,94,105,125};
// Mechanical offsets are anchored at the measured electrical READY posture.
// Yaw keeps its measured radial trim; femur/tibia create a usable, non-singular
// support pose. No calibration value is altered by this transform.
static const Angles readyMechanical[LEG_COUNT]={{-6*PI_F/180,-35*PI_F/180,-70*PI_F/180},{-8*PI_F/180,-35*PI_F/180,-70*PI_F/180},{8*PI_F/180,-35*PI_F/180,-70*PI_F/180},{6*PI_F/180,-35*PI_F/180,-70*PI_F/180}};

inline bool finite(float a) { return isfinite(a); }
inline Vec3 bodyToLeg(uint8_t leg, Vec3 p) { const Mount&m=mounts[leg];float c=cos(m.yaw),s=sin(m.yaw),dx=p.x-m.x,dy=p.y-m.y;return {c*dx+s*dy,-s*dx+c*dy,p.z}; }
inline Vec3 legToBody(uint8_t leg, Vec3 p) { const Mount&m=mounts[leg];float c=cos(m.yaw),s=sin(m.yaw);return {m.x+c*p.x-s*p.y,m.y+s*p.x+c*p.y,p.z}; }
inline Vec3 forwardLocal(Angles a) { float r=COXA+FEMUR*cos(a.femur)+TIBIA*cos(a.femur+a.knee);return {r*cos(a.yaw),r*sin(a.yaw),FEMUR*sin(a.femur)+TIBIA*sin(a.femur+a.knee)}; }
inline Vec3 forward(uint8_t leg, Angles a) { return legToBody(leg,forwardLocal(a)); }

// Same branch as Python inverse(..., knee_sign=-1). Never clamps acos input.
inline bool inverseLocal(Vec3 p, Angles &out) {
  if(!finite(p.x)||!finite(p.y)||!finite(p.z))return false;
  float radius=sqrt(p.x*p.x+p.y*p.y); if(radius<=1e-6f)return false;
  float horizontal=radius-COXA, distance=sqrt(horizontal*horizontal+p.z*p.z);
  if(distance<fabs(FEMUR-TIBIA)||distance>FEMUR+TIBIA)return false;
  float cosine=(distance*distance-FEMUR*FEMUR-TIBIA*TIBIA)/(2*FEMUR*TIBIA);
  if(cosine < -1.0f || cosine > 1.0f)return false;
  float knee=-acos(cosine);
  out={atan2(p.y,p.x),atan2(p.z,horizontal)-atan2(TIBIA*sin(knee),FEMUR+TIBIA*cos(knee)),knee}; return true;
}
inline bool inverse(uint8_t leg, Vec3 point, Angles &out) { return leg<LEG_COUNT && inverseLocal(bodyToLeg(leg,point),out); }

inline bool electricalFromMechanical(uint8_t ch,float radians,float &electrical) {
  if(ch>=CHANNELS || !finite(radians))return false;
  const float *anchor=&readyMechanical[ch/3].yaw;
  electrical=readyElectrical[ch]+calibration[ch].direction*(radians-anchor[ch%3])*180.0f/PI_F;
  return electrical>=calibration[ch].low+5 && electrical<=calibration[ch].high-5;
}
inline bool anglesToElectrical(uint8_t leg,Angles a,float out[3]) {
  return electricalFromMechanical(leg*3,a.yaw,out[0]) && electricalFromMechanical(leg*3+1,a.femur,out[1]) && electricalFromMechanical(leg*3+2,a.knee,out[2]);
}
inline Vec3 readyFoot(uint8_t leg) { return forward(leg,readyMechanical[leg]); }
inline Vec3 lerp(Vec3 a,Vec3 b,float t) { return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t}; }
// Shared Nano/Python walking path. phase 0 lift, 1 swing, 2 lower, 3 return.
inline Vec3 stepTarget(Vec3 start,uint8_t phase,int8_t direction,int8_t turn) {
  if(phase==0)return {start.x,start.y,start.z+10.0f};
  if(phase==1)return {start.x,start.y+(turn?0.0f:direction*10.0f),start.z+10.0f};
  if(phase==2)return {start.x,start.y+(turn?0.0f:direction*10.0f),start.z};
  return start;
}
inline float smoothStep(float t) { t=t<0?0:(t>1?1:t); return t*t*(3.0f-2.0f*t); }
inline Vec3 readyFrame(Vec3 start,Vec3 target,float progress) { return lerp(start,target,smoothStep(progress)); }
inline float canonicalPhase(float phase) { float u=fmod(phase,1.0f);if(u<0)u+=1.0f;return floorf(u*100000.0f+0.5f)/100000.0f; }
inline float gaitPhaseAt(float phase,float offset) { return canonicalPhase(phase+offset); }
inline bool gaitInStance(float phase,float duty) { return phase<duty-1e-6f; }
inline Vec3 gaitSwing(Vec3 start,Vec3 touchdown,float t,float lift) {
  float s=smoothStep(t),z=sin(PI_F*t);
  return {start.x+(touchdown.x-start.x)*s,
          start.y+(touchdown.y-start.y)*s,
          start.z+lift*z*z};
}
inline float supportMargin(const Vec3 *points,uint8_t count,float comX,float comY) {
  if(count<3)return -INFINITY;
  float area2=0;
  for(uint8_t i=0;i<count;++i){const Vec3&a=points[i];const Vec3&b=points[(i+1)%count];area2+=a.x*b.y-b.x*a.y;}
  if(fabs(area2)<1e-5f)return -INFINITY;
  float orientation=area2>0?1.0f:-1.0f,margin=INFINITY;
  for(uint8_t i=0;i<count;++i){const Vec3&a=points[i];const Vec3&b=points[(i+1)%count];float dx=b.x-a.x,dy=b.y-a.y,len=sqrt(dx*dx+dy*dy);if(len<1e-6f)continue;float d=orientation*(dx*(comY-a.y)-dy*(comX-a.x))/len;if(d<margin)margin=d;}
  return margin;
}
// Translate the projected COM toward the support polygon centroid until the
// requested signed edge margin is met. Points must be in cyclic hull order.
inline bool balanceTranslation(const Vec3 *points,uint8_t count,float comX,float comY,float requestedMargin,float &shiftX,float &shiftY) {
  shiftX=shiftY=0;if(count<3)return false;
  float current=supportMargin(points,count,comX,comY);if(!finite(current)||current>=requestedMargin)return false;
  float centerX=0,centerY=0;for(uint8_t i=0;i<count;++i){centerX+=points[i].x;centerY+=points[i].y;}centerX/=count;centerY/=count;
  float centerMargin=supportMargin(points,count,centerX,centerY);if(!finite(centerMargin)||centerMargin<=current)return false;
  float target=fmin(requestedMargin,centerMargin),alpha=(target-current)/(centerMargin-current);alpha=fmax(0.0f,fmin(1.0f,alpha));
  shiftX=(centerX-comX)*alpha;shiftY=(centerY-comY)*alpha;return true;
}
inline Vec3 bodyToWorld(Vec3 p,float x,float y,float z,float yaw) { float c=cos(yaw),s=sin(yaw);return {x+c*p.x-s*p.y,y+s*p.x+c*p.y,z+p.z}; }
inline Vec3 worldToBody(Vec3 p,float x,float y,float z,float yaw) { float c=cos(yaw),s=sin(yaw),dx=p.x-x,dy=p.y-y;return {c*dx+s*dy,-s*dx+c*dy,p.z-z}; }
inline void translateBodyKeepingFeet(Vec3 feet[4],float &x,float &y,float &z,float &yaw,float dx,float dy,float dz) {
  Vec3 world[4];for(uint8_t i=0;i<4;++i)world[i]=bodyToWorld(feet[i],x,y,z,yaw);
  x+=dx;y+=dy;z+=dz;for(uint8_t i=0;i<4;++i)feet[i]=worldToBody(world[i],x,y,z,yaw);
}
inline float approach(float value,float target,float maxDelta) { float d=target-value;if(d>maxDelta)return value+maxDelta;if(d< -maxDelta)return value-maxDelta;return target; }
// One deterministic firmware/simulator gait frame. Foot anchors are world-fixed
// in stance; the next support tripod is preloaded before its leg lifts.
inline void advanceGaitFrame(Vec3 feet[4],Vec3 anchors[4],Vec3 swingStarts[4],Vec3 swingTargets[4],
    float footSpread[4],float &phase,float &bodyX,float &bodyY,float &bodyZ,float &bodyYaw,
    float &spread,float &forward,float &turn,float &minimumBefore,float &minimumAfter,
    float targetForward,float targetTurn,float targetBodyZ,float targetSpread,float dt,
    float stride,float lift,float turnDegrees,float period,float duty,const float offsets[4],
    float comX,float comY,float requestedMargin) {
  static const uint8_t order[4]={0,1,3,2};
  const float lead=.08f;
  forward=approach(forward,targetForward,2.0f*dt);turn=approach(turn,targetTurn,2.0f*dt);
  float nextZ=approach(bodyZ,targetBodyZ,8.0f*dt);translateBodyKeepingFeet(feet,bodyX,bodyY,bodyZ,bodyYaw,0,0,nextZ-bodyZ);
  spread=approach(spread,targetSpread,4.0f*dt);
  bool allStance=true;float until=2.0f;uint8_t nextLeg=0;
  for(uint8_t i=0;i<4;++i){float u=gaitPhaseAt(phase,offsets[i]);if(!gaitInStance(u,duty))allStance=false;else {float d=duty-u;if(d<until){until=d;nextLeg=i;}}}
  // Shift the projected COM into the upcoming three-foot polygon while all
  // four contacts still exist. Blend the correction over the lead interval.
  if(allStance && until<=lead+1e-6f && until>0){
    Vec3 world[4],tri[3];for(uint8_t i=0;i<4;++i)world[i]=bodyToWorld(feet[i],bodyX,bodyY,bodyZ,bodyYaw);
    uint8_t n=0;for(uint8_t k=0;k<4;++k)if(order[k]!=nextLeg)tri[n++]=world[order[k]];
    float c=cos(bodyYaw),s=sin(bodyYaw),cx=bodyX+c*comX-s*comY,cy=bodyY+s*comX+c*comY,dx=0,dy=0;
    if(balanceTranslation(tri,3,cx,cy,requestedMargin,dx,dy)){
      float f=fmin(1.0f,dt/fmax(dt,(until*period)));translateBodyKeepingFeet(feet,bodyX,bodyY,bodyZ,bodyYaw,dx*f,dy*f,0);
    }
  }
  const float old=phase,delta=dt/period,next=canonicalPhase(old+delta);
  Vec3 oldWorld[4];for(uint8_t i=0;i<4;++i)oldWorld[i]=bodyToWorld(feet[i],bodyX,bodyY,bodyZ,bodyYaw);
  const float originX=bodyX,originY=bodyY,originYaw=bodyYaw;
  float dYaw=turn*turnDegrees*delta*PI_F/180.0f,mid=originYaw+dYaw*.5f;
  float move=stride*forward*delta;bodyX-=sin(mid)*move;bodyY+=cos(mid)*move;bodyYaw+=dYaw;
  float fullYaw=turn*turnDegrees*PI_F/180.0f,fullMid=originYaw+fullYaw*.5f,fullMove=stride*forward;
  float tx=-sin(fullMid)*fullMove,ty=cos(fullMid)*fullMove,cs=cos(fullYaw),ss=sin(fullYaw);
  for(uint8_t i=0;i<4;++i){
    float u0=gaitPhaseAt(old,offsets[i]),u1=gaitPhaseAt(next,offsets[i]);bool st0=gaitInStance(u0,duty),st1=gaitInStance(u1,duty);
    if(st0&&!st1){swingStarts[i]=oldWorld[i];float rx=oldWorld[i].x-originX,ry=oldWorld[i].y-originY;swingTargets[i]={originX+tx+cs*rx-ss*ry,originY+ty+ss*rx+cs*ry,oldWorld[i].z};}
    else if(!st0&&st1){anchors[i]=swingTargets[i];footSpread[i]=spread;}
    if(st1)feet[i]=worldToBody(anchors[i],bodyX,bodyY,bodyZ,bodyYaw);
    else {float t=(u1-duty)/(1-duty),rad=hypot(swingTargets[i].x-bodyX,swingTargets[i].y-bodyY),rx=rad>1e-6f?(swingTargets[i].x-bodyX)/rad:0,ry=rad>1e-6f?(swingTargets[i].y-bodyY)/rad:0;Vec3 goal={swingTargets[i].x+rx*(spread-footSpread[i]),swingTargets[i].y+ry*(spread-footSpread[i]),swingTargets[i].z};feet[i]=worldToBody(gaitSwing(swingStarts[i],goal,t,lift),bodyX,bodyY,bodyZ,bodyYaw);}
  }
  phase=next;
  Vec3 world[4],support[4];uint8_t n=0;for(uint8_t k=0;k<4;++k){uint8_t i=order[k];world[i]=bodyToWorld(feet[i],bodyX,bodyY,bodyZ,bodyYaw);if(gaitInStance(gaitPhaseAt(phase,offsets[i]),duty))support[n++]=world[i];}
  float c=cos(bodyYaw),s=sin(bodyYaw),cx=bodyX+c*comX-s*comY,cy=bodyY+s*comX+c*comY,m=supportMargin(support,n,cx,cy);
  if(finite(m))minimumBefore=finite(minimumBefore)?fmin(minimumBefore,m):m;
  float dx=0,dy=0;if(balanceTranslation(support,n,cx,cy,requestedMargin,dx,dy))translateBodyKeepingFeet(feet,bodyX,bodyY,bodyZ,bodyYaw,dx,dy,0);
  for(uint8_t i=0;i<4;++i)world[i]=bodyToWorld(feet[i],bodyX,bodyY,bodyZ,bodyYaw);
  c=cos(bodyYaw);s=sin(bodyYaw);cx=bodyX+c*comX-s*comY;cy=bodyY+s*comX+c*comY;m=supportMargin(support,n,cx,cy);
  if(finite(m))minimumAfter=finite(minimumAfter)?fmin(minimumAfter,m):m;
}
inline bool solveFoot(uint8_t leg,Vec3 foot,float electrical[3]) { Angles a;return inverse(leg,foot,a)&&anglesToElectrical(leg,a,electrical); }
}
