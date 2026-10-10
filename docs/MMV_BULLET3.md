# MMV Bullet3 接入

本分支通过 `MMVPhysics="Bullet3"` 标注识别物理 FX。独立计算项目位于 `D:/MMD-Next/MMV-Bullet3`，Bullet 源码只读引用 `D:/MMD-Next/refs/bullet3-master`。用户入口为该项目的 `effects/Bullet3_mmv.fx` 和同名 `.uifx`。

## 按模型或骨骼启用 Bullet3

内置 MMV 页位于固定的 0 层，可选择模型整行或骨骼子行。模型整行为全模型刚体提供默认规则，骨骼子行选择关联刚体并覆盖默认规则。材质子行不适用于 Bullet3。原因见[分配范围设计](MMV_ASSIGNMENT_SCOPE_DESIGN.md)。

1. 打开「MMV → 效果分配」。
2. 选择 0 层的内置分配页。
3. 要模拟整个模型，选中模型整行；要指定刚体组，右键允许展开，选择骨骼展开后选中对应骨骼。
4. 点击「设置效果」，选择运行目录中的 `MMV/Bullet3/Bullet3_mmv.fx`。
5. 在参数中将「物理引擎」设为「Bullet3」。修改参数后点击「应用」。
6. 如需暂停时拖动骨骼仍继续模拟，勾选「编辑时模拟」并应用。

启用成功后，插件自动把宿主「物理演算」切换为「不计算」。仅手动选择「不计算」不会启动 Bullet3。切回「MMS 原生」或移除效果时，插件按恢复规则处理此前的原生物理模式。

确认窗口底部没有解算错误，再播放或拖动模型骨骼检查网格变化。拖动时间轴可能触发物理重建，不能要求任意跳帧都保留连续播放的物理历史。

## 排查分配位置错误

旧版曾只接受模型主输出，用户在骨骼页遇到以下错误：

```text
Bullet3 FX must be assigned to a model's main output
```

分层版本解除该模型／骨骼范围限制。仍出现此句完整旧错误时，应核对运行目录中的 `MMV.dll` 是否已更新。模型需要有效刚体；选择的骨骼必须确有关联刚体。缺失或不支持的数据会显示具体原因，不自动扩大成全模型范围。

2026-09-27 的用户截图中，「骨骼分配」页模型行显示「就绪」，底部却报告上述错误。该行的「就绪」不能单独证明物理解算成功，应同时查看底部诊断。此记录确认了错误分配，未把改换位置后的模型表现记作已复测通过。

