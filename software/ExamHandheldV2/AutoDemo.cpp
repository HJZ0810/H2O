#include "AutoDemo.h"
#include <algorithm>

namespace exam {
#if defined(ARDUINO)
// Device builds never load answer profiles or drive the exam state machine.
// Keep the interface for the existing menu and configuration callers.
bool AutoDemo::loadAnswers(const std::string& json) { return json.empty(); }
bool AutoDemo::start(const Core&, uint32_t) {
  status_ = "正式固件仅支持手动作答";
  return false;
}
void AutoDemo::stop(const std::string& reason) { status_ = reason; }
DemoStep AutoDemo::tick(Core&, uint32_t, bool, bool) { return {}; }
#else
bool AutoDemo::loadAnswers(const std::string& json) {
  if (json.empty()) { answers_.clear(); return true; }
  JsonDocument doc;
  if (json.size() > MaxProfileBytes || deserializeJson(doc, json, DeserializationOption::NestingLimit(4))) return false;
  const auto rows = doc["answers"].as<JsonArrayConst>();
  if (doc["version"] != 1 || rows.isNull() || !rows.size()) return false;
  std::vector<Answer> answers;
  answers.reserve(rows.size());
  for (JsonArrayConst row : rows) {
    const auto qid = row[0].as<std::string>();
    const auto choice = row[2].as<std::string>();
    if (row.size()!=3 || !row[0].is<const char*>() || qid.empty() || qid.size()>64 ||
        !row[1].is<uint32_t>() || !row[2].is<const char*>() || choice.empty() || choice.size()>7 ||
        choice.size()%2==0) return false;
    unsigned mask=0;
    for (size_t i=0; i<choice.size(); i+=2) {
      if ((i && choice[i-1]!=',') || choice[i]<'A' || choice[i]>'D') return false;
      const unsigned bit=1u<<(choice[i]-'A');
      if (mask & bit) return false;
      mask |= bit;
    }
    if (std::any_of(answers.begin(), answers.end(), [&](const Answer& a){return a.qid==qid;})) return false;
    answers.push_back({qid, row[1].as<uint32_t>(), mask});
  }
  answers_.swap(answers);
  return true;
}

uint32_t AutoDemo::fingerprint(const Frame& f) {
  uint32_t h=2166136261u;
  auto part=[&](const std::string& s){for(unsigned char c:s){h^=c;h*=16777619u;}h^=0;h*=16777619u;};
  part(f.qid);part(f.questionType);part(f.text);
  for(const auto& o:f.options){part(o.key);part(o.text);}return h;
}
bool AutoDemo::start(const Core& c,uint32_t now) {
  if(answers_.empty()) { status_="先在配网页导入演示答案"; return false; }
  if(!c.connected||!c.ready||!c.statusKnown||!c.protocolV2||c.busy()||c.phase==Phase::Finished||c.phase==Phase::Recovery) {
    status_="等待进度同步，已通关组不能重置成绩"; return false;
  }
  active_=true;stage_=Stage::Read;key_.clear();due_=now+3200;target_=0;status_="演示中 F1停止";return true;
}
void AutoDemo::stop(const std::string& reason){active_=false;status_=reason;key_.clear();}
DemoStep AutoDemo::tick(Core& c,uint32_t now,bool lastPage,bool painted) {
  DemoStep step;
  if(!active_)return step;
  if(!c.connected||!c.ready||c.phase==Phase::Recovery){stop("演示暂停，请同步后再启动");return step;}
  if(c.phase==Phase::Finished){stop("演示完成 "+std::to_string(c.score)+" 分");return step;}
  if(c.busy()||!painted)return step;
  const auto key=std::to_string(int(c.phase))+"/"+std::to_string(c.serverRevision);
  if(key_!=key){key_=key;stage_=Stage::Read;target_=0;due_=now+3200;status_="演示阅读 F1停止";return step;}
  if(int32_t(now-due_)<0)return step;
  due_=now+3200;
  if(c.phase==Phase::Menu){step.command=c.start(now);return step;}
  if(c.phase==Phase::Question&&stage_==Stage::Select){
    const unsigned difference=c.frame.questionType=="multi"?(c.selected^target_):(c.selected==target_?0:target_);
    if(difference){
      unsigned i=0;while(!(difference&(1u<<i)))i++;
      if(!c.choose(i)){stop("选项不可选，演示停止");return step;}
      status_="自动选 "+c.frame.options[i].key+" F1停止";due_=now+1200;
      if(c.selected==target_){stage_=Stage::Review;due_=now+3200;step.action=DemoAction::FirstPage;}
      return step;
    }
    stage_=Stage::Review;step.action=DemoAction::FirstPage;return step;
  }
  // Every page of story, question, selection and explanation stays visible.
  if(!lastPage){step.action=DemoAction::NextPage;return step;}
  if(c.phase==Phase::Question){
    if(stage_==Stage::Read){
      const auto answer=std::find_if(answers_.begin(),answers_.end(),[&](const Answer& a){return c.frame.qid==a.qid;});
      if(answer==answers_.end()||answer->fingerprint!=fingerprint(c.frame)){stop("题库有变化，演示停止");return step;}
      target_=answer->mask;
      if((target_&c.removed)||(target_>>c.frame.options.size())||
         (c.frame.questionType!="multi" && (target_&(target_-1)))){stop("答案无法匹配，演示停止");return step;}
      stage_=Stage::Select;step.action=DemoAction::FirstPage;status_="开始选择 F1停止";due_=now+1200;return step;
    }
    if (c.selected != target_) {
      stop("演示选项已改变，未提交");
      return step;
    }
    step.command=c.submit(now);status_="演示提交 F1停止";
  }else if(c.phase==Phase::Story||c.phase==Phase::Judge){
    step.command=c.advance(now);status_="演示继续 F1停止";
  }
  return step;
}
#endif
}
