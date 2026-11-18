#include "ExamCore.h"
namespace exam {
const char* phaseName(Phase p) {
  switch(p) {case Phase::Story:return "story"; case Phase::Question:return "question";
    case Phase::Judge:return "judge"; case Phase::Finished:return "finished";
    case Phase::Recovery:return "recovery"; default:return "menu";}
}
void Core::fail(const std::string& why) {
  // A reply may have been lost after the server changed the score. Recover
  // through an authoritative snapshot, never by resending the answer.
  if(phase!=Phase::Recovery || pending!=Pending::None) {
    statusKnown=false;statusGeneration++;
  }
  phase=Phase::Recovery; pending=Pending::None; hasFollow_=false; notice=why; revision++;
  if(protocolV2){resumeWanted_=true;requestId_.clear();}
}
void Core::link(bool up) {
  if(connected==up) return;
  connected=up; ready=false; statusGeneration++;
  if(protocolV2)resumeWanted_=true;
  if(phase!=Phase::Finished)statusKnown=false;
  revision++;
  if(!up && everActive_ && phase!=Phase::Finished)
    fail("连接中断，等待恢复原进度；不会自动重发答案。");
}
bool Core::synchronize(int totalScore,int health,uint32_t generation,bool finished,const std::string& certificate,int rank) {
  if(protocolV2)return false;
  // HTTP snapshots are not ordered with MQTT judges. Never overwrite active exam state.
  if(generation!=statusGeneration || busy() || totalScore<0 || health<0 || health>3) return false;
  if(phase!=Phase::Menu && phase!=Phase::Recovery && phase!=Phase::Finished)return false;
  if(phase==Phase::Finished && !finished)return false;
  statusKnown=true;score=totalScore;hp=health;
  if(finished) {
    Frame f;f.type="game_over";f.score=totalScore;f.rank=rank;
    f.certificate=certificate;f.text="本组已通关（已同步服务器结果）";
    apply(f);
  } else {
    revision++;
  }
  return true;
}
std::string Core::command(const char* type,Pending p,unsigned long now,const std::string& choice) {
  JsonDocument doc; doc["type"]=type;
  JsonObject data=doc["data"].to<JsonObject>();
  if(protocolV2){
    data["protocol"]=2;requestId_=requestPrefix_+"-"+std::to_string(++requestSequence_);
    data["request_id"]=requestId_;
    if(p!=Pending::Resume){data["exam_id"]=examId;data["revision"]=serverRevision;}
  }
  if(p==Pending::Answer || p==Pending::Hint) data["qid"]=frame.qid;
  if(p==Pending::Answer) data["choice"]=choice;
  std::string out; serializeJson(doc,out);
  pending=p; sentAt_=now; notice.clear(); revision++; return out;
}
std::string Core::resume(unsigned long now) {
  if(!needsResume())return {};
  return command("resume",Pending::Resume,now);
}
std::string Core::start(unsigned long now) {
  if(!connected || !ready || !statusKnown || busy() || phase==Phase::Finished) return {};
  hasFollow_=false; lastJudge_.clear(); everActive_=true; selected=removed=0; hints=1;
  return command("start",Pending::Start,now);
}
std::string Core::advance(unsigned long now) {
  if(!connected || !ready || busy()) return {};
  if(phase==Phase::Story) return command("story_next",Pending::Story,now);
  if(phase==Phase::Judge) {
    if(protocolV2)return command("continue",Pending::Follow,now);
    if(hasFollow_) { auto f=follow_; hasFollow_=false; apply(f); }
    else { pending=Pending::Follow; sentAt_=now; notice="等待服务器下一题"; revision++; }
  }
  return {};
}
bool Core::choose(unsigned index) {
  if(!connected || !ready || phase!=Phase::Question || busy() || index>=frame.options.size()) return false;
  unsigned bit=1U<<index;
  if(removed&bit) return false;
  selected=frame.questionType=="multi" ? selected^bit : bit;
  notice.clear(); revision++; return true;
}
std::string Core::submit(unsigned long now) {
  if(!connected || !ready || phase!=Phase::Question || busy() || !selected) return {};
  std::string choice;
  for(unsigned i=0;i<frame.options.size();i++) if(selected&(1U<<i)) {
    if(!choice.empty()) choice+=",";
    choice+=frame.options[i].key;
  }
  return command("answer",Pending::Answer,now,choice);
}
std::string Core::hint(unsigned long now) {
  if(!connected || !ready || phase!=Phase::Question || busy() || !hints) return {};
  return command("hint",Pending::Hint,now);
}
void Core::tick(unsigned long now) {
  if(busy() && static_cast<unsigned long>(now-sentAt_)>12000UL)
    fail("请求超时，正在同步原进度；不会重复提交答案。");
}
void Core::apply(const Frame& f) {
  frame=f; pending=Pending::None; selected=removed=0; notice.clear(); revision++;
  if(f.type=="story") {phase=Phase::Story; if(f.seq==1) {hints=1;hp=3;}}
  else if(f.type=="question") phase=Phase::Question;
  else if(f.type=="game_over") {phase=Phase::Finished;score=f.score;}
  else if(f.type=="judge") {phase=Phase::Judge;hp=f.hp;}
  else if(f.type=="level_result") {phase=Phase::Judge;score=f.score;hp=f.hp;}
}
static bool boundedText(JsonVariantConst v,size_t max,bool required=false) {
  if(!v.is<const char*>()) return !required && v.isNull();
  auto s=v.as<std::string>();return s.size()<=max && (!required || !s.empty());
}
static bool decodeV2(JsonObjectConst message,Frame& f) {
  if(!message["type"].is<const char*>()||!message["data"].is<JsonObjectConst>())return false;
  f.type=message["type"].as<std::string>();JsonObjectConst d=message["data"];
  if(f.type=="story"){
    if(!d["level"].is<int>()||!d["seq"].is<int>()||!d["total"].is<int>()||!boundedText(d["text"],4000,true))return false;
    f.level=d["level"];f.seq=d["seq"];f.total=d["total"];f.text=d["text"].as<std::string>();
    return f.level>=1&&f.level<=7&&f.seq>=1&&f.seq<=f.total&&f.total<=50;
  }
  if(f.type=="question"){
    if(!d["level"].is<int>()||!boundedText(d["qid"],64,true)||!boundedText(d["text"],4000,true)||!d["options"].is<JsonArrayConst>())return false;
    f.level=d["level"];f.qid=d["qid"].as<std::string>();f.text=d["text"].as<std::string>();f.questionType=d["type"]|std::string();
    if(f.level<1||f.level>7||(f.questionType!="single"&&f.questionType!="multi"&&f.questionType!="bool"&&f.questionType!="scene"))return false;
    unsigned n=0;for(JsonObjectConst o:d["options"].as<JsonArrayConst>()){
      if(n>=4||!boundedText(o["key"],1,true)||o["key"].as<std::string>()!=std::string(1,'A'+n)||!boundedText(o["text"],1000,true))return false;
      f.options.push_back({o["key"].as<std::string>(),o["text"].as<std::string>()});n++;
    }return n>=2;
  }
  if(f.type=="judge"){
    if(!boundedText(d["qid"],64,true)||!d["correct"].is<bool>()||!d["score"].is<int>()||!d["hp"].is<int>()||
       !boundedText(d["correct_answer"],16,true)||!boundedText(d["explain"],4000,true))return false;
    f.level=d["level"]|1;f.qid=d["qid"].as<std::string>();f.correct=d["correct"];f.gain=d["score"];f.hp=d["hp"];
    f.answer=d["correct_answer"].as<std::string>();f.text=d["explain"].as<std::string>();return f.gain>=0&&f.gain<=10&&f.hp>=0&&f.hp<=3;
  }
  if(f.type=="game_over"){
    if(!d["total_score"].is<int>()||!boundedText(d["certificate"],128)||!boundedText(d["message"],1000))return false;
    f.score=d["total_score"];f.rank=d["rank"]|0;f.certificate=d["certificate"]|std::string();f.text=d["message"]|std::string();return f.score>=0&&f.score<=350;
  }
  return false;
}
bool Core::receive(const std::string& json) {
  if(json.empty() || json.size()>8192) {fail("服务器报文长度异常，已暂停");return false;}
  JsonDocument doc;
  if(deserializeJson(doc,json,DeserializationOption::NestingLimit(8)) || !doc.is<JsonObject>() ||
     !boundedText(doc["type"],32,true) || !doc["data"].is<JsonObject>()) {
    fail("服务器报文格式异常，已暂停"); return false;
  }
  const std::string t=doc["type"].as<std::string>(); JsonObjectConst d=doc["data"];
  if(t=="hello_ack") {ready=true;if((d["protocol"]|0)>=2){protocolV2=true;if(!statusKnown)resumeWanted_=true;}revision++;return true;}
  if(t=="sensor_ack" || t=="pong" || t=="leaderboard") return true;
  if(t=="error") {
    if(protocolV2){auto id=d["request_id"]|std::string();if(!id.empty()&&id!=requestId_)return true;}
    fail(d["message"] | "服务器返回错误");return false;
  }
  if(protocolV2){
    if(!busy()||(d["request_id"]|std::string())!=requestId_)return true;
    if(t=="resume_ack"){
      if(pending!=Pending::Resume||!boundedText(d["exam_id"],80,true)||!d["revision"].is<uint64_t>()||
         !d["score"].is<int>()||!d["hp"].is<int>()||!d["started"].is<bool>()||!d["finished"].is<bool>()||
         !d["hints_left"].is<unsigned>()||!d["removed_options"].is<JsonArrayConst>()||
         d["score"].as<int>()<0||d["score"].as<int>()>350||d["hp"].as<int>()<0||d["hp"].as<int>()>3){fail("同步数据异常");return false;}
      Frame f;bool started=d["started"];
      if(started&&(!d["current"].is<JsonObjectConst>()||!decodeV2(d["current"],f))){fail("同步画面异常");return false;}
      examId=d["exam_id"].as<std::string>();serverRevision=d["revision"].as<uint64_t>();
      if(started)apply(f);else {phase=Phase::Menu;frame=Frame();selected=removed=0;}
      score=d["score"];hp=d["hp"];hints=d["hints_left"].as<unsigned>()>0?1:0;
      removed=0;for(JsonVariantConst key:d["removed_options"].as<JsonArrayConst>())
        for(unsigned i=0;i<frame.options.size();i++)if(key.as<std::string>()==frame.options[i].key)removed|=1U<<i;
      pending=Pending::None;hasFollow_=false;statusKnown=true;resumeWanted_=false;requestId_.clear();notice.clear();everActive_=started;revision++;return true;
    }
    if(pending==Pending::Resume)return true;
    if((d["exam_id"]|std::string())!=examId||!d["revision"].is<uint64_t>()||d["revision"].as<uint64_t>()!=serverRevision+1){fail("答题进度不一致，正在同步");return false;}
    if(t=="hint_ack"){
      if(pending!=Pending::Hint||(d["qid"]|std::string())!=frame.qid||!d["remove_options"].is<JsonArrayConst>()||!d["remaining_hints"].is<unsigned>()){fail("锦囊回执异常");return false;}
      for(JsonVariantConst key:d["remove_options"].as<JsonArrayConst>())for(unsigned i=0;i<frame.options.size();i++)if(key.as<std::string>()==frame.options[i].key)removed|=1U<<i;
      selected&=~removed;hints=d["remaining_hints"].as<unsigned>()>0?1:0;
    }else{
      Frame f;if(!decodeV2(doc.as<JsonObjectConst>(),f)||!d["total_score"].is<int>()||d["total_score"].as<int>()<0||d["total_score"].as<int>()>350){fail("服务器画面异常");return false;}
      if(pending==Pending::Answer&&(t!="judge"||f.qid!=frame.qid)){fail("判题题号不一致");return false;}
      apply(f);score=d["total_score"];
    }
    serverRevision=d["revision"].as<uint64_t>();pending=Pending::None;requestId_.clear();revision++;return true;
  }
  if(phase==Phase::Recovery && pending!=Pending::Start) return true;
  if(t=="hint_ack") {
    if(phase!=Phase::Question || pending!=Pending::Hint || (d["qid"] | std::string())!=frame.qid) return true;
    if(!d["remove_options"].is<JsonArrayConst>() || !d["remaining_hints"].is<unsigned>()) {fail("锦囊格式异常");return false;}
    for(JsonVariantConst k:d["remove_options"].as<JsonArrayConst>())
      for(unsigned i=0;i<frame.options.size();i++) if(k.as<std::string>()==frame.options[i].key) removed|=1U<<i;
    selected&=~removed; hints=d["remaining_hints"].as<unsigned>()>0?1:0;
    pending=Pending::None;notice="已排除错误选项";revision++;return true;
  }
  Frame f; f.type=t;
  if(t=="judge") {
    auto qid=d["qid"] | std::string();
    if(qid==lastJudge_ || phase!=Phase::Question || pending!=Pending::Answer || qid!=frame.qid) return true;
    if(!d["correct"].is<bool>() || !d["score"].is<int>() || !d["hp"].is<int>() ||
       !boundedText(d["explain"],4000,true) || !boundedText(d["correct_answer"],16,true) ||
       d["hp"].as<int>()<0 || d["hp"].as<int>()>3 || d["score"].as<int>()<0 || d["score"].as<int>()>100) {
      fail("判题数据异常，已暂停");return false;
    }
    f.qid=qid; f.level=frame.level;f.correct=d["correct"];f.gain=d["score"];f.hp=d["hp"];
    f.text=d["explain"].as<std::string>();f.answer=d["correct_answer"].as<std::string>();
    score+=f.gain;hp=f.hp;lastJudge_=qid;frame=f;phase=Phase::Judge;
    pending=Pending::None;hasFollow_=false;revision++;return true;
  }
  if(t=="story") {
    if(!d["level"].is<int>() || !d["seq"].is<int>() || !d["total"].is<int>() || !boundedText(d["text"],4000,true)) {
      fail("剧情数据异常");return false;
    }
    f.level=d["level"];f.seq=d["seq"];f.total=d["total"];f.text=d["text"].as<std::string>();
    if(f.level<1||f.level>7||f.seq<1||f.total<1||f.total>50||f.seq>f.total) {fail("剧情页码异常");return false;}
  } else if(t=="question") {
    if(!d["level"].is<int>() || !boundedText(d["qid"],64,true) || !boundedText(d["text"],4000,true) ||
       !boundedText(d["type"],12,true) || !d["options"].is<JsonArrayConst>()) {fail("题目格式异常");return false;}
    f.level=d["level"];f.qid=d["qid"].as<std::string>();f.text=d["text"].as<std::string>();f.questionType=d["type"].as<std::string>();
    if(f.level<1||f.level>7 || (f.questionType!="single"&&f.questionType!="bool"&&f.questionType!="multi"&&f.questionType!="scene")) {fail("题型异常");return false;}
    unsigned n=0;
    for(JsonObjectConst o:d["options"].as<JsonArrayConst>()) {
      if(n>=4 || !boundedText(o["key"],1,true) || !boundedText(o["text"],1000,true) ||
         o["key"].as<std::string>()!=std::string(1,'A'+n)) {fail("选项格式异常");return false;}
      f.options.push_back({o["key"].as<std::string>(),o["text"].as<std::string>()});n++;
    }
    if(n<2) {fail("选项数量异常");return false;}
    if(phase==Phase::Question && frame.qid==f.qid) return true;
  } else if(t=="game_over") {
    if(!d["total_score"].is<int>() || !boundedText(d["certificate"],128) || !boundedText(d["message"],1000)) {fail("结算格式异常");return false;}
    f.score=d["total_score"];f.rank=d["rank"] | 0;f.certificate=d["certificate"] | std::string();f.text=d["message"] | std::string();
  } else if(t=="level_result") {
    f.level=d["level"] | 1;f.score=d["score"] | score;f.hp=d["hp"] | hp;f.passed=d["passed"] | false;
    f.text=f.passed?"本关通过，确认继续":"本关未通过，确认重试";
  } else {fail("收到不支持的服务器消息，已暂停");return false;}
  if(phase==Phase::Judge && pending!=Pending::Follow) {
    if(hasFollow_) {
      if(f.type==follow_.type && f.qid==follow_.qid && f.seq==follow_.seq && f.level==follow_.level) return true;
      fail("后续题目队列异常，已暂停");return false;
    }
    follow_=f;hasFollow_=true;revision++;return true;
  }
  if(pending==Pending::Start || pending==Pending::Story || pending==Pending::Follow) {apply(f);return true;}
  // Unsolicited exam progression must not silently replace a question.
  if(t=="game_over" && phase==Phase::Menu) {apply(f);return true;}
  if(t=="story" && phase==Phase::Story && f.level==frame.level && f.seq==frame.seq) return true;
  fail("收到非预期的题目进度，已暂停");return false;
}
}
