# EMM 自动保存修复（2026-09-25）

## 根因与修改

用户报告：打开工程时关闭「自动保存效果分配文件」，之后开启再保存，只更新 PMM。真实 MME 后台测试通过 `WM_COMMAND 0xCF` 触发保存，在修复前重现了该行为。

`SaveSceneFile` 使用 UCRT 的 `_wsopen_s`，绕过了 MMHack 改写的宿主 `CreateFileW` 导入。PMM 加载已有 `NotifyEffectFileOpen`，保存没有对应通知。实测 MMHack 的 `+0x41E0` 文件钩子在目标扩展名为 `.pmm`、访问标志恰好为 `GENERIC_WRITE` 时记录保存路径；MME 消费队列时检查当前自动保存状态。地址仅作此版本的分析证据，产品代码不依赖私有地址。

现在 PMM 成功关闭后调用 `NotifyEffectFileSave`。它通过宿主导入以 `GENERIC_WRITE` 和 `OPEN_EXISTING` 打开后关闭文件，不截断或重写 PMM。MME 继续处理开关及 EMM 序列化，不改变效果分配、菜单状态或 DLL 文件。没有加载 MMHack、路径为空或扩展名不是 PMM 时直接返回。

## 验证结果

对照基线为 `f7c6cb0`。隔离工作树 `.local/worktrees/emm-autosave/` 的两侧使用相同的回归源码、MSVC v143 x64 Release（`/O2 /Ob2 /DNDEBUG`）、MME DLL、单模型 Grass 夹具、相机、分辨率和每步四帧的处理流程。隐藏窗口通过真实保存菜单执行，不操作用户窗口，不覆盖输入素材。

| 场景 | 修复前 | 修复后 |
| --- | --- | --- |
| 关闭自动保存时保存 PMM | PMM 写入，未生成 EMM | 相同 |
| 在关闭状态下打开，再开启并保存 | 两次均缺少 EMM，退出码 7 | 两次均通过，退出码 0 |
| 初始开启、首次保存到新路径 | 缺少 EMM，退出码 7 | 通过，退出码 0 |
| 已有 EMM 后关闭，再保存 | 前项失败，未继续 | 已有 EMM 字节不变 |
| 再次开启并保存 | 前项失败，未继续 | 重写 EMM，效果分配内容不变 |
| 中日文路径另存为 | 前项失败，未继续 | 生成同名 EMM，效果分配内容不变 |

关闭起始场景按修复前、修复后、修复后、修复前的顺序交替运行。末尾另测初始开启。初始开启场景测试的是新路径首次保存，不据此推断用户已有 EMM 的更新情况。空场景也完成了关闭、重开及另存为测试。

正式工程重新构建 `MikuDanceStudio`、`effect_api_tests`、`scene_lifecycle_mme_tests` 成功。合入后的实际 MME 单模型测试退出码 0；`effect_api_regressions` 通过。正式工作树另有正在进行的蒙皮和阴影改动，构建状态保存在原始记录中，本次提交仅包含 EMM 保存与回归相关文件。

本次没有测量播放 FPS、CPU 或 GPU 帧时，性能部分尚未验收。记录中的 `process_seconds` 只是测试进程总耗时，不能换算为播放性能。修复只在工程保存后增加一次不截断的文件打开，不向逐帧渲染实现添加工作。

## 本地产物

- 原始日志、生成的 PMM/EMM、命令、源码状态及哈希：`.local/artifacts/emm-autosave/manifest.json` 和同目录日志。
- 修复前后测试宿主：`.local/releases/emm-autosave/before/`、`after/`；正式工作树测试宿主副本：`integrated/`。这些是后台测试入口，不能作为日常编辑器启动。
- 正式程序：`build/Release/MikuMikuDance.exe`。未重启用户程序。
- 隔离工作树保留作对照，没有清理或删除旧产物。

对照测试 EXE SHA-256：修复前 `1e5acd352741772dad2e84bd531c1dfea900d08a4055170f4e7015623a1e30cd`；修复后 `e725f4534d2a1a1f44917a9990ef82713cd0d3bc51e4f592b5dc0cce6edb62f2`。正式集成 EXE 的哈希另记在清单中。

MMEffect SHA-256 为 `74b3f5882a3bad28131cbc57994e2f960938e6ca850fc7139018ce4822bd87dc`，MMHack 为 `e450b25e5cd2bcf125ca4b8eb9e57f858ec3ca379633c7d25f9dd40e864b390d`。其他版本 DLL 未实测。
