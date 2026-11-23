#pragma once
#include <ArduinoJson.h>
#include <cstdint>
#include <string>
#include <vector>

namespace exam {
struct Option { std::string key, text; };
struct Frame {
  std::string type, text, qid, questionType, certificate, answer;
  std::vector<Option> options;
  int level=1, seq=0, total=0, hp=3, gain=0, score=0, rank=0;
  bool correct=false, passed=false;
};
enum class Phase { Menu, Story, Question, Judge, Finished, Recovery };
enum class Pending { None, Start, Story, Answer, Hint, Follow, Resume };
class Core {
 public:
  Phase phase=Phase::Menu;
  Pending pending=Pending::None;
  Frame frame;
  bool connected=false, ready=false, statusKnown=false;
  bool protocolV2=false;
  uint64_t serverRevision=0;
  std::string examId;
  uint32_t statusGeneration=1;
  unsigned selected=0, removed=0, hints=1, revision=0;
  int score=0, hp=3;
  std::string notice;
  void link(bool up);
  bool receive(const std::string& json);
  std::string start(unsigned long now);
  std::string advance(unsigned long now);
  std::string submit(unsigned long now);
  std::string hint(unsigned long now);
  bool choose(unsigned index);
  bool clearSelection();
  void tick(unsigned long now);
  void fail(const std::string& why);
  void setRequestPrefix(const std::string& prefix) {requestPrefix_=prefix;}
  std::string resume(unsigned long now);
  bool needsResume() const {return protocolV2&&connected&&ready&&resumeWanted_&&!busy();}
  bool synchronize(int totalScore, int health, uint32_t generation,
                   bool finished=false, const std::string& certificate="", int rank=0);
  bool busy() const { return pending!=Pending::None; }
 private:
  Frame follow_;
  bool hasFollow_=false, everActive_=false;
  std::vector<Frame> followQueue_;
  unsigned long sentAt_=0;
  std::string lastJudge_;
  std::string requestPrefix_="boot",requestId_;
  uint32_t requestSequence_=0;
  int v2StartHealth_=5;
  bool resumeWanted_=false;
  void apply(const Frame& f);
  std::string command(const char* type, Pending p, unsigned long now, const std::string& choice="");
};
const char* phaseName(Phase p);
}
