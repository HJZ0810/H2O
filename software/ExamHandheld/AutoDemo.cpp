#include "AutoDemo.h"
#include "DemoAnswers.h"
namespace exam {
uint32_t AutoDemo::fingerprint(const Frame& f) {
  uint32_t h=2166136261u;
  auto part=[&](const std::string& s){for(unsigned char c:s){h^=c;h*=16777619u;}h^=0;h*=16777619u;};
  part(f.qid);part(f.questionType);part(f.text);
  for(const auto& o:f.options){part(o.key);part(o.text);}return h;
}
bool AutoDemo::start(const Core& c,uint32_t now) {
  if(!c.connected||!c.ready||!c.statusKnown||!c.protocolV2||c.busy()||c.phase==Phase::Finished||c.phase==Phase::Recovery)return false;
  active_=true;stage_=Stage::Read;key_.clear();due_=now+3200;target_=0;status_="演示中 F1停止";return true;
}
void AutoDemo::stop(const std::string& reason){active_=false;status_=reason;key_.clear();}
DemoStep AutoDemo::tick(Core& c,uint32_t now,bool lastPage,bool painted) {
  DemoStep step;
  if(!active_)return step;
  if(!c.connected||!c.ready||c.phase==Phase::Recovery){stop("演示暂停，请同步后再启动");return step;}
  if(c.phase==Phase::Finished){stop(c.score==350?"演示完成 350分":"演示完成，保留原成绩");return step;}
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
      const DemoAnswer* answer=nullptr;
      for(const auto& a:DemoAnswers)if(c.frame.qid==a.qid){answer=&a;break;}
      if(!answer||answer->fingerprint!=fingerprint(c.frame)){stop("题库有变化，演示停止");return step;}
      for(const char* s=answer->choice;*s;s++)if(*s>='A'&&*s<='D')target_|=1u<<(*s-'A');
      if(!target_||(target_&c.removed)||(target_>>c.frame.options.size())){stop("答案无法匹配，演示停止");return step;}
      stage_=Stage::Select;step.action=DemoAction::FirstPage;status_="开始选择 F1停止";due_=now+1200;return step;
    }
    step.command=c.submit(now);status_="演示提交 F1停止";
  }else if(c.phase==Phase::Story||c.phase==Phase::Judge){
    step.command=c.advance(now);status_="演示继续 F1停止";
  }
  return step;
}
}
