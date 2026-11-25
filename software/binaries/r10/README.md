# R10 手柄固件 · 免编译烧录包

面向"拿到 bin 直接烧"的场景：不需要装 Arduino IDE、不需要拉 libraries、不需要管板型选项。
三个 bin 覆盖 bootloader、分区表和应用程序，烧完即可运行。

- 固件版本：`10.0.0-rc.2`
- 板型：自研 R10 双板（YD-ESP32-S3，25.40 mm 双排）
- 构建日期：2026-11-25
- 编译参数：`esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB`
- ESP32 Arduino 核心：3.3.12

## 文件与烧录地址

| 文件 | 大小 | 烧录地址 | SHA-256 |
|---|---:|---|---|
| `ExamHandheldV2.ino.bootloader.bin` | 19,968 | `0x0` | `84bb365666003b051f564d04380d0cf5d9d85c420ddafe9fc863071b49e49bfc` |
| `ExamHandheldV2.ino.partitions.bin` | 3,072 | `0x8000` | `ace02503447d0f470692e65fa76002f2d77a92dc81cd3813d8aa66718d716da9` |
| `ExamHandheldV2.ino.bin` | 1,718,544 | `0x10000` | `68e3dcc1131d98c1e5de8b127269b8203a6581cec98dedcf1b355637aee8cfff` |

**地址顺序不能错**：`0x0` → `0x8000` → `0x10000`。两个小文件是引导和分区表，大文件是应用。

## 方式一：esptool（推荐，任何环境都能用）

```bash
pip install esptool
esptool.py --chip esp32s3 -p COM7 -b 921600 \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0    ExamHandheldV2.ino.bootloader.bin \
  0x8000 ExamHandheldV2.ino.partitions.bin \
  0x10000 ExamHandheldV2.ino.bin
```

把 `COM7` 换成实际端口（Linux / macOS 是 `/dev/ttyUSB0`、`/dev/cu.usbserial-*`）。
烧完 esptool 会自动硬复位，屏幕应立刻出现 `智联迷城` 主菜单。

## 方式二：arduino-cli（文件名必须原样保留）

```bash
arduino-cli upload \
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" \
  --port COM7 --input-dir .
```

`--input-dir` 指向本目录。arduino-cli 靠 `ExamHandheldV2.ino.*.bin` 这个命名识别三个文件，**改名会导致找不到**。

## 方式三：flash_download_tool（乐鑫图形工具）

四个下拉框填：

| 文件 | 地址 |
|---|---|
| bootloader.bin | `0x0` |
| partitions.bin | `0x8000` |
| app.bin | `0x10000` |

Flash 参数：`DIO`、`80MHz`、`16MB`、`DoNotChgBin` 勾选。

## 烧录前

1. **拔掉手柄的外部 5 V**（快充模块），只留 USB 供电，避免双电源冲突。
2. USB 插 **CH343 那个口**（UART/COM），不是标 USB 的原生口。
3. 板子没有独立的复位键（SW10 未装时），烧录依赖 DTR/RTS 自动进下载模式；若失败，用镊子短接 SW10 的 2 脚（EN）与 3 脚（GND）复位。

## 烧录后

首次启动配置为空，屏幕会直接进配网页，显示 `ZLC-xxxx` 热点名和临时密码。手机连该热点，访问 `http://192.168.4.1` 填：

- **Wi-Fi 名称与密码**：必须是 **2.4 GHz**，ESP32-S3 不支持 5 GHz。Windows 移动热点要在设置里把频段选 2.4 GHz。
- **组号**：纯数字，例如 `24014`。MQTT 密码由固件按"组号 + 芯片 MAC"自动算出，**不需要手填**。
- 组号对应的设备 MAC 必须已在服务端绑定，否则 MQTT 返回码 5（未授权）。

## 自检

串口 115200 应看到：

```
[boot] serial ok
[boot] panel 160x128
[auth] group credential derived
[net] wifi=3 ip=192.168.x.x online=1 ready=1 grp=<组号>
```

`online=1 ready=1` 表示已经拿到 `hello_ack`，可以开始答题。

**注意**：本版为发布版，`Actuators::begin()` 会占用 GPIO43（B 路 BIN2），而 GPIO43 同时是 UART0 的 TX，**因此执行器初始化之后串口不再输出**。这是 R10 板子的已知冲突，不是固件崩溃——屏幕仍正常工作。需要串口调试时改用源码编译并加 `-DH2O_KEEP_UART0`。

## 已知硬件问题

传感器三路在首件板上不可直接用，详见 [R10 硬件缺陷与勘误](../../docs/R10/硬件缺陷与勘误.md)。简述：

- **J5（DHT11）针序与通用模块相反**，直插会反接电源，需转 180° 或用杜邦线转接；
- **J6/J7（MQ）的 DO/AO 错位一位**，需把模块 AO 飞线到板上第 3 脚；
- **+5V 主干在板内不通**，需飞线补通。
