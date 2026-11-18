#include "ButtonLogic.h"
#include <iostream>
#include <cstdlib>
void require(bool pass,const char* why) {if(!pass) {std::cerr<<why<<"\n";std::exit(1);}}
int main() {
  ButtonLogic b;
  require(b.update(true,100)==Press::None,"press must debounce");
  require(b.update(false,110)==Press::None,"bounce release must not activate");
  b.update(true,115); b.update(true,150);
  b.update(false,200);
  require(b.update(false,235)==Press::Short,"short release must generate once");
  require(b.update(false,250)==Press::None,"release cannot repeat");
  b.update(true,300); b.update(true,335);
  require(b.update(true,1200)==Press::Long,"hold generates long event");
  require(b.update(true,1250)==Press::None,"long cannot repeat");
  b.update(false,1300);
  require(b.update(false,1340)==Press::None,"long release must never submit short");
  ButtonLogic wrap; wrap.update(true,0xfffffff0); wrap.update(true,0x20);
  wrap.update(false,0x50);
  require(wrap.update(false,0x80)==Press::Short,"debounce must survive millis wrap");
  std::cout<<"Button debounce, bounce, short, long exclusivity and wrap: PASS\n";
}
