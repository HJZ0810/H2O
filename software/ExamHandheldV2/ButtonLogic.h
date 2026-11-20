#pragma once
#include <stdint.h>
enum class Press { None, Short, Long };
class ButtonLogic {
 public:
  Press update(bool down,uint32_t now);
  bool takeDownEdge(){bool edge=downEdge_;downEdge_=false;return edge;}
  bool down() const {return stable_;}
 private:
  bool raw_=false, stable_=false, longSent_=false;
  bool downEdge_=false;
  uint32_t changed_=0,pressed_=0;
};
