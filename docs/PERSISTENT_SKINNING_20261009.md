# 跨启动复用自动测速结果

自动顶点计算只在首次启动时测速，将六组 PMD/PMX 结果保存到
`%LOCALAPPDATA%/MikuDanceStudio/skinning-MMS-v1.bin`。
退出后重新打开、打开空工程或重开项目都读取已有结果，不再测速。
实际模型选用 GPU 时才建立计算管线，空启动不用提前创建全部候选管线。

缓存校验 CPU、线程配置、显卡和驱动身份，不依赖重启后可能改变的 LUID。
硬件、驱动或测速版本变化，以及缓存损坏时才重新测速。缓存保存采用原子替换，
MMS 和 MMN 各用一个文件。顶点计算菜单的自动项显示“首次测速”。

## 验证与位置

- 基线 `ee1148330e6dd4b9c21faf611347da480da84ded`，EXE SHA256：
  `79227e91cfabbc3fa6669d576869a0f41ba71822608bc80dbbd9d75ddeeaab1d`。
  备份保留在 `.local/archives/persistent-skinning/`。
- 同一台机器、隐藏原生程序、独立用户配置：原版两次空启动 8.083 / 7.618 秒，
  每次都测速；新版本首次 7.304 秒，第二次 2.799 秒，第二次测速次数为零。
  这是启动个例记录，不是多轮统计。验证使用 EXE SHA256：
  `f693c5aeef85e76775d152b378c068ff1a8199e40e89b74c3df1d2d4f52cd4af`。
- `skinning_backend_tests`：120 组 GPU/CPU 情况通过，包括磁盘重用、LUID/显卡顺序变化、
  硬件失配、截断缓存及重新保存。真实 White.pmx 五种姿态的一致性检查通过。
- 构建日志：`.local/artifacts/persistent-skinning/build.log`。
  两产品统一测试证据保留在 `D:/MMD-Next/MMN/.local/artifacts/pmm-startup-physics/` 下的
  `hardware-MMS/` 与 `empty-launch/MMS-before/`、`empty-launch/MMS-after/`；包含原始日志、
  参数、哈希和私有配置。测试没有改写用户的 PMM 或实际用户配置。
- 正式入口仍为 `build/Release/MikuMikuDance.exe`。本次缓存改动不修改 MMV4MMS 渲染器。
