# MMV 本轮界面与分配验收

2026-09-27。本轮完成后停止在当前会话继续修改。交付范围是参数窗背景、三语名称回退、固定骨骼与材质页签、右键操作，以及通用参数／K 帧面板的整合。

交付源码为 `e2e44eb`，代码树与已验收提交 `342bffe` 相同。正式 `main` 已本地整合，未推送。
运行目录 `build/Release/` 已更新宿主及兼容版 MMV；纯版在 `build/mmv/dx12/Release/MMV.dll`。
完整独立快照为 `.local/releases/mmv-native/ui-scope-e2e44eb/`，包含宿主、两版 DLL、资源、接口头文件、文档和逐文件 SHA256 清单。
旧安装保存在 `.local/archives/mmv-native/before-ui-scope-e2e44eb/`。

| 安装产物 | SHA256 |
| --- | --- |
| 宿主 EXE | `A140B30F61494A4AB875EA6814D199E4F413F000BCBC32F4407FAB2B30AF9499` |
| 兼容版 MMV.dll | `1FFB54C27CDFA99E646D064A08B0B2F97C39BC8062D88A124D5C48028FBC3081` |
| 纯 DX12 版 MMV.dll | `4FE0EFCCDAE47B6341D500B0518E8F2B6841F14A3A3733294C89E41F6B81F082` |

产物从独立工作树构建。正式目录中其他任务的烧录、Alembic 和场景采样改动原样保留，没有混入本轮构建。

## 本轮行为

- MMV 菜单位于右侧 MME 菜单左侧，随宿主切换日、英、中三语。
- 参数窗静态标签填充系统对话框底色；输入框和只读诊断区保留原生输入区背景。
- 模型、骨骼、表情使用宿主名称选择逻辑。PMX 材质加载时保留原名和英文名，优先使用操作语言对应字段，缺失则回退。中文界面沿用 MMS 的原名规则，不自动翻译模型文本。
- 显示名称与存档身份分离。两个固定页签各自保存分配和展开状态，固定展开类型不可修改。FX 扩展页支持 `MMVExpand` 声明和右键操作。
- 骨骼分配先于材质分配执行；骨骼行按该骨骼的归一化权重混合网格效果。刚体和 joint 保持为整个对象的只读查询数据，不增加独立分配组。
- 选中效果行后嵌入真实参数面板；切换页签切换对应实例，支持 `.uifx` 描述、三语、K 帧与配置热重载。保存格式升级至版本 2，并继续读取版本 1。

骨骼分配当前修改的是渲染网格，不是骨骼位姿输出。本轮没有实现 VMD 烧录接口；后续接口只允许烧录骨骼位置与旋转，不能把任意网格变形自动反推为骨骼。

## 验证证据

以下路径相对工作树 `.local/worktrees/mmv-native/`，原始失败记录均保留。

| 验证 | 结果 | 证据目录或日志 |
| --- | --- | --- |
| 分配页、右键、名称、嵌入参数面板 | 2170 项通过 | `.local/artifacts/mmv-native/parameters-integration/mmv_editor_tests-04.log` |
| 通用参数面板三语、DPI、K 操作、事务 | 8688 项通过 | 同目录 `mmv_parameter_editor_tests-04.log` |
| 透视参数窗及名称显示 | 838 项通过 | 同目录 `perspective-ui-03.log` |
| 运行时分配与恢复 | 157 项通过 | 同目录 `mmv_runtime_tests-04.log` |
| 骨骼范围与共享新增顶点 | 127 项通过 | 同目录 `bone-ui-03.log` |
| 存档版本、编码、语言回退 | 通过 | 同目录 `storage-ui-02.log`、`encoding-ui-04.log` |
| 外置描述与示例 | 68 项及 14 个 FX 编译通过 | 同目录 `mmv_parameter_effect_tests-04.log`、`mmv_example_tests-04.log` |
| 真实 DX12 参数与动画 | 180 项通过 | 同目录 `parameter-gpu-ui-04.log` |
| 两版生产 DLL、K 帧、骨骼权重、旧 ABI 宿主 | 各 128 项通过 | 同目录 `plugin-dx12-ui-04.log`、`plugin-compat-ui-04.log` |
| 纯版隔离／兼容版公开 ABI | 60／58 项通过 | 同目录 `isolation-*-06.log` |
| 兼容版真实 DX9 浮点纹理及参数 | 100 项通过 | 同目录 `compat-gpu-06.log` |

参数标签的同条件绘制对照保存在 `.local/artifacts/mmv-native/parameter-appearance/`：旧版 1230 项失败，新版 2065 项全部通过。编辑框和组合框在隐藏 WM_PRINT 下不能完整绘制，因此同时验证颜色消息约定，不把合成图当作桌面截图。空列表边框对照在 `editor-border/`，修复后三语、三种 DPI、两个窗口尺寸均无边框断口。

真实宿主对照在 `.local/artifacts/mmv-native/final-host-01/`。全部进程以隐藏窗口启动，并从 640×480 DX9 渲染目标回读。两轮前后交替运行的恒等图像 SHA256 完全一致：`48E637AFEBB0AF40564F88573A42BE6AAAE0144E869A953EF6D7F75725123686`。新版 PMX／附件新增材质通过 MME 名称查询、实际着色和效果分配保存；独立 DX9 纹理驱动同帧 DX12 网格通过。

前后 EXE、DLL 哈希、工作树状态和逐帧计算／回读／上传／绘制区间保留在该目录。基线诊断宿主复用 `host-production-03`，搭配本轮前正式安装的 `4b107c2` MMV。短时正确性测试的冷启动和调度波动不能证明性能提升，不推算播放 FPS；完整性能矩阵仍未验收。

纯版导入与延迟导入检查无 D3D9／D3DX9，隔离进程也验证了加载旧 FX 配置后不动态加载旧后端。宿主 37 项导出名称和序号与本轮前一致。

## 已知边界

完整旧 MME FX 语义、全部设备恢复与资源泄漏矩阵仍按 [原实现记录](MMV_IMPLEMENTATION.md) 保留为未完成。本轮验收不代表整个 MMV 首版计划已经全部完成。骨骼拓扑范围规则与存档身份见 [FX 接口](MMV_FX.md) 和 [存档格式](MMV_STORAGE.md)。
