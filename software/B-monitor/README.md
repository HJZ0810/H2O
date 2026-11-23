# R10 任务 B 独立固件

运行 `../tools/build-r10.ps1 -Target monitor -Board yd2540`。自研宽版用于三个真实传感器验收。验证板只提供 DHT11 与 LED 输出模拟。


抽签配置（2026-11-18，24 级物联网工程 4 班 · 一氧化二氢）：

| 类别 | 编号 | 器件 |
| --- | --- | --- |
| 传感器 ×3 | S01 / S05 / S06 | DHT11 温湿度 · MQ-2 烟雾可燃气体 · MQ-135 空气质量 |
| 执行机构 ×2 | A05 / A04 | 直流小马达 + 风扇叶 · SG90 舵机 |
| 云平台 | C01 | OneNET（中国移动） |

## 引脚

| 信号 | R9 自研板 | 验证板 |
| --- | --- | --- |
| DHT11 | IO5 | IO8 |
| MQ-2 | IO1（ADC1） | 不存在，IO1 为光敏 |
| MQ-135 | IO4（ADC1） | 不存在 |
| 风扇 PWM | IO41（in1=IO40, in2=IO39） | IO4（板载 LED） |
| 舵机 | IO42 | IO6（板载 LED） |

ADC 只用 GPIO1~10（ADC1）。ADC2（GPIO11~20）与 Wi-Fi 冲突，联网后读数不可用。

## OneNET 物模型

产品协议 MQTT、数据格式物模型。功能点标识符必须与下表完全一致，否则 property/post 会被平台丢弃。

| 标识符 | 名称 | 类型 | 读写 | 单位 |
| --- | --- | --- | --- | --- |
| `temperature` | 温度 | float | 只读 | °C |
| `humidity` | 湿度 | float | 只读 | %RH |
| `smoke` | 烟雾相对值 | int32 | 只读 | 0–1000 |
| `air` | 空气质量相对值 | int32 | 只读 | 0–1000 |
| `fan_level` | 风扇档位 | int32 | 读写 | % |
| `servo` | 舵机角度 | int32 | 读写 | ° |

`servo` 和 `fan_level` 均为读写属性。云端可以设置指令角度与风扇档位。
风扇的开关用 `fan_level` 表达（0 = 停，1~100 = 转速百分比），避免布尔量与档位两套语义打架。

## 接入参数

| 参数 | 值 |
| --- | --- |
| 地址 | `mqtts.heclouds.com:1883` |
| clientId | 设备名称 |
| username | 产品 ID |
| password | 设备端签发的限时 token |
| 上报 | `$sys/{pid}/{dev}/thing/property/post` |
| 下发 | `$sys/{pid}/{dev}/thing/property/set` |
| 应答 | `$sys/{pid}/{dev}/thing/property/set_reply` |


## OneNET 鉴权：密码是 token 不是密钥

实测踩到的坑：把「设备密钥」直接当 MQTT 密码填，服务端一律回 `CONNACK 4`
（用户名或密码错误），很容易误判成设备名/产品 ID 填错。

OneNET 要求的是用设备密钥算出的 **token**：

```
resource           = products/{pid}/devices/{dev}
stringForSignature = et + "
" + method + "
" + resource + "
" + "2018-10-31"
sign               = base64( HMAC-SHA1( base64decode(设备密钥), stringForSignature ) )
password           = version=2018-10-31&res={resource}&et={et}&method=sha1&sign={sign}
```

`et` 是绝对过期时间，所以**设备必须先 NTP 对时**；时钟不对时算出的 token 必然过期，
报错同样是 CONNACK 4。固件里 `OneNetAuth.h` 实现该算法，开机对时后再算。

PC 端可用 `software/tools/onenet_token.py` 复算校验，用 `mqtt_probe.py` 只发 CONNECT
看 CONNACK 返回码，能把「凭据错」和「网络不通」区分开。

## 物模型导入模板

物模型用平台「导入物模型」的 JSON 模板一次性写入，模板与本次使用的文件见
`software/tools/thingmodel-h2o.json`。导入会覆盖产品原有全部功能点，导入后须点「保存」才生效。

## 凭据配置

凭据不写进代码，全部放 NVS。串口 115200 下依次输入：

