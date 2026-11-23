# 24014 组 H2O R10 范围与交付边界

2026-11-23 工作树核查。H2O 的正式考核链分为两条。A 由本组终端接入课程迷城并由课程服务器记分。B 由本组自制板接入抽签指定的 OneNET。手柄与执行板共用主控，两块 PCB 的工程资料和实物证据都要保留。各评分项的验收方法见 [评分矩阵](requirements.md)。

## 哪些项目进入考核链

下表路径以 H2O 仓库为基准。三个独立项目目录位于本机 `D:\PRO`。判断依据是调用入口、协议和数据归属，未对线上服务做连通性或权限测试。

| 对象 | 可复查的代码或配置 | 对 24014 的作用 |
|---|---|---|
| 正式智联迷城 | 课程镜像④给出课程主机及 `/iot`。`software/ExamHandheldV2/Config.cpp` 的默认主机对应同一地址，`ExamNetwork.cpp` 使用按组分开的 `/iot/game/<组号>/up`、`down`。`ExamCore.cpp` 消费下行。 | A1–A3、A5 直接依赖。设备须与本组身份、人工操作及正式成绩记录对应。仓内 `software/tools/iot-broker.js` 是可独立运行的代理实现，不能据此推断线上版本或成绩。 |
| 成绩前端与字形资源 | `software/A-mazecity/web/index.html` 以同源相对路径调用 `/student/api/login`、`/api/data`。`ExamHandheldV2/RemoteGlyphs.cpp` 按字节范围读取 `Board.h` 指定的字形资源。 | A4 需正确的同源部署或反代。稀有字联网补字影响 A3 可读性，常用字显示与成绩 API 分开验证。 |
| OneNET | `software/B-monitor/BMonitor.ino` 与 `software/ExamHandheldV2/OneNet.h` 各有设备鉴权、属性上报、属性设置与应答。`software/tools/thingmodel-h2o.json` 定义温湿度、烟雾、空气质量、风扇与舵机属性。 | B1、B2 正式平台。独立 B 固件与手柄集成模式是两个入口，验收应说明实际烧录哪一个。两者无需同时使用相同设备身份。 |
| 镜像前端 | `ExamHandheldV2/Mirror.h` 在配置本地参数后通过 TLS 发布 `h2o/<设备>/telemetry`。`software/tools/ws_bridge.py` 转发遥测，`web/dashboard.html` 用 WebSocket 订阅并向 `/h2o/api/history` 读历史，用本地 Chart.js 绘图。 | B 的辅助展示，可与 OneNET 状态交叉核对。页面经同源 HTTP 网关向 OneNET 下发，授权凭据与设备白名单在服务端配置，WebSocket 仅订阅遥测。镜像在线和历史曲线均不能证明 OneNET 收到本次上报、执行器已动作或 B3 满档。 |
| ThingsCloud | `software/ExamHandheldV2/ThingsCloud*.cpp`、`software/ThingsCloud-TSL.json` 与 `software/validation/r9-thingscloud.md` 留有另一平台实现及模拟设备联调记录。 | 历史可选实现。⑥给 24014 登记 C01，正式 B 采用 OneNET。 |
| esp32-mqtt-games | `esp32-mqtt-games-20261102/esp-domain.conf` 代理游戏站和其 MQTT 入口，`board-receiver.js` 接收嵌入游戏页父窗口的板键事件。 | 独立游戏输入链。H2O A/B 所查入口未调用该站游戏 API 或接收器。排除其服务器、游戏固件和回放素材。 |
| iot-city | `iot-city-20261021/services/api/src/board/index.ts` 实现 `/api/board/sessions`、关卡、答题及排行。`ecosystem.config.cjs` 启动该项目的 `dist/server.mjs`。 | 独立的板卡 HTTP 会话协议。H2O 正式闯关使用课程 MQTT，上述路由未进入所查入口。其成绩不计入 A5。 |
| nexus-city | `nexus-city-remaster-20261023/engine/ra2/package.json`、`deploy/release-ra2.sh` 给出 RA2 引擎和独立发布入口，工作树另保留 iot-city 服务结构。 | 所查 H2O 固件与两个前端均未引用该引擎、部署脚本或游戏会话。基线归档与重制项目均排除。 |

## official 交付审查

②要求一个 `IOT-Final-24014` 仓库。此处的 official 指面向教师的手动考核交付。审查时的工程树按 `hardware/`、`software/`、`web/` 组织，未见可按官方目录直接验收的完整包。当前 `dist/智联迷城-Arduino-v1.1.0-完整交付.zip` 的 ZIP 目录含 AutoDemo 和答案相关条目，缺报告、视频和 OneNET 交付内容。不得以“完整交付”文件名认定可交。

| 官方位置 | 可选入内容与仍待补齐的材料 |
|---|---|
| `README.md` | 组号、本组经授权的成员分工、双板共主控形态、确切硬件版号、编译与配网步骤、A/B 入口、可访问的视频。核对公开可读或教师已获私库权限。 |
| `H-hardware/` | 从同一实物版本选取两板原理图、正反布局、Gerber、BOM XLSX、EDA 源、采购记录和逐板自测表。每板正反面及上电实拍，按组号命名。RC1 候选源、10P 或旧 Gerber 不得混作同版。 |
| `A-mazecity/` | 手动闯关源码、必要依赖、成绩网页及同源部署说明。`software/A-mazecity/web/` 只有网页，需另列出实际固件入口。 |
| `B-monitor/` | 实际使用的 OneNET 固件、物模型、前端源码与访问入口、仅 24014 的抽签摘录。写清 WS 镜像遥测入口及同源 HTTP 控制入口。 |
| `report/`、`video/` | 综合设计报告 PDF + DOCX，一份演示视频或 `video/README.md` 中的可访问链接。报告和视频的细则见评分矩阵 R1–R7。 |

## 安全与真实性放行条件

- 正式包按文件白名单审查，同时查看压缩包内部、配置入口与实际可调用功能。研究源码和旧归档仍有 `AutoDemo`、答案配置与复现工具。R10 正式候选关闭自动答题入口及串口按键注入，验收交付继续排除演示答案、回放、注册/重置、教师工具及其打包产物。保留人工协议处理、本人登录、本人查询等必要功能。
- 抽签镜像⑥含全班登记，公开交付仅摘取本组必要字段。成绩网页可渲染多组排行，演示只保留本组名次和本组明细。公开截图、录屏和导出件须处理其他组信息，并遮挡 MAC、密码、设备密钥、token、Wi-Fi 配置及浏览器请求头。不得整包复制原快照、工具日志、历史截图、私有配置或含密钥的二进制。
- 课程 MQTT 及默认 OneNET 1883 固件使用普通 WiFiClient。OneNET 可本地配置 PEM 后重新构建 8883 版本。采集网络日志时须保护鉴权数据。`ws_bridge.py` 校验遥测主题格式，但格式校验本身不验证设备归属。公开镜像应在服务侧限制可读设备及历史记录，拒绝通配订阅和控制消息，页面筛选不能替代授权。镜像密钥留在本地或受限服务端。
- 本次仅审查公开资料、本地源码和归档目录。正式进度取自教师认可的本组记录，动作和延迟取自本组实物。正式服务、其他设备、教师接口及任何成绩写入均在本次操作范围外。
