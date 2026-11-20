# Arduino R9 十芯直连固件

`1.2.2-r9-10p` 对应 [十芯直连版 PCB](../hardware/R9-10P/ESP32-handheld-R9-10P.epro2)。引脚和线序见 [装配说明](../docs/R9/十芯直连与直插装配.md)。执行板用 TB6612 成品模块，只烧录手柄的 ESP32-S3。

在 Arduino IDE 中打开 `ExamHandheld/ExamHandheld.ino`，选择 `ESP32S3 Dev Module`，安装 ESP32 核心 **3.3.12**。板型选项为 USB CDC 启用、16 MB Flash、QIO、OPI PSRAM、3 MB APP + FAT。`libraries/` 附有使用的库，或者运行 `tools/build.ps1` 使用随包库编译。

`binaries/` 提供当前版本的编译输出。烧录前拔掉手柄的外接 5 V，把手柄 SW10 按到启用档，再用 ESP32-S3 开发板 USB 烧录。对应版本的实际编译日志在 `validation/r9-10p-compile.txt`，其余旧日志记录历史版本。

在风扇页短按“测试”切换 A、B 路，上下调整所选电机档位，左右调整舵机，按“确认”停两路。初装舵机脉宽为 1.20–1.80 ms。
