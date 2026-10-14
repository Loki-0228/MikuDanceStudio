# 模型载入与格式验证记录

本页按功能归并实现、诊断与验收资料。各记录中的「当前」「已完成」只适用于其注明的源码、构建和测试条件；当前使用入口见[文档导航](../../README.md)。

## 记录索引
- [AllStar 的辅助骨骼模型加载崩溃](#record-allstar-empty-model)
- [PMX loading bottleneck in StageNoRay LowEnd assets](#record-pmx-loading)
- [MMV 案例的实际渲染验证](#record-pmx-optional-fields-mmv-screenshots)

<a id="record-allstar-empty-model"></a>
## AllStar 的辅助骨骼模型加载崩溃

原始记录：`ALLSTAR_EMPTY_MODEL_20261008.md`。其中的版本、日期和验证范围沿用原记录。

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

<a id="record-pmx-loading"></a>
## PMX loading bottleneck in StageNoRay LowEnd assets

原始记录：`PMX_LOADING_2026-09-20.md`。其中的版本、日期和验证范围沿用原记录。

<a id="record-pmx-loading-follow-up-skip-hang-fixed"></a>
### Follow-up: Skip hang fixed

After the user explicitly requested skipping missing PMX files, the actual
Skip path reproduced a separate parser bug. It manually consumed the dense
display key's header, IK flags and parent states, then called
`DiscardPmmDisplayKey`, which consumed those same fields again. The resulting
offset error turned the following existing stage model into an apparent missing
file and let later fields become enormous loop counts. This was a parsing hang,
not simply slow disk I/O.

The duplicate consumption is removed. `frame_benchmark --load-skip-missing`
now completes this unchanged project in 1,120.735 ms, skipping only Null_00 and
restoring 3 models (168,596 vertices) and 10 accessories. Other dialogs still
fail the benchmark. This is an explicitly skipped-resource load, not a claim
that the missing model was recovered.

An end-to-end regression runs:

`scene_lifecycle_tests.exe <StageNoRay LowEnd.pmm> 3 --skip-missing 3 10`

It asserts the expected model/accessory counts, loads in three UI languages,
renders 12 physics frames after each load, and resets/reopens the project.
All three cycles passed: 1,062.127 / 592.514 / 633.837 ms without MME.
The same test also passed with the release directory's d3d9/MMEffect/MMHack
proxy DLLs and exported application entry points: 1,086.955 / 598.893 /
636.891 ms, again restoring 3 models and 10 accessories and rendering all
three cycles. Seven selected existing regressions pass as well.
These timings are for this port and fixture; they are not an original MMD
comparison or a claim about arbitrary third-party effects.

The sections below record the earlier diagnosis before Skip was authorized.

The user selected `极星动力LIVE舞台 Ray用一键渲染/UserFile/StageNoRay LowEnd.pmm`.
The file declares four models. An actual load aborts on the missing `Null_00`
model, whose stored source is
`F:\926MMD_ZN\MME\mme\1Shaders pack\G_ToonShaderCC\GTSC_001.pmx`.
The test reports a dialog and returns failure; its 163 ms aborted load is NOT
a successful PMM loading result. No PMM was changed, saved or substituted.
The missing model's current path is required for a full comparison with MMD 926.

<a id="record-pmx-loading-change"></a>
### Change

The PMX parser issued one CRT `_read` for each small scalar, vertex component
and index. The 129,307-vertex stage requires 3,157,219 such calls to consume
15,667,857 bytes. A bounded 64 KiB sequential reader now supplies these exact
logical reads using 240 underlying reads. Parser field order and arithmetic
are unchanged. Logical offsets remain correct for diagnostics despite read-ahead;
the reader owns and closes its descriptor on every exit.

This optimization is based on measurements of this port. It does not claim
that original MMD uses this particular buffering implementation.

<a id="record-pmx-loading-measurements"></a>
### Measurements

Release x64, RTX 4070 Laptop GPU, no MME. Same executable and parser, switching
only buffered versus raw reads. Each model is loaded into a fresh process;
timing covers `LoadModelFile`, including its existing texture/initialization
work, but excludes application startup and digest calculation. Two repetitions,
reversing the order in the second repetition. These are individual existing
assets from the selected project, NOT a complete PMM benchmark or cold-disk test.

| Asset | Raw reads, mean | Buffered, mean | Ratio | Underlying reads |
|---|---:|---:|---:|---:|
| PolarisLiveStage.pmx | 2,983 ms | 521 ms | 5.73x | 3,157,219 → 240 |
| SourRIN/White.pmx | 566 ms | 143 ms | 3.94x | 509,451 → 47 |
| Sky with box.pmx | 67 ms | 66 ms | ~1.02x | 16,169 → 2 |

Small skybox loads are dominated by other work; no material speedup is claimed.
Raw outputs for all 12 runs are in [PMX_LOADING_2026-09-20.json](data/pmx-loading.json).

<a id="record-pmx-loading-correctness-checks"></a>
### Correctness checks

- All runs load without dialogs and have the same model/vertex/bone/morph counts.
- Digests of initialized GPU input geometry, bone transforms, edge materials
  and triangle indices match between modes and repeated processes. These
  digests are a focused parser regression check, not a hash of every runtime field.
- This exposed inactive PMX weight/SDEF fields that the legacy loader does not
  initialize. GPU input packing now copies only fields used by the vertex's
  weight type, leaving inactive GPU lanes zero; it does not change active math.
- Reader tests cover existing descriptor offsets, small fields, 64 KiB boundary
  crossings, larger reads, short reads without overwriting the destination tail,
  EOF, logical positions and repeated close.
- The real character GPU/CPU skinning test includes BDEF1/2/4, SDEF, nonzero
  vertex morphs, outlines and cached submission reuse.
- All seven selected regressions pass: sequential reader, D3DX import hooks,
  effect API, bone projection, alpha drawing, PMM reload and morph runtime.

Reproduce with `frame_benchmark.exe <model.pmx> --load-unbuffered` and
`frame_benchmark.exe <model.pmx> --load-only`. Both reject any load dialog;
missing assets cannot silently become a successful fast-load result.

<a id="record-pmx-optional-fields-mmv-screenshots"></a>
## MMV 案例的实际渲染验证

原始记录：`PMX_OPTIONAL_FIELDS_MMV_SCREENSHOTS_20260928.md`。其中的版本、日期和验证范围沿用原记录。

2026-09-28，用户反馈加载 FX 后没有效果，并要求截图和位移量。
四个案例直接分配 FX，不加载 `.x`。用 MMS 生产库构建的隐藏宿主加载包内 PMX/VMD，
通过正式 MMV.dll 求值、上传最终网格，再从 D3D9 渲染目标保存 PNG。

<a id="record-pmx-optional-fields-mmv-screenshots-实测结果"></a>
### 实测结果

两侧使用同一 PMX、动作帧、相机、960×720 视口和默认 FX 参数。
两侧原生物理均关闭，FX 位于第 0 层。位移是同帧、同编号顶点在模型空间中的
欧氏距离，单位沿用模型坐标，不能直接换算成厘米。

| 案例 | 帧 | 修复前最大距离 | 修复后最大距离 | 修复后均方根距离 | 变化顶点数 |
| --- | ---: | ---: | ---: | ---: | ---: |
| DDM 重蒙皮 | 60 | 0.586999 | 0.586999 | 0.355207 | 208/210 |
| AVBD 网格刚体 | 30 | 0 | 4.307639 | 2.859075 | 16/24 |
| 弹簧骨骼 | 60 | 0 | 2.684299 | 1.551876 | 24/24 |
| 零层网格物理 | 30 | 1.999819 | 1.999819 | 1.178168 | 99/117 |

四组关闭 FX 的顶点数据在修复前后逐值一致。修复后四组开关截图均有像素差异，
没有放大位移或修改 PNG。DDM 的腰部体积、AVBD 的下方刚体姿态、弹簧骨骼的尾端
方向和网格布片的弯曲均能在对照中看到。

这些结果证明案例的网格输出进入了最终绘制，不构成碰撞精度、动画刚体跟随、
长期稳定性、大模型性能或用户当前工程的完整验收。未测播放 FPS。

<a id="record-pmx-optional-fields-mmv-screenshots-已修复的问题"></a>
### 已修复的问题

`PhysicsChain.pmx` 的 AVBD 和弹簧骨骼在修复前报错：

```text
object '37ff4e85d64d002f' item 'original' : original vertex data is not finite
```

宿主快照无条件读取四组附加 UV，并读取所有权重类型的 SDEF 数据。PMX 加载器
只初始化文件声明的附加 UV 和 SDEF 顶点的 SDEF 数据，未声明部分含未初始化内存。
宿主把这些值复制到 MMV 的固定大小数组后，快照有限数检查拒绝整个模型，
没有发布独立的变形网格。

`CopyPmxOptionalFields` 只读取已声明的附加 UV，仅在权重类型为 SDEF 时读取
SDEF 数据，其他固定数组元素清零。已声明字段中的非有限数继续传递给校验，
不以清零掩盖真实数据错误。PMX 文件和 MMV 求解器 DLL 均未修改。

<a id="record-pmx-optional-fields-mmv-screenshots-验证与复现"></a>
### 验证与复现

定向回归以 NaN 填充未使用字段，遍历 0–4 组附加 UV 和 0–4 权重类型，检查
真实字段逐分量保留、未声明字段归零、无效 UV 数量拒绝和真实 NaN 保留。
共 628 项检查通过。独立测试入口不需要重配置正式宿主或 MMV 构建目录：

```powershell
cmake -S tests/pmx_vertex_export -B .local/artifacts/deformer-no-effect/optional-build -A x64
cmake --build .local/artifacts/deformer-no-effect/optional-build --config Release
ctest --test-dir .local/artifacts/deformer-no-effect/optional-build -C Release --output-on-failure
```

截图素材目录为 `D:/MMD-Next/tests/MMV-4deformer`。共享物理 VMD 的模型名是
`Physics demo`，会触发模型名确认框；截图程序只在各次运行的副本中替换 20 字节
模型名称，50 字节之后的全部关键帧数据与分发文件一致，已做二进制比较。
每帧先调用生产 `SeekModelFrame` 应用姿态，再通过 `FrameDriver` 求值和绘制。

原始证据保存在 `.local/artifacts/deformer-no-effect/`：

- `comparison.html`：四组同帧开关截图。
- `measurements.json`：欧氏距离、均方根、变化顶点和像素数。
- `screens-v3/`：修复前完整截图、顶点二进制、逐帧 CSV 和诊断日志。
- `screens-after/`：同条件修复后结果。
- `render/render.cpp`、`render/prepare.py`：截图程序与独立构建脚本。
- `before-build/`：修复前 EXE、MMV.dll 和哈希。
- `after-hashes.json`：修复后 EXE、DLL 和源文件哈希。
- `fixed-host/Release/`：独立交互宿主修复版本，正式 `build/Release` 未覆盖。

两侧均为 MSVC 19.44.35228、Windows SDK 10.0.26100、x64 Release，链接同一份
既有 `mikudancestudio_core.lib` 和依赖库；修复后以单独编译的宿主适配模块替换
静态库中的旧适配模块。MMV.dll 两侧 SHA256 均为
`72fcc68ecdd5722e632fb089220ce5f1f1f6bc623a1a92dbedd739af91bd6ae8`。
`fixed-host` 为普通交互入口；自动截图运行的是同库、同修复模块的隐藏测试入口。

首次截图脚手架漏调用模型帧求值且相机距离符号错误，结果保留在 `screens/`，
不用于结论。`screens-v2/` 的物理动作导入因名称确认框阻塞，该测试进程已停止。
未重启用户程序或 DSH 宿主。DSH status 返回 `fetch failed`，本轮验证由主会话完成。
