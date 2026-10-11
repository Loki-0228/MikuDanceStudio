# 清理记录与后续候选

核对日期：2026-09-20。标记时的完整版本已保存在提交 `99c3c19`。**该提交不在本仓库**：本仓库从当前工作树起始，只有 `ba6b269` 与 `e3ad30d` 两个提交，因此「删除的脚本可从该提交恢复」在本仓库无法兑现。本轮删除了已满足条件的候选，完成情况见下表；保留项仍需满足各自条件后才能清理。删除的脚本只能从父工程取得，不再另存一份到工程目录。

后续源码候选使用 `CLEANUP(C编号)` 定位；文件使用 `MAINTENANCE: 分类` 标记用途。已删除代码的标记一并移除。查找入口：

```powershell
rg -n 'CLEANUP\(|MAINTENANCE:' src include tests scripts tools
```

## 清理状态

| 编号 | 位置 | 证据与用途 | 状态与后续条件 |
| --- | --- | --- | --- |
| C01 | `src/render/accessory.cpp`、`src/render/debug_geometry.cpp`、`src/window/accessory_paste.cpp` 中的 `At` 模板 | 文件内只有模板定义，没有调用；各 1、2、2 个重载，是字段迁移后的冗余辅助函数。 | **已删除** 5 个定义及对应标记，保留源文件。删除前复查无调用。 |
| C02 | `scripts/promote.py` | 旧 raw/offsets 迁移工具；`ledger` / `apply` 读取的 `include/mikudancestudio/offsets.hpp` 已不存在。 | **已删除**；无构建依赖，历史版本保存在 Git。 |
| C03 | `scripts/regen_layout_pins.py` | 一次性 Wave5-A 转换，期望旧版手写断言墙、旧成员名；当前布局头已是宏表。 | **已删除**；现有 `layout_pins.hpp` 与布局断言继续保留。 |
| C04 | `tools/wave3_*`、`tools/wave4a_*`、`tools/wave5c_*`、`tools/wave5d2_*` | 一次性迁移/验收脚本，依赖旧源码快照、当时 HEAD 或原作者机器路径。 | **已删除** 11 个已标记 `one-shot` 的脚本；删除前核对无调用或构建依赖。 |
| C05 | `MMDApp::BoneFrameScratch()` 及其 x64 镜像 | 当前源码没有调用；相邻 `CameraFrameScratch()` 仍被地面阴影颜色访问使用。 | **已删除** accessor 和 x64 末尾的 140 字节镜像；保留 `MMDAppState` 中全部占位及布局断言，Win32/x64 布局编译通过。 |
| C06 | `src/unported/stubs.cpp`、`src/window/wndproc_stubs.cpp` 的历史命名/重复沿革注释 | 前者包含录像收尾等正式实现，后者含控件回调。 | **保留**。只有重新归类并更新 CMake、声明、引用后才能移动；不能按名称删除。 |
| C07 | `tools/strcheck2_result.json`、`docs/*PERFORMANCE*.json`、`docs/PMX_LOADING_2026-09-20.json` | 某一版本/设备/素材上的结果快照，不能当最新结果；相邻报告依赖这些证据。 | **保留**。已确认性能及 PMX 报告引用对应 JSON；报告归档或重测后再处理。 |
| C08 | `build/`、`build-*/`、`out/` 中的日志、探针、测试可执行文件等 | 本地构建/验证产物，当时由 `.gitignore` 排除；2026-10-11 移除该文件后，本机改用 `.git/info/exclude` 保留忽略规则。`build/Release` 同时包含用户运行的程序。 | **部分清理**：双 ABI 检查后删除 `build/layout-probe/` 及下列 5 份旧日志；保留正式程序、测试程序、符号和其他构建目录。 |
| C09 | `src/io/vsq_load.cpp` 的 `if (j == 0xae) {}` | 仅保留原反编译的自赋值说明，无实际语句。 | **已删除**空分支及其标记。 |

C08 本次删除的旧日志：`asan-followup-tests-build.log`、`build-asan-offset-followup.log`、`build-offset-followup.log`、`followup-tests-build.log`、`offset-followup-build.log`，均原位于 `build/`。

## 本次清理验证

