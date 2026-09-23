#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

namespace Mapping {
struct Calibration { int16_t low, center, high; int8_t direction; };
static const Calibration calibration[13] = {
  {20,90,115,1},{10,110,180,1},{25,90,180,-1},
  {90,110,180,1},{10,90,180,-1},{0,90,170,-1},
  {70,100,180,1},{10,90,180,1},{0,100,160,-1},
  {30,100,140,-1},{10,105,175,-1},{40,115,180,-1},
  {75,95,120,1}
};
enum Command { INVALID, HELP, STATUS, CONFIG, IMU, SELECT, ARM, MOVE, PING, OFF, X };
enum Fault { NONE, IMU_LOST, TILT, HOST_TIMEOUT, BUS_ERROR };
enum Result { OK, BAD_SYNTAX, BAD_CHANNEL, NOT_SELECTED, ALREADY_ARMED,
              NOT_ARMED, OUT_OF_RANGE, UNSAFE_IMU, HARDWARE_ERROR };
struct Request { Command command; int channel; float angle; };

inline bool number(const char *s, float &value) {
  if (!s || !*s) return false;
  const char *p=s;
  if (*p=='+' || *p=='-') ++p;
  bool digit=false;
  while (*p>='0' && *p<='9') { digit=true; ++p; }
  if (*p=='.') { ++p; while (*p>='0' && *p<='9') { digit=true; ++p; } }
  if (!digit || *p) return false;
  value=(float)atof(s);
  return isfinite(value) && fabs(value)<=1000000.0f;
}
inline Request parse(char *line) {
  Request r={INVALID,-1,0};
  char *save=0;
  char *name=strtok_r(line," \t",&save);
  if (!name) return r;
  const char *names[]={"HELP","STATUS","CONFIG","IMU","SELECT","ARM","MOVE","PING","OFF","X"};
  for (int i=0;i<10;++i) if (!strcmp(name,names[i])) r.command=(Command)(i+1);
  if (r.command==INVALID) return r;
  char *a=strtok_r(0," \t",&save), *b=strtok_r(0," \t",&save), *extra=strtok_r(0," \t",&save);
  bool channelCommand=r.command==SELECT || r.command==ARM || r.command==MOVE;
  if (!channelCommand) { if(a) r.command=INVALID; return r; }
  if (!a || !*a || strlen(a)>2) {r.command=INVALID;return r;}
  for(const char *p=a;*p;++p) if(*p<'0'||*p>'9') {r.command=INVALID;return r;}
  r.channel=atoi(a);
  if (extra || (r.command==SELECT ? b!=0 : !number(b,r.angle))) r.command=INVALID;
  return r;
}

// Discard entire oversized/corrupt frames, never parse their suffix.
struct LineBuffer {
  char data[80]; uint8_t used; bool discard;
  LineBuffer():used(0),discard(false) {data[0]=0;}
  int feed(char c) {
    if(c=='\r' || c=='\n') {
      int result=discard ? -1 : (used ? 1 : 0);
      data[used]=0; used=0; discard=false; return result;
    }
    if((unsigned char)c<32 && c!='\t') discard=true;
    if(!discard) {if(used<sizeof(data)-1) data[used++]=c; else discard=true;}
    return 0;
  }
};

struct ImuState {
  bool seen; uint16_t sequence; uint32_t last; float roll,pitch;
  ImuState():seen(false),sequence(0),last(0),roll(0),pitch(0) {}
  bool healthy(uint32_t now) const {return seen && (uint32_t)(now-last)<250;}
  bool level() const {return fabs(roll)<=12 && fabs(pitch)<=12;}
  bool accept(char *line,uint32_t now) {
    if(strncmp(line,"IMU,",4)) return false;
    char *star=strchr(line,'*');
    if(!star || strlen(star+1)!=2) return false;
    uint8_t checksum=0;
    for(char *p=line;p<star;++p) checksum^=(uint8_t)*p;
    unsigned received=0;
    for(char *p=star+1;*p;++p) {
      char c=*p; int n=(c>='0'&&c<='9')?c-'0':(c>='A'&&c<='F')?c-'A'+10:(c>='a'&&c<='f')?c-'a'+10:-1;
      if(n<0) return false; received=received*16+n;
    }
    if(received!=checksum) return false;
    *star=0;
    char *seq=line+4,*comma=strchr(seq,','); if(!comma) return false; *comma=0;
    char *r=comma+1; comma=strchr(r,','); if(!comma) return false; *comma=0;
    char *p=comma+1;
    if(!*seq || strlen(seq)>5) return false;
    for(char *q=seq;*q;++q) if(*q<'0'||*q>'9') return false;
    unsigned long next=strtoul(seq,0,10);
    float rr,pp;
    if(next>65535 || !number(r,rr) || !number(p,pp) || fabs(rr)>180 || fabs(pp)>180) return false;
    if(seen && next==sequence) return false; // duplicates do not renew freshness
    sequence=(uint16_t)next;roll=rr;pitch=pp;last=now;seen=true;return true;
  }
};

inline uint16_t counts(float angle) {
  int a=(int)(angle+0.5f);
  return (uint16_t)(110+(long)a*400/180);
}
// Sink must implement bool allOff() and bool write(channel, counts).
template<class Sink> class Controller {
 public:
  Sink &sink; ImuState imu;
  int selected,active; float target,current; uint32_t heartbeat,lastRamp; Fault fault;
  Controller(Sink &s):sink(s),selected(-1),active(-1),target(0),current(0),heartbeat(0),lastRamp(0),fault(NONE) {}
  void off() {active=-1;selected=-1;if(!sink.allOff()) fault=BUS_ERROR;}
  void abort(Fault f) {fault=f;off();}
  void guard(uint32_t now) {
    if(active<0)return;
    if(!imu.healthy(now)) abort(IMU_LOST);
    else if(!imu.level()) abort(TILT);
    else if((uint32_t)(now-heartbeat)>=1000) abort(HOST_TIMEOUT);
  }
  Result execute(const Request &r,uint32_t now) {
    guard(now);
    if(r.command==INVALID) return BAD_SYNTAX;
    if(r.command==OFF || r.command==X) {off();return fault==BUS_ERROR?HARDWARE_ERROR:OK;}
    if(r.command==PING) {heartbeat=now;return OK;}
    if(r.command!=SELECT && r.command!=ARM && r.command!=MOVE) return OK;
    if(fault==BUS_ERROR) return HARDWARE_ERROR; // latched until reboot; queries/OFF remain available
    if(r.channel<0 || r.channel>=13) return BAD_CHANNEL;
    if(r.command==SELECT) {off(); if(fault==BUS_ERROR)return HARDWARE_ERROR;selected=r.channel;return OK;}
    if(selected!=r.channel) return NOT_SELECTED;
    if(!isfinite(r.angle) || r.angle<calibration[r.channel].low+5 || r.angle>calibration[r.channel].high-5) return OUT_OF_RANGE;
    if(!imu.healthy(now) || !imu.level()) return UNSAFE_IMU;
    if(r.command==ARM) {
      if(active>=0) return ALREADY_ARMED;
      if(!sink.allOff()) {fault=BUS_ERROR;return HARDWARE_ERROR;}
      if(!sink.write(r.channel,counts(r.angle))) {abort(BUS_ERROR);return HARDWARE_ERROR;}
      active=r.channel;current=target=r.angle;heartbeat=lastRamp=now;return OK;
    }
    if(active!=r.channel)return NOT_ARMED;
    target=r.angle;return OK; // Only PING and ARM establish/renew host heartbeat.
  }
  void tick(uint32_t now) {
    guard(now);if(active<0)return;
    uint32_t dt=now-lastRamp;if(dt<20)return;
    lastRamp=now;if(dt>20)dt=20; // delayed loop never catches up with a large jump
    float step=10.0f*dt/1000.0f, delta=target-current;
    float next=current+(delta>step?step:delta < -step?-step:delta);
    if(!sink.write(active,counts(next))) {abort(BUS_ERROR);return;}current=next;
  }
};
}
