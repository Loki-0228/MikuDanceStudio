# MikuDanceStudio

[English](#english) | [中文](#中文) | [日本語](#日本語)

[v2026.10.13.1 release notes / 发布日志 / リリースノート](docs/RELEASE_NOTES_20261013.md)

## Downloads and package differences

[GitHub Releases](https://github.com/Loki-0228/MikuDanceStudio/releases/latest) provides Windows x64 packages with `full` or `minimal` in the filename. Both builds come from the same source branch and receive shared bug fixes. The feature lists later in this README describe the full build. The minimal build uses the `mmd932` build profile:

| Area | Full `full` | Minimal `minimal` |
| --- | --- | --- |
| PMD and PMX loading, original animation tools, rigid-body physics, language fixes, MME interface | Included | Included |
| New transparency sorting | Optional, disabled by default | Omitted; original single-pass material order |
| Shadow reconstruction, dynamic precision, filtering | No-clipping coverage; optional adaptive spline filter, disabled by default | Original projection and SM2/SM3 shaders; no enhanced-filter control |
| Bone scaling, new curve editor, morph curves | Included | Omitted; original Bezier interpolation retained |
| DX12, MMV host, mesh capture, Alembic, additional baking | Included | Omitted |
| Media | Modern imports and MP4/MOV export through Windows media components | WAV audio, AVI backgrounds and AVI export |
| PMM, VMD, VPD saving | Can include MMS extensions | Standard data only; imported MMS extensions are ignored |

Both ZIPs contain only `MikuMikuDance.exe` and the license files in `licenses/`. Neither includes models, user settings, MME, or `MMV4MMS.dll`. MMV remains under separate testing. In `v2026.10.13.1`, the full executable is 4,968,960 bytes and the minimal executable is 2,192,384 bytes, about 56% smaller. This is not a playback speed comparison.

The minimal build checks CP932 encoding and field lengths for names and references stored in files, and rejects values it cannot represent. Model editing saves PMD only and rejects lossy PMX-to-PMD conversion. Save extended full-build projects as separate copies when using the minimal build, because scale, curve, and mesh-cache extensions are discarded. Round-trip loading and saving with the actual MMD 9.32 executable has not yet been validated. See [minimal build limits](docs/ORIGINAL_PROFILE.md).

Extract the two builds into separate directories and run the executable in each directory. Keep your existing installation if it contains settings or plugins.

## 下载与版本区别

[GitHub Releases](https://github.com/Loki-0228/MikuDanceStudio/releases/latest) 同时提供 Windows x64 完整包和最小包。需要 MMS 新增功能时选择文件名含 `full` 的 ZIP；主要使用 MMD 原有功能时选择含 `minimal` 的 ZIP。两档来自同一份源码，均保留 `MikuMikuDance.exe` 文件名。下文新增功能列表描述完整包，最小包以此表为准。

| 项目 | 完整包 `full` | 最小包 `minimal`（构建档 `mmd932`） |
| --- | --- | --- |
| PMD、PMX 载入，原有骨骼、表情、相机、灯光和刚体物理 | 保留 | 保留 |
| 原生功能、输入、编码及 MME 兼容修复 | 保留 | 保留 |
| 新透明排序 | 可选低、标准、高、最高；默认关闭 | 不编入排序与剥层实现，使用原始单遍材质顺序 |
| 阴影重构、动态精度和滤波 | 保留防裁切与动态精度；可选「滤波加强」，默认关闭 | 使用原版投影及 SM2/SM3 着色器，无「滤波加强」 |
| 骨骼缩放、新曲线编辑器和表情曲线 | 保留 | 不编入，保留原有 Bezier 插值 |
| DX12、MMV 宿主、通用 mesh 捕获、Alembic 和新增烘焙 | 保留 | 不编入对应实现 |
| 音视频 | 保留现代音视频导入与 MP4/MOV 输出，依赖 Windows 媒体组件 | WAV 音频、AVI 背景与 AVI 输出 |
| PMM、VMD、VPD 保存 | 可写 MMS 扩展数据 | 仅写标准内容，忽略读取到的 MMS 扩展 |
| MME | 保留原生 DX9 接口，插件需另行安装 | 保留原生 DX9 接口，插件需另行安装 |
| 程序体积（v2026.10.13.1 x64 产物） | 4,968,960 字节，约 4.97 MB | 2,192,384 字节，约 2.19 MB |

两个 ZIP 均仅包含 `MikuMikuDance.exe` 和 `licenses/` 下的许可文件，不附带用户配置、模型、MME 或 MMV 插件。完整包包含 MMV 宿主接口；`MMV4MMS.dll` 仍处于单独测试阶段，不在此次公开附件中。最小包体积比完整包约小 56%，这不是运行速度的对比结果。

最小包仍保留三语界面等便利修复，并非原版 MMD 的逐字节复刻。它按原版格式的名称编码和字段长度检查保存内容：名称与文件内引用须能完整表示为 CP932，超出范围时拒绝保存。模型编辑只提供 PMD 保存；最小包拒绝 PMX 到 PMD 的有损转换。

含缩放、曲线或 mesh 缓存等扩展的完整包工程，经最小包保存后会丢失这些扩展，请另存为副本。真实 MMD 9.32 程序的双向载入及再保存尚未完成验证，不能据此保证任意工程无损互导。选择下载前可查看[最小包范围与格式限制](docs/ORIGINAL_PROFILE.md)及[透明排序验收记录](docs/TRANSPARENT_SORT_VALIDATION_20261011.md)。

两档解压到不同目录，分别运行各自的 `MikuMikuDance.exe`。有旧版配置和插件时，先保留旧目录；不要把两个 ZIP 解压到同一目录后相互覆盖。

## ダウンロードとパッケージの違い

[GitHub Releases](https://github.com/Loki-0228/MikuDanceStudio/releases/latest) では、ファイル名に `full` または `minimal` を含む Windows x64 パッケージを配布しています。両方とも同じソースブランチからビルドし、共通の不具合を修正します。後半の機能一覧は完全版の説明です。最小版は `mmd932` ビルドプロファイルを使用します。

| 項目 | 完全版 `full` | 最小版 `minimal` |
| --- | --- | --- |
| PMD・PMX 読み込み、従来のアニメーション編集、剛体物理、言語関連の修正、MME インターフェース | 含む | 含む |
| 新しい透明ソート | 任意で有効化、初期設定は無効 | 含まない。従来の単一パスの材質順序を使用 |
| シャドウ処理の再構成、動的精度、フィルタリング | クリッピング防止、動的精度、任意の平滑化（初期設定は無効） | 従来の投影と SM2/SM3 シェーダー。平滑化スイッチなし |
| ボーンスケール、新しい曲線エディタ、モーフ曲線 | 含む | 含まない。従来の Bezier 補間を維持 |
| DX12、MMV ホスト、メッシュ取得、Alembic、追加のベイク機能 | 含む | 含まない |
| 音声・動画 | Windows メディアコンポーネントによる各種形式の読み込みと MP4・MOV 出力 | WAV 音声、AVI 背景、AVI 出力 |
| PMM・VMD・VPD 保存 | MMS 拡張を保存可能 | 標準データのみ保存。読み込んだ MMS 拡張は無視 |

どちらの ZIP も `MikuMikuDance.exe` と `licenses/` 内のライセンスファイルのみを含みます。モデル、ユーザー設定、MME、`MMV4MMS.dll` は含まれません。MMV は別途テスト中です。`v2026.10.13.1` の実行ファイルは完全版が 4,968,960 バイト、最小版が 2,192,384 バイトで、最小版は約 56% 小さくなっています。これは再生速度の比較ではありません。

最小版は、ファイル内に保存する名前と参照先の CP932 表現およびフィールド長を確認し、表現できない内容の保存を拒否します。モデル編集の保存形式は PMD のみで、情報が失われる PMX から PMD への変換も拒否します。完全版の拡張を含むプロジェクトを最小版で保存すると、スケール、曲線、メッシュキャッシュの拡張が失われるため、別名で保存してください。実際の MMD 9.32 を使った双方向の読み込みと再保存は、まだ検証できていません。[最小版の制限](docs/ORIGINAL_PROFILE.md)も参照してください。

両方を使う場合は別々のフォルダーに展開し、それぞれの実行ファイルを起動してください。設定やプラグインが入った既存のフォルダーは残してください。

---

## English

MikuDanceStudio (MMS) is a Windows desktop application for creating and editing MMD dance animations. [v2026.10.13.1](https://github.com/Loki-0228/MikuDanceStudio/releases/tag/v2026.10.13.1) provides Windows x64 full and minimal packages, with MMS additions in the full package and original MMD features plus related fixes in the minimal package. Both ZIPs contain only `MikuMikuDance.exe` and the license files in `licenses/`, so if you use both versions, extract them into separate directories and run each directory's executable.

### Changes in v2026.10.13.1

This release includes three fixes shared by the full and minimal builds:

- The vertical bone rotation handle now follows MMD's axis direction when viewed from the side. This builds on the rotation-growth fix already included in `.7`.
- PMD models with a reordered shared Toon file table now retain their textured appearance when bone selection is enabled. The fix also covers fallback to shared Toon textures. Other reported PMD white-scene cases still need confirmation.
- Repeated clicks on overlapping rotation-only and translation-enabled bones now cycle through the eligible bones in model order. Rapid clicks work in both the main and floating viewports.

See the [release notes](docs/RELEASE_NOTES_20261013.md#english) for the changes and package comparison.

For package scope and save limits, see [Downloads and package differences](#downloads-and-package-differences). The following feature list describes the full build.

### Features

- **Models**: PMD / PMX loading (incl. BDEF4/SDEF weights), bone trees, IK,
  morphs, toon rendering
- **Animation**: VMD load/save, VPD export, frame editor (bone / morph /
  camera / lighting / self-made expression / accessory tracks), whole-project
  frame-rate correction, and frame playback with physics preview
- **Physics**: Bullet 2.75 rigid bodies + constraints (6DOF spring),
  deterministic solve order, physics baking and per-frame bone-motion
  resampling. Supports frame ranges, ERP/CFM, substeps, simulation rate,
  undo/cancel, and unencrypted UTF-8 JSON presets. The baking UI has English,
  Chinese, and Japanese labels and tooltips, with Bullet defaults. See
  [physics baking](docs/PHYSICS_BAKING_REQUIREMENTS.md).
- **Mesh baking and export**: Supports per-layer MMV baking. Alembic export
  includes MMV/MME vertex deformations. Exporting ABC and baking into the project can be selected
  independently; project baking warns about its effects. Clear mesh bake
  restores live deformation for the current model or all objects while keeping
  disabled FX disabled. See [mesh export and baking](docs/ALEMBIC_EXPORT.md).
- **Rendering**: Native DX12 preview and GPU skinning with DX9/CPU fallback when
  DX12 is unavailable; MME Effect support, toon textures, ground shadows,
  stereoscopic 3D (NVIDIA 3D Vision), and separate device choices for main
  rendering, vertex processing, and optional MMV processing
- **Multimedia**: Imports many audio formats and MP4/MOV video through Windows
  Media Foundation, depending on installed Windows media components;
  plays Wave audio; records AVI through installed DirectShow codecs, MP4/H.264
  through Media Foundation, and transparent MOV. The export dialog lists
  registered H.264 encoders and supports aspect-ratio-preserving output sizes.
  VSQ and Kinect input are also supported.
- **UI and compatibility**: English/Chinese/Japanese interface, Unicode file
  paths, timeline, accessory editing, undo/redo

#### MMV plug-in (under testing; not released)

MMV is an optional plug-in that uses an MME-style effect assignment workflow. It can take over or extend parts of the original bone and morph deformation, skinning, physics, and rendering pipeline. Effects run in layers and produce meshes for MMS to draw in the final scene. MMV is still under testing and has not been released because mesh modification is currently too slow.

### Curve editor and PMM/VMD extensions

The native curve editor edits model expression, camera, lighting, self-shadow,
gravity, and accessory tracks. Curve data can be exported in a VMD tail
extension; accessory and scene settings are stored in PMM v2 extension records.
PMM v2 also preserves MMS project settings without changing the standard file
header. Bone-scale data uses a VMD tail extension while keeping the standard
VMD sections readable by older tools. Re-saving an extended project in original
MMD may discard its MMS-only extension data. See [curve editor](docs/ANIMATION_CURVE_EDITOR.md)
and [PMM v2 extensions](docs/PMM_V2_EXTENSIONS.md).

Processor choices are saved per Windows user and take effect on the next
startup. They are not stored in PMM, VMM, or VEM files.

The source and history audit is in [MMS native feature audit](docs/MMS_NATIVE_FEATURES_AUDIT_2026-09-27.md).

### Building from Source

Dependencies: Windows 10/11, Visual Studio 2022 (v143 toolset + Desktop
development with C++ workload), [CMake](https://cmake.org/) ≥ 3.21,
[Conan](https://conan.io/) 2.x.

```bat
:: 1) Get the Bullet 2.75 sources (used by the local Conan recipe)
::    Download https://github.com/bulletphysics/bullet3/archive/refs/tags/2.75.zip
::    and extract to recipes/bullet275/bullet-src/

:: 2) Register the local recipe and install dependencies (x64 reference)
conan export recipes/bullet275
conan install . --profile:all profiles/x64 --build=missing -s build_type=Release

:: 3) Configure and build
cmake --preset conan-default
cmake --build build --config Release
```

Machines that cannot run D3D12 can keep the DX9/MME renderer and CPU skinning
while omitting the optional DX12 runtime dependency. Configure a separate build:

```bat
cmake -S . -B build/dx12-disabled -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=D:/MMD-Next/MMS/build/generators/conan_toolchain.cmake ^
  -DMIKUDANCESTUDIO_ENABLE_DX12=OFF
cmake --build build/dx12-disabled --config Release --target MikuDanceStudio
```

This build hides DX12 compute adapters, keeps vertex processing on the CPU,
and skips native DX12 preview. The normal build keeps DX12 enabled. See
[DX12 compatibility build details](docs/DX12_OPTIONAL_COMPAT.md).

The final MMS package is `D:/MMD-Next/MMS/build/Release/`, with
`MikuMikuDance.exe` as its GUI application. After moving the checkout, rerun
`conan install` before using CMake presets to regenerate absolute build paths. The
DirectShow push source used for AVI is linked into the executable, and MP4
recording uses Windows Media Foundation; neither recording path requires an
external `MMDxShow.dll`. Other build products stay out of the runtime directory
(static/import libraries in `build/lib`, PDBs in `build/symbols`). Default Toon
textures are embedded in the executable, so no extra Toon files are required.
To change the default rendering style, place any of `toon01.bmp` through
`toon10.bmp` in the `Data/` directory next to `MikuMikuDance.exe`, then restart
MMS. Each readable file overrides its corresponding embedded texture.
Missing or unreadable files fall back to the embedded defaults. Models with
their own Toon textures continue to use those textures. A release tree
contains the shipped files only:

```bat
cmake --install build --config Release --prefix build/Release
:: build\Release\MikuMikuDance.exe
```

The source repository does not contain compiled release packages. Archives
attached to GitHub Releases are binary distributions, not source files.
`res/assets/` contains application resources; `docs/images/` contains images
used by the documentation.

Regression tests are never part of a release artifact (excluded from the default
build, no install rule). To run them:

```bat
cmake --preset conan-default -DMIKUDANCESTUDIO_BUILD_INPUT_TESTS=ON
cmake --build build --config Release
cmake --build build --config Release --target keyboard_input_tests
ctest --test-dir build -C Release --output-on-failure
```

> Note: the Conan profiles under `profiles/` assume a Windows + MSVC host; if
> your default profile is already set up that way, you can simply
> `include(default)`.

### MikuMikuEffect (MME)

The executable keeps the original name `MikuMikuDance.exe` and meets MME's three
host requirements: the 37 `Exp*` exports by ordinal 1..37
(`exports/MikuMikuDance.def`); **static** imports of the `d3dx9_43.dll` entries
MMHack.dll patches - it rewrites the _host's_ IAT, so a host that resolved D3DX
at runtime exposed no slot and MME aborted with `Initialize error`
(`res/imports/d3dx9_43.def`, import library generated at build time); and the
exact module name, from which MMHack.dll's import table binds those `Exp*`
functions. Copy the x64 `d3d9.dll`, `MMHack.dll` and `MMEffect.dll` next to it
and the `MMEffect` menu appears; `.fx` effects are then assigned as usual. No
DirectX SDK, at runtime or at build time. See
[docs/MME_COMPATIBILITY.md](docs/MME_COMPATIBILITY.md).

### Personal usage changes

These additions exist only for personal convenience; they are neither MMD's
original usage habits nor compatibility requirements, and builders may delete
them by editing the code.

- **Native bone scaling**: Uniform and XYZ controls support local, global and view axes for every bone. PMM preserves full scale data; VMD stores scale in a versioned tail extension while preserving its standard prefix. See [bone scaling](docs/BONE_SCALING.md).
- **Extended self-shadow range**: Removes the original MMD restriction on the range where self-shadows can be generated.
- **Dynamic shadow scaling**: The self-shadow panel can match shadow-map resolution to the distance of visible meshes. It leaves the light and shadow coverage unchanged. With this option enabled, the slider adjusts precision around the matched result: the midpoint (5000) is neutral, left reduces precision, and right increases it, within the selected shadow-quality and device limits. Turning it off restores the slider's original shadow-range function.
- **Error reporting**: The program writes no log files; a fatal error raises a
  modal window whose text can be copied with Ctrl+C, and the PDB under
  `build/symbols/` resolves source lines.
- **IME and hotkeys**: Letter hotkeys keep working with a Chinese/Japanese IME
  active - P play/stop plus A/S/D/F/G/H/I/J/K/L/U/V/X/Z/B/C/R - except inside
  text fields.
- **Transparent sorting preference**: Adds five selectable sorting levels,
  including disable, to adjust transparency ordering for personal scenes.

### Contributions

This project modifies code that [XiaHan](https://blog.han.moe)
reverse-engineered from the original 32-bit mmd932 and refactored into the
[MikuDanceStudio](https://github.com/jstzwj/MikuDanceStudio) port, which carried
out the bulk of the behavior-level port and the x64 alignment. 洛琪
(GitHub: [Loki-0228](https://github.com/Loki-0228)) maintains this repository
and contributes MME compatibility work, Chinese UI improvements, playback and
input fixes, crash diagnostics, and build and release organization.

### Acknowledgments

- Yu Higuchi (樋口優), original author of
  [MikuMikuDance](https://learnmmd.com/downloads/), whose application provides
  the behavior and feature reference for this project.
- [Bullet Physics](https://github.com/bulletphysics/bullet3) developers and
  contributors, for the physics engine used in this project (zlib license).
- [XiaHan](https://blog.han.moe), who reverse-engineered the original 32-bit
  mmd932 and refactored it into the code this project modifies, and the other
  contributors to the
  [MikuDanceStudio port](https://github.com/jstzwj/MikuDanceStudio).
- 舞力介入P, original author of MikuMikuEffect (MME), for bringing effect
  extensions to MMD. See the [MME introduction coauthored by its creator](https://codezine.jp/article/detail/5997).
- MME community effect authors and contributors, for sharing effects,
  tools, and knowledge.
- 老面星星 (StarNoodle495) for continued testing and suggestions ([BowlRoll](https://bowlroll.net/user/182602)); 沙拉酱 ([Bilibili](https://space.bilibili.com/3493080791517295)) for technical guidance; and 微波叮桃子 ([AplayBox](https://www.aplaybox.com/u/972840773)), 靡苦居士 ([Bilibili](https://space.bilibili.com/8545990)), 幽灵_U0 ([Bilibili](https://space.bilibili.com/454701392)), and 低智商鼻嘎 for testing and bug reports.

### License

This project is licensed under the same freeware terms as MikuMikuDance —
see [LICENSE](LICENSE). Third-party components follow their own licenses:
Bullet Physics (zlib), Alembic and Imath (BSD-3-Clause), MinHook (BSD-2-Clause), DirectX SDK / Windows SDK (Microsoft EULA). Export dependency notices are in [licenses](licenses/README.md).

---

## 中文

MikuDanceStudio（MMS）是用于制作和编辑 MMD 舞蹈动画的 Windows 桌面应用。[v2026.10.13.1](https://github.com/Loki-0228/MikuDanceStudio/releases/tag/v2026.10.13.1) 提供 Windows x64 完整包与最小包，完整包保留 MMS 新增功能，最小包保留 MMD 原有功能及相关缺陷修复。两个 ZIP 均仅含 `MikuMikuDance.exe` 和 `licenses/` 下的许可文件，同时使用两版时须分别解压到不同目录，并运行各自目录中的主程序。

### v2026.10.13.1 更新

本版包含三项完整包与最小包共用的修复：

- 修正骨骼竖向旋转柄从模型侧面操作时的轴向，使其与原版 MMD 一致。此项是在 `.7` 已有的旋转放大修复基础上纠正方向。
- 修复共享 Toon 文件表顺序不同的 PMD 在开启骨骼「选择」后丢失贴图颜色、呈灰白色的问题，也覆盖回退到共享 Toon 的情况。其他 PMD 全白报告仍待确认。
- 旋转骨骼与可移动骨骼重叠时，重复点击按模型内顺序循环选择。主视口和独立视口均支持快速连续点击。

更新内容与包的区别见[三语发布日志](docs/RELEASE_NOTES_20261013.md#中文)。

完整包与最小包在同一分支维护，共同缺陷同步修复并分别验收。两档功能与保存限制见[下载与版本区别](#下载与版本区别)，下列新增功能以完整包为准。

### 功能

- **模型**：PMD / PMX（含 BDEF4/SDEF 权重）加载，骨骼树、IK、形态（morph）、toon 渲染
- **动画**：VMD 加载/保存、VPD 导出、帧编辑器（骨骼/形态/相机/照明/自作表情/附件轨道）、
  整个工程的帧率修正、帧回放与物理预览
- **物理**：Bullet 2.75 刚体与约束（6DOF spring），确定性求解顺序；支持烧录物理结果和逐帧重采样骨骼动作。可设置帧范围、ERP、CFM、子步数和计算频率，支持撤销、取消及 UTF-8 非加密 JSON 预设。界面提供中英日三语和悬浮说明，初始值采用 Bullet 默认参数。见[物理烧录](docs/PHYSICS_BAKING_REQUIREMENTS.md)
- **网格烧录与导出**：支持 MMV 逐层烘焙。Alembic 可导出包含 MMV/MME 顶点变形的网格；导出 ABC 与烧录回工程可分别选择，只有选择烧录时才提示影响。清除网格烧录可恢复当前模型或全部对象的实时变形，已停用的 FX 保持关闭。见[网格导出与烧录](docs/ALEMBIC_EXPORT.md)
- **渲染**：原生 DX12 预览与 GPU 蒙皮；DX12 不可用时回退到 DX9/CPU。支持 MME Effect、toon 纹理、地面阴影、立体视觉（NVIDIA 3D Vision），并可分别选择主渲染、顶点计算和可选 MMV 处理设备
- **多媒体**：通过 Windows Media Foundation 导入多种音频格式和 MP4/MOV 视频，具体格式取决于系统媒体组件。支持 Wave 音频播放、DirectShow AVI（使用已安装的传统编码器）、Media Foundation MP4/H.264 录制，以及透明 MOV 录制。导出页列出系统已注册的 H.264 编码器并支持等比例修改分辨率。另支持 VSQ 乐谱导入和 Kinect 骨骼输入
- **UI 与兼容**：英中日三语界面、Unicode 文件路径、时间轴、附件编辑、撤销/重做

#### MMV 插件（测试中，尚未发布）

MMV 是采用类似 MME 的效果分配工作流的可选插件，可接管或扩展原版骨骼与形态变形、蒙皮、物理计算及渲染流程。效果按层运行并生成网格，再交由 MMS 绘制最终场景。由于当前网格修改性能过差，MMV 暂未发布，仍在测试阶段。

### 曲线编辑器与 PMM/VMD 扩展

原生曲线编辑器可编辑模型自作表情、相机、照明、自阴影、重力和附件轨道。曲线数据可通过 VMD 尾部扩展导出；附件及场景设置保存在 PMM v2 扩展记录中。PMM v2 在不改变标准文件头的前提下保存 MMS 工程设置。骨骼缩放数据也通过 VMD 尾部扩展保存，标准 VMD 数据段保持不变。原版 MMD 重新保存扩展工程时可能丢弃 MMS 专有扩展数据。详见[曲线编辑器](docs/ANIMATION_CURVE_EDITOR.md)和[PMM v2 扩展](docs/PMM_V2_EXTENSIONS.md)。

运算器选择按 Windows 用户跨工程保存，下次启动生效，不写入 PMM、VMM 或 VEM。

源码与 Git 历史核对见[MMS 原生功能审计](docs/MMS_NATIVE_FEATURES_AUDIT_2026-09-27.md)。

### 从源码构建

#### 准备环境

- Windows 10/11。
- Visual Studio 2022：安装 **C++ 桌面开发**工作负载和 **v143** 工具集。
- [CMake](https://cmake.org/) ≥ 3.21、[Conan](https://conan.io/) 2.x。

以下命令均在仓库根目录执行，构建目标为 x64 Release。
`profiles/x64` 会通过 `include(default)` 继承 Conan 默认配置，再指定 x64 架构；
请先确保默认 profile 已配置为 Windows + MSVC。

#### 1. 准备 Bullet 源码

下载 [Bullet 2.75 源码压缩包](https://github.com/bulletphysics/bullet3/archive/refs/tags/2.75.zip)，
将其中的源码目录内容解压到 `recipes/bullet275/bullet-src/`。
确认目录结构为 `recipes/bullet275/bullet-src/src/`，不要多套一层压缩包目录。
本地 Conan 配方会使用这些源码构建依赖。

#### 2. 安装依赖并编译

```bat
conan export recipes/bullet275
conan install . --profile:all profiles/x64 --build=missing -s build_type=Release
cmake --preset conan-default
cmake --build build --config Release
```

MMS 最终发布包固定在 `D:/MMD-Next/MMS/build/Release/`。移动仓库目录后，先重新运行
`conan install`，再使用 CMake 预设，避免沿用旧目录的绝对路径。构建完成后，主要文件如下：

| 路径                              | 用途                                 |
| --------------------------------- | ------------------------------------ |
| `build/Release/MikuMikuDance.exe` | GUI 主程序                           |
| `build/lib/`                      | 静态库和导入库                       |
| `build/symbols/`                  | 用于调试和崩溃定位的 PDB 符号文件    |

运行前，还需将 `toon01.bmp` 至 `toon10.bmp` 等 toon 纹理放在**工作目录**的 `Data/` 下。
例如，以 `build/Release/` 为工作目录启动时，纹理应放在 `build/Release/Data/`。

AVI 使用的 DirectShow 推源过滤器已静态链接进主程序；MP4 使用 Windows
Media Foundation。AVI 和 MP4 录制都不要求用户另行放置或注册 `MMDxShow.dll`。

#### 3. 整理发布目录

```bat
cmake --install build --config Release --prefix build/Release
```

正式发布目录为 `build/Release/`，安装命令也使用同一路径：

```text
build/Release/
├── MikuMikuDance.exe
└── Data/
```

默认 Toon 贴图已内置，无需额外放置 Toon 文件。
如需改变默认渲染风格，可将 `toon01.bmp` 至 `toon10.bmp` 中任意所需的文件放入
`MikuMikuDance.exe` 同目录下的 `Data/`，然后重启 MMS。
程序优先读取这些同名文件，分别覆盖对应的内置 Toon；缺失或读取失败时使用内置贴图。
模型自带并指定的 Toon 贴图仍按模型设置加载。
静态库、导入库、PDB 和回归测试程序不随此命令安装。
源码仓库不包含编译后的发行包；GitHub Release 中的压缩包属于二进制分发文件，不是源码。仓库里的 `res/assets/` 是程序使用的资源文件，`docs/images/` 是文档配图。

#### 可选：运行输入回归测试

测试程序不参与默认构建，需启用选项并显式构建测试目标：

```bat
cmake --preset conan-default -DMIKUDANCESTUDIO_BUILD_INPUT_TESTS=ON
cmake --build build --config Release --target keyboard_input_tests
ctest --test-dir build -C Release -R keyboard_input_regressions --output-on-failure
```

### MikuMikuEffect（MME）支持

#### 安装与使用

将 **x64 版 MME** 的以下三个文件放到 `MikuMikuDance.exe` 同一目录：

- `d3d9.dll`
- `MMHack.dll`
- `MMEffect.dll`

启动后，菜单栏会出现 `MMEffect` 菜单，可按 MME 的常规方式为模型和配件分配 `.fx` 特效。
请保留主程序名 `MikuMikuDance.exe`，MME 依赖这个名称识别宿主。
本项目的构建与运行均不要求安装 DirectX SDK。

#### 兼容实现说明

本移植为 MME 提供了以下宿主接口：

- **导出函数**：提供 37 个 `Exp*` 函数，导出序号为 1–37，定义见
  [`exports/MikuMikuDance.def`](exports/MikuMikuDance.def)。
- **D3DX 静态导入**：为 `MMHack.dll` 提供可挂钩的导入地址表（IAT）条目，
  导入库由 [`res/imports/d3dx9_43.def`](res/imports/d3dx9_43.def) 在构建时生成。
- **宿主名称**：使用 `MikuMikuDance.exe`，与 `MMHack.dll` 引用的模块名一致。

对象登记、效果分配及已验证的兼容范围，见 [MME 兼容性说明](docs/MME_COMPATIBILITY.md)。

### 个人习惯更改

特性添加仅为方便本人使用，非MMD原本使用习惯或适配需求，编译者可编辑代码删去。

- **原生骨骼缩放**：所有骨骼支持整体及 XYZ 缩放，统一使用局部、全局和视图坐标系。PMM 保存完整数据，VMD 在保留标准前缀的同时，将缩放写入同一文件尾部。见[骨骼缩放说明](docs/BONE_SCALING.md)。
- **扩展自阴影产生范围**：取消原版 MMD 对自阴影产生范围的限制。
- **动态缩放**：自阴影栏目可按画面中可见网格的远近匹配阴影贴图分辨率，不联动光源或阴影覆盖范围。开启后，滑条在匹配结果基础上继续增减精度，中点（5000）为基准，向左降低、向右提高，受美影设置和显卡上限限制。关闭后，滑条恢复原有的阴影范围调整功能。
- **错误提示**：程序不写日志文件；遇到致命错误时会弹出对话框，可按 `Ctrl+C` 复制内容。
  开发者可结合 `build/symbols/` 下对应构建的 PDB 定位源码。
- **输入法与快捷键**：开启中文或日文输入法时，字母快捷键仍有效，包括 `P`（播放/停止）
  和 `A/S/D/F/G/H/I/J/K/L/U/V/X/Z/B/C/R`；在编辑框内输入文字时除外。
- **透明排序偏好**：增加关闭、低、标准、高、最高五档，便于按个人场景调整透明排序。

### 项目来源与维护

本项目在 [XiaHan](https://blog.han.moe) 对 mmd932 32 位原版逆向并重构的代码基础上修改。
上游的 [MikuDanceStudio](https://github.com/jstzwj/MikuDanceStudio)
移植工程完成了主体行为级移植与 x64 对齐，是本项目继续开发的基础。

本项目由洛琪（[Loki-0228](https://github.com/Loki-0228)）维护，
主要改动包括 MME 兼容适配、中文界面改进、播放与输入修复、崩溃诊断，以及构建与发布整理。

### 致谢

- 樋口優：[MikuMikuDance](https://learnmmd.com/downloads/) 原作者。
  原版 MMD 是本项目行为与功能对齐的基础。
- [Bullet Physics](https://github.com/bulletphysics/bullet3) 开发者与贡献者：
  本项目使用其物理引擎（zlib 许可）。
- [XiaHan](https://blog.han.moe)：本项目在他对 mmd932 32 位原版逆向并重构的代码基础上修改，
  感谢他提供逆向成果与重构代码。也感谢
  [MikuDanceStudio](https://github.com/jstzwj/MikuDanceStudio) 移植工程的其他贡献者。
- 舞力介入P：MikuMikuEffect（MME）原作者，感谢其为 MMD 带来的特效扩展能力。
  可参阅[作者参与撰写的 MME 介绍](https://codezine.jp/article/detail/5997)。
- MME 社区的特效作者与贡献者：感谢他们创作和分享特效、工具与使用经验。
- 老面星星（StarNoodle495，[BowlRoll](https://bowlroll.net/user/182602)）：感谢一直以来持续进行测试并提出建议；感谢沙拉酱（[Bilibili](https://space.bilibili.com/3493080791517295)）提供技术指导；感谢微波叮桃子（[AplayBox](https://www.aplaybox.com/u/972840773)）、靡苦居士（[Bilibili](https://space.bilibili.com/8545990)）、幽灵_U0（[Bilibili](https://space.bilibili.com/454701392)）和低智商鼻嘎参与测试并提交 bug。

### 许可

本工程采用与 MikuMikuDance 相同的免费软件条款，详见 [LICENSE](LICENSE)。
第三方组件遵循各自许可：Bullet Physics（zlib）、Alembic 与 Imath（BSD-3-Clause）、MinHook（BSD-2-Clause）、DirectX SDK / Windows SDK（微软 EULA）。新增导出依赖的许可文本见 [licenses](licenses/README.md)。

---

## 日本語

MikuDanceStudio（MMS）は、MMD のダンスアニメーションを制作・編集する Windows デスクトップアプリです。[v2026.10.13.1](https://github.com/Loki-0228/MikuDanceStudio/releases/tag/v2026.10.13.1) では Windows x64 向けに、MMS の追加機能を含む完全版と、MMD 本来の機能および関連する不具合修正を含む最小版を配布しています。どちらの ZIP も `MikuMikuDance.exe` と `licenses/` 内のライセンスファイルのみを含むため、両方を使う場合は別々のフォルダーに展開し、それぞれの実行ファイルを起動してください。

### v2026.10.13.1 の変更

このリリースには、完全版と最小版で共通の修正が含まれています。

- モデルを横から見たときのボーン縦回転ハンドルの回転軸を、オリジナル MMD と一致させました。`.7` の回転操作による拡大の修正に加え、今回は回転方向を修正しています。
- 共有トゥーンのファイル表が通常と異なる順序の PMD で、ボーンの「選択」を有効にするとテクスチャの色が失われ、灰白色になる問題を修正しました。共有トゥーンへのフォールバックも対象です。ほかの PMD 白化報告については、引き続き確認が必要です。
- 回転のみ可能なボーンと移動可能なボーンが重なっている場合、繰り返しクリックするとモデル内の順序で選択が切り替わります。メイン画面と別ウィンドウのどちらでも素早い連続クリックに対応します。

変更内容とパッケージの違いは[リリースノート](docs/RELEASE_NOTES_20261013.md#日本語)を参照してください。

パッケージの機能と保存の制限は[ダウンロードとパッケージの違い](#ダウンロードとパッケージの違い)を参照してください。以下の機能一覧は完全版の説明です。

### 機能

- **モデル**：PMD / PMX（BDEF4/SDEF ウェイト対応）読み込み、ボーンツリー、IK、
  モーフ、トゥーンレンダリング
- **アニメーション**：VMD 読み込み／保存、VPD エクスポート、フレームエディタ
  （ボーン／モーフ／カメラ／照明／自作表情／アクセサリトラック）、物理プレビュー付き
  フレーム再生、プロジェクト全体のフレームレート修正
- **物理**：Bullet 2.75 剛体とコンストレイント（6DOF スプリング）、決定論的な求解順序。物理結果のベイクとボーン動作のフレーム単位再サンプリングにも対応します。フレーム範囲、ERP、CFM、サブステップ、計算周波数を設定でき、取り消し・キャンセルと暗号化しない UTF-8 JSON プリセットを利用できます。UI とツールチップは日英中に対応し、初期値は Bullet の既定値です。[物理ベイク](docs/PHYSICS_BAKING_REQUIREMENTS.md)
- **メッシュベイクと出力**：MMV のレイヤーごとのベイクに対応します。Alembic は MMV/MME の頂点変形を含むメッシュを出力します。ABC 出力とプロジェクトへのベイクは個別に選択でき、ベイクを選んだ場合だけ影響を警告します。メッシュベイク解除で選択モデルまたは全オブジェクトの変形を元に戻し、無効化済み FX は無効のままです。[メッシュ出力・ベイク](docs/ALEMBIC_EXPORT.md)
- **レンダリング**：ネイティブ DX12 プレビューと GPU スキニングに対応し、DX12 が使えない場合は DX9/CPU にフォールバックします。MME Effect、トゥーンテクスチャ、地面影、立体視（NVIDIA 3D Vision）をサポートし、メイン描画、頂点計算、任意の MMV 処理でデバイスを個別に選べます。
- **マルチメディア**：Windows Media Foundation を使って多くの音声形式と MP4/MOV 動画を読み込みます。利用可能な形式は Windows のメディアコンポーネントに依存します。Wave 再生、インストール済みコーデックを使う DirectShow AVI、Media Foundation による MP4/H.264、透過 MOV の録画にも対応します。出力画面で登録済み H.264 エンコーダーを確認でき、縦横比を保った解像度変更も可能です。VSQ と Kinect 入力も利用できます。
- **UI**：英語・中国語・日本語 UI、タイムライン、
  Unicode ファイルパス、アクセサリ編集、元に戻す／やり直し

#### MMV プラグイン（テスト中・未公開）

MMV は MME に似たエフェクト割り当て方式を使う任意のプラグインです。オリジナルのボーン／モーフ変形、スキニング、物理計算、描画処理を引き継ぐ、または拡張します。エフェクトをレイヤーごとに実行してメッシュを生成し、最終シーンは MMS が描画します。現在はメッシュ変更処理が遅すぎるため公開を見合わせており、引き続きテスト中です。

### 曲線エディタと PMM/VMD 拡張

- **曲線エディタ**：モデルの自作表情、カメラ、照明、セルフシャドウ、重力、アクセサリのトラックを編集できます。アニメーション機能にはプロジェクト全体のフレームレート修正もあります。
- **PMM 拡張**：標準ヘッダーを変えずに、MMS のプロジェクト設定とシーン／アクセサリ曲線を v2 拡張レコードに保存します。
- **VMD 拡張**：標準データを維持したまま、末尾拡張に追加曲線やボーンスケールを保存します。オリジナル MMD で拡張プロジェクトを再保存すると、MMS 固有データが失われることがあります。

詳しくは[曲線エディタ](docs/ANIMATION_CURVE_EDITOR.md)と[PMM v2 拡張](docs/PMM_V2_EXTENSIONS.md)を参照してください。

処理デバイスの選択は Windows ユーザーごとに保存され、次回起動時に反映されます。PMM、VMM、VEM には保存されません。

ソースと Git 履歴の根拠は[MMS ネイティブ機能監査](docs/MMS_NATIVE_FEATURES_AUDIT_2026-09-27.md)を参照してください。

### ソースからのビルド

必要環境：Windows 10/11、Visual Studio 2022（v143 ツールセット＋「C++ による
デスクトップ開発」ワークロード）、[CMake](https://cmake.org/) ≥ 3.21、
[Conan](https://conan.io/) 2.x。

```bat
:: 1) Bullet 2.75 のソースを入手（ローカル Conan レシピで使用）
::    https://github.com/bulletphysics/bullet3/archive/refs/tags/2.75.zip をダウンロードし、
::    recipes/bullet275/bullet-src/ に展開

:: 2) ローカルレシピを登録して依存関係をインストール（x64 版に対応）
conan export recipes/bullet275
conan install . --profile:all profiles/x64 --build=missing -s build_type=Release

:: 3) 構成してビルド
cmake --preset conan-default
cmake --build build --config Release
```

成果物は `build/Release/MikuMikuDance.exe`（GUI アプリケーション）です。AVI 用の
DirectShow プッシュソースは実行ファイルに静的リンクされ、MP4 録画は Windows Media
Foundation を使用します。いずれも外部の `MMDxShow.dll` を配置・登録する必要はありません。
静的／インポートライブラリ（`build/lib`）と PDB（`build/symbols`）は実行ディレクトリに入りません。
既定のトゥーンテクスチャは実行ファイルに内蔵されているため、追加の配置は不要です。
既定の描画スタイルを変更する場合は、`toon01.bmp` から `toon10.bmp` のうち必要なファイルを
`MikuMikuDance.exe` と同じディレクトリの `Data/` に配置し、MMS を再起動してください。
読み込める同名ファイルが対応する内蔵テクスチャより優先されます。
ファイルがない場合や読み込みに失敗した場合は、内蔵テクスチャを使用します。
モデル独自のトゥーンテクスチャは、引き続きモデルの設定に従って読み込まれます。
テスト実行ファイルは `EXCLUDE_FROM_ALL` のため既定のビルドにも配布物にも含まれません。
配布ディレクトリには配布ファイルのみが含まれます：

```bat
cmake --install build --config Release --prefix build/Release
:: build\Release\MikuMikuDance.exe
```
ソースリポジトリにはビルド済みの配布パッケージを含めません。GitHub Releases に添付する
アーカイブはソースではなくバイナリ配布物です。`res/assets/` はアプリケーションのリソース、
`docs/images/` はドキュメント用の画像です。

> 注：`profiles/` 配下の Conan プロファイルはホストが Windows + MSVC であることを
> 前提としています。デフォルトプロファイルが既にその設定であれば、そのまま
> `include(default)` できます。

### MikuMikuEffect（MME）対応

実行ファイルはオリジナルと同じ `MikuMikuDance.exe` 名で、MME がホストに要求する 3 点を
満たします：37 個の `Exp*` エクスポート（序数 1..37）、MMHack.dll が書き換える
`d3dx9_43.dll` の IAT スロットを**静的インポート**で用意（`res/imports/d3dx9_43.def` から
ビルド時にインポートライブラリを生成。以前は D3DX を動的解決していたため書き換え先がなく
`Initialize error` になっていました）、そしてモジュール名そのもの（MMHack.dll のインポート表が
`MikuMikuDance.exe` から `Exp*` を束縛します）。x64 版の `d3d9.dll`、`MMHack.dll`、
`MMEffect.dll` を同じフォルダに置くとメニューバーに `MMEffect` 欄が出て、あとは通常どおり
`.fx` を割り当てるだけです。実行時もビルド時も DirectX SDK は不要です。詳細は
[docs/MME_COMPATIBILITY.md](docs/MME_COMPATIBILITY.md)。

### 個人的な使用習慣による変更

追加機能は個人的な利便性のためのもので、MMD 本来の使用習慣でも互換要件でもありません。
ビルドする方はコードを編集して削除できます。

- **ボーンスケール**：すべてのボーンを一括または XYZ 軸ごとに拡大縮小できます。ローカル・グローバル・ビュー座標に対応し、PMM に保存します。VMD は標準部分を維持し、同じファイルの末尾にスケール拡張を保存します。[詳細](docs/BONE_SCALING.md)。
- **セルフシャドウ範囲の拡張**：オリジナル MMD にあるセルフシャドウ生成範囲の制限をなくしました。
- **動的スケーリング**：セルフシャドウ欄で、画面内のメッシュまでの距離に応じてシャドウマップの解像度を調整できます。光源と影の範囲は変更しません。有効時のスライダーは自動判定した解像度を基準に精度を調整し、中央（5000）が基準、左が低精度、右が高精度です。美影設定と GPU の上限が適用されます。無効にすると、スライダーは従来の影の範囲調整に戻ります。
- **エラー表示**：ログファイルは出力せず、致命的エラー時はモーダルなエラーウィンドウを
  表示します（Ctrl+C で内容をコピーでき、`build/symbols/` の PDB でソース行を特定できます）。
- **入力メソッドとショートカット**：中国語／日本語の入力メソッドを ON にしたままでも
  英字ショートカット（P 再生／停止、ほか A/S/D/F/G/H/I/J/K/L/U/V/X/Z/B/C/R）が有効です
  （テキスト入力欄を除く）。
- **透明ソート設定**：個人のシーンに合わせて、無効・低・標準・高・最高の五段階を選べます。

### 貢献

本プロジェクトは、[XiaHan](https://blog.han.moe) 氏が mmd932 32 ビット版オリジナルを
リバースエンジニアリングしてリファクタリングしたコードを基に改変したものです。
移植本体と x64 対応は同氏によるもので、上流の
[MikuDanceStudio](https://github.com/jstzwj/MikuDanceStudio) 移植プロジェクトとして
公開されています。洛琪（GitHub：[Loki-0228](https://github.com/Loki-0228)）が
本リポジトリを保守し、MME 互換対応、中国語 UI の改善、再生・入力の不具合修正、
クラッシュ診断、ビルド・配布構成の整備に取り組んでいます。

### 謝辞

- 樋口優氏：[MikuMikuDance](https://learnmmd.com/downloads/) の原作者。
  オリジナル MMD の動作と機能を本プロジェクトの目標としています。
- [Bullet Physics](https://github.com/bulletphysics/bullet3) の開発者・貢献者の皆様：
  本プロジェクトで使用する物理エンジン（zlib ライセンス）に感謝します。
- [XiaHan](https://blog.han.moe) 氏：本プロジェクトは、同氏が mmd932 32 ビット版
  オリジナルをリバースエンジニアリングしリファクタリングしたコードを基に改変した
  ものです。同氏ならびに上流の
  [MikuDanceStudio](https://github.com/jstzwj/MikuDanceStudio) の貢献者の皆様に
  感謝します。
- 舞力介入P 氏：MikuMikuEffect（MME）の原作者。
  MMD にエフェクト拡張機能をもたらしてくださったことに感謝します。
  [作者が共著した MME 紹介記事](https://codezine.jp/article/detail/5997)もご覧ください。
- MME コミュニティのエフェクト作者・貢献者の皆様：
  エフェクト、ツール、ノウハウの共有に感謝します。
- 老面星星氏（StarNoodle495、[BowlRoll](https://bowlroll.net/user/182602)）：継続的なテストと提案に感謝します。沙拉酱氏（[Bilibili](https://space.bilibili.com/3493080791517295)）には技術指導を、微波叮桃子氏（[AplayBox](https://www.aplaybox.com/u/972840773)）、靡苦居士氏（[Bilibili](https://space.bilibili.com/8545990)）、幽灵_U0 氏（[Bilibili](https://space.bilibili.com/454701392)）、低智商鼻嘎氏にはテストとバグ報告に感謝します。

### ライセンス

本プロジェクトは MikuMikuDance と同じフリーウェア条項の下で配布されます。
詳細は [LICENSE](LICENSE) を参照してください。サードパーティコンポーネントは
それぞれのライセンスに従います：Bullet Physics（zlib）、
DirectX SDK / Windows SDK（マイクロソフト EULA）。
