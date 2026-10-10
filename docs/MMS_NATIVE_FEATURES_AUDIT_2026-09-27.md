# MMS 原生增强审计（截至 80bc305）

本文是 [近期新增特性](ADDITIONAL_FEATURES.md) 的证据底稿。汇总文档面向使用者说明入口、默认值、保存方式和兼容性；本文记录每一条结论的 Git 提交短哈希、源码位置、判定状态和未核实项，供复核使用。

核对日期：2026-09-27。

补查至 `7f28800`：`80bc305..7f28800` 仅有 `375906f`（测试夹具效果范围往返）与 `7f28800`（MMV 分配说明）两个提交，`src`、`include` 没有新增差异。下文原生能力审计范围仍以 `80bc305` 标识；当前未提交的骨骼／表情编辑器和帧率修正不计入已交付清单。

## 审计范围与复核命令

审计的分支为本地 `main`，建库快照为 `ba6b269`，截止提交为 `80bc305`。

```powershell
git show -s --format="%H %ad %s" ba6b269
git show -s --format="%H %ad %s" 80bc305
git rev-list --count ba6b269..80bc305
git log --oneline --reverse ba6b269..80bc305
git diff --stat ba6b269 80bc305 -- src include res docs tests
```

已核对的数字：

| 项目 | 值 |
| --- | --- |
| 建库快照 | `ba6b269548323cf9cd8e837f91b3e420e56140cb`（2026-09-23 02:10:20 +0800） |
| 截止提交 | `80bc305a2f29dcd6dd9fa8c014cc0c0bfe4b4b98`（2026-09-27 12:33:20 +0800） |
| 区间提交数 | 254 |
| `src` 与 `include` 变更文件数 | 214 |
| `src` 与 `include` 净变更 | 50640 行新增、1652 行删除 |

判定正文说法时以当前源码（`80bc305`）为准，不以提交标题为准。凡提交标题所述内容在源码中找不到对应实现，本文在该条目的状态列写明。

## 判定口径

每条改动归入下面七类之一。同一个提交只按最终行为归类，不按提交前缀归类。

| 类别 | 含义 |
| --- | --- |
| 已交付增强 | 相对原版 MMD 9.32 新增，且已合入 `main` 的宿主可见能力。 |
| 建库快照已有 | 功能存在于 `ba6b269`，无法归属到本区间 254 个提交。原版核验证据不足，标为来源不明。 |
| 纯移植兼容修复 | 把移植实现改回原版 MMD 行为，或修复移植实现自身的缺陷。不计入增强。 |
| 未合入或已停用 | 实验分支、停用路径、未提交规划。不计为已交付。 |
| 插件自有能力 | 属于可选 MMV 插件自身，不计入 MMS 原生；宿主侧的加载与交接单独列出。 |
| 工作树待集成 | 存在于当前未提交工作树，尚未进入 `main`。不计为已交付。 |
| 未核实 | 缺少原版证据或缺少运行验收，无法给出最终判定。 |

## 一、已交付增强：平台、语言与界面

### 原生 Win64 构建

性质是平台移植属性。构建脚本把 `MikuMikuDanceE_v932x64` 记为 x64 参考程序（`CMakeLists.txt:7`）。寻址布局集中在 `include/mikudancestudio/layout_pins.hpp`。

| 项目 | 内容 |
| --- | --- |
| 入口 | 无用户入口。构建产物为 x64 程序。 |
| 默认行为 | 64 位指针宽度。 |
| 证据 | `CMakeLists.txt:42-44`（`CMAKE_SIZEOF_VOID_P EQUAL 8` 分支）、`include/mikudancestudio/layout_pins.hpp`、`CMakeLists.txt:334-342`（x64 导入库处理）。 |
| 引入提交 | 建库快照 `ba6b269` 已存在。 |
| 状态 | 建库快照已有。 |

### 简体中文界面（第三种界面语言）

原版 MMD 9.32 只有日文与英文两种界面语言。本程序在原有 `EnglishUI` 字节上增加第三个取值 `2` 表示简体中文。

| 项目 | 内容 |
| --- | --- |
| 入口 | 语言切换命令 260，循环日文 → 英文 → 中文 → 日文。 |
| 默认行为 | 无存档值时按 Windows 界面语言选择：主语言 `0x04` 为中文，`0x11` 为日文，其余为英文。 |
| 保存方式 | 界面语言字节随项目设置保存。 |
| 数据来源 | 固定词表见 `src/window/ui_translation.cpp` 与 `src/window/localize_ui.cpp`；模型名称按语言取原名或英文名。 |
| 证据 | `include/mikudancestudio/ui_language.hpp`（`kUiChinese = 2`、`ResolveStartupUiLanguage`、`UiUsesEnglishNames`）、`src/window/command_view_menu.cpp:1854-1862`（命令 260 的 `(cur + 1) % 3`）、`src/window/window_init.cpp:229,252`、`src/window/ui_panel_sweep.cpp:78-79`（中文用 Microsoft YaHei UI）。 |
| 引入提交 | 建库快照 `ba6b269` 已包含 `ui_language.hpp`、`localize_ui.cpp` 的中文列与命令 260 的 `% 3` 循环。 |
| 状态 | 建库快照已有。 |

