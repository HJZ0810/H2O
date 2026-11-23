#include "PortalHtml.h"
static std::string escape(const std::string& s) {
  std::string out;
  for(char c:s) {
    switch(c) {
      case '&':out+="&amp;";break; case '<':out+="&lt;";break;
      case '>':out+="&gt;";break; case '\"':out+="&quot;";break;
      case '\'':out+="&#39;";break; default:out+=c;
    }
  }
  return out;
}
static std::string field(const char* label,const char* name,const std::string& value,const char* type="text",const char* extra="") {
  return std::string("<label>")+label+"<input name='"+name+"' type='"+type+"' value='"+escape(value)+"' "+extra+"></label>";
}
std::string buildPortalHtml(const PortalFields& f) {
  std::string h=R"HTML(<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>智联迷城 · 手柄设置</title><style>
  *{box-sizing:border-box}body{margin:0;background:#111a25;color:#edf3fa;font:16px system-ui}main{max-width:568px;margin:24px auto;padding:24px}h1{font-size:26px;line-height:1.3}p{color:#a9bbc9;line-height:1.7}form{display:grid;gap:16px}label{display:grid;gap:7px}input,select,button{font:inherit;padding:12px;border:1px solid #4c5a6a;border-radius:8px;background:#1b2a3b;color:#fff;width:100%;min-width:0}button{background:#1c758c;border:0;cursor:pointer}input:focus-visible,select:focus-visible,button:focus-visible{outline:2px solid #70d4f0;outline-offset:3px}small{color:#a9bbc9;line-height:1.7}.row{display:flex;gap:10px;align-items:center}.row input{width:20px;height:20px;flex-shrink:0}@media(max-width:420px){main{padding:20px;margin:8px auto}h1{font-size:24px}}
  </style></head><body><main><h1>智联迷城 · 手柄设置</h1>)HTML";
  h+="<p>MAC：<b>"+escape(f.mac)+"</b><br>把此 MAC 和组号交给教师注册，再填写教师提供的 MQTT 密码。这里只保存设备设置，不会注册或修改服务器账号。</p><form action='/save' method='post'><input type='hidden' name='token' value='"+escape(f.token)+"'>";
  h+=field("Wi-Fi 名称","ssid",f.ssid,"text","maxlength='32' required");
  h+=field("Wi-Fi 密码（留空保留，首次可为空）","wifi","","password","maxlength='64' autocomplete='new-password'");
  h+="<label class='row'><input type='checkbox' name='openwifi' value='1'>使用无密码 Wi-Fi</label>";
  h+="<h2>智联迷城</h2>";
  h+=field("组号 / MQTT 用户名","group",f.group,"text","maxlength='20' pattern='[A-Za-z0-9_\\-]+'");
  h+=field("MQTT 密码（留空保留已存密码）","mqpass","","password","maxlength='64' autocomplete='new-password'");
  h+=field("本组昵称","name",f.name,"text","maxlength='16'");
  h+=field("MQTT / HTTP 服务器","host",f.host,"text","maxlength='63' required");
  h+=field("MQTT 端口","port",std::to_string(f.port),"number","min='1' max='65535' required");
  h+="<h2>ThingsCloud</h2>";
  h+=field("设备 MQTT 接入点","tc_host",f.cloudHost,"text","maxlength='140' placeholder='mqtt://设备域名:1883'");
  h+=field("设备 AccessToken","tc_token","","password","maxlength='128' autocomplete='new-password'");
  h+=field("项目 ProjectKey","tc_key","","password","maxlength='128' autocomplete='new-password'");
  if (f.hasCloudCredentials) h+="<small>密钥留空会保留现有设置。</small><label class='row'><input type='checkbox' name='cloud_off'>断开 ThingsCloud 并清除凭据</label>";
  h+="<h2>屏幕与传感器</h2>";
  h+="<label>横屏方向<select name='rotation'><option value='1'"+std::string(f.rotation==1?" selected":"")+">横屏 1</option><option value='3'"+std::string(f.rotation==3?" selected":"")+">横屏 3（转180度）</option></select></label>";
  h+="<label>ST7735S 初始化<select name='panel'>";
  const char* variants[]={"黑标签（默认）","红标签","绿标签"};
  for(unsigned i=0;i<3;i++) h+="<option value='"+std::to_string(i)+"'"+(f.panel==i?" selected":"")+">"+variants[i]+"</option>";
  h+="</select></label><label class='row'><input type='checkbox' name='mq1' value='1'"+std::string(f.mq1?" checked":"")+">已安装 MQ-2</label><label class='row'><input type='checkbox' name='mq2' value='1'"+std::string(f.mq2?" checked":"")+">已安装 MQ-135</label><small>MQ 上报模块 AO 电压。保存会重启手柄。</small><button type='submit'>保存并重启手柄</button></form></main></body></html>";
  return h;
}
