#pragma once
#include <Arduino.h>
#include <time.h>
#include <mbedtls/base64.h>
#include <mbedtls/md.h>

// OneNET 安全鉴权（2018-10-31）
//
// MQTT 的密码不是设备密钥本身，而是用密钥算出的 token：
//   resource            = products/{pid}/devices/{dev}
//   stringForSignature  = et \n method \n resource \n version
//   sign                = base64( HMAC-SHA1( base64decode(key), stringForSignature ) )
//   token               = version=2018-10-31&res=...&et=...&method=sha1&sign=...
//
// et 是绝对过期时间，所以设备必须先完成 NTP 对时。时钟不对时算出的 token
// 一定过期，服务端会回 CONNACK 4（用户名或密码错误），很容易误判成凭据填错。
static String onenetEscape(const String& text) {
  String out;
  const char hex[] = "0123456789ABCDEF";
  for (size_t i = 0; i < text.length(); ++i) {
    const unsigned char c = text[i];
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-' || c == '.' || c == '_' || c == '~') out += char(c);
    else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
  }
  return out;
}
static String onenetMakeToken(const String& pid, const String& dev, const String& keyB64,
                              uint32_t ttlSec = 3600u) {
  unsigned char keyBuf[192];
  size_t keyLen = 0;
  if (mbedtls_base64_decode(keyBuf, sizeof(keyBuf), &keyLen,
                            (const unsigned char*)keyB64.c_str(), keyB64.length()) != 0) {
    Serial.println("[auth] key base64 decode failed");
    return String();
  }
  time_t now = time(nullptr);
  if (now < 1700000000) {
    Serial.println("[auth] NTP not synced yet");
    return String();
  }
  const uint32_t et = uint32_t(now) + ttlSec;
  const String res = "products/" + pid + "/devices/" + dev;
  const String forSign = String(et) + "\nsha1\n" + res + "\n2018-10-31";

  unsigned char mac[20];
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  const bool ok = mbedtls_md_setup(&ctx, info, 1) == 0 &&
                  mbedtls_md_hmac_starts(&ctx, keyBuf, keyLen) == 0 &&
                  mbedtls_md_hmac_update(&ctx, (const unsigned char*)forSign.c_str(),
                                         forSign.length()) == 0 &&
                  mbedtls_md_hmac_finish(&ctx, mac) == 0;
  mbedtls_md_free(&ctx);
  if (!ok) {
    Serial.println("[auth] hmac failed");
    return String();
  }

  unsigned char signBuf[64];
  size_t signLen = 0;
  if (mbedtls_base64_encode(signBuf, sizeof(signBuf), &signLen, mac, sizeof(mac)) != 0) {
    return String();
  }
  const String sign((const char*)signBuf, signLen);
  return "version=2018-10-31&res=" + onenetEscape(res) + "&et=" + String(et) + "&method=sha1&sign=" + onenetEscape(sign);
}