本区间对中文界面的改动只有文案修正，不构成新增语言：`4ecc7ad`（蒙皮与省电菜单中文名）、`ce5dd99`（坐标标识字体与描边）。

### Unicode 文件名与路径

原版 MMD 仅支持 ANSI/CP932 路径。本程序使用 Unicode 窗口类和消息处理，PMM 路径字段在保留原布局的前提下按多代码页回退，无法用旧代码页表示时写入带 BOM 的 UTF-8 字段。

| 项目 | 内容 |
| --- | --- |
| 入口 | 无独立入口，作用于文件打开/保存、配件名称与 PMX 名称显示。 |
| 默认行为 | 保留原字段布局；旧工程按 CP932、系统代码页、CP936、CP950、CP949 及 UTF-8 依次尝试。 |
| 兼容性 | 带 BOM 的 UTF-8 路径字段是本程序扩展，旧版 MMD 与第三方插件不保证读取。 |
| 证据 | `src/window/unicode_window.cpp`、`src/main/text_encoding.cpp`、`docs/FILENAME_ENCODING.md`。 |
| 引入提交 | 建库快照已有；本区间仅 `342bffe` 触及 `src/main/text_encoding.cpp`。 |
| 状态 | 建库快照已有。原版核验证据未在库内。 |

### 视口操作按钮配色与配件 `.x` 角标

| 项目 | 内容 |
| --- | --- |
| 入口 | 视口旋转、移动按钮。 |
| 默认行为 | 配件坐标状态下六个按钮显示完整 `.x`，周围透明；相机状态仍为相机标识。 |
| 证据 | `src/render/sprite_overlay.cpp`、`res/assets/hud_sprites.png`、[配件角标记录](ACCESSORY_HUD_2026-09-26.md)。 |
| 引入提交 | `b48ade1`、`3da02e4`、`65eaf27`、`33ec81b`、`83d833e`、`dbe5c6b`。 |
| 验证范围 | 六个角标各 37 个笔画像素、缺失 0；渲染目标回读，三种缩放、两种背景。未重做整场景性能验收。 |
| 状态 | 已交付增强。 |

### 视口光标图标统一

| 项目 | 内容 |
| --- | --- |
| 入口 | 视口旋转环与移动箭头。 |
| 默认行为 | 旋转环 70% 不透明度；移动光标原点改为方块；三角箭头内部为单一纯色。 |
| 证据 | `src/render/viewport_cursor_style.cpp`、`src/window/ui_init.cpp`、`src/render/frame_scene.cpp`、[图标记录](VIEWPORT_CURSOR_STYLE_2026-09-27.md)。 |
| 引入提交 | `e27972b`。 |
| 验证范围 | 模块级验证：图集左半逐像素差异为 0、圆环 alpha 179、四个移动状态无旧手形残留。 |
| 未核实 | 该功能的记录写明整包集成与真实 PMM/MME 场景验收当时未完成，只有模块级验证。见本文第九节。 |
| 状态 | 已交付增强（验收范围不完整）。 |

## 二、已交付增强：模型格式、骨骼缩放与物理

### 原生骨骼缩放

原版 MMD 9.32 没有骨骼缩放。PMD 与 PMX 的全部骨骼可等比或分轴缩放，包含末端、IK 与物理骨骼。运行时与存储都保留完整 3×3 变换，不压成三个独立数值。

| 项目 | 内容 |
| --- | --- |
| 入口 | 视口移动按钮下方新增的整体、X、Y、Z 四个按钮；局部/全局/视图选择器决定分轴方向。 |
| 默认行为 | 每向上拖动 100 像素约为 `e` 倍；Ctrl 精调，Shift 加速；配件、相机、灯光与无骨骼选择时禁用；播放期间不能编辑。 |
| 保存方式 | 当前姿态与缩放关键帧写入 PMM v2 扩展记录 `org.mikudancestudio.model.<slot>.boneScale.v1`；VMD 标准段不变，缩放写在同文件尾部的 `MDSXMDSX`/`MDSXVMDX` 扩展中。PMM 全部扩展上限 16 MiB，VMD 扩展上限 8 MiB。 |
| 兼容性 | 原版 MMD 读取标准段并忽略尾部扩展；通过原版重新保存会丢弃扩展。 |
| 证据 | `ad032d9` 为功能提交；`src/model/bone_scale.cpp`（`Enabled` 第 385 行、`Drag` 第 393 行、`Interpolate` 第 146 行）、`include/mikudancestudio/bone_scale.hpp:71-77`、`src/io/bone_scale_io.cpp`、`include/mikudancestudio/bone_scale_io.hpp:40-48`、[骨骼缩放说明](BONE_SCALING.md)。 |
| 验证范围 | 14 组通过：缩放求值、关键帧登记与编辑、撤销重做、真实 VMD 往返、真实 PMM 保存重开、损坏扩展拒绝、HUD 三种 DPI 两组背景共 18 组回读。PMX 最大绝对差 `1.91e-6`，PMD `1.79e-7`。 |
| 状态 | 已交付增强。 |

