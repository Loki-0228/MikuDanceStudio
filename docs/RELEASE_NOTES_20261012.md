# v2026.10.12.1 release notes / 发布日志 / リリースノート

[English](#english) | [中文](#中文) | [日本語](#日本語)

## English

`v2026.10.12.1` · 2026-10-12 · Windows x64. Changes since `v2026.10.11.7`.

### Fixes

The full and minimal builds share these fixes:

- Corrected the vertical bone rotation handle's axis to match original MMD. From a side view, dragging the handle now rotates around the expected axis instead of mixing unintended X and Y rotation.
- Fixed textured PMD models turning gray or white with bone **Select** enabled when their shared Toon file table uses a different order. Shared textures and their colors now follow the referenced filename. The fix also covers fallback to shared Toon textures. The full build uses the same mapping for Toon data passed to MMV.
- Fixed repeated clicks getting stuck on one bone when rotation-only and translation-enabled bones overlap. Each click advances to the next eligible bone in model order and wraps to the first. This works with rapid clicks in the main and floating viewports. Shift selection and existing visibility and physics filters are preserved.

### Package differences

| Package | Included functionality |
| --- | --- |
| `full` | MMS additions, including bone scaling, the new curve editor, shadow reconstruction, optional transparency sorting, DX12, modern media, the MMV host, and additional baking. |
| `minimal` | Original MMD functionality and shared bug fixes, including original Bezier interpolation, rigid-body physics, WAV audio, AVI, and the native MME interface. The additions listed above are omitted. |

Both builds use the same source branch. The distribution layout remains `MikuMikuDance.exe` plus `licenses/`, without models, effects, user settings, or MME and MMV plugins. `MMV4MMS.dll` remains under separate testing. See the [README package comparison](../README.md#downloads-and-package-differences).

## 中文

`v2026.10.12.1` · 2026-10-12 · Windows x64。记录 `v2026.10.11.7` 之后的变化。

### 修复内容

完整包与最小包共用以下修复：

- 修正骨骼竖向旋转柄的轴向，使其与原版 MMD 一致。从模型侧面拖动时，按预期轴旋转，不再混入非预期的 X、Y 轴旋转。
- 修复共享 Toon 文件表顺序不同的 PMD 在开启骨骼「选择」后丢失贴图颜色、呈灰白色的问题。共享贴图及其颜色按引用的文件名匹配，也覆盖回退到共享 Toon 的情况。完整包提交给 MMV 的 Toon 数据同步采用此规则。
- 修复旋转骨骼与可移动骨骼重叠时，重复点击始终选中同一骨骼的问题。现在按模型内顺序选择下一个可选骨骼，末尾返回第一个。主视口和独立视口均支持快速连续点击，保留 Shift 选择、可见性和物理锁定规则。

### 完整包与最小包

| 包 | 功能范围 |
| --- | --- |
| `full` 完整包 | 保留骨骼缩放、新曲线编辑器、阴影重构、可选透明排序、DX12、现代媒体、MMV 宿主和新增烘焙等 MMS 功能。 |
| `minimal` 最小包 | 保留 MMD 原有功能与共享缺陷修复，包括原有 Bezier 插值、刚体物理、WAV、AVI 和原生 MME 接口，不编入上述新增模块。 |

两档在同一源码分支维护。发布目录仍仅包含 `MikuMikuDance.exe` 与 `licenses/`，不附带模型、效果、用户配置、MME 或 MMV 插件。`MMV4MMS.dll` 仍单独测试。详细对照见 [README 的包说明](../README.md#下载与版本区别)。

## 日本語

`v2026.10.12.1` · 2026-10-12 · Windows x64。`v2026.10.11.7` 以降の変更です。

### 修正内容

完全版と最小版に共通する修正です。

- ボーン縦回転ハンドルの回転軸を、オリジナル MMD と一致させました。モデルを横から見てドラッグしたとき、意図しない X・Y 軸の回転が混ざらず、想定した軸で回転します。
- 共有トゥーンのファイル表が通常と異なる順序の PMD で、ボーンの「選択」を有効にするとテクスチャの色が失われ、灰白色になる問題を修正しました。共有テクスチャとその色を、参照するファイル名に従って選びます。共有トゥーンへのフォールバックも対象です。完全版から MMV に渡すトゥーンデータにも同じ規則を適用します。
- 回転のみ可能なボーンと移動可能なボーンが重なったとき、繰り返しクリックしても同じボーンしか選べない問題を修正しました。モデル内の順序で次の選択可能なボーンに切り替わり、最後まで進むと最初に戻ります。メイン画面と別ウィンドウのどちらでも素早い連続クリックに対応し、Shift 選択、表示状態、物理による選択制限を維持します。

### 完全版と最小版

| パッケージ | 機能範囲 |
| --- | --- |
| 完全版 `full` | ボーンスケール、新しい曲線エディタ、シャドウ処理の再構成、任意の透明ソート、DX12、各種メディア形式、MMV ホスト、追加のベイク機能を含みます。 |
| 最小版 `minimal` | 従来の Bezier 補間、剛体物理、WAV、AVI、ネイティブ MME インターフェースなど、MMD 本来の機能と共通の不具合修正を含みます。上記の追加機能は含みません。 |

両方を同じソースブランチで保守します。配布内容は引き続き `MikuMikuDance.exe` と `licenses/` のみです。モデル、エフェクト、ユーザー設定、MME・MMV プラグインは含みません。`MMV4MMS.dll` は別途テスト中です。詳細は [README のパッケージ比較](../README.md#ダウンロードとパッケージの違い)を参照してください。
