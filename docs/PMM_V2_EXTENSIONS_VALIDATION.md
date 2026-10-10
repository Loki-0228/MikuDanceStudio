# PMM v2 项目扩展验证记录

日期：2026-09-25。格式与后续开发规则见 [PMM v2 项目扩展格式](PMM_V2_EXTENSIONS.md)。

## 结论与范围

Release 生产程序编译成功。独立编解码的 145 项断言通过，隐藏窗口中的实际 PMM 保存、加载、另存和新建流程通过。运算器与透明排序均随项目恢复；缺少指定显卡类型时回退自动，保存时保留原来的类型意图。PMM 文件头仍为 `Polygon Movie maker 0002`。

本次没有运行原版 MMD 的交互窗口。原版读取附加尾部、再次保存丢弃尾部的结论来自静态分析，不能推广为所有第三方读取器的保证。性能对照已采样，但波动及共享工作树的并行修改使本次性能归因未验收。

## 构建与产物

工作目录为 `D:/MMD-Next/MikuDanceStudio-MME`。所有临时证据保留在 `.local/artifacts/pmm-project-settings/`，不进入 Git。此目录是本次验证归档，保留前后构建、原始结果及失败过程，不是发布目录。

基线 HEAD 为 `f7c6cb0ff5c73bccb070c8a51f3445dc5f7a59d8`，包含当时其他任务的未提交修改；准确范围见 `before-status.txt`、`before.patch` 和 `before-file-hashes.json`。最终源码记录见 `after-head.txt`、`after-status.txt`、`after.patch`、`after-file-hashes.json`。基线并非干净的该提交，不能仅用提交号重现其全部内容。

两侧均为 MSVC v143、Visual Studio 2022 x64、Release、Conan Bullet 2.75，`MIKUDANCESTUDIO_PERF=OFF`。最终构建启用 `MIKUDANCESTUDIO_BUILD_UI_TESTS` 与 `MIKUDANCESTUDIO_BUILD_EFFECT_TESTS`。独立构建使用主构建生成的 Conan toolchain：

```powershell
cmake -S . -B .local/artifacts/pmm-project-settings/build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=D:/MMD-Next/MikuDanceStudio-MME/build/generators/conan_toolchain.cmake -DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DMIKUDANCESTUDIO_BUILD_UI_TESTS=ON -DMIKUDANCESTUDIO_BUILD_EFFECT_TESTS=ON
cmake --build .local/artifacts/pmm-project-settings/build --config Release --target MikuDanceStudio pmm_extension_tests pmm_project_settings_tests frame_benchmark --parallel 4
```

构建日志为 `before-build.log`、`after-configure.log`、`after-build.log`、`after-build-final.log`。编译器报告了现有源码中的有符号/无符号比较警告，没有构建错误。

| 产物 | SHA256 |
| --- | --- |
| 基线生产 EXE | `093a7d1b87e44293489264f87e51a3e94d58063d93026a68a5ef69baddf9d8ef` |
| 最终独立生产 EXE | `2bfabe0af4966390af8bb15f4e201b3818ceaa34a53b4101056ddde05ff46530` |
| 基线 frame_benchmark | `981d26c0caed7724625ba5605ff7981b86c520f769eb06eb529206fd1d6ec41e` |
| 最终独立 frame_benchmark | `e21b6b8e918fb6a33052360742867665052ed2a05023b549fce8794d1060f1d9` |

正式 `build/Release/MikuMikuDance.exe` 已最终增量构建成功，SHA256 为 `6ce061f2f7b1fada58f5f001d437780917d27733d8f6afd878547f9be2c4aaa7`，日志为 `main-final-build.log`。

完整测试二进制哈希见 `final-binary-hashes.json`。独立构建与正式 `build/` 的链接路径不同，EXE 哈希不要求相同。

## 正确性

`pmm_extension_tests.exe`：145 项断言通过，原始输出为 `codec-final.log`。覆盖手工构造的小端字节布局、独立 CRC32 参考计算、空扩展、记录顺序和二进制值、重复键、截断、长度溢出、16 MiB 边界、16384 条记录边界、非法尾标字段、文件位置恢复及拒绝无效写入。

