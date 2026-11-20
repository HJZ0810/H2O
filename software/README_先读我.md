# Arduino R9 固件 · ThingsCloud 与智联迷城

`1.4.0-r9-outline` 对应 [R9.1.3 双板工程](../hardware/R9-10P/ESP32-handheld-R9-10P.epro2)。只给手柄的 ESP32-S3 烧录，执行板的 TB6612 和 MG996R 由十芯排线上的控制信号驱动。电压检测已按本版需求取消。

## 编译、烧录和配网

1. 在 Arduino IDE 打开 [ExamHandheld.ino](ExamHandheld/ExamHandheld.ino)，安装 Espressif ESP32 核心 **3.3.12**，选择 **ESP32S3 Dev Module**。板型设置为 USB CDC 启用、16 MB Flash、QIO、OPI PSRAM、3 MB APP + FAT。Windows 也可用 `powershell -File software/tools/build.ps1` 直接使用随仓库的库。
2. 拔掉手柄的外接 5 V，把 SW10 按到启用档，再通过 ESP32-S3 开发板 USB 烧录。可以在 Arduino IDE 上传，或使用 [发布二进制](binaries/) 和 `powershell -File software/tools/flash.ps1 -Port COM5`，其中端口填设备实际使用的 COM 号。
3. 初次启动或按住 F1 开机进入配网。手机连接屏幕显示的热点，用屏幕给出的密码访问 `http://192.168.4.1`。
4. 填写 Wi-Fi。智联迷城使用教师端登记的组号和 MQTT 密码。ThingsCloud 使用 **R9 手柄实物**设备的「连接」页所列的 MQTT 接入点、AccessToken 和 ProjectKey。密钥输入项留空时保留该设备上已有的值。保存后手柄重启。

| 部位 | 接口与 GPIO |
| --- | --- |
| ST7735S 1.8 英寸屏幕，横屏 160 × 128 | J3，CS 15、DC 2、RST 21、MOSI 13、SCK 14，背光接 3.3 V |
| 三个传感器 | J5 DHT11 GPIO5，J6 MQ-2 AO 经分压入 GPIO1，J7 MQ-135 AO 经分压入 GPIO4 |
| 答题按键 | F1 GPIO7，F2 GPIO8，上/下 GPIO11/12，左/右 GPIO16/17，OK GPIO18 |
| 执行板 | MG996R GPIO42，TB6612 A 路 PWM/IN1/IN2 GPIO41/40/39，B 路 GPIO48/38/43 |

IDC 红边对两端的 1 脚，完整针序见 [装配说明](../docs/R9/十芯直连与直插装配.md)。

## 智联迷城

- 菜单选择「开始 / 继续考试」。手柄先同步本组进度，由服务器为新场次抽题。当前线上配置每场 20 题，每题首次答对得 10 分。
- 上下滚动题干，左右移动光标，F1 勾选，F2 清空已选项，OK 提交。多选题可勾多项。长按 OK 使用锦囊。看完判题和解析后按 OK 继续。
- 在配网页选择与当前题库对应的私有答案 JSON。主菜单短按 F1 自动演示整场，答题中长按 F1 从当前进度演示。任意键按下可停止演示。题号、题型、题干及选项指纹全部匹配时，固件才按该题答案勾选并提交。
- 协议顺序为 `hello`、`resume`、`start`、`story_next`、`answer`、`continue`。服务器核对组号、会话 ID、进度和题号，比较答案集合并返回得分和解析。掉线恢复时，手柄向服务器重新同步进度。

演示答案由 [make_demo_answers.cjs](tools/make_demo_answers.cjs) 从授权取得的本地题库生成，输出文件只在需要演示的手柄上导入。

## 中文显示

固件将 UTF-8 解码成完整 Unicode 码点。常用字直接从板载压缩字库解码到画布。遇到缺字时，后台向服务器请求该码点的字形轮廓，由 ESP32-S3 实时生成点阵，缓存最近使用的 96 个字。等待期间显示空框，字模生成后刷新当前页，折行位置保持稳定。