### 刚体与 joint 的缩放继承

| 项目 | 内容 |
| --- | --- |
| 入口 | 无需额外入口，绑定骨骼的刚体自动继承骨骼缩放。 |
| 默认行为 | 球、盒、胶囊保留完整仿射碰撞边界；joint 两端连接点与平移限位随之更新；角度限位、弹簧刚度、质量、阻尼保持原值。 |
| 证据 | `d214cd4`；`src/physics/physics_scale.cpp`、`src/physics/scene_gizmo_data.inc`。 |
| 验证范围 | 十组后台回归；真实 PMM 为「测试刚体.pmm」，含 140 根骨骼、45 个刚体、27 个关节。 |
| 状态 | 已交付增强。 |

### 物理与关键帧烧录

原版 MMD 之外的新增烘焙功能。入口为 **烘焙 → 烧录关键帧…**，在独立 Bullet 世界中从第 0 帧顺序计算。

| 项目 | 内容 |
| --- | --- |
| 入口 | 菜单「烘焙 → 烧录关键帧…」，命令 ID `32800`（`include/mikudancestudio/physics_bake.hpp:72`）；弹出菜单 ID `32802`（同文件第 77 行）。 |
| 默认行为 | ERP `0.2`、CFM `0`、子步数 `1`、内部倍率 `1`、基准频率 `60 Hz`；参数只用于本次烧录。范围含两端，帧号 0 至 100000。 |
| 保存方式 | 结果为原生骨骼关键帧，随 PMM/VMD 常规流程保存；物理键带原生「禁用物理」标记。高级参数可另存为不加密 UTF-8 JSON 预设，预设不写入 PMM 扩展。 |
| 证据 | `51e2f64`；`src/physics/physics_bake.cpp`、`src/physics/physics_bake_settings.cpp`、`src/window/physics_bake_dialog.cpp`、[物理烧录说明](PHYSICS_BAKING_REQUIREMENTS.md)。 |
| 验证范围 | 三语隐藏对话框、真实 PMM 烧录、事务与撤销、VMD 往返；记录见 [物理烧录验收](PHYSICS_BAKING_VALIDATION_2026-09-26.md)。 |
| 状态 | 已交付增强。 |

### 通道选择、稀疏采样与动态骨骼键池

| 项目 | 内容 |
| --- | --- |
| 入口 | 烧录窗口的「通道…」；相机/灯光/附件主页可选择相机、灯光、自阴影、重力与每个 `.x` 的子参数，模型页可选择骨骼、表情与显示/IK 轨道。 |
| 默认行为 | 起止帧与步长决定采样，总包含结束帧；骨骼稀疏烘焙替换区间内旧键；边界恢复原状态。 |
| 证据 | `80bc305`、`df9f00a`；`src/animation/channel_bake.cpp`、`include/mikudancestudio/channel_bake.hpp`、`src/model/bone_key_pool.cpp`、[通道烘焙验收](BAKING_CHANNELS_VALIDATION_2026-09-27.md)。 |
| 验证范围 | `channel_bake_tests` 16 阶段通过；真实 PMM 键池增长至 983,040 槽、实际写入槽 600,000。该记录明确这不是生成 983,040 个有效键的压力测试。 |
| 已知限制 | 表情、显示及部分场景轨道仍有既有容量，超限拒绝提交；新通道尚未做成统一长期撤销历史。 |
| 状态 | 已交付增强。 |

### 宿主侧可选 Bullet3 物理接入

计算 DLL 与适配器属于独立仓库 `D:/MMD-Next/MMV-Bullet3`，不在本仓库内。本仓库交付的是宿主侧的可选服务与菜单配合。

| 项目 | 内容 |
| --- | --- |
| 入口 | 可选 MMV 效果以 `MMVPhysics="Bullet3"` 标注；引擎选择作用于整个场景。宿主侧通过 `MmvHostServices` 的可选尾部提供 `physicsInput` 与 `selectBullet3`。 |
| 默认行为 | `selectBullet3` 关闭原生物理并保存此前模式，释放时恢复；原先已关闭则仍恢复为关闭。宿主继续拥有原生物理世界。 |
| 证据 | `8506356`、`b2b5e79`；`include/mmv/mmv.h`（ABI 版本 1）、`src/mmv_host/host.cpp`、[Bullet3 接入](MMV_BULLET3.md)、[集成验收](MMV_BULLET3_MAIN_INTEGRATION.md)。 |
| 验证范围 | Rou 模型 96 个刚体、139 个关节；慢帧播放与固定帧编辑；ABC 与网格缓存集成。 |
| 未核实 | Bullet3 视频导出、原生 VMD 物理烧录、所有第三方 MME 效果组合未验证。 |
| 状态 | 已交付增强（宿主侧交接部分）。 |

## 三、已交付增强：附件

### 附件曲线编辑入口与附件动作轨道

