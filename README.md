# 智联迷城 ESP32 手柄 · R9

R9 包含两块板。手柄集成 ESP32-S3、横置 LCD、按键和传感器接口，尺寸为 **99.60 × 99.60 mm**。执行器板尺寸为 **67.056 × 50.292 mm**，舵机在下方转向，直流电机和扇叶装在舵机上方。板间一根 10 芯防呆排线同时传输 5 V、3.3 V、地和控制信号。

![手柄正面](hardware/images/pcb-front-3d.png)

| 要找的内容 | 文件 |
|---|---|
| 这版改了什么、碰到的问题和处理结果 | [R9 变更与验收记录](docs/R9/CHANGELOG.md) |
| 嘉立创 EDA 专业版工程、三页原理图 | [工程文件](hardware/ESP32-S3-handheld.epro2) · [原理图 PDF](hardware/ESP32-S3-schematic.pdf) |
| 两板制造文件 | [手柄 Gerber](hardware/gerber/PCB1-手柄-Gerber.zip) · [执行器 Gerber](hardware/gerber/PCB2-执行器-Gerber.zip) |
| 模块、按键、排针排母、排线与安装件 | [采购与装配](hardware/采购与装配.md) · [焊接 BOM](hardware/PCB焊接BOM.csv) · [1:1 对孔图](hardware/assembly/装配与对孔图.pdf) · [电机托架 DXF](hardware/assembly/电机托架分面孔位.dxf) |
| 嘉立创实际画面与检查报告 | [2D 走线及 3D](hardware/images/) · [DRC、DFM 和网表核对](hardware/validation/) |
| Arduino 固件 `1.2.0-r9` | [打开 ExamHandheld.ino](software/ExamHandheld/ExamHandheld.ino) · [编译与烧录](software/README_先读我.md) · [烧录文件](software/binaries/) |

R9 交付检查记录显示，两板保存并重载后原生 DRC 均为 **0**，原理图、PCB 和飞针网表共核对 **152** 个电气脚。固件已编译并通过本地测试，烧录和实物联调留待首件。装配前用 1:1 图核对快充板孔位、舵机固定孔和屏幕位置，装好后再测 5 V 带载压降和扇叶净空。

`dist/` 保存与早期 PCB 对应的 **v1.1.0** 历史交付包。R9 开发和制造使用上表列出的工程、固件和 Gerber。
