#include <Wire.h>
#include <SoftwareSerial.h>
#include <Adafruit_PWMServoDriver.h>
#include "mapping_core.h"

#ifndef MAPPING_BUILD_ID
#define MAPPING_BUILD_ID "local-" __DATE__ "-" __TIME__
#endif

Adafruit_PWMServoDriver pca(0x40);
SoftwareSerial pico(2,8); // GP4 -> D2; D8 TX MUST remain physically unconnected.
struct PwmSink {
  bool allOff() {
    bool ok=true;
    for(uint8_t ch=0;ch<13;++ch) if(pca.setPWM(ch,0,4096)!=0) ok=false;
    digitalWrite(5,LOW);return ok;
  }
  bool write(uint8_t ch,uint16_t value) {bool ok=pca.setPWM(ch,0,value)==0;digitalWrite(5,ok?HIGH:LOW);return ok;}
} sink;
Mapping::Controller<PwmSink> controller(sink);
Mapping::LineBuffer hostLine,imuLine;
uint8_t prescale=121;

void fieldAngle(float angle) {
  Serial.print(angle,2);Serial.print(F(" counts="));Serial.print(Mapping::counts(angle));
  Serial.print(F(" us_nominal="));Serial.print(Mapping::counts(angle)*0.04f*(prescale+1),2);
}
void response(const char *cmd, int ch, int result, bool accepted, float angle) {
  Serial.print(F("REPLY cmd="));Serial.print(cmd);Serial.print(F(" ch="));Serial.print(ch);
  Serial.print(F(" result="));Serial.print(result);Serial.print(F(" accepted="));
  if(accepted) fieldAngle(angle);else Serial.print(F("NA counts=NA us_nominal=NA"));
  Serial.println();
}
void status() {
  response("STATUS",controller.active,0,controller.active>=0,controller.target);
  Serial.print(F("BUILD name=MINI_QPOD_SERVO_MAPPING version=1.0.0 id="));
  Serial.println(F(MAPPING_BUILD_ID));
  Serial.print(F("STATE pwm="));
  if(controller.fault==Mapping::BUS_ERROR)Serial.print(F("UNKNOWN_BUS_ERROR"));
  else Serial.print(controller.active>=0?F("ACTIVE"):F("OFF"));
  Serial.print(F(" selected="));Serial.print(controller.selected);
  Serial.print(F(" energized="));Serial.print(controller.active);
  Serial.print(F(" target="));if(controller.active>=0) Serial.print(controller.target,2);else Serial.print(F("NA"));
  Serial.print(F(" current="));if(controller.active>=0)fieldAngle(controller.current);else Serial.print(F("NA counts=NA us_nominal=NA"));
  Serial.print(F(" imu="));Serial.print(controller.imu.healthy(millis()));
  Serial.print(F(" tilt_ok="));Serial.print(controller.imu.level());
  Serial.print(F(" watchdog_active="));Serial.print(controller.active>=0);
  Serial.print(F(" host_age_ms="));Serial.print((uint32_t)(millis()-controller.heartbeat));
  Serial.print(F(" watchdog_ms=1000 abort="));Serial.println((int)controller.fault);
}
void serviceSafety() {
  int prior=controller.active;
  controller.guard(millis());
  if(prior>=0 && controller.active<0) {
    response("ABORT",prior,(int)controller.fault,false,0);
  }
}
void handle(char *line) {
  char cmd[12];uint8_t n=0;
  while(line[n] && line[n]!=' ' && line[n]!='\t' && n<sizeof(cmd)-1){cmd[n]=line[n];++n;}cmd[n]=0;
  Mapping::Request r=Mapping::parse(line);
  Mapping::Result result=controller.execute(r,millis());
  if(r.command==Mapping::STATUS){status();return;}
  bool accepted=result==Mapping::OK && (r.command==Mapping::ARM || r.command==Mapping::MOVE);
  response(cmd,r.channel,result,accepted,r.angle);
  if(result!=Mapping::OK)return;
  if(r.command==Mapping::HELP) {
    Serial.println(F("HELP | STATUS | CONFIG | IMU | SELECT <ch> | ARM <ch> <deg> | MOVE <ch> <deg> | PING | OFF | X"));
    Serial.println(F("result:0=OK 1=SYNTAX 2=CHANNEL 3=SELECT 4=ARMED 5=NOT_ARMED 6=RANGE 7=IMU 8=BUS"));
    Serial.println(F("abort:0=NONE 1=IMU_LOST 2=TILT 3=HOST_TIMEOUT 4=BUS_ERROR"));
  } else if(r.command==Mapping::CONFIG) {
    for(int i=0;i<13;++i){
      serviceSafety();const Mapping::Calibration &c=Mapping::calibration[i];
      response("CONFIG",i,0,true,c.center);
      Serial.print(F("CONFIG ch="));Serial.print(i);Serial.print(F(" min="));Serial.print(c.low);
      Serial.print(F(" center="));Serial.print(c.center);Serial.print(F(" max="));Serial.print(c.high);
      Serial.print(F(" direction="));Serial.print(c.direction);Serial.print(F(" margin=5 safeMin="));Serial.print(c.low+5);
      Serial.print(F(" safeMax="));Serial.println(c.high-5);
    }
  } else if(r.command==Mapping::IMU) {
    Serial.print(F("IMU healthy="));Serial.print(controller.imu.healthy(millis()));
    Serial.print(F(" roll="));Serial.print(controller.imu.roll,2);Serial.print(F(" pitch="));Serial.print(controller.imu.pitch,2);
    Serial.print(F(" age_ms="));Serial.print((uint32_t)(millis()-controller.imu.last));Serial.println(F(" max_tilt=12 timeout_ms=250"));
  }
}
void setup() {
  pinMode(5,OUTPUT);digitalWrite(5,LOW);pinMode(4,OUTPUT);digitalWrite(4,LOW);
  Serial.begin(115200);Wire.begin();Wire.setWireTimeout(3000,true);
  bool ready=pca.begin();
  if(ready){controller.off();pca.setOscillatorFrequency(25000000);pca.setPWMFreq(50);prescale=pca.readPrescale();controller.off();}
  pico.begin(38400);
  Serial.print(F("MINI Q-POD SERVO MAPPING version=1.0.0 build="));Serial.println(F(MAPPING_BUILD_ID));
  Serial.print(F("PWM_requested=OFF EEPROM=UNUSED prescale="));Serial.println(prescale);
  if(!ready || controller.fault==Mapping::BUS_ERROR){controller.fault=Mapping::BUS_ERROR;Serial.println(F("FATAL PCA9685 unavailable; reboot required"));}
}
void loop() {
  // Bounded UART draining: continuous input cannot starve OFF/watchdog.
  for(uint8_t i=0;i<32 && pico.available();++i){
    int frame=imuLine.feed((char)pico.read());
    if(frame==1)controller.imu.accept(imuLine.data,millis());
  }
  serviceSafety();
  for(uint8_t i=0;i<16 && Serial.available();++i){
    char c=(char)Serial.read();
    // X is an emergency byte, even inside an incomplete or overflowing line.
    if(c=='X'){controller.off();hostLine=Mapping::LineBuffer();response("X",-1,controller.fault==Mapping::BUS_ERROR?Mapping::HARDWARE_ERROR:Mapping::OK,false,0);continue;}
    int frame=hostLine.feed(c);
    if(frame<0)response("INVALID",-1,Mapping::BAD_SYNTAX,false,0);
    if(frame==1)handle(hostLine.data);
    serviceSafety();
  }
  int prior=controller.active;
  controller.tick(millis());
  if(prior>=0 && controller.active<0)response("ABORT",prior,(int)controller.fault,false,0);
}
