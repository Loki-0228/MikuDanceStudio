# MMV 独立项目与构建目录

用户已确认拆出 MMV 本体源码，并要求每个项目在自身 `build/` 下构建。

| 项目 | 源码目录 | 构建目录 |
| --- | --- | --- |
| MMV | `D:/MMD-Next/MMV` | `D:/MMD-Next/MMV/build` |
| MMV-Bullet3 | `D:/MMD-Next/MMV-Bullet3` | `D:/MMD-Next/MMV-Bullet3/build` |
| MMS-MME | `D:/MMD-Next/MikuDanceStudio-MME` | `D:/MMD-Next/MikuDanceStudio-MME/build` |

拆分时，三个项目的 `.gitignore` 均忽略 `build/`。2026-10-11 清理后，MMS 本机改用
`.git/info/exclude` 保留忽略规则。MMV 和 Bullet3 各有独立
`scripts/build.ps1` 与 CMake preset；本宿主不再提供 `mmv_all` 等本体构建目标。
宿主默认从 `../MMV/include` 读取公开头，可通过 `MMV_INCLUDE_DIR` 指定 SDK 位置。
生成运行包时，选择独立 MMV 构建的一版 DLL 及资源复制到宿主 EXE 旁。

本体源码、资源及独立测试已迁出宿主；CMake 宿主接入在 `cmake/mmv_host.cmake`。
现有并行任务的未提交 MMV 实现已保存在新仓库及原始来源快照中。
宿主导出修复及其他宿主修改仍由对应会话维护。

前后 FX 和三版生产 DLL 回归通过。编辑器套件前后均有相同的三个既有失败，
已保留日志并交回页签任务。详细范围、来源和原始证据见
[MMV 迁移记录](../../MMV/docs/PROJECT_SPLIT_2026-09-28.md)。

旧 `build/mmv/` 只保留为历史基线，不再作为 MMV 编译入口。
用户随后清理了 `.local`，原始归档与前后日志已不可作为现存文件引用。
源码迁移保存在三个项目的本地提交中；最终清单位于
`../MMV/build/verification/project-split-20260928/manifest.json`，
保存最终版本、构建路径和二进制哈希。
