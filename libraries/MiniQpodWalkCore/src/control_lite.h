#pragma once
#include <math.h>
#include "ps2j_packet.h"

struct ControlLiteAxes { float forward,turn; };
inline float shapeLiteAxis(int8_t value,float deadzone) {
  float x=fmax(-1.0f,fmin(1.0f,(float)value/127.0f)),a=fabs(x);
  if(a<=deadzone)return 0.0f;
  return (x<0?-1.0f:1.0f)*(a-deadzone)/(1.0f-deadzone);
}
inline ControlLiteAxes decodeControlLiteAxes(const PS2J_Packet &p,float deadzone=0.18f) {
  if(!(p.flags&PS2J_FLAG_LITE) || p.mode3!=2)return {0,0};
  return {shapeLiteAxis((int8_t)-p.ry,deadzone),shapeLiteAxis(p.rx,deadzone)};
}
