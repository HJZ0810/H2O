# H2O R10 · 硬件装配审查

> **首件实测缺陷（2026-11-25）**：J5（DHT11）针序与市面通用模块**相反**，直插会把模块电源反接，属致命缺陷；J6/J7（MQ）的 `DO`/`AO` 与板上 `AO`/空位**错开一位**，ADC 恒读 0；另有 `+5V` 主干在板内不通。三路传感器因此全部不可用，与固件无关。详见 [硬件缺陷与勘误](../../docs/R10/硬件缺陷与勘误.md)。

**工程入口**　[source/H2O-R10-YD254.epro2](source/H2O-R10-YD254.epro2) 包含原理图、PCB1 手柄和 PCB2 执行器。以源地 YD-ESP32-S3 的 25.40 mm 宽版为基础。RC2 维持 RC1 的工程源与制造基线。离线重建可回读，待原生重开。

**制造资料**　[PCB1 原始宽版 Gerber](gerber-baseline/wide-PCB1-after-silk-gerber.zip) 与 [PCB2 原始宽版 Gerber](gerber-baseline/wide-PCB2-probe-gerber.zip) 是 11 月 20 日的制造基线，两包均通过 ZIP CRC 和 SHA-256 检查。**这两包仍含原丝印。R10 新丝印待重新导出，目前不能据此下单 R10。**

**机械附件**　[SG90 切割 DXF](mechanical/SG90-adapter-cut-mm.dxf) · [1:1 配钻纸样](mechanical/SG90-template-1to1.pdf) · [装配与首件检查](mechanical/assembly.md)

## 修改了哪些工程对象

两板均在顶层加入登记队名、班级、5 人全名、板名、版本和年月。人员取自 24014 抽签行。PCB1 修正底丝印和尺寸线，尺寸线两端对准实际两排的 1 脚。PCB2 补上 SG90 转接说明，并把版本移出 J10 连接器包络。共 6 个文字对象和 1 个尺寸对象。脚本逐对象解包回读，并断言内层工程流其余记录逐行一致。归档缩略图和旧字体缓存沿用基线，实际中文字体、字框、铜皮及新丝印应在原生编辑器重开后检查并重新出图。

[工程改动逐对象记录](evidence/source-diff.json) · [PCB1 顶层](evidence/PCB1-top.png) · [底层](evidence/PCB1-bottom.png) · [PCB2 顶层](evidence/PCB2-top.png) · [底层](evidence/PCB2-bottom.png)。四图为离线走线与文字定位图，底层采用正面透视坐标，覆铜及封装图形省略。

## 从两份制造文件反量

坐标以正面左上为原点，X 向右、Y 向下，单位 mm。直接解析 GKO 闭合轮廓和 Excellon 镀孔，按同包飞针坐标定位零件和网络。机械铣孔位于 GKO，不应只在 DRL 搜索 Ø3.2。

| 核对项 | 结果 |
|---|---|
| PCB1 | 99.60 × 99.60，GKO 只有一条外框，PTH 最大 Ø1.40，未找到四角 M3 孔。J1/J2 各 22 孔，钻径 1.04999，节距 2.54，首尾 53.34。两排 Y 为 13.612 / 39.012，相差 25.400。 |
| PCB2 | 86 × 64。四个支撑孔中心为 (3.80,3.90)、(82.20,3.90)、(3.80,45.00)、(73.80,53.20)，Ø3.20。后两孔错位，与对称四角要求有差异。 |
| 舵机孔 | PCB2 四个 Ø3.20 铣孔，X=29.75 / 78.25，Y=12.00 / 22.00。孔矩 48.50 × 10.00。区分支撑孔、舵机孔、电机穿线槽。 |
| IDC | PCB1.J8 对 PCB2.J10，2×5，横纵节距均为 2.54。1 脚分别在 (44.72,5.08) 与 (21.08,50.23)。针号顺序对应 +5V、SERVO_PWM、MOTOR_BIN1、MOTOR_PWM、GND、MOTOR_AIN1、+3V3、MOTOR_AIN2、MOTOR_BIN2、MOTOR_B_PWM。 |
| 第 10 针 | PCB1 的飞针名 NET_9 通 J2.16；PCB2 的 NET_2 通 J13.7 和 J24.1。两板原理图网络均为 MOTOR_B_PWM。实物压线后逐芯测通断与相邻短路。 |

