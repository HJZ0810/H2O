#pragma once
// Test demonstration data; generated from the server bank on 2026-11-11.
// A changed question or option fails the fingerprint check before submission.
namespace exam {
struct DemoAnswer {const char* qid;uint32_t fingerprint;const char* choice;};
static constexpr DemoAnswer DemoAnswers[]={
  {"L1-BOSS",0x89b0cd6fu,"B"},
  {"L1-01",0x11d1825au,"C"},
  {"L1-02",0xf234da42u,"A"},
  {"L1-03",0xd5046407u,"B"},
  {"L1-04",0xd99909fcu,"A,B,C"},
  {"L2-BOSS",0x6030ee83u,"B"},
  {"L2-01",0x176d5881u,"B"},
  {"L2-02",0xc7dbc122u,"A"},
  {"L2-03",0x88ffdcbau,"A"},
  {"L2-04",0x990dedcdu,"A,B,C"},
  {"L3-BOSS",0x32bd6a90u,"B"},
  {"L3-01",0x0ebda9c2u,"C"},
  {"L3-02",0x05592076u,"A"},
  {"L3-03",0x085b6dd1u,"B"},
  {"L3-04",0x27d483f2u,"A,C,D"},
  {"L4-BOSS",0x380a8747u,"B"},
  {"L4-01",0x83611226u,"C"},
  {"L4-02",0x49419ed8u,"A"},
  {"L4-03",0xaf705e17u,"B"},
  {"L4-04",0xc14fb3aeu,"A,B,C"},
  {"L5-BOSS",0x966e3e72u,"B"},
  {"L5-01",0x564dc8a6u,"B"},
  {"L5-02",0x2c534462u,"A"},
  {"L5-03",0x52558ae7u,"B"},
  {"L5-04",0x680ad3afu,"A,B,C"},
  {"L6-BOSS",0x3251c777u,"B"},
  {"L6-01",0x354f511bu,"B"},
  {"L6-02",0x2d0cce9du,"A"},
  {"L6-03",0x26a17539u,"B"},
  {"L6-04",0x7d3a3f9cu,"A,B,C"},
  {"L7-BOSS",0xb9ed17aau,"C"},
  {"L7-01",0x0e376cb1u,"B"},
  {"L7-02",0x87a817c5u,"A"},
  {"L7-03",0xb650d1a7u,"B"},
  {"L7-04",0x25969b1fu,"A,B,C,D"},
};
}
