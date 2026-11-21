#pragma once
#include <Arduino.h>
#include <esp_mac.h>

// 任务 A 的 MQTT 接入密码，必须由固件按“组号 + MAC”算出，不能手填。
// 算法与服务器端 gen_pass.py 一致：
//   1. MAC 去分隔符，得 12 位十六进制
//   2. SUM   = 组号(十进制) + 2026
//   3. 12 位串每 4 位一组转十进制，得 M1 M2 M3
//   4. TOTAL = SUM + M1 + M2 + M3
//   5. 密码  = TOTAL mod 2^24，转 8 位十六进制（不足补 0，小写）
// 例：99002 + 64:E8:33:7F:4D:B4 -> 000270bf
namespace auth {

// 必须在 WiFi.mode() 之前也能拿到 MAC：WiFi.macAddress() 在 Wi-Fi 未初始化时
// 返回 00:00:00:00:00:00，用它算密码会得到完全错误的 8 位串。
// esp_read_mac 直接读 eFuse，开机任何时刻都有效。
inline String deviceMac() {
  uint8_t mac[6] = {0};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char out[18];
  snprintf(out, sizeof(out), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(out);
}

inline String normaliseMac(String mac) {
  mac.replace(":", "");
  mac.replace("-", "");
  mac.replace(" ", "");
  mac.toUpperCase();
  return mac;
}

// 组号必须是纯十进制数字，否则返回空串（此时退回配置里手填的密码）。
inline bool numericGroup(const String& group) {
  if (group.isEmpty() || group.length() > 9) return false;
  for (size_t i = 0; i < group.length(); i++)
    if (!isDigit(static_cast<unsigned char>(group[i]))) return false;
  return true;
}

inline String derivePassword(const String& group, const String& mac) {
  const String hex = normaliseMac(mac);
  if (hex.length() != 12 || !numericGroup(group)) return String();
  uint32_t total = static_cast<uint32_t>(strtoul(group.c_str(), nullptr, 10)) + 2026u;
  for (int i = 0; i < 12; i += 4) {
    char part[5];
    memcpy(part, hex.c_str() + i, 4);
    part[4] = 0;
    total += static_cast<uint32_t>(strtoul(part, nullptr, 16));
  }
  total &= 0xFFFFFFu;  // mod 2^24
  char out[9];
  snprintf(out, sizeof(out), "%08x", static_cast<unsigned>(total));
  return String(out);
}

}  // namespace auth
