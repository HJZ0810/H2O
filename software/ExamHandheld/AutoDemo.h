#pragma once
#include "ExamCore.h"
namespace exam {
enum class DemoAction {None,NextPage,FirstPage};
struct DemoStep {DemoAction action=DemoAction::None;std::string command;};
class AutoDemo {
 public:
  bool start(const Core& core,uint32_t now);
  void stop(const std::string& reason="演示已停止");
  bool active() const {return active_;}
  const std::string& status() const {return status_;}
  DemoStep tick(Core& core,uint32_t now,bool lastPage,bool painted);
  static uint32_t fingerprint(const Frame& frame);
 private:
  enum class Stage {Read,Select,Review};
  bool active_=false;
  Stage stage_=Stage::Read;
  uint32_t due_=0;
  unsigned target_=0;
  std::string key_,status_;
};
}
