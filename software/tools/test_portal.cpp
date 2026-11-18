#include "PortalHtml.h"
#include <iostream>
#include <cstdlib>
#include <fstream>
void require(bool pass,const char* why){if(!pass){std::cerr<<why<<"\n";std::exit(1);}}
int main(int argc,char** argv) {
  PortalFields f;f.mac="AA:BB:CC:11:22:33";f.token="LOCAL_BROWSER_TEST";f.name="本地验证手柄";f.host="124.70.71.221";
  auto html=buildPortalHtml(f);
  require(html.find("name='mqpass' type='password' value='' maxlength='64' autocomplete='new-password' required")!=std::string::npos,"first setup needs MQTT password");
  require(html.find("name='mq1' value='1' checked")==std::string::npos,"MQ defaults disabled");
  if(argc>1){std::ofstream file(argv[1],std::ios::binary);file<<html;require(file.good(),"cannot save portal HTML");}
  f.hasMqttPassword=true;f.mq1=true;f.rotation=3;f.panel=2;f.ssid="x' onfocus='alert(1)";f.name="<script>oops</script>";
  html=buildPortalHtml(f);
  require(html.find("<script>")==std::string::npos,"HTML values must be escaped");
  require(html.find("x&#39; onfocus=&#39;alert(1)")!=std::string::npos,"attribute escaping");
  require(html.find("name='mqpass' type='password' value='' maxlength='64' autocomplete='new-password' required")==std::string::npos,"blank should preserve existing MQTT password");
  require(html.find("name='mq1' value='1' checked")!=std::string::npos,"installed module setting");
  require(html.find("<option value='3' selected")!=std::string::npos,"rotation setting");
  require(html.find("<option value='2' selected")!=std::string::npos,"panel setting");
  std::cout<<"Actual portal HTML: password handling, defaults, escaping, screen/module settings PASS\n";
}
