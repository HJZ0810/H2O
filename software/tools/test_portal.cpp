#include "PortalHtml.h"
#include <iostream>
#include <cstdlib>
#include <fstream>
void require(bool pass,const char* why){if(!pass){std::cerr<<why<<"\n";std::exit(1);}}
int main(int argc,char** argv) {
  PortalFields f;f.mac="AA:BB:CC:11:22:33";f.token="LOCAL_BROWSER_TEST";f.name="本地验证手柄";f.host="124.70.71.221";
  auto html=buildPortalHtml(f);
  require(html.find("name='mqpass' type='password' value=''")!=std::string::npos,"exam password is never rendered");
  require(html.find("name='tc_token' type='password' value=''")!=std::string::npos,"cloud token is never rendered");
  require(html.find("name='tc_key' type='password' value=''")!=std::string::npos,"project key is never rendered");
  require(html.find("name='mq1' value='1' checked")!=std::string::npos,"MQ-2 enabled for the populated R9 board");
  require(html.find("name='mq2' value='1' checked")!=std::string::npos,"MQ-135 enabled for the populated R9 board");
  require(html.find("id='answer-file' type='file'")!=std::string::npos,"answer profile upload");
  if(argc>1){std::ofstream file(argv[1],std::ios::binary);file<<html;require(file.good(),"cannot save portal HTML");}
  f.hasCloudCredentials=true;f.hasDemoAnswers=true;f.mq1=false;f.rotation=3;f.panel=2;
  f.ssid="x' onfocus='alert(1)";f.name="<script>oops</script>";
  html=buildPortalHtml(f);
  require(html.find("<script>oops</script>")==std::string::npos,"HTML values must be escaped");
  require(html.find("x&#39; onfocus=&#39;alert(1)")!=std::string::npos,"attribute escaping");
  require(html.find("name='cloud_off'")!=std::string::npos,"cloud credential reset");
  require(html.find("name='demo_clear'")!=std::string::npos,"answer profile reset");
  require(html.find("name='mq1' value='1' checked")==std::string::npos,"optional module setting");
  require(html.find("<option value='3' selected")!=std::string::npos,"rotation setting");
  require(html.find("<option value='2' selected")!=std::string::npos,"panel setting");
  std::cout<<"Actual portal HTML: password handling, defaults, escaping, screen/module settings PASS\n";
}
