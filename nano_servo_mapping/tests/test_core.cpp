#include "../mapping_core.h"
#include <assert.h>
#include <stdio.h>
using namespace Mapping;
struct Fake {
  int outputs[13];bool broken=false;
  Fake(){for(int &x:outputs)x=0;}
  bool allOff(){for(int &x:outputs)x=0;return !broken;}
  bool write(int ch,int value){if(broken)return false;outputs[ch]=value;return true;}
};
Request request(const char *s){char text[100];strcpy(text,s);return parse(text);}
void fresh(Controller<Fake>&c,uint32_t now){c.imu.seen=true;c.imu.last=now;c.imu.roll=c.imu.pitch=0;}
bool frame(ImuState &imu,const char *body,uint32_t now){
  char line[100];unsigned crc=0;for(const char*p=body;*p;++p)crc^=(unsigned char)*p;
  snprintf(line,sizeof(line),"%s*%02X",body,crc);return imu.accept(line,now);
}
int main(){
  const char *invalid[]={"ARM 0 nan","ARM 0 inf","ARM 0 9x","ARM 0 1e2","ARM 0","SELECT 0 90","PING extra","SAVE","EXPAND MIN 1","MOVE -1 90","STATUS x"};
  for(const char*s:invalid)assert(request(s).command==INVALID);
  assert(request("ARM 0 90.5").angle==90.5f);
  assert(request("SELECT 12").channel==12);
  LineBuffer line;for(int i=0;i<100;++i)line.feed('a');assert(line.feed('\n')==-1);
  for(char c: "STATUS")if(c)line.feed(c);assert(line.feed('\n')==1);assert(request(line.data).command==STATUS);
  LineBuffer endings;
  for(char c: "PING")if(c)endings.feed(c);assert(endings.feed('\r')==1);assert(request(endings.data).command==PING);
  for(char c: "PING")if(c)endings.feed(c);assert(endings.feed('\r')==1);assert(endings.feed('\n')==0);
  ImuState imu;
  assert(frame(imu,"IMU,1,0.00,-1.00",0));assert(imu.healthy(249));assert(!imu.healthy(250));
  assert(!frame(imu,"IMU,1,0,0",240));assert(!imu.healthy(250));
  assert(!frame(imu,"IMU,2,nan,0",250));assert(!frame(imu,"IMU,2,0,0,extra",250));
  char bad[]="IMU,2,0,0*GG";assert(!imu.accept(bad,250));
  assert(frame(imu,"IMU,65535,0,0",260));assert(frame(imu,"IMU,0,0,0",280));
  for(int ch=0;ch<13;++ch){
    Fake f;Controller<Fake> c(f);c.off();fresh(c,0);
    assert(c.execute({ARM,ch,90},0)==NOT_SELECTED);
    assert(c.execute({SELECT,ch,0},0)==OK);for(int x:f.outputs)assert(x==0);
    float lo=calibration[ch].low+5,hi=calibration[ch].high-5;
    assert(c.execute({ARM,ch,lo-0.01f},0)==OUT_OF_RANGE);assert(c.active==-1);
    assert(c.execute({ARM,ch,lo},0)==OK);
    for(int i=0;i<13;++i)assert((f.outputs[i]!=0)==(i==ch));
    assert(c.execute({ARM,ch,hi},0)==ALREADY_ARMED);
    float old=c.target;int pulse=f.outputs[ch];
    assert(c.execute({MOVE,(ch+1)%13,lo},0)==NOT_SELECTED);
    assert(c.execute(request("MOVE 0 nan"),0)==BAD_SYNTAX);
    assert(c.target==old && f.outputs[ch]==pulse);
    assert(c.execute({MOVE,ch,hi+0.01f},0)==OUT_OF_RANGE);assert(c.target==old && f.outputs[ch]==pulse);
    assert(c.execute({MOVE,ch,hi},0)==OK);c.tick(20);assert(fabs(c.current-lo-0.2f)<0.001f);
    fresh(c,900);c.tick(900);assert(c.current<=lo+0.401f);
    fresh(c,1000);c.tick(1000);assert(c.active==-1 && c.fault==HOST_TIMEOUT);for(int x:f.outputs)assert(!x);
    assert(c.execute({PING,-1,0},1000)==OK);assert(c.active==-1);
    fresh(c,1001);assert(c.execute({SELECT,ch,0},1001)==OK);
    assert(c.execute({ARM,ch,hi},1001)==OK); // upper boundary is inclusive
    assert(c.execute({MOVE,ch,lo-0.01f},1001)==OUT_OF_RANGE);
    assert(c.target==hi && c.current==hi);
  }
  const Fault faults[]={IMU_LOST,TILT};
  for(Fault expected:faults){
    Fake f;Controller<Fake> c(f);fresh(c,0);c.execute({SELECT,0,0},0);c.execute({ARM,0,90},0);
    if(expected==TILT)c.imu.roll=12.01f;
    c.tick(expected==TILT?20:250);assert(c.active==-1 && c.fault==expected);
  }
  const Command stops[]={OFF,X,SELECT};
  for(Command stop:stops){
    Fake f;Controller<Fake> c(f);fresh(c,0);c.execute({SELECT,0,0},0);c.execute({ARM,0,90},0);
    c.execute({stop,1,0},1);for(int x:f.outputs)assert(x==0);assert(c.active==-1);
  }
  Fake f;Controller<Fake> c(f);c.execute({SELECT,0,0},0);
  assert(c.execute({ARM,0,90},0)==UNSAFE_IMU);
  fresh(c,0);c.imu.pitch=13;assert(c.execute({ARM,0,90},0)==UNSAFE_IMU);
  fresh(c,0);f.broken=true;assert(c.execute({ARM,0,90},0)==HARDWARE_ERROR);assert(c.active==-1);
  f.broken=false;assert(c.execute({ARM,0,90},0)==HARDWARE_ERROR);
  assert(c.execute({STATUS,-1,0},0)==OK);c.execute({OFF,-1,0},0);for(int x:f.outputs)assert(!x);
  assert(counts(90)==310 && counts(110)==354 && counts(90.5f)==312);
  Fake wrapSink;Controller<Fake> wrap(wrapSink);
  uint32_t start=UINT32_MAX-400;
  fresh(wrap,start);wrap.execute({SELECT,0,0},start);wrap.execute({ARM,0,90},start);
  // Health and watchdog use unsigned subtraction across millis wrap.
  for(uint32_t dt=200;dt<=2000;dt+=200){
    fresh(wrap,start+dt);assert(wrap.execute({PING,-1,0},start+dt)==OK);
    wrap.tick(start+dt);assert(wrap.active==0);
  }
  fresh(wrap,start+2999);wrap.tick(start+2999);assert(wrap.active==0);
  fresh(wrap,start+3000);wrap.tick(start+3000);assert(wrap.fault==HOST_TIMEOUT && wrap.active==-1);
  ImuState wrapImu;wrapImu.seen=true;wrapImu.last=UINT32_MAX-100;
  assert(wrapImu.healthy(148));assert(!wrapImu.healthy(149));
  puts("mapping core: parser, frames, 13 channels, ramp, atomic rejection, IMU, watchdog, OFF/X, bus failure OK");
}
