# H2O R9 全项目复核

2026 年 11 月 20 日 · 嘉立创 EDA 4.1.78 · 发布工程 R9.1.3，固件 1.4.0

**结论**　已找到能复现插不上故障的开发板规格，并在嘉立创独立副本修正排母孔阵、受影响的布线和执行板支撑孔。两块候选板已通过保存重开后的原生 DRC，制造文件也已独立反量。射频布局、实物装配和联动带载测试仍待闭环，当前状态是装配验证候选。

## 排母故障

| 对象 | 物理针数 | 单排脚距 | 两排中心距 | 板身宽 |
| --- | ---: | ---: | ---: | ---: |
| H2O R9.1.3 PCB1 发布 Gerber | 2×22 | 2.54 mm | **22.86 mm** | — |
| 乐鑫 ESP32-S3-DevKitC-1 v1.1 | 2×22 | 2.54 mm | **22.86 mm** | 25.40 mm |
| 源地工作室 YD-ESP32-S3 双 Type-C 版 | 2×22 | 2.54 mm | **25.40 mm** | 27.94 mm |
| 嘉立创宽版候选 PCB1 | 2×22 | 2.54 mm | **25.40 mm** | — |

原先按乐鑫版孔阵设计，并把它用作 N16R8 开发板的装配基线。N16R8 标识的是模组存储规格，存在不同宽度的成品板。宽版只要一排针与旧板对齐，另一排就会整排偏开 **2.54 mm**。公开资料证实了这一失配，手头实物的板号和排距仍待首件确认。[乐鑫机械图](https://dl.espressif.com/dl/schematics/esp_idf/DXF_ESP32-S3-DevKitC-1_V1.1_20220429.pdf) · [源地工作室机械图](https://github.com/vcc-gnd/YD-ESP32-S3/blob/main/5-public-YD-ESP32-S3-Hardware%20info/ESP32-S3-Metric.pdf) · [厂家针序](https://github.com/vcc-gnd/YD-ESP32-S3#%E6%8E%92%E9%92%88)

候选固定 J1，J2 在正面坐标系下向 +Y 移 2.54 mm。两排仍同向摆放在背面，单排首尾中心距为 53.34 mm。已重布受影响的信号及地、电源局部铜线，并从候选 Excellon 核实所有 **44 个 Ø1.05 mm** 金属化钻孔。两排排母采用 Samtec SSW-122-01-F-S。[排距叠合图](images/esp32-header-overlay.png) · [Samtec 推荐焊盘图](https://suddendocs.samtec.com/prints/ssw-svs.pdf)

## 两板装配和位置

| 项目 | 数据与视觉复核 |
| --- | --- |
| PCB1 手柄 | Gerber 外框 99.60 × 99.60 mm，已核 29 个器件锚点、面别、角度和 CAD 包络。双排母的背面位置、J3 屏幕插座、J8 开口、六个侧边端子和背面电阻可在原生 3D 对照。 |
| PCB2 执行板 | Gerber 外框 86.00 × 64.00 mm，已核 9 个器件锚点、面别、角度和 CAD 包络。电机线槽 6.00 × 3.00 mm，两扎带孔 Ø2.50 mm。 |
| MG996R | PCB 四孔 Ø3.20 mm，孔矩 48.50 × 10.00 mm，中心 X=29.75/78.25 mm、Y=12.00/22.00 mm。厂家机壳 40.7 × 19.7 × 42.9 mm，安装耳、支撑高度和扇叶扫掠按选定批次实装核验。 |
| PCB2 后部支撑孔 | 旧板右下 Ø3.20 mm 孔与 J24 端子包络重叠 **0.59 mm**。左下孔边到 J10 仅 **0.24 mm**。按 M3 Ø7 垫片校核，候选中心取 (3.80, 45.00) 和 (73.80, 53.20) mm，孔径 Ø3.20 mm。垫片与相邻器件包络净空分别 **1.20 mm**、**0.57 mm**。外壳或支撑板要使用修订后的孔坐标。 |
| 电机驱动插接 | J12/J13 两排各 8 针，中心距 12.70 mm，单排节距 2.54 mm，逐针与指定 Pololu #713 对照。第三方 TB6612 模块的供电、STBY 顺序和孔距要按对应件复核。 |
| 屏幕、传感器与电源模块 | 已核接插件尺寸、连接方向、焊盘和 CAD 包络。外接件本体厚度、USB 插头外壳、端子工作空间、Type-C 触发板的实体位置和与背面排母的高度叠装待实物验证。 |

[固定件净空图](images/actuator-mount-clearance.png) · [候选板原生 3D](images/candidate-native-3d.png) · [Pololu 尺寸及针序](https://www.pololu.com/file/0J1724/tb6612fng-dual-motor-driver-carrier-dimensions.pdf) · [TowerPro MG996R](https://towerpro.com.tw/product/mg996r/) · [Bossard M3 垫片尺寸图](https://www.tme.eu/Document/515331066d6ede6b62c48015b5dff1fd/BN_602_1_Bossard_1.pdf)

## 电路与固件

- **逐针验证**　五页原理图与 PCB、发布飞针数据、钻孔按位号与脚号对账。PCB1 **124** 个、PCB2 **43** 个焊盘，坐标与网络差异 **0**。飞针导出中的 `NET_n` 以同组端点逐一重建核验。候选再次回读，两板焊盘网络仍与同一基线一致。
- **板间连接**　J8→J10 的十脚信号全部按针号直通。确认 2×5 接头采用横纵均 2.54 mm 的针阵，线束的 1.27 mm 是线芯间距。实际压头仍需按针号通断与防呆方向验收。[XFCN 接头厂家尺寸图](https://uploadcdn.oneyac.com/attachments/files/brand_pdf/xfcn/C9/5C/4-240Z415332Q53.pdf)
- **外设逻辑**　两组 22 kΩ/33 kΩ 分压的标称比例为 0.60。MQ 模块 AO 为 5.00 V 时，ADC 端标称 3.00 V。DHT11 接口按已含上拉的 3.3 V 模块核对。SW10 控制 ESP32 EN，SW20 控制 TB6612 STBY 和舵机 PWM 路径。两处开关的逻辑停用与实际断电是不同状态，完整断电由上游 5 V 控制。
- **固件**　22 个业务 GPIO 全部核对到板上功能。ESP32-S3 以 16 MB Flash、8 MB OPI PSRAM 配置完成全编译，程序 1,332,394 字节，静态全局变量 59,364 字节。28 个协议测试及既有按键、防抖停机、配置页面、ThingsCloud/PWM、ST7735 帧缓冲与字形测试通过。首件还需要实际烧录和外设联动。
- **普通 DRC 与制造提示**　发布基线与改版两板原生 DRC 各为 0。另跑的 DFM 给旧版 PCB1/PCB2 分别报告了 42/15 条警告，包括回流支线宽度、丝印、走线局部几何等。候选移开压住焊盘的 J22、H₂O 两处丝印，PCB1 的压焊盘警告清零，另有 39 条 DFM 警告。排母插拔警告使用包络回退值，实际排母按厂家尺寸及实物叠装复核。警告和判据保存在数据复核文件。

## 打样前仍需闭环的项目

1. **天线区域**　YD 原厂图的天线伸出端投影在 PCB1 上落到现有禁布框外，并与 J7 区域投影交叠。需根据选定开发板重核禁铜范围、周围金属件和安装高度，再验证 Wi-Fi 连接、距离与吞吐量。[乐鑫天线布局要求](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/pcb-layout-design.html)
2. **执行器供电与启动**　十芯排线只有一芯承载 5 V、另一芯回流。MG996R 的工作下限为 4.8 V，5 V 标称条件仅有约 0.2 V 压降预算。需要用已选电源、排线和电机实测联合启动与峰值负载。YD 的 GPIO48 同时接板载 RGB，GPIO43 是 UART0 启动输出。候选沿用现有针序与固件，需核对所选板的 RGB 跳线状态，并测量复位、USB 烧录、切换 SW20 和失联时的执行器输出。[YD 板载外设定义](https://github.com/vcc-gnd/YD-ESP32-S3) · [乐鑫 UART0 规范](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html#uart) · [TowerPro 供电规格](https://towerpro.com.tw/product/mg996r/)
3. **首件机械验收**　用实际采购件试插 44 针、核对插针露出长度和 USB 线头，测量屏幕背面、触发板焊区、开关、舵机及扇叶净空，并按实际支撑件检查 PCB2。候选中仍可看到旧工程的 22.860 mm 文档层量尺，候选实际钻孔为 25.40 mm，正式图面须更新该标注。

## 复核记录

- [Gitee R9.1.3 发布页](https://gitee.com/hjz0810/H2O/releases/tag/r9.1.3) · 主分支基线 `b50cdb66260fb44c305425f125334ab7d7b312f7`
- 嘉立创候选工程 **H2O-R9-YD25.40-装配验证副本-20261120**。
- [数据与 DFM 明细](verification.json) · [167 个焊盘和钻孔表](pin-drill-audit.csv) · [38 个器件位置及包络](component-coordinate-audit.csv)
- [R9.1.3 五页原理图复核图](images/schematic-review.png) · [R9.1.3 两板原生 3D](images/baseline-native-3d.png)
