#include "ButtonLogic.h"
Press ButtonLogic::update(bool down,uint32_t now) {
  if(raw_!=down) {raw_=down;changed_=now;}
  if(raw_!=stable_ && uint32_t(now-changed_)>=30) {
    stable_=raw_;
    if(stable_) {pressed_=now;longSent_=false;downEdge_=true;}
    else if(!longSent_) return Press::Short;
  }
  if(stable_ && raw_ && !longSent_ && uint32_t(now-pressed_)>=800) {
    longSent_=true;return Press::Long;
  }
  return Press::None;
}
