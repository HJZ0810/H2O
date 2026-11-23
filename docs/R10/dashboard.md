# B 网关与环境监测面板交接

2026-11-23 · 本机候选版本。浏览器通过同源 `/h2o/api/control` 设置风扇和舵机，WS 仅供订阅指定设备的遥测。以下生产操作供现场负责人安排维护窗口时使用。

## 生产部署来源

| 对象 | 本次能够确认的内容 |
| --- | --- |
| 公网页面 | 对 `https://air-quality.luo-jin-ai.com/h2o/` 只读 GET 返回 200，HTML 经换行标准化后 SHA-256 为 `8b095078bd8beeb552834e68547fc40d41d0c7b418e3a7c0b483b9464edc36d7`，内容匹配仓库提交 `a952fc6` 的 `web/dashboard.html`。同站 `/h2o/dashboard.html` 返回 404，部署时应核对 `/h2o/` 实际使用的入口文件。 |
| 图表资源 | `/h2o/lib/chart.umd.min.js` 返回 200，SHA-256 `d2af8974e95271638772e9e9524db5b9a6f58d6ec2d5d781400447b4a31c681e` 与仓库一致。 |
| 遥测后端 | `59e7a69` 记录美服 broker、systemd 和 nginx `/mqtt` → `127.0.0.1:9002`。`77036f2` 记录 `/h2o/api/` 反代与 recorder。仓库脚本使用 `127.0.0.1:9003` 和 `/opt/h2o-bridge/history.json`。`software/validation/deployment.json` 的文件清单属于另一套服务。 |
| 源站核验边界 | 通过指定的 server-manager 尝试只读核对。server2 的 SSH 连接被关闭，server6 的中继 TLS 握手未完成。实际 nginx root/alias、systemd 单元及脚本摘要、broker ACL 和运行版本仍待从源站核对。公网 HTML 匹配仅限该静态资源。 |

## 弄清页面上的读数

页面显式显示正在查看的设备。输入其他名称时立即禁止发指令，按“切换设备”才清空旧数据、令牌和设定值，重新订阅并查询该设备权限。历史接口回放曲线和快照。断线期间记录器继续收到的数据在重载后补入曲线。只有当前连接收到所选主题的有效遥测，且未超过 20 秒，才显示实时在线。OneNET 状态取自这帧设备自报。缺测显示 `--`，曲线断开，数值 0 单独保留。控制回执和遥测读数各自更新，舵机卡片表示设备上报的设定角。

## HTTP 写入契约

官方文档要求 OneJSON 设备在线。服务端对固定 HTTPS 主机发送 `POST /thingmodel/set-device-property`，Header `authorization` 携带平台 API 凭据，JSON body 包含 `product_id`、`device_name`、`params`。属性格式是 `{"fan_level":55}` 或 `{"servo":120}`。网关只接受整数 0–100 或 0–180。平台 `code=0` 且设备 `data.code=200` 时页面才显示确认。异常、超时或缺少设备回执时，应先查遥测再决定是否重发。单个设备同时只处理一条指令，结束后冷却一秒。