两种分配的作用范围见[页签与分配范围](MMV_PAGES_AND_ACCESSORIES.md#选择作用范围)，界面歧义及改进建议见[分配范围设计分析](MMV_ASSIGNMENT_SCOPE_DESIGN.md)。

## 参数与状态

| 参数 | 默认值 | 行为 |
| --- | --- | --- |
| `PhysicsEngine` | 1 | 0 恢复 MMS 原模式并透传网格；1 关闭原生物理并执行 Bullet3；不接受关键帧 |
| `GravityScale` | 1 | 宿主重力倍率，范围 0～10，可记录关键帧 |
| `SolverIterations` | 20 | 约束迭代次数，范围 1～200 |
| `SimulationHz` | 60 | 固定步频率，面板提供 30、60、120、240 Hz |
| `GroundCollision` | 开启 | 添加 Y=0 地面，遵循碰撞组与掩码 |
| `SimulateWhileEditing` | 开启 | 同一帧编辑时按宿主帧间隔模拟；关闭则冻结暂停结果 |

参数使用 MMV 参数系统和 `.vmm` 存档。引擎选择作用于整个场景。各骨骼组分别设置重力、地面碰撞和编辑模拟，共享求解采用组内最大的模拟频率与约束迭代次数。所有 Bullet3 组位于同一 MMV 层，保持组间碰撞和跨组关节。未选中的刚体仍作为动画碰撞体。

`MmvHostServices` 的可选尾部新增两个源码接口，ABI 版本保持 1：

- `physicsInput` 返回只读的形变后、蒙皮前顶点，刚体绑定变换、惯量、关节局部坐标和原生物理状态。数组生命周期为本次 `evaluate`。
- `selectBullet3` 通过现有 `WM_COMMAND` 菜单处理关闭原生物理，保存此前模式，释放时恢复。原先已关闭则仍恢复为关闭。

宿主继续拥有原生物理世界。计算 DLL 正常加载，只输出独立刚体状态；适配器生成 MMV 网格，不改写宿主骨骼、原生刚体或原生步进函数。没有注入或 hook。

## 模拟与限制

球、盒、Y 轴胶囊、6DoF 弹簧关节和碰撞组使用独立 Bullet3 世界。PMX 物理模式 1 驱动骨骼位置与旋转；模式 2 保留动画骨骼位置并使用物理旋转。BDEF1、BDEF2、BDEF4 和 SDEF 使用现有蒙皮数学。没有使用 GPU Bullet3。

通用 deformer 路径按固定时间步求解，每次求值最多推进 32 步，未消费时间留待后续求值。播放和手动向前拖动时间轴均按时间轴增量推进；同帧编辑使用实际帧间隔，关闭 `SimulateWhileEditing` 可冻结暂停结果。向后跳帧从当前动画姿态重建，不反向重放历史。初始化可能等待一个宿主帧，以避免使用关闭原生物理之前的快照。

`MMVPhysicsOutput="Direct"` 声明 FX 的着色器只是恒等回退。场景中只有这种模型或骨骼物理效果，且没有子页或材质效果时，直接返回 CPU 物理网格，无需 DX12 上传与回读。其他组合使用分层图执行路径。自定义着色器改变几何时不得声明 `Direct`。

通用适配器读取当前层的完整上游快照，包括蒙皮前顶点、骨骼、绑定、刚体、形状和关节。生成骨骼或顶点具有合法绑定时可参与求解，不能仅凭原始 PMX 索引判断有效性。求解后的骨骼与刚体快照供下层读取，不写回 MMS 原生状态。受物理影响的 QDEF、缺失蒙皮前顶点、凸包／组合形状和跨对象关节会明确报告不支持。盒与胶囊的非对角惯量也会拒绝，不丢弃惯量分量近似计算。

2026-09-27 的通用适配器和按层烘焙仍在集成验收。独立适配器测试已覆盖事务丢弃、共享世界及合法生成顶点；这不能代替生产 MMV DLL、实际宿主与烘焙回放验收。下文早期正式版本记录保留用于溯源。

## 构建与验收

CMake 的 `MMV_BULLET3_SOURCE_DIR` 指向独立项目后，兼容版和纯 DX12 版都会包含适配器。留空时维持原构建能力。完整构建步骤见独立项目的 `docs/build.md`。

### 将计算 DLL 放在 FX 文件夹

通用 deformer 版本将 `MMVBullet3.dll` 随 FX 一起分发，放在 FX 同目录或子目录。将以下声明加进该 FX 已有的 `MMVConfig` 注解中，不要新建第二个 `MMVConfig`：

```hlsl
float MMVConfig <
    string MMVBackend = "DX12";
    string MMVPhysics = "Bullet3";
    string MMVPhysicsOutput = "Direct";
    string MMVDeformerLibrary = "MMVBullet3.dll";
>;
```

此时目录可以是：

```text
任意效果文件夹/
  Bullet3_mmv.fx
  Bullet3_mmv.uifx
  MMVBullet3.dll
```

`MMVDeformerLibrary` 相对于入口 FX 文件解析。放在子文件夹时可写 `"dll/MMVBullet3.dll"`；DLL 的本地依赖放在 DLL 同目录。DLL 缺失或接口不匹配时报告错误，不改用其他目录的同名 DLL。未声明此字段时，默认从 FX 同目录加载 `MMVBullet3.dll`，不搜索宿主目录或其他 FX 文件夹。

该方式需要配套的通用 deformer 版 `MMV.dll` 和导出 `MMVDeformerGetApi` 的 `MMVBullet3.dll`。旧版专用 Bullet3 适配器不支持通过此声明改路径。`MMV.dll` 自身仍放在宿主运行目录。

2026-09-27 的生产 runtime 验证在宿主目录不含计算 DLL 的条件下，通过 FX 相对路径加载 SHA256 为 `A2914C4A6383CEEB0A8C4DC1FBAA7A786404AE4694156AC401DFCC41508AA832` 的适配器。CPU 物理检查通过 866 项，DX12 分层检查通过 173 项，包含骨骼分配、两份 FX 的独立重力与共享碰撞，以及下层读取求解后骨骼。原始结果为 `.local/artifacts/mmv-bone-physics/fx-relative-cpu-01.log` 和 `fx-relative-gpu-01.log`；这两项是生产 runtime 与真实计算 DLL 的集成测试，未冒充完整宿主界面验收。

后台验收数据位于此工作树的 `.local/artifacts/mmv-bullet3/`。`host-rin-dx12-02`、`host-rin-compat-final`、`host-luo-dx12-final` 使用生产 MMV DLL 和真实 PMX，检查网格变化、暂停、原生刚体不回写、切换恢复及移除效果。PNG 来自 640×480 渲染目标回读，未截取桌面。

`comparison/` 保存基线 `8dfa7103` 与接入后独立 build 的交替原生模式对照。双方使用相同模型、测试驱动、相机、分辨率、参数和时间步；初始帧与第 60 帧 PNG 的 SHA256 在六次运行中完全一致。构建来源、Git 状态、EXE/DLL 哈希和编译配置见 `comparison/manifest.json`，原始计时见各运行的 `timing.csv`。这不是 Bullet2 与 Bullet3 的性能比较，不据此宣称播放 FPS 或速度提升。

Bullet3 接入已合入 `main`，并保留最新曲线编辑和参数窗口修复。正式运行目录为 `build/Release/`，宿主 EXE 旁放置配套的 `MMV.dll`；`MMV/Bullet3/` 内一起放置 FX、UIFX 和 `MMVBullet3.dll`。本地构建缓存已启用 `MMV_BULLET3_SOURCE_DIR=D:/MMD-Next/MMV-Bullet3`；新的构建目录仍需显式设置该选项。

主分支合并和组合测试见 [集成验收](MMV_BULLET3_MAIN_INTEGRATION.md)。早期测试与 Rou 修复分别保留在独立项目的 `docs/validation.md`、`docs/rou-fix.md`。