生成的字模统一为 12 × 12 像素、16 级灰阶。数据逐行从上到下，每字节高四位对应左侧像素。屏幕驱动接收已绘好的 RGB565 帧，题库可以直接发原始 UTF-8 字符串。曲线、扫描填充、像素位序与代码见 [汉字实时取模与显示](汉字实时取模与显示.md)。 [原取模记录](https://blog.csdn.net/hjz0810/article/details/159386848)

服务器轮廓库覆盖 Unicode 17.0 的 102,998 个统一及兼容汉字，另含 `〇`。每个码点使用默认字形。字库取自 Jigmo 2025-09-12，使用 CC0 1.0。生成工具为 [generate_han_outlines.py](tools/generate_han_outlines.py)，覆盖和校验值见 [字库清单](validation/r9-han-outline-atlas.json)。 [Jigmo 字库](https://kamichikoichi.github.io/jigmo/) · [Unicode 字符定义](https://www.unicode.org/Public/17.0.0/ucd/UnicodeData.txt)

固件默认从 `Board.h` 中的 `HanOutlineUrl` 读取静态轮廓库。服务器需要支持 HTTP Range。固件先以 `8 × Unicode 码点` 读取索引，再读取索引指向的轮廓记录。可以通过 `test.ps1 -GlyphAtlas <han-outline.bin>` 将同一字库用于无硬件屏幕测试。

## ThingsCloud

设备类型的 13 项属性见 [控制台导出的 TSL](ThingsCloud-TSL.json)。设备通过 ThingsCloud 公有云普通 MQTT 接入，使用端口 1883。设备发布 `attributes`，订阅 `attributes/response` 和 `attributes/push`。周期上报每 120 秒一次，动作改变会提前回报实际输出设定。智联迷城走独立 MQTT 连接，传感器上报周期为 5 秒。 [ThingsCloud MQTT 协议](https://www.thingscloud.xyz/docs/guide/connect-device/mqtt.html)

| 属性 | 说明 |
| --- | --- |
| `temperature`、`humidity`、`dht_valid` | DHT11 的 °C、相对湿度和本次采样状态 |
| `mq2_mv`、`mq135_mv` | 由板上的分压比换算得到的传感器 AO 毫伏值 |
| `mq2_valid`、`mq135_valid` | 已启用、完成采样且未超量程 |
| `motor_a_pct`、`motor_b_pct` | 0 至 100 的 PWM 占空比，0 停止。读写 |
| `servo_position` | −100 至 100 的目标朝向，0 为居中。读写 |
| `servo_enabled` | 舵机 PWM 开关。读写 |
| `servo_pulse_us`、`outputs_ready` | 手柄实发的 PWM 脉宽与驱动输出是否就绪。0 μs 表示停用舵机 PWM |

在设备详情的「属性」页可以下发如下 JSON。舵机脉宽范围为 1200–1800 μs，符合现版初装设定。局部属性更新可只包含需要调整的字段。云端控制期间发现连接中断，固件将输出归零。板上 SW20 仍可直接控制执行板启停。

```json
{"motor_a_pct":60,"motor_b_pct":40,"servo_position":-50,"servo_enabled":true}
```

手柄「风扇控制」页中，F1 切换电机 A、B，上下调速，左右调舵机，OK 停止。环境采样页显示三路数据，设备信息页显示两条 MQTT 链路的连接状态。

## 验证

详见 [本版对照与验证](validation/r9-outline.md) 和 [ThingsCloud 图文联调记录](validation/r9-thingscloud.md)。在支持 C++17 的 Windows 上运行 `software/tools/test.ps1` 可复现协议、按键、屏幕帧缓冲和执行板 PWM 模拟。提供只读取得的线上后端源码目录参数 `-ServerSource` 时，脚本同时运行抽题、判题和断线恢复。ThingsCloud 真云往返脚本为 [simulate_thingscloud.py](tools/simulate_thingscloud.py)，用专门的仿真设备连接参数执行。
