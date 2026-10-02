#pragma once

#include <Wire.h>
#include <SPI.h>
#include <RF24.h>
#include <Adafruit_PWMServoDriver.h>
#include "ik_walk_core.h"
#include "gait_presets.h"
#include "ps2j_packet.h"
#include "control_lite.h"

// The board wrapper owns the physical buses and declares these peripherals.
extern Adafruit_PWMServoDriver pca;
extern RF24 radio;
const byte RADIO_ADDRESS[6]="JR001";
const uint16_t UPDATE_MS=40;                 // 25 Hz
const float READY_DURATION_S=1.2f;
const uint32_t NRF_FAILSAFE_MS=500;

enum Motion { IDLE, READYING, GAIT };
Motion motion=IDLE;
IkWalk::Vec3 feet[IkWalk::LEG_COUNT], readyTarget[IkWalk::LEG_COUNT], readyStart[IkWalk::LEG_COUNT];
IkWalk::Vec3 anchors[IkWalk::LEG_COUNT], swingStarts[IkWalk::LEG_COUNT], swingTargets[IkWalk::LEG_COUNT];
float footSpread[4]={0,0,0,0};
float gaitPhase=0, bodyX=0, bodyY=0, bodyZ=0, bodyYaw=0, gaitSpread=IkWalk::gaitPresets[0].opening, targetSpread=IkWalk::gaitPresets[0].opening, targetBodyZ=IkWalk::gaitPresets[0].bodyHeight;
float walkForward=0,walkTurn=0,targetForward=0,targetTurn=0,minimumMarginBefore=INFINITY,minimumMarginAfter=INFINITY;
float readyElapsed=0;
float GAIT_STRIDE_MM=IkWalk::gaitPresets[0].stride, GAIT_LIFT_MM=IkWalk::gaitPresets[0].lift;
float GAIT_TURN_DEG=IkWalk::gaitPresets[0].turn, GAIT_PERIOD_S=IkWalk::gaitPresets[0].period;
float GAIT_DUTY=IkWalk::gaitPresets[0].duty, GAIT_START_PHASE=IkWalk::gaitPresets[0].startPhase;
float GAIT_OFFSETS[4]={0,.5f,.75f,.25f};
float GAIT_COM_X_MM=IkWalk::defaultComX, GAIT_COM_Y_MM=IkWalk::defaultComY;
float GAIT_COM_MARGIN_MM=IkWalk::defaultComMargin;
char gaitPattern[8]="WALK";
uint32_t lastUpdate=0,lastRadio=0;
bool pwmEnabled=false;
char serialLine[32]; uint8_t serialUsed=0;
uint16_t previousRadioButtons=0;
bool deferredReady=false;
bool radioPaused=false;
bool singleFlightOnly=false;
uint8_t singleFlightLeg=0;
float savedGaitOffsets[4],savedGaitDuty=0;