[两包完整 SHA 与轮廓尺寸](evidence/cam-audit.json) · [逐针钻孔 CSV](evidence/drill-net.csv) · [CAM 反量图](evidence/cam-readback.png)

## 对应官方硬件 30 分

采用 2026-11-23 09:49 的课程评分页和硬件页快照，两者均为 **E15 + F6 + I3 + G≤6**。分值是课程上限，本表记录可交证据和缺项。[课程评分页](https://www.kdocs.cn/l/clzMvHkySYP2) · [硬件要求](https://www.kdocs.cn/l/cp88DEqEI8U0) · [抽签依据与条款摘录](evidence/course.json)

| 项目 | 可核对的材料 | 尚缺的证据或分差 |
|---|---|---|
| E1 4 / E2 3 | 可编辑工程、两包宽版基线、两板队名与五人姓名对象。队名用“一氧化二氢”，归档组号为 24014。 | 两板原生重开、新丝印导出、按 24014-A/B 归档、两板打样与实物正反面。V1 与 V2 以实际交板为准。 |
| E3 5 / E4 1.5 / E5 1 / E6 0.5 | YD 核心板插接，板间 IDC 与执行器模块有对应针位。四张走线图供查阅。 | 核心板插接按实际档评。PCB1 缺 M3 孔，PCB2 支撑孔不对称；接口包络与 3 mm 板边、天线净铜、新丝印可读性及焊接待核。旧宽版 PCB1 历史 DFM 有 39 条警告。 |
| F 6 / I 3 | PCB1 提供屏、键、DHT11、两路 MQ 接口；PCB2 提供舵机及电机驱动。A、B 共用同一主控，官方表对应两板共用主控档 2/3。 | 抽签项为 DHT11、MQ-2、MQ-135、A04 SG90、A05 小马达风扇、OneNET。完整分值依通电与演示，MG996R 仅为兼容选项。 |
| G1 1.5 / G2 1.5 / G3 1 | 工程保留双面铺铜和过孔。可由 J8/J10 核对 5 V、3.3 V、共地及开关控制。 | 铺铜连通、边缘缝合须按原生重铺确认。TB6612 共地驱动不具备光电隔离证据。5 V 单芯去、单芯回，执行器联合启动、压降、保护器件和独立供电按实物与电路逐项取证。 |
| G4 1 / G5 1 | 可加工转接件、1:1 模板、手柄轮廓、双板孔位表。 | 转接件装机和扫掠视频、握持理由与孔边距仍缺。现有轮廓不支持直接申报 G5 满分。 |

工程归档所需的采购基础数据可回溯至[宽版板载 BOM](../sourcing-r9/data/pcb-bom.csv)。专属于 R10 的两板原理图 PDF、带真实价格渠道的 XLSX、订单、实物照片及逐板自测表，按实际导出或实测补齐。V1 规定两块板均未打样时 H ≤12、总分封顶 70；V2 规定任一板缺队名、班级或成员姓名时 E2 为 0 并额外扣 2 分。

## 复核入口

依次运行 `python audit/source_candidate.py`、`python audit/measure.py`、`python audit/adapter.py`、`python audit/board_views.py`。脚本从自身位置定位输入，DXF / 图表生成使用 ezdxf 与 matplotlib。原始 zip SHA 来自上级 [wide_candidate](../R9-1-4-RC1/verification.json)。[typed EasyEDA 状态](evidence/easyeda-health.json) 显示 CLI/daemon v1.16.0，连接器窗口为零。RC2 在独立目录复跑工程对象、制造文件、SG90 DXF 和板图检查。[原生重开记录](evidence/native-recheck/v4-open-archive.txt) 显示桌面桥可见，但编辑器命令空间未就绪。R10 原生 DRC、DFM、新丝印 Gerber、样机及带载实测尚待完成。
