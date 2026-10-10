# MMV 骨骼形变接入记录

原版 MMS 向 MMV 提供只读场景快照和形变输入服务。MMV 执行 Compute 网格效果或原生非物理形变，按骨骼顶点组权重混合私有网格，再完成阴影、模型、透明和后处理绘制。MMS 接收最终渲染目标，保留编辑器、操作控件及显示和录制入口。

## 接口与资源

`MMV_RENDER_ABI` 为 `3`。EXE 和 `MMV.dll` 必须配套更新。插件仍可选，不带 Modular 后缀，不与 MME 共存。

宿主增加原始顶点、刚体和 Joint 的快照需求，并提供只读 `physicsInput`。MMV 不写回输入网格、骨骼或约束。当前 DX12 网格输出回读到 MMV 私有内存，经权重混合后交给 D3D9 绘制；该路径尚未消除 GPU 回读。

骨骼分配采用 `[MMVBoneEffects]`，骨骼名称采用 `[MMVBones]`，参数采用 `[MMVBoneParameters@...]`。骨骼索引与名称共同校验身份。刚体按骨骼读取，Joint 保留两端和约束依赖。执行器当前接收完整场景数据，是依赖集合的保守超集。

运行包位于 `.local/releases/mmv-deformation-v3/`，包含配套 EXE、MMV DLL、FX 头文件、权重位移示例和原生 DDM。来源、工作树状态、构建参数和 SHA256 记录于包内 `source.json`。测试夹具 DLL 不随包发布。[分配步骤](../../MMV/docs/DEFORMATION_V3_USAGE.md)说明参数编辑和 EMM 保存方法。

## 已验证的行为

最终后台 CTest 的 3 项测试全部通过：渲染、连续顶点组权重、物理依赖读取。渲染测试包含原生 DDM、跨骨骼 Joint 两端读取、参数保存重载、材质形变继承对象绘制参数、失败候选回滚，以及输入快照保持不变。

真实 PMM 为 1 个模型、134 个顶点。Compute 和 DDM 两组分别接受 3/3 个采样帧，无失败，MMS 默认模型 CPU 与 GPU 上传计数均为 0。该 PMM 不含刚体或 Joint，约束读取由独立 CPU 和 GPU 用例验证。

原始记录保留于 `.local/artifacts/mmv-deformation-v3/`。`final/` 使用最终 DLL，其中 `compute.json`、`ddm.json` 和对应日志保留真实宿主结果。`final-ctest.log` 保留回归输出。先前记录与运行文件保留，不覆盖。

## 未完成的验收与能力

当前仅完成上述正确性检查。生产源码的修复前后配对构建、交替采样和性能对照尚未完成，不能据此认定性能提升。MMS 构建包含其他会话尚未提交的改动，具体列表保留于 `source.json`。

原生物理求解器的动画前接管尚未接入。声明 `MMV_DEFORMER_NATIVE_PHYSICS` 的插件被拒绝，宿主物理继续运行。Bullet3、AVBD、SDS、VBD 的接管不属于本次完成范围。

纹理与跨层 Compute 图、原生 `.x` 全局控制器分配、复杂多组重叠场景和完整 MME 兼容仍待接入或验收。本轮未重新验证 AVI 录制。
