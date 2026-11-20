#include "ExamCore.h"
#include "AutoDemo.h"
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc, char** argv) {
  exam::Core core;
  exam::AutoDemo demo;
  if (argc == 2) {
    std::ifstream file(argv[1], std::ios::binary);
    if (!file || !demo.loadAnswers({std::istreambuf_iterator<char>(file), {}})) return 3;
  }
  core.setRequestPrefix("NATIVE"); std::string line;
  while(std::getline(std::cin,line)) {
    JsonDocument in, out;
    if(deserializeJson(in,line)) return 2;
    std::string op=in["op"] | "";
    std::string command;
    unsigned long now=in["now"] | 100UL;
    if(op=="link") core.link(in["up"] | false);
    else if(op=="prefix") core.setRequestPrefix(in["value"] | "NATIVE");
    else if(op=="demo_start") demo.start(core,now);
    else if(op=="demo_stop") demo.stop();
    else if(op=="demo_tick") {auto step=demo.tick(core,now,in["lastPage"]|true,in["painted"]|true);command=step.command;out["pageAction"]=int(step.action);}
    else if(op=="receive") { std::string payload; serializeJson(in["payload"],payload); core.receive(payload); }
    else if(op=="raw") core.receive(in["payload"].as<std::string>());
    else if(op=="resume") command=core.resume(now);
    else if(op=="start") command=core.start(now);
    else if(op=="advance") command=core.advance(now);
    else if(op=="choose") core.choose(in["index"] | 0U);
    else if(op=="clear") core.clearSelection();
    else if(op=="submit") command=core.submit(now);
    else if(op=="hint") command=core.hint(now);
    else if(op=="tick") core.tick(now);
    else if(op=="sync") core.synchronize(in["score"] | 0,in["hp"] | 3,
      in["generation"] | core.statusGeneration,in["finished"] | false,
      in["certificate"] | std::string(),in["rank"] | 0);
    out["phase"]=exam::phaseName(core.phase);
    out["selected"]=core.selected; out["removed"]=core.removed;
    out["hints"]=core.hints; out["score"]=core.score; out["hp"]=core.hp;
    out["busy"]=core.busy(); out["ready"]=core.ready;
    out["statusKnown"]=core.statusKnown;out["generation"]=core.statusGeneration;
    out["certificate"]=core.frame.certificate;
    out["type"]=core.frame.type; out["qid"]=core.frame.qid; out["text"]=core.frame.text;
    out["level"]=core.frame.level; out["seq"]=core.frame.seq; out["total"]=core.frame.total;
    out["correct"]=core.frame.correct;
    out["notice"]=core.notice;
    out["demoActive"]=demo.active();out["demoStatus"]=demo.status();
    if(!command.empty()) { JsonDocument cmd; deserializeJson(cmd,command); out["command"]=cmd; }
    else out["command"]=nullptr;
    serializeJson(out,std::cout); std::cout<<std::endl;
  }
}
