# 本地工作区布局

原生媒体与透明 MOV 的历史快照和构建记录位于 `.local/releases/` 与 `.local/artifacts/`。模型模式光照前后验证位于 `.local/artifacts/model-view-light/20261007/`，详见[光照修复记录](MODEL_VIEW_LIGHTING_20261007.md)。这些历史资料不随应用发布。当前 MMS 发布目录为 `build/Release/`。

阴影、录制预览与 MME 显示修复的基线工作树为 `.local/worktrees/shadow-preview-before/`（`d1e88fb`），独立基线构建为 `.local/builds/shadow-preview-before/`。它们作为本次验收参考保留。更早的构建快照位于 `.local/releases/shadow-preview-before/`，开发构建为 `.local/builds/mme-926-profile/`。原始采样、失败记录、版本与哈希清单位于 `.local/artifacts/shadow-preview-20261010/01/`，见[验收说明](SHADOW_PREVIEW_MME_20261011.md)。这些目录不进入发布包。

`v2026.10.11.1` 使用独立发布工作树 `.local/worktrees/shadow-preview-release/` 与构建目录 `.local/builds/shadow-preview-release/`，关闭诊断和阶段计时。发布 EXE 与来源清单存放于 `.local/releases/v2026.10.11.1/`；上传、公开下载核验和旧版本归档记录位于 `.local/artifacts/github-release/v2026.10.11.1/`。替换正式目录前的程序备份位于 `.local/archives/shadow-preview-release/`。工作树作为已验收的发布参考保留。

空场景默认着色器后台编译的源码快照、前后启动对照和隐藏窗口测试记录位于 `.local/artifacts/deferred-effect-20261011/`。独立构建为 `.local/builds/deferred-effect-x64-20261011/`，最终验证包为 `.local/releases/deferred-effect-20261011-integrated/`，性能对照包为 `.local/releases/deferred-effect-20261011/`。这些版本作为验收参考保留，不替换正式发布包。见[启动验收记录](STARTUP_DEFERRED_EFFECT_20261011.md)。

`v2026.10.11.2` 已正式发布并更新 `build/Release/`。独立源码与发布记录位于 `.local/artifacts/github-release/v2026.10.11.2/`，构建在 `.local/builds/startup-release-20261011/`，发布文件在 `.local/releases/v2026.10.11.2/`。正式程序的六次启动对照位于 `.local/artifacts/release-startup-20261011/`，替换前程序备份在 `.local/archives/startup-release-20261011/`。这些目录保留为验收参考。此前 `v2026.10.11.1` 未完成公开发布，仍为空草稿。旧正式版因删除审批未通过而保留。见[本次发布及启动测量](RELEASE_20261011_2.md)。
