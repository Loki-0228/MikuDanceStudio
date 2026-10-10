# AllStar 的辅助骨骼模型加载崩溃

`tests/UserFile/sample(AllStar).pmm` 含 11 个模型，其中两个只有骨骼，没有顶点。MMV4MMS 宿主快照对空顶点缓冲区调用 `GetDesc`，导致 `0xC0000005`。零索引模型随后也会被误判为无效拓扑。

现在零顶点、零索引直接生成空网格，继续提供对象及骨骼数据；非零数据缺少缓冲区时报告可处理的错误。原 PMM 和模型没有修改。

后台回归由同一 `scene_lifecycle_tests --allstar-mmv` 入口执行，窗口隐藏，使用相同原工程和真实 MMV4MMS DLL，Release x64，原生物理保持开启。修复前在第一个渲染帧读取空地址，退出码 3221225477；修复后保留全部 11 个模型和两组辅助骨骼，连续 12 帧完成渲染，堆检查通过，退出码 0。

| 记录 | SHA-256 |
| --- | --- |
| 修复前测试 EXE | `444508350053bd9e3cbf825f61964020b585c8f0773c7f1722d1f78beb0d8c40` |
| 修复后测试 EXE | `31c00cc7ca7b75dff7e7f4f46ec396aca6983a20878d66db77df4bc69c4cfd03` |
| 两侧 MMV4MMS.dll | `30842dd302c54b663bf0379edc98facf00b9b5596a7cb84259e41be5e65e536b` |
| 两侧原 PMM | `56e7307d762f0d6cc9f6b444dd48f7028a700738472f0d11331dd254bfd0d74a` |

源码基线为 `c186438b9a7a2ee47392600c1ecb634705d32914`，原始日志、源码哈希、工作区差异和隔离运行目录保留在 `.local/artifacts/allstar-empty-model/`。有效对照为 `before-corrected` 与 `after-corrected`；早期 `after` 在测试中重复 BeginScene，属于测试调用错误，保留但不用于验收。未测播放 FPS、CPU/GPU 时间或画质对照，不能据此声称性能或画质改进。
