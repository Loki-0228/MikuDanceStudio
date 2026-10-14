# 工程、动作与容量验证记录

本页按功能归并实现、诊断与验收资料。各记录中的「当前」「已完成」只适用于其注明的源码、构建和测试条件；当前使用入口见[文档导航](../../README.md)。

## 记录索引
- [PMM v2 项目扩展验证记录](#record-pmm-v2-extensions-validation)
- [PMM 启动追踪镜头修复](#record-pmm-camera-load)
- [VMD 捩骨旋转载入修复](#record-vmd-fixed-axis-import)
- [骨骼与表情关键帧容量验收记录](#record-keyframe-capacity)
- [关键帧持久化与烧录检查](#record-keyframe-persistence-audit)
- [文件名编码与 PMM 往返读取](#record-filename-encoding)

<a id="record-pmm-v2-extensions-validation"></a>
## PMM v2 项目扩展验证记录

原始记录：`PMM_V2_EXTENSIONS_VALIDATION.md`。其中的版本、日期和验证范围沿用原记录。

日期：2026-09-25。格式与后续开发规则见 [PMM v2 项目扩展格式](../../reference/pmm-extensions.md)。

<a id="record-pmm-v2-extensions-validation-结论与范围"></a>
### 结论与范围

Release 生产程序编译成功。独立编解码的 145 项断言通过，隐藏窗口中的实际 PMM 保存、加载、另存和新建流程通过。运算器与透明排序均随项目恢复；缺少指定显卡类型时回退自动，保存时保留原来的类型意图。PMM 文件头仍为 `Polygon Movie maker 0002`。

本次没有运行原版 MMD 的交互窗口。原版读取附加尾部、再次保存丢弃尾部的结论来自静态分析，不能推广为所有第三方读取器的保证。性能对照已采样，但波动及共享工作树的并行修改使本次性能归因未验收。

<a id="record-pmm-v2-extensions-validation-构建与产物"></a>
### 构建与产物

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

<a id="record-pmm-v2-extensions-validation-正确性"></a>
### 正确性

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

类型缺失使用合成设备列表验证，不声称实际移除了本机硬件。MME 保存通知沿用现有接口，具体 MME 验证见 [EMM 自动保存记录](../effects/compatibility.md#record-emm-autosave)。

<a id="record-pmm-v2-extensions-validation-后台性能对照"></a>
### 后台性能对照

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

<a id="record-pmm-v2-extensions-validation-静态兼容证据与委派"></a>
### 静态兼容证据与委派

原版 MMD 9.32 x64 的 SHA256 为 `07516fd3bf1e6b1339836b6773a156f61bdd6f848eeb621fdda012375df313a1`。PMM 读取末尾、保存截断及 MME 0.37 对未知 EMM 字段的处理，分别记录在 `.local/artifacts/pmm-format-inspection/inspection.txt` 和 `.local/artifacts/emm-format-inspection/inspection.txt`。

编解码模块通过 DSH 子任务 `dad152e0-83bb-4e21-91ad-85f040a8959c` 实现，实际工作目录为上述仓库。其修改范围为 `include/mikudancestudio/pmm_extension.hpp`、`src/io/pmm_extension.cpp`、`tests/pmm_extension_tests.cpp`。主任务完成代码复核、独立编译、功能集成和验收；正式子任务结果保留为 `dsh-result.txt`。

<a id="record-pmm-camera-load"></a>
## PMM 启动追踪镜头修复

原始记录：`PMM_CAMERA_LOAD_20261015.md`。其中的版本、日期和验证范围沿用原记录。

2026-10-15。反馈现象：工程打开后镜头错误，开始播放或原地点击第 0 帧恢复正常。

<a id="record-pmm-camera-load-原因与修复"></a>
### 原因与修复

在 `tests/QSPEED菠萝能量仓库 Ray用一键渲染/UserFile/Stage.pmm` 上复现。工程保存在模型编辑模式，并开启相机追踪。PMM 保存的编辑视角距离为 `87.7497101`，第 0 帧相机关键帧的距离为 `-15.0999985`。加载器直接使用前者；刷新帧或播放才重新计算关键帧。

PMM v1、v2/v3 的加载成功尾部现在仅在「模型编辑模式且相机追踪开启」时清除临时父级变换抑制状态，并计算当前镜头关键帧。操作发生在模型、帧号和相机父级映射恢复之后。自由视角、相机面板中未登记的编辑、保存的视图偏移和模型姿势继续保留；加载不会将工程标记为已修改。

<a id="record-pmm-camera-load-验证"></a>
### 验证

- 修复前，反馈工程打开与原地刷新后的三组矩阵最大元素差为 `102.849709`，回归退出码为 7。
- 修复后，原生及真实 MME 接入的隐藏测试均通过，镜头矩阵差为 0。MME 测试确认 `MMEffect.dll`、`MMHack.dll` 已加载；本轮没有进行 Ray 特效像素对比。
- 七种隔离 PMM 保存回读通过：有无父骨骼各覆盖第 0 帧与插值帧；关闭追踪的模型自由视角，以及相机面板的追踪开、关状态均保留未登记编辑。
- 旧版 PMM v1 副本在模型模式、有效父模型和追踪尾段下通过。测试只重新计算镜头，不刷新模型骨骼，避免清除示例内保存的未登记姿势。
- 正式 `build/Release/MikuMikuDance.exe` 构建成功。反馈工程 SHA-256 保持不变：`159ad3df96759adc64af5667146124d081258b7df16ab9866782c35aa38c4649`。

证据保存在 `.local/artifacts/pmm-camera-load-20261015/`：`baseline.log`、`fixed-native.log`、`fixed-mme.log`、`fixed-v1-tracked.log` 和构建日志。源工程及特效素材不纳入仓库。

本次 EXE 来自共享工作区，包含另一窗口尚未提交的 GPU 读回改动；镜头修复的提交不包含这些文件。构建时 EXE SHA-256 为 `c268f9d8c36b3f293e37a282297b73f1c71a52848bb98bb5e4d1560362c48865`。

<a id="record-pmm-camera-load-ui-检查后续修复"></a>
### UI 检查后续修复

镜头修复时，`coordinate_axis_tests` 的文字居中检查失败（实际 Y=335，期望 Y=330），`camera_follow_tests` 的子控件重绘检查也失败。后续排查确认两者的测试预期已过时，运行代码无须修改。

- 文字位置测试原先固定使用场景矩形下边界。底部工具条压缩后，HUD 可用区域会向下扩展；测试现在按当前面板的 `FrameOverlayRect` 检查居中，保留原有像素误差阈值和三语、双面板覆盖，并固定测试中的停靠角，避免读取用户偏好。
- 场景变更结束时已使用 `RDW_UPDATENOW` 同步重绘，绘制完成会清空更新区域。旧测试却要求更新区域仍非空。测试现在记录两个子控件的实际 `WM_PAINT`，确认绘制发生在场景恢复之后、更新区域已清空，并检查嵌套变更不会提前重绘。

2026-10-15 重建后，`coordinate_axis_regressions` 和 `camera_follow_regressions` 均通过（2/2）。日志保存在 `.local/artifacts/ui-regressions-20261015/ctest.log` 和 `LastTest.log`。本次仅修改测试及记录，无需重新构建正式 EXE。

<a id="record-vmd-fixed-axis-import"></a>
## VMD 捩骨旋转载入修复

原始记录：`VMD_FIXED_AXIS_IMPORT_20261012.md`。其中的版本、日期和验证范围沿用原记录。

载入 VMD 时，固定轴骨骼的旋转被错误地重新计算。例如测试动作中的 −60° 被载入为约 +32.204228°，使捩骨旋转反向并改变角度。左右腕捩、手捩会影响手臂和手腕姿态。

`RegisterBoneKey` 原先将带符号的半角正弦乘以 `D3DXQuaternionToAxisAngle` 返回的向量。实际 D3DX 函数直接返回四元数的 xyz，该向量并非单位轴；此处应乘以模型骨骼自身的固定轴。修复只更换这三个分量的乘数，保留符号判断和最后的单位化。

原版依据为 `refs/MikuMikudance 932 fix/MikuMikuDance.exe`，SHA-256：`3ebe6ead8076478c06539b14a0e32e70f30b0edc9c8f045819d04ad8d62bee49`。x64 地址 `0x1400C96EF..0x1400C9719` 将带符号的半角正弦乘以保存在 xmm11/xmm10/xmm12 中的骨骼轴，随后在 `0x1400C971E` 单位化。镜像载入分支已经使用骨骼轴，无需修改。该错误已存在于初始公开快照，不是此前鼠标旋转柄方向修复所引入。

<a id="record-vmd-fixed-axis-import-验证"></a>
### 验证

- `vmd_load_safety_tests --case twist` 经实际 VMD 文件解析和关键帧登记入口检查 576 组：VMD 1/2、PMD/PMX、固定轴骨骼、带固定轴的 IK 骨骼、普通骨骼对照、正反轴、斜轴、正负角度及 q/−q。修复前 312 组失败，修复后全部通过，最大四元数分量误差约 `1.19e-7`。
- 完整 VMD 载入测试的 8 组分类全部通过，包括截断输入、计数边界、尾部扩展、相机及 IK 状态。
- `bone_rotation_drag_tests` 完整档 188 项全部通过，包括原版旋转方向比较和连续 10,000 次拖拽检查。上述测试不创建窗口或图形设备。
- 完整档正式程序已重新编译到 `build/Release/MikuMikuDance.exe`。

用户提供的模型名已在 `tests/pmx/鸣潮_锁暝（修模绑骨后-改）_by_鸣潮/鸣潮_锁暝1.0.pmx` 找到。文件 SHA-256 为 `ad7c02e901732b782f94a62d5e23ab6b8b32605fb32b66f2f8c9a315bd3984aa`。模型共有 568 根骨骼，左腕捩、左手捩、右腕捩、右手捩的索引分别为 43、48、76、81，标志均为 `0x041a`，包含固定轴标志，属于本次修复覆盖的类型。

日志保存在 `.local/artifacts/twist-import-20261012/`：`before.log`、`after.log`、`rotation.log`、`model-bones.json`。用户无法提供照片所用的 VMD 和帧号，因此未复现照片中的完整动作；已验证的是实际载入入口的旋转错误及模型骨骼类型。已经保存进工程的错误关键帧不会自动恢复，应重新载入原始 VMD。

<a id="record-keyframe-capacity"></a>
## 骨骼与表情关键帧容量验收记录

原始记录：`KEYFRAME_CAPACITY_20261004.md`。其中的版本、日期和验证范围沿用原记录。

2026-10-04，工作目录 `D:/MMD-Next/MMS`，Win64 Release。

<a id="record-keyframe-capacity-行为"></a>
### 行为

骨骼池已有动态扩容，600000 是 Win64 初始容量。表情池现在也按需扩容，20000 只作为初始容量。注册、曲线写回、烘焙、选择复制、时间轴编辑及 PMM、VMD 读写按实际容量操作。池扩容保持槽索引和 ModelRecord 布局，曲线状态及编辑器用稳定池身份识别同一模型。

表情烘焙准备阶段计算容量并保存快照，提交阶段扩容。回滚恢复旧槽和帧范围，并清空新增槽。顺序写入保留每个表情的链游标。曲线层的百万关键帧数量限制也改为索引和可寻址内存边界。

分配失败不改写原池。注册提示内存或索引错误；PMM 载入无法扩容时中止加载并提示错误，避免把缺失关键帧的项目当作完整项目。

<a id="record-keyframe-capacity-验证结果"></a>
### 验证结果

- 动态池跨 20000 扩至 90001 槽，记录、链索引和空槽正确；越过结构边界失败时原池不变。
- 满池的直接注册及记录注册通过，共享游标来自骨骼池时仍能定位表情空槽。
- 22001 帧表情烘焙、未选表情保留及新增槽回滚通过。
- 22001 个曲线关键帧写回、其他表情自定义切线保留、编辑器身份及 VMD 扩展往返通过。
- 超过百万关键帧的曲线验证、烘焙和编码通过。
- 用 `D:/MMD-Next/tests/UserFile/Model/Miku_Hatsune_Ver2.pmd` 验证原上限之外的稀疏槽，两次真实 PMM 保存重载及标准 VMD 导出保留帧、数值和链接。
- 骨骼扩容、表情导入、时间轴改速及 PMM 重载回归通过。

容量专项命令为 `build/tests/Release/channel_bake_tests.exe --morph-capacity`。真实文件往返命令为 `build/tests/Release/pmm_project_settings_tests.exe <输出目录> D:/MMD-Next/tests/UserFile/Model/Miku_Hatsune_Ver2.pmd --key-capacity`。

修改前后的原始结果、Git 状态和 EXE 哈希保存在 `.local/artifacts/key-capacity/before/` 与 `after/`。正式可执行文件为 `build/Release/MikuMikuDance.exe`。

<a id="record-keyframe-capacity-未完成的验收与边界"></a>
### 未完成的验收与边界

通道烘焙完整套件仍有相机采样和菜单相关的 6 项失败，与修改前相同。原版重新编译的表情 UI 套件另有 4 项箭头布局断言失败；修改前运行的是已有 UI 测试 EXE，此项不能作为相同源码构建的有效前后对照。相关布局函数未在本次修改中改动。完整回归未全部通过。

本次验证功能正确性，没有进行播放 FPS 或 GPU 性能验收。内存不足、32 位槽索引溢出以及现有曲线扩展的序列化字节长度保护仍然生效；帧号范围不属于本次数量上限修改。含超出旧容量槽的 PMM 需要本次构建才能完整加载。

<a id="record-keyframe-persistence-audit"></a>
## 关键帧持久化与烧录检查

原始记录：`KEYFRAME_PERSISTENCE_AUDIT_2026-09-27.md`。其中的版本、日期和验证范围沿用原记录。

本轮检查原生骨骼缩放、MMV 参数动画、网格缓存的保存路径，以及「烘焙 → 烧录关键帧…」的入口和执行行为。验收以 `2e684bf` 为共同基线，只加入本轮修复；同一工作区中并行开发的完整曲线编辑器另行验收。

<a id="record-keyframe-persistence-audit-当前支持范围"></a>
### 当前支持范围

| 数据 | PMM 工程 | VMD 动作 | 关键帧烧录 |
| --- | --- | --- | --- |
| 骨骼位移、旋转、表情权重、IK 开关 | 标准数据 | 标准数据 | 逐帧重采样骨骼；保留表情与 IK 轨道 |
| 骨骼缩放和剪切 | PMM v2 尾部扩展，保存完整 3×3 矩阵 | 单文件尾部扩展，按选中关键帧导出并支持导入偏移；原版 MMD 忽略扩展 | 随骨骼关键帧保存，撤销和重做包含缩放 |
| MMV 参数关键帧 | 同目录同名 `.vmm`，依赖 MMV 自动保存/加载设置；未内嵌进 PMM | 尚未接入 | 不写入骨骼关键帧；最终顶点变形可经 ABC 导出或网格烧录 |
| 附件位移、旋转、统一缩放等原生附件轨道 | PMM 标准附件记录 | 当前原生 VMD 导入导出没有附件轨道分支 | 不属于当前骨骼烧录；ABC 包含最终网格 |
| 已烧录的变形网格 | PMM 扩展引用旁边的 `.mdsmesh.abc` 文件 | 不支持网格缓存 | 当前模型有网格缓存时禁止再次烧录原始骨骼；清除网格烧录后恢复 |

MMV 参数当前提供线性、平滑、阶梯插值。已有实现会把参数关键帧写入 VMM，也会随 VEM 对象存档保存并自动加载。将 VEM 限定为手动静态预设、完整曲线和更多 VMD 扩展，属于 [MMV 曲线规划](../extensions/interface.md#record-mmv-curve-motion-plan)，不能据规划认定当前已实现。

VMD 导入是合并动作：普通 VMD 将本次导入登记的骨骼关键帧缩放重置为单位矩阵，没有覆盖到的旧帧仍保留。名称受标准 VMD 的 15 字节骨骼名字段限制；扩展对找不到或存在歧义的骨骼不猜测匹配。

<a id="record-keyframe-persistence-audit-修复"></a>
### 修复

- 菜单状态和直接命令统一检查所选模型、可烧录骨骼、播放、录制、ABC 导出状态及当前模型的网格缓存。其他对象有缓存不妨碍当前模型烧录。
- 关键帧对话框明确列出位移、旋转、缩放范围，提示 MMV/MME 的最终变形走 ABC。
- 关键帧烧录和 ABC／网格烧录期间禁用同一界面线程的其他窗口，包括独立的非模态编辑窗口。进度对话框只保留取消；排队的编辑命令、快捷键、拖入文件和主窗口关闭命令被拒绝。结束、取消、失败后只恢复原本启用的窗口。
- 修复 PMM 中能解码却与当前骨骼身份或帧不匹配的缩放扩展在下次保存时被删除的问题。没有修改缩放时保留原始字节；真实缩放编辑或模型替换仍遵循原来的替换规则。

<a id="record-keyframe-persistence-audit-验收记录"></a>
### 验收记录

原始构建日志、测试结果、失败复现和哈希清单保存在 `.local/artifacts/keyframe-audit/`。两个独立工作树为 `.local/worktrees/keyframe-audit-before` 与 `.local/worktrees/keyframe-audit-after`。

旧代码已复现两项失败：当前模型带网格缓存仍能打开关键帧烧录；被拒绝应用的 PMM 缩放记录在再次保存时被删除。原始日志分别为 `before/menu-cache-export.log`、`before/pmm-refused.log`。新增回归驱动临时链接到旧生产代码后运行，旧工作树的测试源码随后恢复，生产代码未修改。

修复后的结果：

- 5 项 CTest 通过：骨骼缩放行为、缩放编码、MMV 存档、参数和运行时。
- 重新构建的 MMV 兼容版 DLL 通过 128 项真实插件参数检查，包括存档重开后关键帧驱动 GPU 顶点变形。
- 隐藏界面单元回归、真实物理工程回归、ABC／网格缓存集成回归均通过。两类烧录都实际执行了完成和取消路径，并检查独立编辑窗口恢复状态。
- 物理工程包含 140 根骨骼、45 个刚体、27 个关节，烧录 23 根物理骨骼。实际 PMM 保存重开保留所有缩放矩阵；298 个导出关键帧通过 VMD 在偏移 0 和 37 下的物理标记与完整缩放矩阵比较。普通关键帧重采样另验证了插入帧的动画缩放、剪切和位移。

独立验收提交为 `6c11054`，同一修改集在主分支提交为 `df9f00a`。正式程序 `build/Release/MikuMikuDance.exe` 已更新为该独立构建，SHA256 为 `b1e63bcf46c00e173fbea31d13dd7ef716a54c06755eeb5902f4f11ddfd8cc0a`。它包含本轮修复，不包含并行曲线编辑器尚未完成的集成。旧 EXE 保存在 `.local/archives/keyframe-audit/`；完整独立运行目录为 `.local/releases/keyframe-audit-20260927/`。源文件、编译参数、EXE 和 DLL 哈希见 `final-manifest.json`。

前后均为 VS 2022 x64 Release，使用同一素材、1280×720 原生 D3D9、预热 15 帧、采样 60 帧、两轮 ABBA：

| 场景 | 修复前中位耗时及范围，ms/帧 | 修复后中位耗时及范围，ms/帧 |
| --- | --- | --- |
| 单份草地 | 204.879（198.775–206.618） | 203.625（190.786–214.491） |
| 人物移动相机 | 7.219（6.458–8.343） | 6.734（6.383–7.772） |

前后范围重叠，没有观察到稳定的性能回退，也不据此宣称性能提升。16 次原始采样和环境清单位于 `performance/`。早期编译引用错误和测试自身的 Win32 类名／延迟销毁判断错误保留在日志中，不计入通过结果；最终正确性结果位于 `after-final/`。

所有窗口测试均在后台隐藏运行，未使用桌面截图。性能口径为等待 GPU 完成后的批量帧耗时，不是播放 FPS 或独立 GPU 时间。

<a id="record-filename-encoding"></a>
## 文件名编码与 PMM 往返读取

原始记录：`FILENAME_ENCODING.md`。其中的版本、日期和验证范围沿用原记录。

保存路径不能依赖界面语言或进程的 `japanese` CRT locale。原先 CP932
转换失败后调用 `wcstombs_s`，实际上仍用日文编码转换，中文路径可能被清空；
ANSI 控件和旧 MME 窗口过程还会在显示时再次损坏 Unicode 名称。

<a id="record-filename-encoding-当前处理"></a>
### 当前处理

- 命令行直接读取 UTF-16；主窗口使用 Unicode 窗口类和消息处理。
- PMM 模型、配件、音频、背景路径使用严格、可逆的编码转换，拒绝默认字符、
  best-fit 替换及截断。空路径写零，固定字段的剩余字节清零。
- 保留旧格式字段布局：优先 CP932，然后系统代码页、CP936、CP950、CP949。
  旧工程读取按这些代码页及 UTF-8 尝试，以实际存在的文件消除路径歧义。
- 无法用旧代码页完整表示的路径使用带 BOM 的 UTF-8 字段。这是本程序的兼容
  扩展，旧版 MMD 和第三方插件不保证能读取这类字段；需要与旧版交换工程时，
  应使用其代码页能够表示的素材路径。PMM 的 256 字节路径字段仍保留，过长时
  在打开/截断保存目标之前报错，避免写出残缺路径。
- 配件名称使用 Unicode 控件传递与绘制，包括下拉框、时间轴、重命名/排序。
  旧名称与已解析的文件名匹配后再转为显示文本；自定义名称不被文件名覆盖。
  配件名称仍受原格式 100 字节字段限制，重命名超限会提示。
- PMX 模型名称直接使用模型中的 Unicode 文本，保存时也从这一文本生成名称
  字段；PMD 名称按格式要求使用 Shift-JIS。切换界面语言不会再次转码 PMX 名称。
  模型绘制/计算排序窗口使用 Unicode，并按实际名称长度分配文本缓冲区。
- 旧 MME 安装 ANSI 窗口过程后，用 Unicode 包装过程保护窗口标题的
  `WM_SETTEXT/WM_GETTEXT`，其余消息继续交给原窗口过程。

无编码标记且找不到对应素材的旧字节串，无法仅凭内容可靠判断原始代码页。
已经保存成问号或空路径的文件也无法凭空恢复，需从未损坏的工程/素材重新载入。

<a id="record-filename-encoding-验证"></a>
### 验证

启用 `MIKUDANCESTUDIO_BUILD_ENCODING_TESTS=ON`，显式构建
`filename_encoding_tests`，然后执行 `ctest --test-dir build -C Release --output-on-failure`。
测试覆盖简体、繁体、日文、韩文、混合字符和表情路径，固定字段写入后重读、
旧代码页解析、超长字段拒绝、PMX Unicode 名称，以及 ANSI 窗口过程下的标题。

实机使用桌面 `lty.pmm` 的副本，另存为 `保存_日本語_😀.pmm` 后重开，
保存文件中的 13 个模型/配件引用均能解析到原素材；桌面原始 PMM/EMM 不改写。
配件自定义名称 `测试_髪_😀.x` 也用于保存与重读验证。
测试模型自身的英文名称字段已经是乱码，中文名称字段为 `石英式洛天依七夕Ver`；
程序保留素材原文，不猜测性改写模型元数据。
此验证针对 PMM 文件名与引用路径，不把第三方 MME 的 EMM 自动保存视为已验证。