| 项目 | 内容 |
| --- | --- |
| 入口 | 附件操作区的曲线按钮（命令 `32900`）；附件区的「读取」按钮（命令 `32901`）；文件菜单的「读取当前附件动作（VMD）」（命令 `32903`）。 |
| 默认行为 | 附件变换导出为 `Accessory` 骨骼轨道，其他参数写为表情代理；完整名称与曲线保存在 VMD 尾部 `MMVCURV1` 扩展中。 |
| 保存方式 | 宿主主页与附件曲线写入 PMM 扩展；VMD 先写标准骨骼、表情、相机、灯光、自阴影及显示/IK 段，再追加扩展。 |
| 证据 | `a017903`、`f576e96`；`include/mikudancestudio/animation_curve_editor.hpp:52-55`、`src/window/animation_curve_host_ui.cpp:64-65,114-152`、`src/animation/animation_curve_motion.cpp:17-18,477-485`、`src/io/vmd_save.cpp`、[曲线编辑器](ANIMATION_CURVE_EDITOR.md)。 |
| 验证范围 | 公共曲线、宿主轨道、曲线窗口、VMD、MMV 参数、运行时、分配窗口共 7/7 CTest 通过；真实 MMS + MMV 窗口 0 失败。 |
| 已知限制 | 没有扩展的多骨骼 VMD 不自动猜测附件目标，会报告无法选择唯一轨道。 |
| 状态 | 已交付增强。 |

## 四、已交付增强：曲线编辑与 VMD/PMM 扩展

### 原生浮窗曲线编辑器

宿主原生非模态浮窗，来源包括当前附件、摄像机、灯光、自阴影与重力。

| 项目 | 内容 |
| --- | --- |
| 入口 | 附件操作区曲线按钮（`32900`）；MMV 的材质分配与骨骼分配页在参数区提供同一入口。 |
| 默认行为 | 数值/速度两种视图；支持阶梯、线性、贝塞尔以及独立、对齐、水平、线性、自动切线；布尔与枚举通道保持阶梯。 |
| 保存方式 | 宿主主页与附件曲线写入 PMM 扩展；MMV 参数关键帧与曲线写入 `.vmm`。 |
| 证据 | `f576e96`（公共曲线核心）、`a017903`（宿主窗口集成）、`7d783ca`（与 Bullet3 后端合并）；`src/window/animation_curve_editor.cpp`、`src/animation/animation_curve.cpp`、`src/animation/animation_curve_host.cpp`、[曲线编辑器](ANIMATION_CURVE_EDITOR.md)、[曲线与 MMV 窗口验收](MMV_CURVE_UI_VALIDATION_2026-09-27.md)。 |
| 验证范围 | 参数浮窗专项 9,200 项检查 0 失败；真实 PMM 烘焙、菜单、VMD 往返与界面锁 0 失败。 |
| 已知限制 | 当前入口不包含模型表情曲线。`kMorphCurve = 32902` 在 `80bc305` 的 `include/mikudancestudio/animation_curve_editor.hpp:54` 已声明，但在该提交的 `src`、`include`、`tests` 中没有任何使用点。工作树正在接通表情曲线对比显示，尚未合入，见第十一节。 |
| 状态 | 已交付增强。 |

### PMM v2 扩展容器

| 项目 | 内容 |
| --- | --- |
| 入口 | 无独立入口。正常保存、另存和打开 PMM 时自动读写。 |
| 默认行为 | 文件头仍为 `Polygon Movie maker 0002`；标准数据之后是扩展记录串与 32 字节尾标 `MDSXPMM2`，含标准长度、扩展长度、CRC32 与保留字段。 |
| 保存方式 | 已定义设置：`org.mikudancestudio.skinning.processor`（运算器类型意图）、`org.mikudancestudio.view.transparent_sort_precision`（透明排序档位）、`org.mikudancestudio.model.<slot>.boneScale.v1`（骨骼缩放）。未知且有效的记录原样写回。 |
| 兼容性 | 原版 MMD 9.32 x64 读完标准数据后关闭文件，忽略附加数据的结论来自库内静态反汇编核对，未启动原版界面验证。原版重新保存会丢失扩展。 |
| 证据 | `58f4ea1`；`src/io/pmm_extension.cpp:1`、`src/io/pmm_project_settings.cpp`、[PMM v2 扩展格式](PMM_V2_EXTENSIONS.md)。 |
| 验证范围 | `pmm_extension_tests` 与 `pmm_project_settings_tests` 覆盖编解码、边界、CRC、未知记录保留与菜单状态。 |
| 状态 | 已交付增强。 |

### VMD 尾部扩展

| 项目 | 内容 |
| --- | --- |
| 入口 | 骨骼缩放导出与曲线导出共用同一条尾部扩展通道。 |
| 默认行为 | 标准 VMD 段不变；没有尾部扩展的 VMD 导入时骨骼缩放为单位矩阵，不会报错。 |
| 证据 | `src/io/bone_scale_io.cpp:3`、`include/mikudancestudio/bone_scale_io.hpp:40-48`（`MDSXMDSX`/`MDSXVMDX`）、`src/animation/animation_curve_motion.cpp:12`（`MMVCURV1` 尾部 40 字节页脚）。 |
| 状态 | 已交付增强。 |