```text
cfg ssid <WiFi名>
cfg wifi <WiFi密码>
cfg pid  <产品ID>
cfg dev  <设备名称>
cfg key  <设备密钥>
save
reboot
```

`show` 查看当前配置（密钥只显示长度）。

## 联动逻辑

在 ESP32 端执行，串口会打印 `[link]` 记录，便于答辩时逐条对照：

1. 温度 ≥ 30 °C 开风扇，30→40 °C 之间档位从 40% 线性升到 100%；
2. 烟雾 ≥ 300 立即把风扇顶到 80% 以上（排烟优先）；
3. 空气质量 ≥ 500 舵机转到 90°（模拟开窗），低于阈值回到 0°；
4. 每次成功的手动设置仅保持对应执行器 30 秒，另一路继续自动联动。

阈值在 `BCfg.h`。独立采样任务每轮间隔至少 2 秒，Wi-Fi 未配置、MQTT 断线或正在重连时继续运行。若任务创建失败，主循环进入本地采样降级模式。

每路 MQ 检查 16 次采样，任一原始码 ≥4090 或任一读数 ≥3050 mV 即丢弃本轮该通道。DHT11 的温度和湿度须同时为有限数，且分别在 0–50 °C 和 0–100 %RH 内。失效字段从本轮上报中省略。温度和烟雾各自保留最近一次有效采样对应的风扇需求，两者取较高档。冷启动需求为零。空气通道失效时保留已写入的舵机设定。恢复有效采样后重新判断阈值。

PWM 写入失败后对应设定记为未知，云端收到 503，遥测省略该字段。已挂载的通道在后续采样周期重试。驱动初始化失败的通道需排查并重启。两属性命令执行到第二项失败时，第一项已写入的设定仍会保留并上报。读数表示 PWM 驱动接受的设定，实际转速和舵机行程需用实物核对。

## 标定说明（报告必写）

MQ 系列按 3.3 V 参考线性折算为 0–1000 相对值，用于趋势与阈值判断。达饱和判据的采样记为无效。
实测时应记录：洁净空气基线、点燃线香后的上升曲线、阈值触发点，并在报告中写明
「读数未做温湿度补偿，与标准仪器的偏差约 ±X%」。
DHT11 的精度为 ±2 °C / ±5 %RH，采样间隔 ≥2 s（本固件 2 s 采一次、10 s 上报一次）。

## 编译

```bash
# R9 自研板
arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi" \
  --libraries ../software/libraries --library "<DHT sensor library>" \
  --library "<Adafruit Unified Sensor>" BMonitor

# 验证板（无 MQ 传感器、无功率级）
# 追加 --build-property "compiler.cpp.extra_flags=-DBOARD_VERIFY"
```

## 已验证 / 未验证

- 已验证：验证板上 DHT11 读数正常（26.0 °C / 21 %RH），模拟量通道可读，联动能触发风扇档位、串口 `[link]` 有记录。
- 未验证：OneNET 实际连接与上报、云端下发控制、MQ-2 与 MQ-135 的真实读数、舵机与风扇的负载动作。

## 与任务 A 融合

任务 A 与任务 B 现在是**同一份固件** `software/ExamHandheldV2`：

- 传感器与执行器层复用任务 A 的 `Sensors` / `Actuators`，不重复实现；
- 新增 `OneNet.h`（接入层）与 `OneNetCfg.h`（NVS 凭据 + 串口配置）；
- 菜单第 6 项「云端监测」切换：开启后每 10 秒上报一次并跑联动，与闯关链路并存。

串口命令（115200）：

```
on <产品ID> <设备名称> <设备密钥>   写入 NVS
onshow                              查看状态
onon / onoff                        开/关云端监测
```

实测：开启后 `[onenet] connected` 与 `[net] ... online=1 grp=240401` 同时成立，
两条链路互不影响。

## 独立监测回归

在仓库根目录运行 `python -B software/tools/test_bmonitor_io.py`。用例以两套板型配置编译固件中的采样、联动、上报和云端应答代码，以可控 ADC、时钟及 PWM 驱动注入故障，同时检查正常采样、饱和、传感器恢复、30 秒手动保持跨时钟回绕及 503 回执。`BOARD_VERIFY` 的光敏输入不会作为 MQ 数据上报。
