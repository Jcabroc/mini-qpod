#include <cstdio>
#include <cstdlib>
#include "../../libraries/MiniQpodWalkCore/src/ik_walk_core.h"
#include "../../libraries/MiniQpodWalkCore/src/gait_presets.h"
#include "../../libraries/MiniQpodWalkCore/src/control_lite.h"
int main(int argc,char **argv) {
  if(argc==7 && argv[1][0]=='Q') {
    uint8_t leg=(uint8_t)atoi(argv[2]);IkWalk::Vec3 start={(float)atof(argv[3]),(float)atof(argv[4]),(float)atof(argv[5])};
    IkWalk::Vec3 p=IkWalk::readyFrame(start,IkWalk::readyFoot(leg),(float)atof(argv[6]));printf("READY %.7f %.7f %.7f\n",p.x,p.y,p.z);return 0;
  }
  if(argc==6 && argv[1][0]=='R') {
    PS2J_Packet p={};p.rx=(int8_t)atoi(argv[2]);p.ry=(int8_t)atoi(argv[3]);p.mode3=(uint8_t)atoi(argv[4]);p.flags=(uint8_t)atoi(argv[5]);
    ControlLiteAxes a=decodeControlLiteAxes(p);printf("RADIO %u %.7f %.7f\n",(unsigned)sizeof(p),a.forward,a.turn);return 0;
  }
  if((argc==20||argc==28) && argv[1][0]=='C') {
    int frames=atoi(argv[2]);float dt=(float)atof(argv[3]),stride=(float)atof(argv[4]),lift=(float)atof(argv[5]);
    float turnDeg=(float)atof(argv[6]),period=(float)atof(argv[7]),duty=(float)atof(argv[8]),offsets[4];
    for(uint8_t i=0;i<4;++i)offsets[i]=(float)atof(argv[9+i]);
    float targetForward=(float)atof(argv[13]),targetTurn=(float)atof(argv[14]),targetZ=(float)atof(argv[15]),targetSpread=(float)atof(argv[16]);
    float comX=(float)atof(argv[17]),comY=(float)atof(argv[18]),margin=(float)atof(argv[19]);
    IkWalk::Vec3 feet[4],anchors[4],starts[4],targets[4];float plantedSpread[4]={0,0,0,0};
    float phase=0,bx=0,by=0,bz=0,yaw=0,spread=0,forward=0,turn=0,minBefore=INFINITY,minAfter=INFINITY;
    for(uint8_t i=0;i<4;++i)feet[i]=anchors[i]=starts[i]=targets[i]=IkWalk::readyFoot(i);
    for(int k=0;k<frames;++k){
      float tf=targetForward,tt=targetTurn,tz=targetZ,ts=targetSpread;
      if(argc==28 && k>=atoi(argv[20])){tf=(float)atof(argv[21]);tt=(float)atof(argv[22]);tz=(float)atof(argv[23]);ts=(float)atof(argv[24]);}
      IkWalk::advanceGaitFrame(feet,anchors,starts,targets,plantedSpread,phase,bx,by,bz,yaw,spread,forward,turn,minBefore,minAfter,tf,tt,tz,ts,dt,stride,lift,turnDeg,period,duty,offsets,comX,comY,margin);
      uint8_t mask=0;for(uint8_t i=0;i<4;++i)if(!IkWalk::gaitInStance(IkWalk::gaitPhaseAt(phase,offsets[i]),duty))mask|=(1u<<i);
      printf("FRAME %d %.7f %.7f %.7f %.7f %.7f %.7f %.7f %.7f %u",k,phase,bx,by,bz,yaw,forward,turn,spread,mask);
      for(uint8_t i=0;i<4;++i){IkWalk::Vec3 w=IkWalk::bodyToWorld(feet[i],bx,by,bz,yaw);IkWalk::Angles a;float e[3];bool ok=IkWalk::inverse(i,feet[i],a)&&IkWalk::anglesToElectrical(i,a,e);printf(" %.7f %.7f %.7f %u",w.x,w.y,w.z,ok?1:0);if(ok)printf(" %.7f %.7f %.7f %.7f %.7f %.7f",a.yaw,a.femur,a.knee,e[0],e[1],e[2]);else printf(" 0 0 0 0 0 0");}
      printf(" %.7f %.7f\n",minBefore,minAfter);
    }return 0;
  }
  if(argc>=12 && argv[1][0]=='B') {
    uint8_t n=(uint8_t)atoi(argv[2]);if((n!=3&&n!=4)||argc!=(int)(6+2*n))return 2;
    IkWalk::Vec3 points[4];for(uint8_t i=0;i<n;++i)points[i]={(float)atof(argv[3+2*i]),(float)atof(argv[4+2*i]),0};
    float cx=(float)atof(argv[3+2*n]),cy=(float)atof(argv[4+2*n]),margin=(float)atof(argv[5+2*n]),dx=0,dy=0;
    IkWalk::balanceTranslation(points,n,cx,cy,margin,dx,dy);
    printf("BALANCE %.8f %.8f %.8f\n",dx,dy,IkWalk::supportMargin(points,n,cx+dx,cy+dy));return 0;
  }
  if(argc==3 && argv[1][0]=='D') {
    uint8_t i=(uint8_t)atoi(argv[2]);if(i>=sizeof(IkWalk::gaitPresets)/sizeof(IkWalk::gaitPresets[0]))return 2;
    const IkWalk::GaitPreset&p=IkWalk::gaitPresets[i];
    printf("PRESET %s %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f\n",p.key,p.stride,p.lift,p.turn,p.period,p.duty,p.startPhase,p.offset[0],p.offset[1],p.offset[2],p.offset[3],IkWalk::defaultComX,IkWalk::defaultComY,IkWalk::defaultComMargin,p.bodyHeight,p.opening);return 0;
  }
  if(argc==8 && argv[1][0]=='T') {
    IkWalk::Vec3 s={(float)atof(argv[2]),(float)atof(argv[3]),(float)atof(argv[4])};
    IkWalk::Vec3 p=IkWalk::stepTarget(s,(uint8_t)atoi(argv[5]),(int8_t)atoi(argv[6]),(int8_t)atoi(argv[7]));
    printf("TARGET %.8f %.8f %.8f\n",p.x,p.y,p.z); return 0;
  }
  if(argc==10 && argv[1][0]=='G') {
    IkWalk::Vec3 a={(float)atof(argv[2]),(float)atof(argv[3]),(float)atof(argv[4])};
    IkWalk::Vec3 b={(float)atof(argv[5]),(float)atof(argv[6]),(float)atof(argv[7])};
    IkWalk::Vec3 p=IkWalk::gaitSwing(a,b,(float)atof(argv[8]),(float)atof(argv[9]));
    printf("GAIT %.8f %.8f %.8f\n",p.x,p.y,p.z); return 0;
  }
  if(argc==5 && argv[1][0]=='P') {
    float p=IkWalk::gaitPhaseAt((float)atof(argv[2]),(float)atof(argv[3]));
    printf("PHASE %.8f %d\n",p,IkWalk::gaitInStance(p,(float)atof(argv[4]))?1:0); return 0;
  }
  if(argc!=5)return 2;
  int leg=atoi(argv[1]); IkWalk::Vec3 p={(float)atof(argv[2]),(float)atof(argv[3]),(float)atof(argv[4])};
  IkWalk::Angles a; float e[3];
  if(!IkWalk::inverse((uint8_t)leg,p,a)){puts("IK_REJECT");return 0;}
  if(!IkWalk::anglesToElectrical((uint8_t)leg,a,e)){printf("LIMIT_REJECT %.8f %.8f %.8f\n",a.yaw,a.femur,a.knee);return 0;}
  printf("OK %.8f %.8f %.8f %.8f %.8f %.8f\n",a.yaw,a.femur,a.knee,e[0],e[1],e[2]);
}