## 五、已交付增强：烘焙、网格缓存与输出

菜单「烘焙」由宿主追加，位置在「帮助」左侧，命令 ID 为 `32800`（烧录关键帧）、`32801`（导出 ABC）、`32803`（清除网格烧录）、`32804`（烘焙网格），弹出菜单 ID 为 `32802`，定义在 `include/mikudancestudio/physics_bake.hpp:68-86`，菜单构建在 `src/window/physics_bake_dialog.cpp:788-838`。

### 网格烘焙与 Alembic 导出

| 项目 | 内容 |
| --- | --- |
| 入口 | 「烘焙 → 烘焙网格…」（默认只写工程缓存）与「烘焙 → 导出 ABC…」（默认只导出文件）；两项可同时勾选。清除用「烘焙 → 清除网格烧录…」。 |
| 默认行为 | 采样 **MMS/MMV 交接处、MME 着色之前**的几何；帧范围 0–100000 按 30 fps 时间轴解释；采样率 1–120，默认 30。 |
| 保存方式 | PMM 记录相对路径映射，几何另存为旁边的 `<工程名>.<内容标识>.mdsmesh.abc`；VMM 保存 FX 开关键帧。 |
| 兼容性 | 原版 MMD 忽略本工程的缓存扩展。旧版没有区间标记的缓存仍按旧规则读取，新规则不追溯改写旧存档。 |
| 证据 | `54bfe14`；`src/io/mesh_bake_cache.cpp`、`src/io/alembic_export.cpp`、`src/window/alembic_export_dialog.cpp`、`src/render/vertex_capture.cpp`、`src/render/deformed_mesh_capture.cpp`、[网格导出与烘焙](ALEMBIC_EXPORT.md)。 |
| 验证范围 | `vertex_capture_tests` 95 项、`mesh_bake_cache_tests` 真实 Ogawa 往返、`mmv_runtime_tests` 208 项；Unicode 文件名、30/60 fps 采样、取消保留旧文件、ABC 原生路径与 MME 顶点变形均通过。 |
| 已知限制 | 屏幕后期、自定义投影、依赖历史渲染目标的效果与非标准绘制有明确限制；不支持局部顶点过滤的纯 ABC 输出会被界面拒绝。 |
| 状态 | 已交付增强。 |

## 六、已交付增强：渲染、透明排序与蒙皮

### 五档透明排序上限

原版 MMD 没有该选项。菜单为「显示 → 透明排序上限」，命令 ID `304` 至 `308`，定义在 `include/mikudancestudio/view_options.hpp:10-12`。

| 档位 | 命令 ID | 行为 |
| --- | ---: | --- |
| 低 | 304 | 16 桶计数排序，远到近，同桶保持输入顺序；不分层、不分割。 |
| 标准（默认） | 305 | 64 桶计数排序，远到近，同桶保持输入顺序。 |
| 高 | 306 | 按三角形中心深度稳定排序，同深度按原始三角形序号。 |
| 最高 | 307 | 分层上限为裁剪后参与排序的三角形数量加一。 |
| 关闭 | 308 | 不执行自动透明排序。 |

| 项目 | 内容 |
| --- | --- |
| 默认行为 | 新建工程与无字段工程使用标准。关闭只把唯一字节写成 `4`，不记忆先前档位。 |
| 保存方式 | PMM v2 扩展记录 `org.mikudancestudio.view.transparent_sort_precision` 的单字节。 |
| 回退预算 | 仅最高档使用 BSP，额外 4,000,000 个三角形、深度 512、epsilon `2e-13`。三档下调与最新验证见[强度调整记录](TRANSPARENT_SORT_STRENGTHS_20260927.md)。 |
| 证据 | `ab7c2f0`、`211d740`、`904012c`、`c86bf18`、`1e54b05`；`src/render/transparent_geometry.cpp:80-206`、`src/render/model_renderers.cpp`、`src/window/command_view_menu.cpp:2896-2920`、[五档上限](TRANSPARENT_SORT_FIVE_TIERS_2026-09-27.md)。 |
| 历史验证范围（下调前） | 15 次独立运行通过；真实 ToonCloud PMM 保存/重开/另存；低、标准、高、最高各带 4x MSAA 的像素驱动；最高档实际完成 1152 与 4608 层。 |
| 状态 | 已交付增强。 |

旧入口说明：命令 `303` 是历史开关的兼容入口，已没有对应菜单项，只在「关闭」与「标准」之间切换，处理代码见 `src/window/command_view_menu.cpp:2879-2894`。早期六档测试格式不做字节换算，读取规则见 [PMM v2 扩展格式](PMM_V2_EXTENSIONS.md)。

### 正式 D3D9/MME 蒙皮运算器

