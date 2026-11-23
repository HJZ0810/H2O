#pragma once
#include "ExamCore.h"
namespace exam {
enum class DemoAction {None,NextPage,FirstPage};
struct DemoStep {DemoAction action=DemoAction::None;std::string command;};
class AutoDemo {
 public:
  static constexpr size_t MaxProfileBytes=3900;
  bool loadAnswers(const std::string& json);
  bool start(const Core& core,uint32_t now);
  void stop(const std::string& reason="演示已停止");
#if defined(ARDUINO)
  bool active() const {return false;}
#else
  bool active() const {return active_;}
  static uint32_t fingerprint(const Frame& frame);
#endif
  const std::string& status() const {return status_;}
  DemoStep tick(Core& core,uint32_t now,bool lastPage,bool painted);
 private:
  std::string status_;
#if !defined(ARDUINO)
  enum class Stage {Read,Select,Review};
  struct Answer {std::string qid; uint32_t fingerprint; unsigned mask;};
  std::vector<Answer> answers_;
  bool active_=false;
  Stage stage_=Stage::Read;
  uint32_t due_=0;
  unsigned target_=0;
  std::string key_;
#endif
};
}
