# DX12 缺失时的启动兼容验证

默认发行程序已移除 `d3d12.dll`、`dxgi.dll` 和 `d3dcompiler_47.dll` 的普通、
延迟导入。默认构建仍启用 DX12 功能，运行时不可用时转入 DX9 和 CPU 蒙皮。
正式输出为 `D:/MMD-Next/MMS/build/Release/MikuMikuDance.exe`，集成分支为 `main`。

## 版本与产物

修复前源码为 `49020e8`，构建前工作树无改动。修复后源码为本记录所在提交。
两侧均用 Visual Studio 17 2022、x64、Release 和同一 Conan 工具链。
默认构建参数为 `MIKUDANCESTUDIO_ENABLE_DX12=ON`。

| 版本 | EXE SHA-256 |
| --- | --- |
| 修复前 | `075e87c0440d32b6bdab738f61350e07a840f015d4131153eb42c3b2c142d4b4` |
| 修复后默认构建 | `97ed3103f6024c98a6d3f420d9d79421cd6a9f4f01fe5739a9a71020e007ffe0` |
| 修复后关闭 DX12 | `70b8da9dc3cbcc097b4e4ab1db89392d6436538716c7c0abe925123d870f3915` |

原始日志、导入表、哈希和启动检查脚本保存在
`.local/artifacts/dx12-optional-startup/20261006/`。
独立运行快照位于 `.local/releases/dx12-startup-before-49020e8/`、
`.local/releases/dx12-startup-after-20261006/` 和 `.local/releases/dx9-startup-20261006/`。
这些目录用于本次对照，未纳入 Git，也未替代正式发布目录。

关闭 DX12 的构建目录为 `build/dx9-compat-20261006/`。原有
`build/dx12-disabled/` 包含迁移前路径的 CMake 缓存，配置失败日志已保留；
未删除或覆盖该目录。

## 验证结果

| 检查 | 修复前 | 修复后 |
| --- | --- | --- |
| 正式 EXE 可选图形 DLL 导入检查 | 检出三个 DLL，按预期返回 1 | 无普通或延迟导入，返回 0 |
| 默认程序隐藏启动、消息响应、正常关闭 | 通过 | 通过 |
| 全局运算器设置回归 | 99 项通过 | 99 项通过 |
| 既有 `skinning_backend_tests` | 菜单文字断言失败 | 同一断言失败 |

既有失败为 `FAIL the automatic choice never names a device`。测试在进入主体
蒙皮对照前退出，因此本次不将该完整测试标为通过。未修改这项无关菜单断言。

新增的六个后台场景均通过：系统正常、DX12 DLL 缺失、DXGI DLL 缺失、
编译器 DLL 缺失、`D3D12CreateDevice` 缺失、`CreateDXGIFactory2` 缺失。
测试在独立进程内拦截加载或符号解析，不更改系统 DLL。每个场景均验证隐藏
DX9 硬件设备、实际 CPU 骨骼变换、Present 和渲染目标像素回读。
需要回退的场景还验证自动模式及已保存失效 GPU 选择的主体、描边顶点与 CPU 一致。
系统正常和旧 DXGI 入口场景确认 DX12 硬件初始化仍可成功。

纯 DX9 构建编译通过、导入检查通过，隐藏启动、消息响应和正常关闭也通过。
启动检查使用同一份 Data 的独立副本，三次均无可见窗口和模态错误窗口，退出码为 0。
这证明本机实际发布 EXE 可以初始化主窗口；旧系统缺少 DX12 的行为由故障注入覆盖。

## 尚未验收的范围

未在用户截图中的旧系统实机上运行。没有进行完整模型、动作、物理、效果的前后
渲染性能对照，也未报告 FPS、GPU 时间或性能提升。当前结论限于可选图形依赖、
启动和 CPU 回退，不代表所有旧系统依赖均已验证。

实现说明与可重复执行的检查命令见 [DX12 可选运行时与 DX9 回退](DX12_OPTIONAL_COMPAT.md)。
