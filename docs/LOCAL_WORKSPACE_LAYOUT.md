# 本地工作区布局

第一批审查修复的前后工作树为两仓各自 `.local/worktrees/repair-b1-before/` 和 `.local/worktrees/repair-b1-integration/`，构建为 `.local/builds/repair-b1-before/` 和 `.local/builds/repair-b1-after/`，作为验收参考保留。实现工作树 `repair-b1-vmd`、`repair-b1-physics-input`、`repair-b1-render-contract` 合入后记为冗余保留，不删除。证据集中在 MMS 的 `.local/artifacts/repair-b1-20261011/01/`，详见[第一批验收](REPAIR_BATCH1_20261011.md)。本批没有替换正式 `build/Release/`。

当前 `build/Release/MikuMikuDance.exe` 为本地 CPU 忙等待修复版（源码 `6ca7c98`），尚未发布到 GitHub。前后工作树为 `.local/worktrees/cpu-wait-before/` 和 `.local/worktrees/cpu-wait-after/`，构建为 `.local/builds/cpu-wait-before/` 和 `.local/builds/cpu-wait-after/`，证据在 `.local/artifacts/cpu-wait-fix-20261011/01/`。新版快照位于 `.local/releases/cpu-wait-fix-20261011/`，被替换的完整运行目录位于 `.local/archives/cpu-wait-fix-20261011/Release/`。这些目录作为验收参考保留，见[修复记录](CPU_WAIT_FIX_20261011.md)。

`v2026.10.11.3` 是此前正式发布版本。固定源码、构建和发布验证位于 `.local/artifacts/github-release/v2026.10.11.3/`，独立构建为 `.local/builds/names-release-20261011/`，发布包位于 `.local/releases/v2026.10.11.3/`。旧程序备份在 `.local/archives/japanese-name-gaps-20261011/`。这些目录保留为验收参考，见[发布记录](RELEASE_20261011_3.md)。

日区名称边界修复的基线工作树为 `.local/worktrees/japanese-name-before/`（`b3cfb98`，保留同一份新增回归测试及行为不变的测试入口提取）。前后构建分别位于 `.local/builds/japanese-name-before/` 与 `.local/builds/japanese-name-after/`。测试、补丁与版本清单位于 `.local/artifacts/japanese-name-gaps-20261011/01/`；替换前的正式程序备份位于 `.local/archives/japanese-name-gaps-20261011/`，新程序快照位于 `.local/releases/v2026.10.11.3/`。测试与备份作为验收参考保留，不随安装包上传。详见[日区名称修复记录](JAPANESE_NAME_GAPS_20261011.md)。

原生媒体与透明 MOV 的历史快照和构建记录位于 `.local/releases/` 与 `.local/artifacts/`。模型模式光照前后验证位于 `.local/artifacts/model-view-light/20261007/`，详见[光照修复记录](MODEL_VIEW_LIGHTING_20261007.md)。这些历史资料不随应用发布。当前 MMS 发布目录为 `build/Release/`。

阴影、录制预览与 MME 显示修复的基线工作树为 `.local/worktrees/shadow-preview-before/`（`d1e88fb`），独立基线构建为 `.local/builds/shadow-preview-before/`。它们作为本次验收参考保留。更早的构建快照位于 `.local/releases/shadow-preview-before/`，开发构建为 `.local/builds/mme-926-profile/`。原始采样、失败记录、版本与哈希清单位于 `.local/artifacts/shadow-preview-20261010/01/`，见[验收说明](SHADOW_PREVIEW_MME_20261011.md)。这些目录不进入发布包。

`v2026.10.11.1` 使用独立发布工作树 `.local/worktrees/shadow-preview-release/` 与构建目录 `.local/builds/shadow-preview-release/`，关闭诊断和阶段计时。发布 EXE 与来源清单存放于 `.local/releases/v2026.10.11.1/`；上传、公开下载核验和旧版本归档记录位于 `.local/artifacts/github-release/v2026.10.11.1/`。替换正式目录前的程序备份位于 `.local/archives/shadow-preview-release/`。工作树作为已验收的发布参考保留。

空场景默认着色器后台编译的源码快照、前后启动对照和隐藏窗口测试记录位于 `.local/artifacts/deferred-effect-20261011/`。独立构建为 `.local/builds/deferred-effect-x64-20261011/`，最终验证包为 `.local/releases/deferred-effect-20261011-integrated/`，性能对照包为 `.local/releases/deferred-effect-20261011/`。这些版本作为验收参考保留，不替换正式发布包。见[启动验收记录](STARTUP_DEFERRED_EFFECT_20261011.md)。

`v2026.10.11.2` 已正式发布并更新 `build/Release/`。独立源码与发布记录位于 `.local/artifacts/github-release/v2026.10.11.2/`，构建在 `.local/builds/startup-release-20261011/`，发布文件在 `.local/releases/v2026.10.11.2/`。正式程序的六次启动对照位于 `.local/artifacts/release-startup-20261011/`，替换前程序备份在 `.local/archives/startup-release-20261011/`。这些目录保留为验收参考。此前 `v2026.10.11.1` 未完成公开发布，仍为空草稿。旧正式版因删除审批未通过而保留。见[本次发布及启动测量](RELEASE_20261011_2.md)。
