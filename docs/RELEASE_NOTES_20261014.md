# MikuDanceStudio v2026.10.14.6 release notes / 发布说明 / リリースノート

[English](#english) | [中文](#中文) | [日本語](#日本語)

## English

Windows x64. These notes cover the cumulative changes since the previous official release, [v2026.10.12.1](https://github.com/Loki-0228/MikuDanceStudio/releases/tag/v2026.10.12.1).

| Item | Previous official release | This release |
| --- | --- | --- |
| Version | `v2026.10.12.1` | `v2026.10.14.6` |
| Commit | `1b80cbf02b884890abb8939715fbbb258d7428e9` | Pinned by the `v2026.10.14.6` tag |
| Status | Published on 2026-10-12 | Published on 2026-10-14 |

The comparison covers all changes since the previous official release and describes the final behavior in this version. The full and minimal packages use the same fixed source revision and exclude uncommitted workspace changes.

### Export, backups, and editing

- Added image sequence export with output directory, filename prefix, image format, and frame range settings. It supports the formats already available for single-image export. The number of digits in filenames adjusts to the output range.
- Added auto-save settings for the interval in minutes, backup directory, maximum number of backups, and replacement of older backups. The default directory is `backups` beside the application. Manually saved projects are not overwritten. The application always attempts a backup after a crash; success depends on the crash state and whether the destination is writable.
- `Ctrl+F` force paste now supports pasting values between different morphs. The full package adds a force-paste button on the timeline and a recent-projects menu.
- The full package adds independent curves for nine bone channels: position, XYZ rotation, and scale, with playback, seeking, and PMM and VMD extension storage. The curve editor adds double-click key insertion that preserves the curve shape, numeric control-point input, slope and weight dragging, keyframe navigation, and sidebar adjustment.
- Fixed incorrect rotation angles and directions when loading VMD motion for fixed-axis bones. The full package also fixes extra deformation when a child bone rotates under a parent with nonuniform scale.
- Fixed menu state and drawing boundaries after deleting, switching, or reloading accessories. The bottom panel supports width adjustment by dragging, with the layout applied on mouse release. This also fixes some controls extending outside their bounds, flickering, or leaving stale images.

### Preview and anti-aliasing

- The full package adds viewport render scaling, with automatic window matching, reduced-resolution preview, and supersampling. Export dimensions remain unchanged. Low-resolution previews use nearest-neighbor upscaling to display the pixels directly.
- The HUD, frame rate, and upper and lower toolbars use the entire preview area, including letterbox bars. They render at display resolution after the scene is upscaled. Fixed repainting in the main window, floating viewport, and settings windows obscuring the HUD.
- FPS now counts scene frames that actually finish rendering and are successfully submitted. It excludes idle iterations used to save processing time, repeated submissions, and offline recording samples.
- The anti-aliasing menu offers MMD original (MSAA), FXAA, TAA, and SMAA. SMAA has four options: 1x and T2x, each with standard edges or diagonal and corner enhancement. Clicking SMAA applies the last choice; the initial choice is SMAA 1x with diagonal and corner enhancement. Hover over SMAA and move right to open the options. Separate MSAA levels in older projects remain readable.
- TAA and SMAA T2x add motion-vector reprojection for the camera, bones, and deformed geometry, with depth and history validation. Current color, historical color, and depth use their corresponding sample offsets. When history is rejected, the current offset is removed. Motion capture also includes the ground grid.
- When the GPU supports vertex texture sampling, TAA and SMAA T2x retain both frames of deformed positions on the GPU, avoiding a CPU readback and upload of the captured results each frame. Materials with identical inputs share captures, and verified opaque native materials skip the extra coverage draw. MME and alpha textures retain coverage checks. Devices without this capability use the compatibility path. SMAA 1x behavior is unchanged.
- Static images from projects containing only frame 0 receive the missing temporal samples at the same time and pose, without advancing physics or producing extra images. Continuous image sequences retain history: SMAA T2x renders the first image twice and each subsequent image once. If motion data is unavailable, it falls back to single-frame anti-aliasing.
- The anti-aliasing switch, algorithm, and last SMAA option are saved in the PMM v2 extension. Loading a project does not overwrite local defaults. Temporary hardware fallback does not replace the project's original choice.
- Fixed the viewport-scale menu accidentally triggering Open scene, flicker when opening recent projects, and incorrect restoration of viewport dimensions and DPI state after export. Viewport tools support continuous relative dragging and no longer pull the cursor back after losing capture.

### Shadows and compatibility

- Full-package self-shadowing restores the light-grid orientation, projection transform, continuous depth gradients in modes 1 and 2, and linear depth sampling from MMD 9.32. Dynamic shadow resolution shares one range setting, allocates textures according to the actual projection density, and preserves the high-resolution range to avoid clipping.
- The full package adds stronger shadow filtering, disabled by default. When enabled, two passes of B-spline filtering process the current frame's shadow depth map before native materials, MME, and MMV versions supporting the new interface receive it. This prevents external FX from missing the filtered result and avoids filtering it twice in the host. Failure falls back to the current frame's original depth map.
- Fixed the full package still separating opaque and transparent materials under MME when transparency sorting was disabled. With sorting off, materials now draw in the model's original order.
- Fixed ground shadows becoming too dark on some materials after enabling MME. Native rendering and MME now read the same shadow color. Existing shadow brightness, alpha blending switches, and alpha values remain unchanged.
- Fixed the full package's highest transparency-sorting level treating precompiled effects as source text to append to. Transparency depth peeling now uses a separate, complete effect resource, avoiding loading failures, missing transparent layers, or crashes caused by corrupted bytecode.
- Improved original MME compatibility for read-only access to Chinese paths and existing effect references. When the full package sends model paths to MME, it prefers paths that the system code page can represent accurately, improving lookup of EMD files with the same base name.
- The full package includes the optional `MMSNvidiaAAFix.dll` for installing an NVIDIA compatibility setting for double-sided rendering in the current application. When the DLL is present, the Help menu offers installation or removal according to the detected state and requests Windows administrator permission when run. Installation or removal takes effect the next time MMS starts. Deleting the DLL removes the menu entry but does not undo an installed driver setting. Installation stops if no NVIDIA driver is available; AMD and Intel do not receive this setting.
- New settings and prompts support Chinese, Japanese, and English, with revised Japanese and English wording, button labels, and window layouts.

### Full and minimal packages

| Added feature or change | Full | Minimal |
| --- | --- | --- |
| Image sequences, periodic backups, and crash backup attempts | Included | Included |
| Morph force paste with `Ctrl+F` and bottom-panel width adjustment | Included | Included |
| Shared fixes for fixed-axis VMD motion, ground-shadow color, accessory state, and export viewport restoration | Included | Included |
| Recent projects, force-paste button, and viewport render scaling | Included | Not included |
| Nine-channel bone curves and nonuniform-scale fix | Included | Bone scaling and the new curve module are not included |
| Dynamic shadows, clipping prevention, and stronger filtering | Included | Retains original shadow rendering and range clipping |
| AA algorithm selection, TAA, SMAA, and NVIDIA patch DLL | Included | Retains the original AA switch; new modules are not included |
| Effect-loading fix for the highest transparency-sorting level | Included | Transparency sorting is not included |

The full ZIP contains `MikuMikuDance.exe`, `MMSNvidiaAAFix.dll`, and `licenses/`. The minimal ZIP contains the application and licenses. Neither package includes models, user settings, or MME. The main ZIPs do not include `MMV4MMS.dll`.

## 中文

Windows x64。本说明记录上次正式发布 [v2026.10.12.1](https://github.com/Loki-0228/MikuDanceStudio/releases/tag/v2026.10.12.1) 至本次发布的累计变化。

| 项目 | 上次正式发布 | 本次发布 |
| --- | --- | --- |
| 版本 | `v2026.10.12.1` | `v2026.10.14.6` |
| 提交 | `1b80cbf02b884890abb8939715fbbb258d7428e9` | 由版本标签 `v2026.10.14.6` 固定 |
| 状态 | 2026-10-12 已发布 | 2026-10-14 发布 |

对比范围覆盖上次正式发布之后的累计修改，以本次发布的最终行为为准。完整包与最小包来自同一固定源码，不包含工作区尚未提交的修改。

### 导出、备份与编辑

- 新增图片序列输出，可设置目录、文件名前缀、图片格式和帧范围。支持单帧图片输出已有的格式，序号位数随输出范围调整。
- 新增「自动保存设置…」，支持分钟间隔、备份目录、数量上限和覆盖旧备份。默认保存到程序同级的 `backups`，不覆盖手动保存的工程。发生崩溃时始终尝试备份，成功与否受崩溃状态及目录可写性等条件限制。
- `Ctrl+F` 强制粘贴支持不同表情间的数值粘贴。完整包增加时间轴强制粘贴按钮和最近工程菜单。
- 完整包增加骨骼九通道自由曲线，分别编辑位移、XYZ 角度和缩放，支持播放、定位及 PMM、VMD 扩展存储。曲线编辑器增加保持形状的双击插帧、控制点数值输入、斜率与权重拖动、关键帧跳转和侧栏调整。
- 修复固定轴骨骼载入 VMD 后的旋转角度及方向偏差。完整包修复非等比父级缩放后，子骨骼旋转产生额外形变的问题。
- 修复配件删除、切换或重载后的菜单状态和绘制分界。底部面板支持拖动调整宽度，松开鼠标后应用布局，修复部分控件越框、闪烁和残影。

### 预览与抗锯齿

- 完整包新增视口渲染比例，支持自动匹配窗口、降低比例预览和超采样，不改变导出尺寸。低分辨率画面用最近邻放大，直接显示像素块。
- HUD、帧率及上下工具条按整个预览框布局，包含黑边区域，并在场景放大后按显示分辨率绘制。修复主窗、浮窗及设置窗口重绘遮挡 HUD 的问题。
- FPS 改为统计实际完成并成功提交的场景帧，排除省性能空循环、重复提交和离线录制采样。
- 抗锯齿菜单提供「MMD 原始（MSAA）」「FXAA」「TAA」和「SMAA」。SMAA 有 1x、T2x 各自的「标准」与「斜线与拐角增强」四档。直接点击应用上次选择，初始为「SMAA 1x（斜线与拐角增强）」；悬浮后向右移动可展开选项。旧工程的独立 MSAA 档位仍兼容读取。
- TAA 和 SMAA T2x 增加相机、骨骼及变形后几何的运动矢量重投影，并检查深度与历史有效性。当前色彩、历史色彩及深度使用对应的采样偏移；拒绝历史时消除当前偏移，地面网格也纳入运动捕获。
- 显卡支持顶点纹理读取时，TAA、SMAA T2x 将两帧变形后位置保留在 GPU，省去捕获结果每帧读回 CPU 再上传的过程。输入相同的材质共享捕获结果，已确认不透明的原生材质省去覆盖重绘。MME 和透明贴图保留覆盖检查；不支持该能力时使用兼容路径。SMAA 1x 行为不变。
- 仅有第 0 帧的静止图片会在同一时间和姿态下补足时间采样，不推进物理或增加输出图片。连续图片序列保留历史，SMAA T2x 首张绘制两次，后续每张一次。无法取得运动数据时回退到单帧抗锯齿。
- 抗锯齿开关、算法和上次 SMAA 档位随 PMM v2 扩展保存。加载工程不改写本机默认值，临时硬件回退不覆盖工程原选择。
- 修复视口比例菜单误触「打开场景」、最近工程菜单展开闪烁，以及导出后视口尺寸和 DPI 状态未正确恢复的问题。视口工具支持连续相对拖动，失去捕获时不再拉回光标。

### 阴影与兼容性

- 完整包自阴影恢复 MMD 9.32 的光源网格方向、投影变换及模式 1、2 的连续深度渐变，恢复线性深度采样。动态阴影精度共用同一范围设置，按实际投影密度分配纹理，保留高精度范围防裁切。
- 完整包新增「滤波加强」，默认关闭。开启后先对当帧阴影深度图进行两遍基础样条滤波，再交给原生材质、MME 和支持新接口的 MMV，避免外部 FX 漏读增强结果或宿主重复滤波。失败时回退到当帧原始深度图。
- 修复完整包在 MME 下关闭透明排序后，仍拆分不透明和透明材质的问题。关闭时按原模型材质顺序绘制。
- 修复部分材质启用 MME 后地面阴影异常加深的问题，使原生与 MME 读取相同的阴影颜色。原有阴影亮度、透明混合开关及 alpha 值保持不变。
- 修复完整包「最高」透明排序将预编译效果当作源码拼接的问题。透明剥层使用独立的完整效果资源，避免损坏字节码导致加载异常、透明层丢失或崩溃。
- 改善原版 MME 对中文路径和已有特效引用的只读访问兼容性。完整包向 MME 提交模型路径时，优先使用系统代码页能够准确表达的路径，改善同名 EMD 的查找。
- 完整包附带可选的 `MMSNvidiaAAFix.dll`，用于安装当前程序的 NVIDIA 双面绘制兼容设置。DLL 存在时，帮助菜单按检测结果提供安装或卸载入口；执行时请求 Windows 管理员授权。安装或卸载在下次启动 MMS 时生效。删除 DLL 会移除菜单入口，但不会撤销已安装的驱动设置。没有可用 NVIDIA 驱动时停止安装，AMD、Intel 不应用此设置。
- 新增设置和提示覆盖中文、日文、英文，并调整日英文案、按钮含义和窗口布局。

### 完整包与最小包

| 本次新增功能或变化 | 完整包 | 最小包 |
| --- | --- | --- |
| 图片序列、周期备份、崩溃备份尝试 | 包含 | 包含 |
| 表情 `Ctrl+F` 强制粘贴、底部面板调宽 | 包含 | 包含 |
| 固定轴 VMD、地面阴影颜色、配件状态、导出视口等共享修复 | 包含 | 包含 |
| 最近工程、强制粘贴按钮、视口渲染比例 | 包含 | 不包含 |
| 九通道骨骼自由曲线、非等比缩放修复 | 包含 | 不包含骨骼缩放及新曲线模块 |
| 动态阴影、防裁切、滤波加强 | 包含 | 保留原版阴影方式及范围裁切 |
| AA 算法选择、TAA、SMAA、NVIDIA 补丁 DLL | 包含 | 保留原抗锯齿开关，不包含新增模块 |
| 最高透明排序效果加载修复 | 包含 | 不包含透明排序模块 |

完整包 ZIP 包含 `MikuMikuDance.exe`、`MMSNvidiaAAFix.dll` 和 `licenses/`；最小包 ZIP 包含程序和许可证。不附带模型、用户设置或 MME。主 ZIP 不包含 `MMV4MMS.dll`。

## 日本語

Windows x64 向け。前回の正式リリース [v2026.10.12.1](https://github.com/Loki-0228/MikuDanceStudio/releases/tag/v2026.10.12.1) から今回のリリースまでの累積変更を記載しています。

| 項目 | 前回の正式リリース | 今回のリリース |
| --- | --- | --- |
| バージョン | `v2026.10.12.1` | `v2026.10.14.6` |
| コミット | `1b80cbf02b884890abb8939715fbbb258d7428e9` | タグ `v2026.10.14.6` が指すコミット |
| 公開日 | 2026-10-12 | 2026-10-14 |

比較範囲は前回の正式リリース以降の変更全体で、今回の最終的な動作を基準としています。完全版と最小版は同じ固定済みソースからビルドされており、作業ディレクトリの未コミット変更は含みません。

### 出力、バックアップ、編集

- 連番画像出力を追加。保存先、ファイル名の接頭辞、画像形式、フレーム範囲を設定できます。単一画像出力で利用できる既存の形式に対応し、連番の桁数は出力範囲に合わせて調整されます。
- 自動保存設定を追加。保存間隔（分）、バックアップ先、保存数の上限、古いバックアップの上書きを設定できます。既定の保存先は実行ファイルと同じ場所にある `backups` です。手動保存したプロジェクトは上書きしません。クラッシュ時には常にバックアップを試みますが、成功するかどうかはクラッシュの状態や保存先への書き込み可否などに依存します。
- `Ctrl+F` による強制貼り付けで、異なるモーフ間の数値貼り付けに対応。完全版にはタイムラインの強制貼り付けボタンと最近使ったプロジェクトのメニューを追加しました。
- 完全版に、ボーンの移動、XYZ 回転、拡大縮小を個別に編集する 9 チャンネルの自由曲線を追加。再生、フレーム移動、PMM・VMD 拡張による保存に対応します。曲線エディターには、形状を維持するダブルクリックでのキー挿入、制御点の数値入力、傾きとウェイトのドラッグ、キーフレームへの移動、サイドバーの調整を追加しました。
- 固定軸を持つボーンに VMD を読み込んだ際、回転角度や方向がずれる問題を修正。完全版では、親ボーンの非等方スケール下で子ボーンを回転させたときに余分な変形が生じる問題も修正しました。
- アクセサリの削除、切り替え、再読み込み後に、メニュー状態や描画の区切りが不正になる問題を修正。下部パネルの幅をドラッグで変更でき、マウスを離した時点でレイアウトを適用します。一部のコントロールのはみ出し、ちらつき、残像も修正しました。

### プレビューとアンチエイリアス

- 完全版にビューポートの描画倍率を追加。ウィンドウサイズへの自動調整、低解像度プレビュー、スーパーサンプリングに対応し、出力サイズは変更しません。低解像度の画像は最近傍補間で拡大し、ピクセルをそのまま表示します。
- HUD、フレームレート表示、上下のツールバーを、黒帯を含むプレビュー領域全体に配置。シーンの拡大後に表示解像度で描画します。メインウィンドウ、フローティングビュー、設定ウィンドウの再描画で HUD が隠れる問題を修正しました。
- FPS は、描画が完了して正常に送出されたシーンフレームだけを数えるように変更。処理負荷を抑えるための空ループ、同じフレームの再送出、オフライン録画のサンプルは除外します。
- アンチエイリアスメニューに「MMD 標準（MSAA）」「FXAA」「TAA」「SMAA」を用意。SMAA は 1x と T2x のそれぞれに「標準」「斜線・コーナー強化」があり、計 4 種類です。直接クリックすると前回の選択を適用し、初期値は「SMAA 1x（斜線・コーナー強化）」です。項目にカーソルを合わせて右へ移動すると詳細を選択できます。旧プロジェクトの独立した MSAA 段階設定も引き続き読み込めます。
- TAA と SMAA T2x に、カメラ、ボーン、変形後ジオメトリのモーションベクトルによる再投影を追加し、深度と履歴の有効性を確認します。現在の色、履歴の色、深度にはそれぞれ対応するサンプルオフセットを使い、履歴を採用しない場合は現在のオフセットを取り除きます。地面グリッドもモーション取得の対象に含めました。
- 頂点テクスチャ参照に対応する GPU では、TAA と SMAA T2x の 2 フレーム分の変形後座標を GPU 上に保持。取得結果を毎フレーム CPU に読み戻して再転送する処理を省きます。入力が同じ材質では取得結果を共有し、不透明と確認できた標準材質ではカバレッジ取得用の追加描画を省略します。MME とアルファ付きテクスチャではカバレッジ確認を維持し、この GPU 機能に非対応の場合は互換処理を使います。SMAA 1x の動作は変更していません。
- フレーム 0 だけの静止画でも、時刻と姿勢を固定したまま時間方向のサンプルを補います。物理演算を進めたり、余分な画像を出力したりしません。連番画像では履歴を保持し、SMAA T2x は最初の画像だけ 2 回、以後は各画像を 1 回描画します。モーションデータを取得できない場合は、単一フレームのアンチエイリアスに切り替えます。
- アンチエイリアスの有効・無効、方式、前回の SMAA 選択を PMM v2 拡張に保存。プロジェクトを読み込んでも、この PC の既定値は書き換えません。ハードウェア制約による一時的な切り替えでも、プロジェクト本来の選択を上書きしません。
- ビューポート倍率メニューで「シーンを開く」が誤作動する問題、最近使ったプロジェクトのメニューを開く際のちらつき、出力後にビューポートのサイズや DPI 状態が正しく戻らない問題を修正。ビューポートツールは相対移動による連続ドラッグに対応し、マウスキャプチャを失ったときにカーソルを引き戻さなくなりました。

### 影と互換性

- 完全版のセルフシャドウで、MMD 9.32 の光源グリッドの向き、投影変換、モード 1・2 の連続的な深度グラデーション、線形深度サンプリングを復元。動的な影解像度は共通の範囲設定を使い、実際の投影密度に応じてテクスチャを割り当てます。高解像度での範囲も保持し、影の欠けを防ぎます。
- 完全版に「フィルター強化」を追加。既定では無効です。有効にすると、そのフレームの影深度マップに 2 パスの B スプラインフィルターを適用してから、標準材質、MME、新インターフェース対応の MMV に渡します。外部 FX が強化後の結果を読み逃すことや、ホスト側で二重にフィルター処理することを防ぎます。失敗時は、そのフレームの元の深度マップに戻します。
- 完全版で透明ソートを無効にしても、MME 使用時に不透明材質と透明材質を分けていた問題を修正。無効時はモデル本来の材質順で描画します。
- 一部の材質で MME を有効にすると地面影が過度に濃くなる問題を修正。標準描画と MME が同じ影色を参照するようになりました。既存の影の明るさ、アルファブレンドの有効・無効、アルファ値は変更していません。
- 完全版の透明ソート「最高」で、プリコンパイル済みエフェクトをソースコードとして扱い、文字列を追記していた問題を修正。透明レイヤーのデプスピーリングには独立した完全なエフェクトリソースを使い、バイトコード破損による読み込み失敗、透明レイヤーの欠落、クラッシュを防ぎます。
- オリジナル MME による中国語パスや既存エフェクト参照の読み取り専用アクセスの互換性を改善。完全版から MME にモデルパスを渡す際は、システムのコードページで正確に表現できるパスを優先し、同名 EMD の検索を改善しました。
- 完全版には、現在のプログラムに NVIDIA の両面描画互換設定を適用するための任意 DLL `MMSNvidiaAAFix.dll` を同梱。DLL が存在する場合、検出結果に応じてヘルプメニューにインストールまたはアンインストールの項目を表示し、実行時に Windows の管理者権限を要求します。設定の適用・解除は次回の MMS 起動時に反映されます。DLL を削除するとメニュー項目は消えますが、適用済みのドライバー設定は解除されません。利用可能な NVIDIA ドライバーがない場合はインストールを中止し、AMD・Intel にはこの設定を適用しません。
- 新しい設定とメッセージを中国語、日本語、英語に対応させ、日本語・英語の文言、ボタン表記、ウィンドウレイアウトを調整しました。

### 完全版と最小版

| 今回の追加機能・変更 | 完全版 | 最小版 |
| --- | --- | --- |
| 連番画像、定期バックアップ、クラッシュ時のバックアップ試行 | 対応 | 対応 |
| モーフの `Ctrl+F` 強制貼り付け、下部パネルの幅調整 | 対応 | 対応 |
| 固定軸 VMD、地面影の色、アクセサリ状態、出力後のビューポート復元などの共通修正 | 対応 | 対応 |
| 最近使ったプロジェクト、強制貼り付けボタン、ビューポート描画倍率 | 対応 | 非搭載 |
| ボーンの 9 チャンネル自由曲線、非等方スケール修正 | 対応 | ボーン拡大縮小と新しい曲線モジュールは非搭載 |
| 動的シャドウ、影の欠け防止、フィルター強化 | 対応 | 元の影描画方式と範囲クリッピングを維持 |
| AA 方式の選択、TAA、SMAA、NVIDIA 修正 DLL | 対応 | 元の AA 有効・無効設定を維持し、新モジュールは非搭載 |
| 透明ソート「最高」のエフェクト読み込み修正 | 対応 | 透明ソートモジュールは非搭載 |

完全版 ZIP には `MikuMikuDance.exe`、`MMSNvidiaAAFix.dll`、`licenses/` が含まれます。最小版 ZIP にはプログラムとライセンスが含まれます。モデル、ユーザー設定、MME は同梱していません。メイン ZIP に `MMV4MMS.dll` は含まれません。
