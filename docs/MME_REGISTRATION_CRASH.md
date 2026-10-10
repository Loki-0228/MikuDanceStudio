# MMV 网格登记时的 MME 后处理崩溃修复

2026-09-27。修复范围是 `MMEffect.dll+0x2655` 读取空深度表面。宿主在保存分配、替换网格和恢复分配时，原先插入四个空的 `BeginScene/EndScene`。MME 请求离屏绘制后，需要宿主递减重复次数并提交对应绘制，才能进入主目标并捕获深度状态。直接结束场景会跳过这一步。

`DrainEffectRegistration` 现在使用现有阴影和模型渲染器，完整消费 `ExpSetRenderRepeatCount` 请求。它保存和恢复渲染目标、深度表面、视口、设备状态与宿主当前 pass，不推进动画、物理或 MMV，也不额外 Present。三个分配转移入口共用此流程。第三方 DLL、FX 和 37 个导出保持原状。

## 验收

后台测试由生产核心链接的 `mme_postprocess_tests` 运行，加载原路径 AutoLuminous.x 及其同名 FX。旧构建在首次 MMV 分配时稳定复现同一 DLL 偏移和空指针。新版通过初次分配、重载、打开参数窗口、保存 EMM 和退出。

- 兼容版、纯 DX12 版均通过真实 Miku PMD、AutoLuminous 和 MMV 恒等效果。640×480 渲染目标包含 20585 种颜色，分配前、分配后和重载后的 BGRA 完全相同，两版也相同。SHA256 为 `53c5f63d1e9bbc3c5f0a43437e6ef553086a0200d81d8f32a5eb6d7e24b8d625`。
- 原故障中的 Bullet3 FX 分配组合通过。此项证明该配置不再触发登记崩溃，不替代 Bullet3 模型物理精度验收。
- `mmv_host_tests production-extra` 通过生成材质命名、效果分配保存与真实绿色像素着色断言。
- 保存后的 EMM 保留 `[Effect@AL_EmitterRT]` 离屏页。测试同时检查 MMS 附件源 mesh 未被替换。
- 共同可运行的 MME-only 场景采用 60 帧、8 轮 ABBA/BAAB 交替采样。修复前墙钟中位数 614.832 ms，范围 609.429–617.442 ms；修复后 614.615 ms，范围 602.068–617.436 ms。差异处于波动内。这是含消息泵和 Present 的帧循环墙钟时间，不是纯 GPU 时间或播放 FPS。
- 旧版分配场景崩溃，不能提供该场景成功运行时的性能对照。新增登记绘制仅在结构更换或分配恢复时发生，成本不按普通帧计入；未声称这段维护流程性能提升。

## 证据与复现

证据目录：`.local/artifacts/mme-null-crash/`。有效画面在 `after-target3/`、`validated-dx12/`、`validated-bullet3/`，性能原始日志为 `perf-*.log`。`validated-readbacks.json` 保存图像哈希；`manifest.json` 保存构建和运行文件哈希。

`after-identity-model`、`after-bullet3`、`after-material`、`after-dx12` 的早期空目标回读不作为画面依据。`after-readback` 因目标为空主动失败。`build-target` 链接失败后误跑旧程序的样本也排除。全部保留用于溯源。

构建目标：`cmake --build build --config Release --target mme_postprocess_tests mmv_host_tests MikuDanceStudio -j 8`。测试 EXE 需与参考 MMEffect.dll、MMHack.dll、d3d9.dll、待测 MMV.dll、Data 和 MMV 资源同目录，文件名必须保持 `MikuMikuDance.exe`。测试参数依次为 AutoLuminous.x 绝对路径、输出目录、MMV FX 绝对路径、可选模型绝对路径。两个 DLL 版本顺序运行，不并发占用测试目录。

DSH 在正式工作区完成了空指针类型和重复绘制门控的只读分析。长时间后续扫描由主 agent 中止，子任务终态为 `missing-turn-end`；报告不是独立验收结论。主 agent 复核实际调用链并完成上述运行验收。
