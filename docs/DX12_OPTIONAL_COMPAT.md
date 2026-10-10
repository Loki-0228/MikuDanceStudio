# DX12 可选运行时与 DX9 回退

MMS 的窗口、MME 效果和最终绘制仍运行在 DX9。DX12 只为可选的 GPU 蒙皮
及原生预览提供后端。默认构建按需加载系统目录中的 `d3d12.dll`、
`dxgi.dll` 和 `d3dcompiler_47.dll`，EXE 不再强制导入这三个 DLL。
缺少 DLL、缺少入口点或没有可用 DX12 设备时，程序继续使用 DX9 和 CPU 蒙皮。
支持 DX12 的设备仍可使用 GPU 蒙皮，无需另建发行分支。

原问题发生在进入 `WinMain` 之前：Windows 加载 EXE 的静态导入时找不到
`d3d12.dll`，现有 CPU 回退代码还没有执行机会。运行时加载通过失败的
`HRESULT` 进入原有回退流程。`CreateDXGIFactory2` 不存在时，设备枚举尝试
`CreateDXGIFactory1`；DXGI 完全不可用时，DX9 设备枚举仍可继续。
DLL 句柄保留至进程退出，避免静态 COM 对象析构时访问已卸载的代码。

此修复解决可选 DX12 依赖导致的启动失败，不代表已在所有旧版 Windows 上完成验收。
程序仍需要 DX9、D3DX9 和当前构建使用的 Visual C++ 运行库。

## 完全关闭 DX12 的构建

使用已有的 Conan 依赖，在宿主项目根目录配置单独的构建目录：

```bat
cmake -S . -B build/dx12-disabled -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=D:/MMD-Next/MMS/build/generators/conan_toolchain.cmake ^
  -DMIKUDANCESTUDIO_ENABLE_DX12=OFF
cmake --build build/dx12-disabled --config Release --target MikuDanceStudio
```

关闭时，CMake 不生成或链接 DX12 shader、DX12 skinning 和 DX12 原生预览实现；
运算器设置中不枚举 DX12 GPU，自动模式改走 CPU。传入 `--d3d12-preview`
也不会启用预览。DX9/MME 设备和 FX 行为保持原有路径。

默认构建已能处理 DX12 runtime 缺失。关闭选项用于明确不需要 DX12 功能的发行构建。

## 回归验证

`optional_graphics_imports` 检查正式 EXE 的普通导入表和延迟导入表，拒绝三个可选
DLL 的强制依赖。此检查不需要 GPU。

`graphics_runtime_tests` 需要本机具有 DX9 和 DX12 硬件支持。它通过进程内钩子
模拟缺少 DX12、DXGI、着色器编译器，以及缺少设备创建和新 DXGI 入口点。
每个场景单独启动进程，不修改系统 DLL。测试创建隐藏 DX9 窗口，比较自动模式、
指定失效 GPU 与 CPU 模式的主体和描边顶点，并验证 Present 与渲染目标回读。

验证命令：

```bat
cmake --build build --config Release --target MikuDanceStudio graphics_runtime_tests
ctest --test-dir build -C Release -R "^(graphics_runtime_|optional_graphics_imports)" --output-on-failure
```