官方原文可从文档中心查阅。核对正文时使用了同域文档服务 `/doc/v5api/article?path=1418` 和 `?path=1464`。[设置设备属性](https://open.iot.10086.cn/doc/aiot/fuse/detail/1418) · [平台 API 安全鉴权](https://open.iot.10086.cn/doc/aiot/fuse/detail/1464) · [项目绑定与鉴权范围](https://open.iot.10086.cn/doc/aiot/fuse/detail/1566)

## 网关配置

将 `r10_control.py` 放在 `recorder.py` 同目录。生产上使用已有的 Python 服务环境，recorder 与 WS 桥均需要 paho-mqtt 2.x，WS 桥还需要兼容的 websockets。MQTT 服务账号通过 `H2O_MQTT_USER`、`H2O_MQTT_PASSWORD` 注入。以下五项控制配置缺少任一项，HTTP 写入关闭，历史和遥测仍可独立运行。

| recorder 服务端变量 | 配置来源及作用 |
| --- | --- |
| `H2O_CONTROL_ORIGIN` | 准确的页面 HTTPS Origin，无末尾斜线。回环 HTTP 加端口仅供本机验收。 |
| `H2O_CONTROL_DEVICES` | 负责人确认的设备名，多个名称用英文逗号分隔。只接受字母、数字、下划线和短横线，长度 1–64。 |
| `H2O_ONENET_PRODUCT` | 该允许列表共用的实际产品 ID，由服务端固定。 |
| `H2O_ONENET_AUTHORIZATION` | 按设备项目绑定关系签发的最小范围、限时平台 API 授权串，提前安排续期。签名和访问密钥均留在受控服务环境。 |
| `H2O_OPERATOR_SHA256` | 新生成的高熵随机操作令牌的 UTF-8 字节 SHA-256，小写十六进制。使用密码管理器生成至少 32 字节随机值，通过独立渠道交付。服务端只配置摘要。 |

浏览器将操作令牌放在当前页面内存，通过 `Authorization: Bearer` 发往同源 HTTP 网关。清除、切换设备、令牌被拒和离开页面都会清空输入。页面只查询当前设备的控制能力，不下载允许列表。网关最多读取 512 字节的请求体，等待上限 2 秒。它校验 Origin、令牌、设备名和属性范围，回送明确的拒绝或超时状态。控制路由关闭跨域、缓存和重定向，生产反代应保留 `Origin`、`Authorization` 并限制写路由请求速率及请求体大小。日志与监控均应跳过认证头和请求体。建议对页面发送 `Content-Security-Policy: frame-ancestors 'none'`，避免网页控件被其他站点嵌入。

## 协调迁移与回退

1. 由现场负责人通过现行 server-manager 核对源站、静态根目录、nginx 生效配置、systemd `FragmentPath` 与实际启动脚本，按摘要备份页面、服务代码、ACL 和历史数据。记录受影响板卡、当前固件、设备端能否更新凭据以及维护窗口。凭据备份存放在受控位置。
2. 与设备持有人协调并行凭据。为设备配置各自的遥测发布权限，给 recorder 和 WS 桥配置只读遥测权限。逐台验证新凭据持续上报、客户端 ID 不冲突、镜像命令通道已移除。现场负责人确认切换完成后撤销旧共享凭据和旧写权限。若旧固件仍依赖这些权限或持有人尚未确认，维持当前实物连接，暂缓上线。
3. 在独立端口用副本预检新服务。保持旧数据文件原样，验证历史、所选设备 WS 和默认只读状态。按源站实测位置配置 `/h2o/`、`/h2o/lib/`、`/mqtt` 和 `/h2o/api/`。API 反代需去掉该前缀，例如外部 `/h2o/api/control/status` 到本机 `/control/status`。只向公网开放 HTTPS 反代。确认现网旧版页面缓存与旧匿名写通道的退出顺序，再由负责人统一切换。
4. 从指定来源发放新的操作者令牌并加载允许列表与云端授权串。上线后的实物动作验收需设备持有人在场并另行安排。未知回执先观察遥测，避免重复指令。
5. 回退控制时先从反代禁用写路由或以无控制配置重载 recorder，保留 WS、历史和设备上报。回退页面时使用经过确认的只读版本。保留历史文件和迁移后的 ACL，避免重新打开旧命令通道。

## 验收记录与复现

在仓库根目录执行以下两条本机命令。第一条只用 Python 标准库，第二条使用已有 Playwright 和 Edge。测试 HTTP 服务绑定随机回环端口，上游写调用由内存替身接收，WS 帧从浏览器夹具发送。

```powershell
python -B software/tools/r10_gateway_test.py -v
python -B software/tools/r10_dashboard_test.py
```

网关 4 组测试通过，覆盖只读与授权边界、属性名及整数范围、忙时抑制、OneNET 两层回执、遥测订阅限制、retain 消息和记录器时间戳。桌面 1440×900 与手机 390×844 两组普通界面验收通过，验证错误令牌、风扇 55%、舵机 120°、合法零值、缺测、设备切换、旧历史迟到、跨设备 WS 帧、只读设备和令牌清除。浏览器测试拒绝外部请求。生产凭据、broker 至实物的完整连接和实际机械行程保留给现场验证。

证据在 `C:\Users\Administrator\Pictures\Agent截图\H2O-R10-B`，`00-before-history.png` 为修复前历史快照造成的双在线，`01-history.png` 为同类快照的正确状态，`02-live-desktop.png` 和 `02-live-mobile.png` 为 mock 控制后的页面，`03-readonly-mobile.png` 为切换后的只读空态，`04-recovered-history.png` 为断线后补齐记录，`05-gateway-unavailable-mobile.png` 为权限接口异常提示。
