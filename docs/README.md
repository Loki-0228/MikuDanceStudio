# 文档导航

查看原版 MMD 没有的近期新增能力，请从[近期新增特性](ADDITIONAL_FEATURES.md)开始。该文按使用入口说明默认值、项目保存和兼容限制，区分新增能力、默认值调整、兼容修复与实验，并给出判断改动属于 MMS 增强还是兼容修复的分类规则。

## 功能使用与格式

| 主题 | 文档 |
| --- | --- |
| 正式 D3D9/MME GPU 蒙皮 | [设备选择与兼容范围](GPU_SKINNING_2026-09-25.md)、[自动实测与当前设备显示](SKINNING_AUTOSELECT_2026-09-26.md) |
| 透明排序上限、美影默认值 | [五档上限与分层边界](TRANSPARENT_SORT_FIVE_TIERS_2026-09-27.md)、[有界排序与限制](TRANSPARENT_LAYER_ORDERING_2026-09-24.md)、[开关时期的默认行为](AUTO_TRANSPARENT_SORTING_2026-09-25.md) |
| PMM 项目设置扩展 | [格式与兼容规则](PMM_V2_EXTENSIONS.md)、[验证记录](PMM_V2_EXTENSIONS_VALIDATION.md)、[单一档位字段与旧记录处理](TRANSPARENT_SORT_FIVE_TIERS_2026-09-27.md) |
| 配件操作按钮标识 | [完整 .x 角标](ACCESSORY_HUD_2026-09-26.md) |
| MME 兼容与 EMM 保存 | [MME 兼容说明](MME_COMPATIBILITY.md)、[EMM 自动保存修复](EMM_AUTOSAVE_2026-09-25.md) |
| 原生曲线编辑器与模型表情曲线 | [关键帧与曲线编辑](ANIMATION_CURVE_EDITOR.md) |
| 整个工程的帧率修正 | [修正范围、帧合并与报告](FRAME_RATE_CORRECTION.md) |
| MMV 原生网格插件 | [安装与三语分配窗口](MMV_INSTALL.md)、[FX 接口](MMV_FX.md)、[实现与验收范围](MMV_IMPLEMENTATION.md) |
| MMV 分配窗口全局撤销 | [待实现范围与验收](MMV_EDITOR_UNDO_PLAN.md) |

## 开发、验收与历史记录

| 主题 | 文档 |
| --- | --- |
| 行为核对与问题记录 | [验证方法](VERIFY_METHOD.md)、[用户测试修复记录](USER_TEST_FIXES_2026-09.md) |
| 骨骼反复旋转与视频导出范围 | [原因、修复及两档验收](ROTATION_EXPORT_FIXES_20261011.md) |
| 性能采样 | [帧基准说明](FRAME_BENCHMARK.md) |
| 透明排序主分支集成 | [合并、后台回归与双 build 对照](CLOUD_SORT_MAIN_INTEGRATION_2026-09-27.md) |
| 先前的六档排序上限格式 | [六档上限、双字段迁移与验证（历史）](TRANSPARENT_SORT_LIMIT_PMM_2026-09-27.md) |
| 本地目录、构建与历史版本 | [工作区清单](LOCAL_WORKSPACE_LAYOUT.md) |
| D3D12 独立预览 | [实验路径与缺失功能](D3D12_MIGRATION_2026-09-20.md) |
| 已停用的全量透明排序 | [实验及性能验收记录](TRANSPARENCY_GLOBAL_SORT_2026-09-25.md) |
| 原版缺陷案例 | [时间轴隐藏表情安全处理（已验收）](HIDDEN_MORPH_PANEL_SAFETY.md) |

带日期的记录描述当时的实现与验收条件。阅读历史实验前先确认文首状态，当前新增特性以[汇总文档](ADDITIONAL_FEATURES.md)注明的 Git 范围为准。

原生骨骼缩放的坐标系、整体按钮、动画记录和 PMM/VMD 兼容规则见[骨骼缩放](BONE_SCALING.md)。
