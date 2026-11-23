# H₂O R10 RC3 · 答题状态恢复修复

RC3 针对 ESP32 手柄答题过程中出现的“进入下一关后像重新开始”现象进行了无硬件主机复现和修复。版本仍是 25.40 mm 双板系统。

## 固件修复

- 旧协议判题后可能连续收到多条后续报文。状态机现在按顺序保存有限数量的后续帧，覆盖关卡结果、剧情和下一题的连续到达。
- 新关剧情不再把生命值写回固定初始值，继续使用服务器最近一次判题或恢复快照中的 `hp`。
- 生命耗尽、请求超时和连接中断仍进入恢复流程，不会自动重发答案。

## 无硬件复现结果

修复前，向实际 `ExamCore` 注入 `judge → level_result → story → question`，状态变为 `recovery`，提示“后续题目队列异常，已暂停”。修复后按顺序消费全部帧，状态保持在当前答题链路。

当前协议 2 的已记录服务器顺序是 `judge → continue → question/story/game_over`，服务端没有 `level_result` 生成路径。RC3 同时覆盖当前协议 2 和旧协议兼容路径。

24 项旧协议状态机测试、6 项协议 2 测试、9 项 OneNET 停机与恢复测试、两种监测板型回归均通过。正式服务器是否在用户历史时段发生断线、超时或发送了旧协议帧，需要设备串口日志或服务端 MQTT 日志确认。

## 下载

- [固件包](https://gitee.com/hjz0810/H2O/releases/download/r10.0.0-rc.3/H2O-r10.0.0-rc.3-firmware.zip)
- [硬件包](https://gitee.com/hjz0810/H2O/releases/download/r10.0.0-rc.3/H2O-r10.0.0-rc.3-hardware.zip)
- [网页与服务端源码包](https://gitee.com/hjz0810/H2O/releases/download/r10.0.0-rc.3/H2O-r10.0.0-rc.3-web.zip)
- [构建清单](https://gitee.com/hjz0810/H2O/releases/download/r10.0.0-rc.3/release-manifest.json)
- [SHA-256 清单](https://gitee.com/hjz0810/H2O/releases/download/r10.0.0-rc.3/SHA256SUMS.txt)

[RC2 历史发布](https://gitee.com/hjz0810/H2O/releases/tag/r10.0.0-rc.2) · [任务 A 验证记录](task-a.md)