| 项目 | 内容 |
| --- | --- |
| 入口 | 「显示 → 蒙皮运算器」，菜单 ID `32000`、自动项 `32001`、CPU 项 `32002`、适配器项从 `32100` 起（`include/mikudancestudio/skinning_menu.hpp:6-9`），只读状态项 `32003`（`src/window/skinning_menu.cpp:85`）。 |
| 默认行为 | 「自动（优先高性能）」，逐模型实测 CPU 与全部可用 GPU 的完整蒙皮耗时，选择结果正确且中位数最低者。CPU 胜出属于正常结果。 |
| 保存方式 | 本机偏好写 EXE 同级的 `Data/skinning.ini`；工程另存一份设备类型意图（自动、CPU、集成显卡、独立显卡），不保存品牌、型号或测速结果。 |
| 兼容性 | QDEF、未知权重类型及不满足 GPU 输入条件的模型走 CPU 分支；手动显卡失败后回退 CPU；PMM 指定类型缺失时运行时回到自动，但再次保存仍保留原类型意图。 |
| 证据 | `7523928`、`d3f2e59`、`cf09d31`；`src/model/skinning_backend.cpp`（`AutomaticEntry` 第 44 行、`Median` 第 255 行、`Calibrate` 第 268 行、`ForgetModel` 第 396 行）、`src/window/skinning_menu.cpp`、[GPU 蒙皮](GPU_SKINNING_2026-09-25.md)、[自动选择](SKINNING_AUTOSELECT_2026-09-26.md)。 |
| 验证范围 | `skinning_backend_tests` 60 组 CPU/GPU 比较；真实角色 183698 顶点、969 骨骼三组姿态误差 0；MME CPU/GPU 完整渲染通过。 |
| 性能口径 | 该方案包含同步回读与重新上传，在已测样本上慢于 CPU，性能收益未通过验收。交付的是可选择的 GPU 蒙皮能力，不是已完成的蒙皮加速。 |
| 状态 | 已交付增强。 |

### 不透明像素预通道与浮点合成

| 项目 | 内容 |
| --- | --- |
| 入口 | 无独立入口，属于透明渲染路径的一部分。 |
| 证据 | `e73eaaf`（私有浮点目标累积后再提交）、`6f01562`（MSAA 下保留不透明遮挡）、`305f4f9`、`3109a20`。 |
| 状态 | 已交付增强。原版 MMD 没有逐像素透明分层，此路径服务于上面的高档与最高档。 |

## 七、已交付增强：MMV 插件加载与交接（宿主侧）

MMV 插件自身的界面、参数面板、透视窗口、FX 编译与 `.uifx`/`.vmm`/`.vem` 存档属于插件，不计入 MMS 原生。宿主侧提供下面这些部分，属于宿主扩展。

| 项目 | 内容 |
| --- | --- |
| 插件契约 | `include/mmv/mmv.h` 定义 `MMV_ABI_VERSION 1` 的纯 C ABI，不含 DirectX、Bullet 与 STL 类型。 |
| 宿主交接 | `src/mmv_host/host.cpp:1` 以 `MMV_HOST_MME_BRIDGE` 标识的代码在蒙皮之后、主设备 `BeginScene` 之前获取快照，把插件结果复制到独立渲染记录（同文件第 258、323 行）。 |
| 菜单接入 | `src/exports/effect_api.cpp:452` 与 `src/window/enhance_model_io.cpp:1109` 调用 `mmv_host::RefreshMenu`；命令路由在 `src/window/command_dispatch.cpp:393`。菜单未安装插件时不显示。 |
| 网格烘焙交接 | `src/window/alembic_export_dialog.cpp:170` 调用 `mmv_host::ClearMeshBakeObjects`；烘焙采样 MMS/MMV 交接处几何。 |
| 引入提交 | `b1de95c`、`8506356`。 |
| 宿主导出 | 宿主保持 37 个导出名称与序号不变。 |
| 状态 | 已交付增强（宿主侧）。插件自有能力见 [MMV 实现记录](MMV_IMPLEMENTATION.md)。 |

## 八、建库快照已有的自定义功能（来源不明，待原版核验）

以下能力存在于 `ba6b269`，无法归属到本区间 254 个提交。它们相对原版 MMD 9.32 确实属于扩展，但引入时间与原始出处没有记录，原版核验证据也不在库内。