首次单模块测试发现测试夹具未将文件位置置于末尾、失败后继续下标访问，以及生产 CRC32 查表步进错误。修正后，独立参考算法与输出字节一致。失败及修正过程保留在 `dsh-events-*.txt`，最终以主任务重新编译执行的 `codec-final.log` 为准。

集成测试命令格式如下，实际执行均使用隐藏窗口和隐藏进程：

```text
pmm_project_settings_tests.exe <测试产物目录> [可选的PMX模型绝对路径]
```

空工程日志为 `integration-1.log`；真实模型 `D:/MMD-Next/tests/Grass/HGrass.pmx` 的最终日志为 `integration-model.log`。两次退出码均为 0，错误输出为空。测试产生的 PMM 位于各自同名目录，未修改输入模型或用户工程。

覆盖内容：

- CPU、自动、集显与独显的类型映射；不同品牌、型号、设备 ID 的同类匹配；集显或独显缺失时回退自动。
- CPU 与关闭透明排序的真实保存、读取、菜单勾选、另存；带模型文件同时检查标准模型数据仍可读取。
- 任意未知标识及二进制内容、原始记录顺序的逐字节往返，不重复追加历史尾部。
- 无扩展 PMM 与新建工程恢复自动和开启排序，清空前一个工程的扩展状态。
- 临时自动回退不覆盖原来的集显意图；已知键的未来取值在未编辑时保留，明确修改后仅替换对应字段。
- 损坏 CRC 不影响标准工程加载，不应用部分设置，也不沿用前一个工程的扩展。

类型缺失使用合成设备列表验证，不声称实际移除了本机硬件。MME 保存通知沿用现有接口，具体 MME 验证见 [EMM 自动保存记录](EMM_AUTOSAVE_2026-09-25.md)。

## 后台性能对照

顺序为基线 1、修改后 1、基线 2、修改后 2。两侧均使用 `HGrass.pmx`、CPU 蒙皮、1 个模型、471122 个顶点、无刚体与关节、相同自动取景和默认排序状态；未加载 MME 效果。设备为 RTX 4070 Laptop GPU 的 D3D9 HAL。

```text
frame_benchmark.exe D:\MMD-Next\tests\Grass\HGrass.pmx --throughput --processor cpu --warmup 15 --samples 45 --viewport 320x240 --json <输出文件>
```

每次均检查 60 帧的模型视口，错配为 0；视口为 320×240，渲染目标为 1463×914，投影宽高比相同。原始记录为 `before-performance-{1,2}.json`、`after-performance-{1,2}.json` 及同名日志。

| 轮次 | 等待 GPU 完成后的批次每帧耗时（ms） | 单帧中位数（ms） | P95（ms） |
| --- | ---: | ---: | ---: |
| 基线 1 | 16.6122 | 15.6853 | 22.3409 |
| 修改后 1 | 16.9138 | 16.3606 | 20.6592 |
| 基线 2 | 12.7570 | 12.3755 | 15.0478 |
| 修改后 2 | 13.2552 | 12.6295 | 16.8569 |

两组修改后批次耗时分别增加约 1.8% 和 3.9%，但基线本身跨轮波动约 23.2%。测试期间还有 GPU 与透明排序任务在共享源码上整合，不能把差异单独归因于 PMM 改动。保留全部样本，不宣称性能提升或已证明无退化。这些数值是 GPU 排空后的吞吐耗时，不是窗口播放 FPS，也不是独立 GPU 时间。

## 静态兼容证据与委派

原版 MMD 9.32 x64 的 SHA256 为 `07516fd3bf1e6b1339836b6773a156f61bdd6f848eeb621fdda012375df313a1`。PMM 读取末尾、保存截断及 MME 0.37 对未知 EMM 字段的处理，分别记录在 `.local/artifacts/pmm-format-inspection/inspection.txt` 和 `.local/artifacts/emm-format-inspection/inspection.txt`。

编解码模块通过 DSH 子任务 `dad152e0-83bb-4e21-91ad-85f040a8959c` 实现，实际工作目录为上述仓库。其修改范围为 `include/mikudancestudio/pmm_extension.hpp`、`src/io/pmm_extension.cpp`、`tests/pmm_extension_tests.cpp`。主任务完成代码复核、独立编译、功能集成和验收；正式子任务结果保留为 `dsh-result.txt`。
