## H₂O R10 RC2 · 25.40 mm 双板验收候选

24014 组使用源地 YD-ESP32-S3 双 Type-C 宽版。综合手柄支持任务 A 人工闯关和任务 B 环境监测。独立监测固件用于单独验收任务 B。抽签配置为 OneNET、DHT11、MQ-2、MQ-135、SG90 和直流风扇。

### 下载

| 附件 | 内容 |
| --- | --- |
| [固件](https://gitee.com/hjz0810/H2O/releases/download/r10.0.0-rc.2/H2O-r10.0.0-rc.2-firmware.zip) | `handheld/` 与 `monitor/`。各含四段映像及 `flash_args`。 |
| [硬件](https://gitee.com/hjz0810/H2O/releases/download/r10.0.0-rc.2/H2O-r10.0.0-rc.2-hardware.zip) | 双板嘉立创工程、两份基线 Gerber、钻孔与针序、SG90 DXF 和 1:1 纸样。 |
| [网页与服务端源码](https://gitee.com/hjz0810/H2O/releases/download/r10.0.0-rc.2/H2O-r10.0.0-rc.2-web.zip) | 任务 A 成绩页、任务 B 面板与网关、项目档案页更新文件。档案页沿用已部署的历史静态资源。 |
| [构建清单](https://gitee.com/hjz0810/H2O/releases/download/r10.0.0-rc.2/release-manifest.json) · [SHA-256](https://gitee.com/hjz0810/H2O/releases/download/r10.0.0-rc.2/SHA256SUMS.txt) | 源提交、两套构建参数、逐段散列及附件校验。 |

### RC1 到 RC2

| 项目 | 变更与验证 |
| --- | --- |
| 综合手柄 | 本地停机先关闭执行器，再释放 OneNET 客户端及手动保持状态。匹配 MQTT SUBACK 后才显示已订阅。连续 9 项回归覆盖停机重入、离线、缺测、拒绝、超时及错序。串口停止输出 Task A 登录凭据。 |
| 独立监测 | 采样和联动移入独立任务。过滤饱和 ADC、无效 DHT11 和过期数据。记录已确认的 PWM 状态，两路执行器的手动覆盖分别计时。串口新配置在保存并重启后加载。YD 宽板及功能验证板均通过主机回归。 |
| 固件构建 | 构建隔离输入和输出，记录源码 SHA-256 与运行状态。拒绝同目标并发写入，失败或镜像不全时留存失败状态。使用 Arduino ESP32 Core 3.3.12 构建两套 25.40 mm 映像。 |
| 课程任务 A | 当前手柄状态机用人工按键路径回归 28 项，包含重复、超时和断线恢复。官方成绩以本组课程记录为准。 |
| 网关和项目网页 | 网关返回明确的鉴权错误，面板补齐断线期间的历史样本，区分设备只读和权限接口异常。桌面及手机获得 4 次模拟确认。RC2 下载和 RC1 档案分别可见。 |
| 硬件 | 复跑离线检查，回读 7 个版面对象。两包基线 Gerber 的排针孔 44/44 符合 25.400 mm 排距，两板 IDC 10/10 按针号对应。SG90 转接孔位回读通过。 |

![25.40 mm 排距及 IDC 反量](https://gitee.com/hjz0810/H2O/releases/download/r10.0.0-rc.2/H2O-r10.0.0-rc.2-hardware.png)

![任务 B 桌面面板与模拟控制](https://gitee.com/hjz0810/H2O/releases/download/r10.0.0-rc.2/H2O-r10.0.0-rc.2-dashboard.png)

### 首件、线上与课程验收

R10 源工程仍待嘉立创原生重开、DRC/DFM 和新丝印 Gerber 导出。附件的 Gerber 为旧丝印基线。打样前先完成原生审查，首件验证 SG90 安装、天线净空、IDC 通断、两个执行器及其带载 5 V。OneNET 默认候选通过 1883 接入，真实设备联调及适用的 TLS 证书待核对。项目档案页和监测面板通过本地浏览器验证，公网部署待源站连接恢复。

[RC2 工程与操作说明](https://gitee.com/hjz0810/H2O/tree/r10.0.0-rc.2/docs/R10) · [硬件核验记录](https://gitee.com/hjz0810/H2O/tree/r10.0.0-rc.2/hardware/R10) · [RC1 历史发布](https://gitee.com/hjz0810/H2O/releases/tag/r10.0.0-rc.1)
