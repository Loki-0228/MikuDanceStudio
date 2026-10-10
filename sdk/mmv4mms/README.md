# MMV4MMS 接口快照

本目录保存 2026-10-08 从 `D:/MMD-Next/MMV4MMS/include/mmv/` 同步的接口。

- `mms.h` 定义 MMS 专用接口版本 1。
- `mmv.h` 定义场景、编辑器及运行时 ABI 3。
- `menu.h` 保留版本 1 状态接口，新增渲染禁用状态位。菜单内容由插件经通用 `MMSGetMenuHostApi` 注册，宿主不创建占位菜单。
- `mesh.hpp` 是宿主采集时使用的私有 C++ 数据辅助代码，不跨 DLL 边界传递。

MMS 独立编译，不读取相邻插件仓库、不链接插件导入库，也不打包插件。运行时从 `MikuMikuDance.exe` 同目录可选加载 `MMV4MMS.dll` 并验证 `MMV4MMSGetPluginApi`。

同步接口时必须同时核对结构大小、版本号和双方编译结果。

`mmv.h` 和 `mms.h` 于 2026-10-08 从 MMV4MMS 提交 `213921c` 同步。`MmvMmsFrame` 的可选尾字段传递附件面板原始值，避免旋转控制在矩阵分解时折回。`MmvObject` 和内嵌 `MmvFrame` 的尺寸不变。插件继续接受不含尾字段的旧调用。
