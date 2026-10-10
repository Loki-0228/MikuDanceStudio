# MMV 案例的实际渲染验证

2026-09-28，用户反馈加载 FX 后没有效果，并要求截图和位移量。
四个案例直接分配 FX，不加载 `.x`。用 MMS 生产库构建的隐藏宿主加载包内 PMX/VMD，
通过正式 MMV.dll 求值、上传最终网格，再从 D3D9 渲染目标保存 PNG。

## 实测结果

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

## 已修复的问题

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

## 验证与复现

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