| 能力 | 位置 | 说明 |
| --- | --- | --- |
| 简体中文界面（第三语言） | `include/mikudancestudio/ui_language.hpp`、`src/window/command_view_menu.cpp:1854-1862`、`src/window/ui_translation.cpp`、`src/window/localize_ui.cpp` | 命令 260 的三态循环与中文词表在快照中已存在。 |
| Unicode 文件名与路径 | `src/window/unicode_window.cpp`、`src/main/text_encoding.cpp`、`docs/FILENAME_ENCODING.md` | 快照已存在，本区间只由 `342bffe` 触及 `text_encoding.cpp`。 |
| 实验性 D3D12 预览渲染器 | `src/render/d3d12_native.cpp`、`src/render/d3d12_skinning.cpp`、`include/mikudancestudio/d3d12_native.hpp`、`docs/D3D12_MIGRATION_2026-09-20.md` | 建库快照已包含全部 D3D12 源文件。本区间只有 `7523928` 把计算结果接进正式 D3D9 蒙皮。D3D12 预览本身仍是实验路径，启动方式与缺失功能见 [迁移记录](D3D12_MIGRATION_2026-09-20.md)。 |
| FPS 覆盖显示 | `src/app/app_defaults.cpp:57`（`FpsOverlayEnabled() = 0`）、`src/app/frame_driver.cpp:525` | 默认关闭。快照已存在，原版是否有对应显示未核。 |
| 视口操作按钮的既有坐标状态与命中区域 | `src/render/sprite_overlay.cpp`、`src/window/ui_init.cpp` | 本区间只改配色与角标，不增加新的坐标模式。 |
| NVIDIA 3D Vision 立体桥 | `src/render/stereo_nvapi.cpp` | 建库快照已有，本区间无提交触及，未做运行验收。 |
| AVI/DirectShow 录制与 MMdxShow | `src/app/avi_record_start.cpp`、`src/app/dshow_record_graph.cpp`、`src/app/record_readback.cpp`、`src/mmdxshow/` | 建库快照已有；录制与输出在本区间没有新增宿主可见能力。 |
| Kinect 运动捕捉 | `src/app/oni_kinect.cpp`、`src/app/oni_skeleton_pump.cpp` | 建库快照已有。 |

## 九、纯移植兼容修复（不计入增强）

| 改动 | 提交 | 判定 |
| --- | --- | --- |
| 鼠标越过视口边缘时重新居中并继续拖动 | `70a5c15`、`3033b6c` | 恢复原版操作按钮已有行为。前缀为 `feat` 不改变结论。 |
| 保存 PMM 时通知 MME 自动保存同名 EMM | `ad7058e` | 修复原有保存通知链路。见 [EMM 自动保存](EMM_AUTOSAVE_2026-09-25.md)。 |
| 录制回读的水平采样相位与非法尺寸 | `8b6b0aa`、`6a1843f` | 修复移植实现自身的缺陷：基线第二行即失败，修复后通过。见 [代码审查记录](CODE_REVIEW_2026-09-24.md)。 |
| MME toon 纹理经宿主导入槽加载 | `9facf26` | 让 MME 能注册 toon 纹理，修复移植侧 `LoadLibraryA`/`GetProcAddress` 绕过导入地址表的写法。见 [黑草问题](MME_TOON_BLACK_2026-09-23.md)。 |
| 视点保持（跟随开关、相机关键帧、面板切换、启动） | `116e3aa`、`1c142c6`、`bb6a9b3`、`96ce788`、`eb1ae97` | 改回原版行为。 |
| 坐标选择器双次推进 | `c485cfb`、`bede0b8`、`4c11fa5`、`d03c23b` | 修复移植实现的事件处理缺陷。 |
| 配件面板相机参考模式补偿符号与父级相机 | `45ae9b2`、`57f68ff`、`2284dc5` | 改回原版行为。 |
| 场景灯光编辑框提交 | `e3f291e` | 改回原版行为。 |
| PMX 顶点布局与表情帧填入 | `90f6b67` | 改回原版行为。 |
| 文件加载器越界与畸形输入加固 | `b0645b8`、`2986d17`、`69d4301`、`6b1dd24`、`2283f0f`、`0a14f5e`、`f6c16b2` | 安全性修复。按分类规则需逐条确认缺陷来自原版还是移植实现；本区间没有为每条补原版证据，维持原分类并标为待核。 |
| 非法 PMX 关节与非有限骨骼姿态 | `c57af81` | 稳定性修复。原版证据未补，标为待核。见 [物理安全修复](PHYSICS_JOINT_POSE_SAFETY_2026-09-25.md)。 |
| 视口坐标标识字体与描边 | `ce5dd99`、`8dd274f` | 移植实现的绘制一致性修复。 |

## 十、未合入或已停用的实验

| 项目 | 状态 | 依据 |
| --- | --- | --- |
| 全量透明排序（global sort） | 停用 | 保留为实验与性能验收记录。见 [全量实验状态](TRANSPARENCY_GLOBAL_SORT_2026-09-25.md)。 |
| 六档排序上限与双字段迁移 | 被五档取代，只作为历史记录 | 见 [六档上限记录](TRANSPARENT_SORT_LIMIT_PMM_2026-09-27.md)。本程序不做字节换算。 |
| BSP 云排序实验 | 失败保留，只作为回退路径预算的来源 | `4173b97`、`dcea555`。 |
| `3026`/实验分支上的其它排序试验 | 分支保留，未合入 `main` | `baseline/*`、`codex/alpha-*` 等分支。 |
| 视口缩放模式与手柄、骨骼三态选择、表情曲线对比 | 未提交 | 见第十一节。 |

## 十一、工作树待集成（未交付）

截至 2026-09-27 12:50，`main` 为 `80bc305`。工作树里同时有其他会话正在写入的未提交改动，文件数量在审计期间仍在变化，因此本节只列能力与证据，不按当时的文件计数下结论。下面这些能力只在工作树内，尚未进入 `main`，不计为已交付。

