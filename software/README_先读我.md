# Arduino R9 固件

在 Arduino IDE 中打开 `ExamHandheld/ExamHandheld.ino`，选择 `ESP32S3 Dev Module`，芯片包 ESP32 **3.3.12**。参考 `validation/fqbn.txt`：16 MB Flash、QIO、OPI PSRAM、3 MB 应用分区 + FAT、USB CDC 启用。`libraries/` 附有本次使用的库；可运行 `tools/InstallLibraries.ps1` 安装到草稿本，或执行 `tools/build.ps1` 使用随包库直接编译。

- `validation/arduino-build.txt`：ESP32-S3 编译结果。`validation/firmware-host-test.log`：23 项核心、5 项协议测试及按键消抖、自动演示停止、配网页检查。
- `binaries/`：本次编译产物。烧录前先拔手柄的外接 5 V，用开发板 USB 烧录。
- 按住测试键上电可进入配网。测试键的自动演示只用于测试组；本次未烧录、未用生产系统实际提交测试记录。
- 执行器通过一根 IDC 线，电机提供 PWM 速度档，舵机可在风扇页转向。固件只发送 1.20–1.80 ms 的初装限位脉宽，具体角度以实物为准。