uint16_t pwmCounts(float degrees) { return (uint16_t)(110.0f+degrees*400.0f/180.0f+0.5f); }
void restoreSingleStep() { if(!singleFlightOnly)return;for(uint8_t i=0;i<4;++i)GAIT_OFFSETS[i]=savedGaitOffsets[i];GAIT_DUTY=savedGaitDuty;singleFlightOnly=false; }
void pwmOff() { for(uint8_t ch=0;ch<16;++ch)pca.setPWM(ch,0,4096);pca.setPWM(12,0,4096);restoreSingleStep();pwmEnabled=false;motion=IDLE;targetForward=targetTurn=walkForward=walkTurn=0;deferredReady=false;radioPaused=false; }
bool setHeightTarget(float mm){if(!isfinite(mm)||mm< -12||mm>12)return false;targetBodyZ=mm;return true;}
bool setOpeningTarget(float mm){if(!isfinite(mm)||mm<0||mm>12)return false;targetSpread=mm;return true;}
bool writeFeet() {
  float command[IkWalk::CHANNELS];
  for(uint8_t leg=0;leg<4;++leg) { float a[3]; if(!IkWalk::solveFoot(leg,feet[leg],a))return false; for(uint8_t j=0;j<3;++j)command[leg*3+j]=a[j]; }
  for(uint8_t ch=0;ch<IkWalk::CHANNELS;++ch)pca.setPWM(ch,0,pwmCounts(command[ch]));
  pca.setPWM(12,0,4096); // Neck is always disabled.
  pwmEnabled=true; return true;
}
bool beginReady() {
  if(motion==GAIT){targetForward=targetTurn=0;deferredReady=true;return true;}
  for(uint8_t i=0;i<4;++i){readyStart[i]=feet[i];readyTarget[i]=IkWalk::readyFoot(i);}
  readyElapsed=0;motion=READYING; return true;
}
IkWalk::Vec3 toWorld(IkWalk::Vec3 p) {
  float c=cos(bodyYaw),s=sin(bodyYaw);
  return {bodyX+c*p.x-s*p.y,bodyY+s*p.x+c*p.y,bodyZ+p.z};
}
IkWalk::Vec3 toBody(IkWalk::Vec3 p) {
  float c=cos(bodyYaw),s=sin(bodyYaw),x=p.x-bodyX,y=p.y-bodyY;
  return {c*x+s*y,-s*x+c*y,p.z-bodyZ};
}
void beginGait() {
  gaitPhase=GAIT_START_PHASE;minimumMarginBefore=minimumMarginAfter=INFINITY;
  for(uint8_t i=0;i<4;++i) { anchors[i]=toWorld(feet[i]); swingStarts[i]=swingTargets[i]=anchors[i];footSpread[i]=gaitSpread; }
  motion=GAIT;
}
void beginSingleStep(uint8_t leg) {
  for(uint8_t i=0;i<4;++i)savedGaitOffsets[i]=GAIT_OFFSETS[i];savedGaitDuty=GAIT_DUTY;
  GAIT_DUTY=.85f;uint8_t slot=1;for(uint8_t i=0;i<4;++i)GAIT_OFFSETS[i]=i==leg?0.0f:.25f*slot++;
  singleFlightLeg=leg;singleFlightOnly=true;targetForward=1;targetTurn=0;beginGait();gaitPhase=.77f;
}
bool loadGaitPreset(const char *key) {
  for(uint8_t i=0;i<sizeof(IkWalk::gaitPresets)/sizeof(IkWalk::gaitPresets[0]);++i) {
    const IkWalk::GaitPreset &p=IkWalk::gaitPresets[i];
    if(strcmp(key,p.key))continue;
    GAIT_STRIDE_MM=p.stride;GAIT_LIFT_MM=p.lift;GAIT_TURN_DEG=p.turn;
    GAIT_PERIOD_S=p.period;GAIT_DUTY=p.duty;GAIT_START_PHASE=p.startPhase;
    targetBodyZ=p.bodyHeight;targetSpread=p.opening;
    for(uint8_t j=0;j<4;++j)GAIT_OFFSETS[j]=p.offset[j];
    strncpy(gaitPattern,p.key,sizeof(gaitPattern)-1);gaitPattern[sizeof(gaitPattern)-1]=0;
    return true;
  }
  return false;
}
void advanceGait(float dt) {
  float old=gaitPhase;
  if(singleFlightOnly && !IkWalk::gaitInStance(IkWalk::gaitPhaseAt(gaitPhase,GAIT_OFFSETS[singleFlightLeg]),GAIT_DUTY))targetForward=0;
  IkWalk::advanceGaitFrame(feet,anchors,swingStarts,swingTargets,footSpread,gaitPhase,bodyX,bodyY,bodyZ,bodyYaw,
      gaitSpread,walkForward,walkTurn,minimumMarginBefore,minimumMarginAfter,targetForward,targetTurn,targetBodyZ,targetSpread,dt,
      GAIT_STRIDE_MM,GAIT_LIFT_MM,GAIT_TURN_DEG,GAIT_PERIOD_S,GAIT_DUTY,GAIT_OFFSETS,GAIT_COM_X_MM,GAIT_COM_Y_MM,GAIT_COM_MARGIN_MM);
  if(!writeFeet()) { pwmOff(); Serial.println(F("ERR GAIT_IK_OR_LIMIT")); }
  bool anyFlight=false;for(uint8_t i=0;i<4;++i)if(!IkWalk::gaitInStance(IkWalk::gaitPhaseAt(gaitPhase,GAIT_OFFSETS[i]),GAIT_DUTY))anyFlight=true;
  if(singleFlightOnly && !anyFlight && fabs(walkForward)<.02f && fabs(walkTurn)<.02f){restoreSingleStep();motion=IDLE;if(deferredReady){deferredReady=false;beginReady();}}
  else if(gaitPhase<old && fabs(targetForward)<.01f && fabs(targetTurn)<.01f && fabs(walkForward)<.02f && fabs(walkTurn)<.02f && !anyFlight){motion=IDLE;if(deferredReady){deferredReady=false;beginReady();}}
}
void advanceMotion(float dt) {
  if(motion==IDLE)return;
  if(motion==GAIT) { advanceGait(dt); return; }
  if(motion==READYING) {
    readyElapsed=fmin(READY_DURATION_S,readyElapsed+dt);float t=readyElapsed/READY_DURATION_S,s=t*t*(3-2*t);
    for(uint8_t leg=0;leg<4;++leg)feet[leg]=IkWalk::readyFrame(readyStart[leg],readyTarget[leg],t);
    if(!writeFeet()){pwmOff();Serial.println(F("ERR READY_IK"));return;} if(t>=1){motion=IDLE;Serial.println(F("OK READY"));} return;
  }
}
void command(const char *line) {
  if(!strcmp(line,"HELP")){Serial.println(F("HELP: READY | STEP 0..3 | CYCLE | PATTERN WALK/WAVE/TROT | FORWARD/BACKWARD/LEFT/RIGHT/FORWARD_LEFT/FORWARD_RIGHT/BACKWARD_LEFT/BACKWARD_RIGHT | HEIGHT mm | OPENING mm | PAUSE | STOP | X"));return;}
  if(!strncmp(line,"PATTERN ",8)){
    const char *p=line+8;
    if(motion!=IDLE){Serial.println(F("ERR STOP_BEFORE_PATTERN"));return;}
    if(!loadGaitPreset(p)){Serial.println(F("ERR PATTERN"));return;}
    Serial.print(F("OK PATTERN "));Serial.println(gaitPattern);return;
  }
  if(!strcmp(line,"X")){pwmOff();Serial.println(F("OK OFF"));return;}
  if(!strcmp(line,"STOP")||!strcmp(line,"PAUSE")){targetForward=targetTurn=0;if(motion!=GAIT)motion=IDLE;Serial.println(F("OK STOPPING"));return;}
  if(!strncmp(line,"HEIGHT ",7)){if(!setHeightTarget(atof(line+7))){Serial.println(F("ERR HEIGHT_RANGE"));return;}Serial.println(F("OK HEIGHT"));return;}
  if(!strncmp(line,"OPENING ",8)){if(!setOpeningTarget(atof(line+8))){Serial.println(F("ERR OPENING_RANGE"));return;}Serial.println(F("OK OPENING"));return;}
  if(!strcmp(line,"READY")){beginReady();Serial.println(F("OK READYING"));return;}
  if(!strncmp(line,"STEP ",5) && line[6]==0 && line[5]>='0' && line[5]<='3'){
    if(!pwmEnabled){Serial.println(F("ERR READY_REQUIRED"));return;}
    if(motion!=IDLE){Serial.println(F("ERR STOP_BEFORE_STEP"));return;}
    beginSingleStep((uint8_t)(line[5]-'0'));Serial.println(F("OK STEP"));return;
  }
  if(!strcmp(line,"CYCLE")||!strcmp(line,"FORWARD")||!strcmp(line,"BACKWARD")||!strcmp(line,"LEFT")||!strcmp(line,"RIGHT")||!strcmp(line,"FORWARD_LEFT")||!strcmp(line,"FORWARD_RIGHT")||!strcmp(line,"BACKWARD_LEFT")||!strcmp(line,"BACKWARD_RIGHT")){
    if(!pwmEnabled){Serial.println(F("ERR READY_REQUIRED"));return;}
    targetForward=(!strcmp(line,"BACKWARD")||!strcmp(line,"BACKWARD_LEFT")||!strcmp(line,"BACKWARD_RIGHT"))?-1.0f:((!strcmp(line,"LEFT")||!strcmp(line,"RIGHT"))?0.0f:1.0f);
    targetTurn=(!strcmp(line,"LEFT")||!strcmp(line,"FORWARD_LEFT")||!strcmp(line,"BACKWARD_LEFT"))?1.0f:((!strcmp(line,"RIGHT")||!strcmp(line,"FORWARD_RIGHT")||!strcmp(line,"BACKWARD_RIGHT"))?-1.0f:0.0f);
    if(!strcmp(line,"CYCLE"))targetForward=1;
    if(motion!=GAIT)beginGait();Serial.println(F("OK GAIT"));return;
  }
  Serial.println(F("ERR COMMAND"));
}
bool radioButtonEdge(const PS2J_Packet &p,uint16_t mask) {
  return (p.btn&mask) && !(previousRadioButtons&mask);
}
void handleRadioPacket(const PS2J_Packet &p) {
  if(!(p.flags&PS2J_FLAG_LITE))return;
  lastRadio=millis();
  bool select=radioButtonEdge(p,PS2J_SELECT),start=radioButtonEdge(p,PS2J_START),pause=radioButtonEdge(p,PS2J_CIRCLE);
  if(select)pwmOff();
  else if(start){radioPaused=false;beginReady();}
  else if(pause){radioPaused=!radioPaused;if(radioPaused){targetForward=targetTurn=0;if(motion!=GAIT)motion=IDLE;}}
  else if(pwmEnabled && motion!=READYING && !singleFlightOnly) {
    if(radioPaused)targetForward=targetTurn=0;
    else {
      if(motion==GAIT){
        if(radioButtonEdge(p,PS2J_TRIANGLE))setHeightTarget(targetBodyZ+4);
        if(radioButtonEdge(p,PS2J_CROSS))setHeightTarget(targetBodyZ-4);
        if(radioButtonEdge(p,PS2J_SQUARE))setOpeningTarget(targetSpread+3);
        if(radioButtonEdge(p,PS2J_R3))setOpeningTarget(targetSpread-3);
      }
      const ControlLiteAxes axes=decodeControlLiteAxes(p);targetForward=axes.forward;targetTurn=axes.turn;
      if((fabs(targetForward)>.01f||fabs(targetTurn)>.01f)&&motion!=GAIT)beginGait();
    }
  }
  previousRadioButtons=p.btn;
}
void receiveRadio() {
  while(radio.available()) {
    // Control Lite uses RF24's default fixed 32-byte payload mode; its 9-byte
    // struct is followed by padding. read() copies these 9 fields and drains it.
    PS2J_Packet packet={};radio.read(&packet,sizeof(packet));handleRadioPacket(packet);
  }
}
void setupWalkRuntime() {
  Serial.begin(115200);pca.begin();pwmOff();pca.setOscillatorFrequency(25000000);pca.setPWMFreq(50);pwmOff();
  radio.begin();radio.setChannel(76);radio.setDataRate(RF24_250KBPS);radio.openReadingPipe(1,RADIO_ADDRESS);radio.startListening();
  for(uint8_t i=0;i<4;++i)feet[i]=IkWalk::readyFoot(i);
  Serial.println(F("MINI_QPOD_IK_WALK_MVP PWM=OFF UPDATE=25HZ"));
}
void loopWalkRuntime() {
  receiveRadio(); uint32_t now=millis();
  if(lastRadio && (uint32_t)(now-lastRadio)>NRF_FAILSAFE_MS && (fabs(targetForward)>.01f||fabs(targetTurn)>.01f)){
    targetForward=targetTurn=0;Serial.println(F("FAILSAFE NRF STOPPING"));
  }
  while(Serial.available()){
    char c=(char)Serial.read();
    if(c=='X'){pwmOff();serialUsed=0;Serial.println(F("OK OFF"));continue;}
    if(c=='\r'||c=='\n'){
      uint8_t first=0,last=serialUsed;
      while(first<last && serialLine[first]==' ')++first;
      while(last>first && serialLine[last-1]==' ')--last;
      if(last>first){
        serialLine[last]=0; char *line=serialLine+first;
        // Some USB/TTL adapters echo the Nano's own banner/replies to RX.
        // They are protocol output, not operator commands, and must be silent.
        if(strncmp(line,"MINI_QPOD_",10) && strncmp(line,"OK ",3) &&
           strncmp(line,"ERR ",4) && strncmp(line,"HELP:",5) &&
           strncmp(line,"FAILSAFE ",9))command(line);
      }
      serialUsed=0;
    } else if((uint8_t)c<32 || (uint8_t)c>126) {
      // Reset/boot noise is never a command frame.
    } else if(serialUsed<sizeof(serialLine)-1)serialLine[serialUsed++]=c;
    else serialUsed=0;
  }
  if((uint32_t)(now-lastUpdate)>=UPDATE_MS){lastUpdate=now;advanceMotion(UPDATE_MS/1000.0f);}
}