| 待集成能力 | 工作树证据 | 状态 |
| --- | --- | --- |
| 骨骼三态选择按钮 | `include/mikudancestudio/panel_controls.hpp`（`kBoneModeGrid` 新增）、`src/window/localize_ui.cpp`、`src/window/command_control_450.cpp` | 源码与测试已写，未构建、未运行。 |
| 视口缩放手柄（`拡大縮小` 模式） | `include/mikudancestudio/bone_scale_gizmo.hpp`（未跟踪新文件）、`src/model/bone_scale.cpp`（`DragViewPlane`）、`src/render/debug_geometry.cpp` | 同上。 |
| 骨骼缩放插值/视图平面拖动路径 | `include/mikudancestudio/bone_scale.hpp`（新增 `DragViewPlane` 声明）、`src/model/bone_scale.cpp` 第 449、474、490 行为工作树行号 | 同上。 |
| 表情曲线对比显示 | `include/mikudancestudio/animation_curve_comparison.hpp`（未跟踪新文件）、`src/window/animation_curve_editor.cpp`、`include/mikudancestudio/animation_curve_editor.hpp`（新增 `kCompareOff`／`kCompareSameGroup`／`kCompareAllGroups`／`kCompareFree` 与 `Channel::group`） | 源码已写，未构建、未运行。 |

工作树对应的文档记录为 `docs/BONE_SCALING.md` 中「缩放模式与视口手柄的回归（2026-09-27，已写未构建）」一节与 `tests/bone_scale_gizmo_tests.cpp`。该文件写明本轮只写了源码、测试与文档，没有构建，也没有运行任何测试、渲染回读或 GPU 验收。

## 十二、未核实项与证据缺口

| 未核实项 | 原因 |
| --- | --- |
| 简体中文界面的引入时间与原始出处 | 功能已在建库快照内，本区间无对应提交；缺少原版对照。 |
| Unicode 路径支持的引入时间 | 同上。 |
| D3D12 预览是否应当对外发布 | 源码在快照内，但没有本区间的整体验收记录。 |
| FPS 覆盖显示是否相对原版为新增 | 快照已有默认关闭的实现，未做原版对照。 |
| NVIDIA 3D Vision 立体桥 | 快照已有，本区间无提交，也没有运行验收记录。 |
| Kinect 运动捕捉 | 快照已有，本区间无提交，也没有运行验收记录。 |
| 原版 MMD 隐藏表情双击崩溃的根因 | 用户报告未在原版复现，原版版本号未记录。见 [隐藏表情安全处理](HIDDEN_MORPH_PANEL_SAFETY.md)。 |
| 文件加载器加固与非法关节/姿态修复的来源 | 未逐条补原版证据，维持待核。 |
| 视口光标图标统一的整包集成与真实场景验收 | 该功能自己的记录写明模块级验证完成、整包集成未完成。提交 `e27972b` 之后是否有补偿性整包验证，库内没有对应清单。 |
| GPU 蒙皮与 MMV/Bullet3 的第三方效果覆盖 | 各记录均声明未穷举所有第三方 MME 效果与设备故障场景。 |
| ABC 捕获对屏幕后期、自定义投影与依赖历史渲染目标的效果 | 记录声明有明确限制，未逐项验证。 |
| 有编码标记但内容损坏的旧 PMM 路径字段 | 记录声明无法凭内容判断原始代码页，需从未损坏工程重新载入。 |

## 十三、本次审计发现的既有文档差异

| 位置 | 差异 |
| --- | --- |
| 原 `docs/ADDITIONAL_FEATURES.md` 的 Git 范围 | 写为「`ba6b269` 至 `dbe5c6b`」，未覆盖 `dbe5c6b` 之后的提交。本次更新为 `ba6b269` 至 `80bc305`。 |
| 原 `docs/ADDITIONAL_FEATURES.md` 的原生骨骼缩放入口 | 只写视口四个按钮。工作树新增的面板「拡大縮小」模式按钮与视口手柄尚未进入 `main`，本次改为标注待集成。 |
| 原 `docs/ADDITIONAL_FEATURES.md` 的烘焙入口 | 只写「编辑 → 关键帧烧录…」。当前实现是宿主追加的顶层「烘焙」菜单，含四个命令和一个弹出菜单 ID，入口名称为「烘焙 → 烧录关键帧…」。 |
| 原 `docs/ADDITIONAL_FEATURES.md` 未覆盖的能力 | 曲线编辑器、VMD 尾部扩展、PMM v2 容器（只提了项目设置字段）、网格烘焙与 ABC 导出、通道选择、MMV 宿主菜单与插件交接。 |
| `docs/AUTO_TRANSPARENT_SORTING_2026-09-25.md` 描述的排序开关 | 描述的是命令 `303` 的开关形态；当前形态是五档上限，命令 `303` 只作兼容入口。该文档保留为历史记录，不作为当前行为。 |
| `tests/README.md` 的核对日期 | 标注为 2026-09-20，未随本区间新增测试更新。 |