- 临时探针包含 `mmd_app.hpp` 和 `model.hpp`，按正式工程的 `/utf-8 /permissive-` 配置分别编译 x64、Win32，全部布局断言通过。探针完成使命后已删除。
- 正式程序及 19 个 CTest 目标的 Release 重建成功；`ctest --test-dir build -C Release --output-on-failure --timeout 40`：19/19 通过。测试在可创建 Win32 窗口及 D3D 设备的本机环境运行。
- 删除后复查：除本清单的历史记录外，无已删除脚本或 accessor/镜像的残留引用；`git diff --check` 通过。

## 应保留的测试、工具与布局信息

- [测试用途表](../tests/README.md) 给出全部测试源文件的定位和清理条件。长期回归测试标为 `regression-keep`，不是发布包内容；CMake 仍以 `EXCLUDE_FROM_ALL` 显式构建。
- `src/unported/stubs.cpp` 等生产实现，不能因名字像临时文件而删。
- `scripts/gen_resources.py`、`scripts/dialog_rc.py` 是资源维护工具；CMake 链接的 `.res`、`.rc` 和导出表是实际产品输入。
- `tools/inspect_reference_pe.py`、`mdump2.py`、`mdiff.py`、`x64_ref_scan.py`、`x64_disasm_case498.py`、`strcheck2.py`、`resolve_addr.cpp` 是手动参考/故障分析工具。原版进程读取中的固定地址是版本专用证据，不属于本机 C++ 运行时成员引用；复用前必须核对原版版本。
- `tools/pmxlib.py`、`make_pmd_perf_fixture.py` 用于模型数据处理/可再生性能样本，当前无证据证明冗余。
- `offsetof`、`sizeof`、RawPad、ABI 断言、二进制文件字段大小不是“残留偏移 bug”的充分证据；先确认数据语义。
- 用户的 `ref/`、舞台、模型和 MME 文件不是本清单的删除候选。

## 仍需后续审计的代码

| 范围 | 当前判断 | 后续核对 |
| --- | --- | --- |
| `features/dialog_gaps.cpp`、`query_audio_gaps.cpp`、`app/oni_skeleton_pump.cpp` 的 308 字节追踪记录 | 无指针的 77 浮点记录；仍手算通道位置。 | 可以引入具名记录统一生产/回放，但须核对 OpenNI 输入、镜像和全部关节通道；不能只替换写端。 |
| `model/key_registrars.cpp`、`model/vmd_load.cpp`、`window/frame_range_apply.cpp` | 部分剪贴板/文件记录仍按固定字节访问。 | 在已有 clipboard 类型覆盖下继续迁移，并检查短读、异常长度和文件字节兼容性。 |
| `MMDApp::GravityNoiseModeWord()`、`RecordedFrameCountByte1()`、`GroundShadowColor()` | 仍有跨字段或共享存储视图；未在本轮更改其行为。 | 根据所有读写点核对别名语义，再决定拆字段或具名 union；不可机械挪动。 |
| `io/vsq_load.cpp` | `99c3c19` 修复歌词覆盖时间、单事件区段误匹配和速度记录容量，仍有旧版顺序文本扫描。 | 缺失段、截断 MIDI、复杂速度变化的错误处理应专项检查；已有的有效合成 VSQ 不覆盖所有损坏文件。 |

没有标记的代码不代表已经证明无冗余或无偏移问题。每次清理后应更新对应条目的状态和验证结果。

## 2026-09 全量审查的交接

全量代码审查（199 个文件、从零重建、22/22 回归）的结果与缺陷清单见
[代码审查报告](CODE_REVIEW_2026-09.md) 与 [缺陷清单](CODE_REVIEW_FINDINGS.md)。
与本清单直接相关的结论：

- 本清单上表「仍需后续审计的代码」四行，审查中各自有对应的具体条目：
  308 字节追踪记录的通道约定（`FB-01`、`FB-02`）、固定字节访问的记录迁移
  （`B3-22`、`B3-25`）、跨字段别名语义（`HB-01` 的 x64 别名冲突、`HB-08`）、
  `vsq_load.cpp` 的缺失段与截断处理（`B3-16` 至 `B3-19`、`B3-23`）。
- 本节开头提到的 `99c3c19` 不在本仓库，C02/C03/C04 的「可从该提交恢复」需要改口径。
- 新增：`docs/PORTING_STATUS.md`（源码引用 12 处）与 `docs/ARCHITECTURE.md`
  （15 处）、`translated/` 语料（37 处）在本仓库均不存在，注释里的依据无法追溯。
