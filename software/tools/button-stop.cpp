#include "ButtonLogic.h"
#include <cassert>
#include <iostream>
int main(){
 ButtonLogic key;
 assert(key.update(true,100)==Press::None);assert(!key.takeDownEdge());
 assert(key.update(true,129)==Press::None);assert(!key.takeDownEdge());
 assert(key.update(true,130)==Press::None);assert(key.takeDownEdge());assert(key.down());
 assert(!key.takeDownEdge());assert(key.update(true,900)==Press::None);assert(!key.takeDownEdge());
 assert(key.update(true,930)==Press::Long);assert(!key.takeDownEdge());
 assert(key.update(false,950)==Press::None);assert(key.update(false,980)==Press::None);assert(!key.down());
 assert(key.update(true,1000)==Press::None);assert(key.update(true,1030)==Press::None);assert(key.takeDownEdge());
 assert(key.update(false,1100)==Press::None);assert(key.update(false,1130)==Press::Short);assert(!key.down());assert(!key.takeDownEdge());
 std::cout<<"Debounced stop edge precedes release/long event: PASS\n";
}
