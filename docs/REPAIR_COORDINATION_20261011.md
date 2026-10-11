# MMS 与 MMV4MMS 协同修复方案

按本方案依次处理[合并审查记录](D:/MMD-Next/MMS/docs/REVIEW_MERGED_20261011.md)中的问题。先修复内存安全和输入输出错误，再接入通用 mesh 变换，最后比较性能及体积。第一批 A、B、C 已完成实现和专项验收，详见[第一批修复验收记录](REPAIR_BATCH1_20261011.md)。遗留项已验收提交，第二批 D/E/F 已完成验收并提交，详见[第二批记录](REPAIR_BATCH2_20261011.md)。第三批及以后尚未启动。

采用主进程加最多 3 个修复子进程。主进程固定契约、管理公共文件、串行合并与验收。子进程只修改任务允许的文件。并行开发并不代表可以并行修改同一文件、同时更新公共接口或同时测量性能。

本批用户后续明确要求直接使用 ChatGPT 子进程，因此第一批 A/B/C 和第二批 D/E/F 由内部子进程协同实现，主进程统一整合和验收。以下 DSH 条款保留为默认约定，不适用于这次明确覆盖。

当时的默认分工约定为：独立单模块修改交给 DSH，主进程负责跨模块整合。派发前读取已安装的 dsh-delegate 技能并核验宿主状态；每次显式传绝对 workspace，核对返回 cwd。宿主不可用时报告原因，不启动第二个宿主或用其他进程冒充。子进程的最终改动和交叉复核结果见对应批次验收记录。

先由主进程完成以下准备。

1. 记录两个仓库的提交与工作树状态。原审查基线为 MMS `145cbda`、MMV4MMS `74edd1d`。整理期间 MMS 新增 OpenMP 修复 `6ca7c98`，可在核对交接后作为开发基线。实施前若有新提交，重新检查对应问题，避免重复修复。
2. 从固定提交创建每包独立 Git 工作树，不复制当前未提交修改。预定路径分别为 `D:/MMD-Next/MMS/.local/worktrees/<包名>/` 和 `D:/MMD-Next/MMV4MMS/.local/worktrees/<包名>/`。第一批实际工作树位置见验收记录。
3. 每包使用所属仓库内独立的 `.local/builds/` 与 `.local/artifacts/` 目录。实验依赖使用独立缓存或唯一包身份，不改写正式依赖缓存。不要复用另一个工作树的 CMake 缓存。
4. 登记下列文件占用与公共文件所有权。任务需要越界修改时，将所需改动和理由交给主进程；主进程先调整分工，再继续相关修改。
5. 记录基础编译环境和实际加载的运行库。OpenMP 修复已提交为 `6ca7c98`，无需再创建同目标修复包；核对原会话的完整验收及文件交接。每项前后对照必须采用相同等待策略和构建条件，不能将 OpenMP 的收益算到其他任务上。

下列外部改动已核对：OpenMP 工作提交后，第一批基于 `7a531c0` 开发；主进程仅在 CMake 增加本批测试注册。OpenMP 的五个文件已在 `6ca7c98` 提交；原技术路线文档仍是既有删除状态，不能恢复或清理。

- [CMakeLists.txt](D:/MMD-Next/MMS/CMakeLists.txt)。
- [geometry_workers.hpp](D:/MMD-Next/MMS/include/mikudancestudio/geometry_workers.hpp)。
- [geometry_workers.cpp](D:/MMD-Next/MMS/src/model/geometry_workers.cpp)。
- [geometry_workers_tests.cpp](D:/MMD-Next/MMS/tests/geometry_workers_tests.cpp)。
- [window_init.cpp](D:/MMD-Next/MMS/src/window/window_init.cpp)。
- `D:/MMD-Next/MMS/MMS技术路线与插件接口讨论.md`（已有删除状态）。

根 CMake、测试注册、两侧公开 SDK 与 ABI 版本、公共模型结构、持久化格式、生成的 res、正式发布目录和跨仓库版本清单由主进程独占。`D:/MMD-Next/MMS/src/render/device_reset.cpp` 与 `D:/MMD-Next/MMS/src/app/shutdown_cleanup.cpp` 的接入也由主进程维护。OpenMP 会话尚未交接时，主进程同样不争改其 CMake 和启动文件。

按下表分批派发。同一批允许并行开发，合入与 GPU 验收始终串行。批次是保守安排；只有依赖已验收且文件所有权已交接的任务才可提前。

| 批次 | 子进程一 | 子进程二 | 子进程三 | 主进程与进入条件 |
| --- | --- | --- | --- | --- |
| 第一批 | A：VMD 读入安全 | B：MMV 输入与材质契约 | C：PhysicsInput 正确性 | 固定现有 ABI；B 中两项分提交、同 owner。 |
| 第二批 | D：设备丢失等待 | E：烘焙属性与 v3 清理 | F：仿真状态清理 | 接收第一批；逐包建立前后对照。 |
| 第三批 | G：计算与显示设备分离 | K：Alembic 局部优化 | 可安排只读验收，不新增交叉修改 | 主进程 H 固定构建、清单与运行库条件；G 接收 B、C，K 接收 E。 |
| 第四批 | J：加载与交付条件 | L：透明资源复用 | M：Imath 无查表实验 | J 接收 G、F、H；L 接收 D；M 接收 H、E、K。主进程可以设计 N，但此时不改公共接口。 |
| 第五批 | N1：MMS 通用结果接入 | N2：MMV 求值适配 | 独立只读评审 | 主进程先冻结 N 契约，再分别派发 N1、N2；两者不各自修改 SDK。 |
| 第六批 | O：快照与隐藏编辑器优化 | P：空闲求值抑制 | 独立只读评审 | N1、N2 集成验收后，先固定变更版本和时间语义再并行。 |
| 第七批 | Q：同帧 GPU 蒙皮批处理 | 独立只读评审 | 不启动竞争 GPU 的任务 | P、L 已验收，主进程统一 GPU ticket 和 Reset 接入。 |
| 第八批 | R：LTO 与 Gw 对照 | 独立只读评审 | 不启动竞争资源的采样 | 功能、运行库、OpenMP及依赖方案均稳定；每次只改变一组编译因素。 |

K、L、M 是优化任务，可以后移。通用 mesh 接入不以这些优化取得收益为前提；提前派发仍须检查实际文件所有权，并停止竞争资源的性能采样。

各包的允许修改清单如下。不存在的专属 helper、测试或实验 profile 是预定新文件，其名称不代表当前已有实现。清单外文件先交主进程处理，不因文件位于同目录就自动扩大权限。

A：VMD 读入安全。负责人：子进程。没有其他修复包的硬性前置。

- [vmd_load.cpp](D:/MMD-Next/MMS/src/model/vmd_load.cpp)。
- [vmd_read_guard.hpp](D:/MMD-Next/MMS/src/model/vmd_read_guard.hpp)（第一批新增私有读取器）。
- [key_registrars.cpp](D:/MMD-Next/MMS/src/model/key_registrars.cpp)（第一批交叉复核追加，仅限定 IK 名称比较宽度）。
- [vmd_load_safety_tests.cpp](D:/MMD-Next/MMS/tests/vmd_load_safety_tests.cpp)。

验收及边界：真实读入回归；容量、长度、短读、错误回滚与正常VMD均通过。新增测试文件由主进程注册。

B：MMV 输入与材质契约。负责人：子进程。没有其他修复包的硬性前置。

- [runtime_scene.inl](D:/MMD-Next/MMV4MMS/src/mms/runtime_scene.inl)。
- [scene_mesh_cache.hpp](D:/MMD-Next/MMV4MMS/src/mms/scene_mesh_cache.hpp)。
- [runtime.cpp](D:/MMD-Next/MMV4MMS/src/mmv/runtime.cpp)。
- [mms_plugin_tests.cpp](D:/MMD-Next/MMV4MMS/tests/mms_plugin_tests.cpp)。
- [mms_scene_mesh_cache_tests.cpp](D:/MMD-Next/MMV4MMS/tests/mms_scene_mesh_cache_tests.cpp)。

验收及边界：F02与F07分两个提交、同一所有者。覆盖需求随重载改变，使用已有sourceMaterial；不得擅改公开ABI。宿主适配若必需，由主进程先补充契约和改动范围。

C：PhysicsInput 正确性。负责人：子进程。没有其他修复包的硬性前置。

- [physics_input.inl](D:/MMD-Next/MMS/src/mmv4mms/physics_input.inl)。
- `D:/MMD-Next/MMS/tests/mmv4mms_physics_input_tests.cpp`（预定新增）。

验收及边界：最小修复在回调内更新动态输入和可变结构；先不向ModelRecord加epoch，也不广泛修改编辑器。后续性能包再引入有证据的缓存。

D：设备丢失等待。负责人：子进程。没有其他修复包的硬性前置。

- [frame_driver.cpp](D:/MMD-Next/MMS/src/app/frame_driver.cpp)。
- [record_readback.cpp](D:/MMD-Next/MMS/src/app/record_readback.cpp)。
- `D:/MMD-Next/MMS/src/app/device_lost_wait.hpp`（预定新增）。
- `D:/MMD-Next/MMS/tests/device_lost_wait_tests.cpp`（预定新增）。

验收及边界：消息、退出和失效表面覆盖。新私有helper可新增。device_reset与shutdown接线由主进程独占。

E：烘焙属性与 v3 清理。负责人：子进程。没有其他修复包的硬性前置。

- [native_mesh.cpp](D:/MMD-Next/MMS/src/render/native_mesh.cpp)。
- [alembic_export.cpp](D:/MMD-Next/MMS/src/io/alembic_export.cpp)。
- [mesh_bake_cache.cpp](D:/MMD-Next/MMS/src/io/mesh_bake_cache.cpp)。
- [alembic_export_tests.cpp](D:/MMD-Next/MMS/tests/alembic_export_tests.cpp)。
- [mesh_bake_cache_tests.cpp](D:/MMD-Next/MMS/tests/mesh_bake_cache_tests.cpp)。
- `D:/MMD-Next/MMS/tests/native_bake_attributes_tests.cpp`（预定新增）。

验收及边界：F05与F06分提交、同一所有者；优先既有sourceVertices映射。公共格式改变须先交主进程固定。

F：仿真状态清理。负责人：子进程。没有其他修复包的硬性前置。

- [deformer_engine.cpp](D:/MMD-Next/MMV4MMS/src/mmv/deformer_engine.cpp)。
- [mmv_deformer_engine_tests.cpp](D:/MMD-Next/MMV4MMS/tests/mmv_deformer_engine_tests.cpp)。

验收及边界：覆盖scene/object reset、借出lease、设备丢失与禁用/再启用；不改公开ABI。

G：计算与显示设备分离。负责人：主进程先定契约，子进程实现内部策略。前置：B：MMV 输入与材质契约；C：PhysicsInput 正确性。

- [menu.inl](D:/MMD-Next/MMS/src/mmv4mms/menu.inl)。
- [mmv4mms_menu_tests.cpp](D:/MMD-Next/MMS/tests/mmv4mms_menu_tests.cpp)。
- [runtime.cpp](D:/MMD-Next/MMV4MMS/src/mmv/runtime.cpp)。
- [runtime.hpp](D:/MMD-Next/MMV4MMS/src/mmv/runtime.hpp)。
- [compat.cpp](D:/MMD-Next/MMV4MMS/src/mmv/compat_dx9/compat.cpp)。
- [mmv_runtime_tests.cpp](D:/MMD-Next/MMV4MMS/tests/mmv_runtime_tests.cpp)。

验收及边界：跨仓库分别提交；冻结显示设备借用规则，不靠补一行callback完成；不能与Runtime或menu所有者并行。

H：构建与运行库基线及清单。负责人：主进程。没有其他修复包的硬性前置。公共构建接入仍须等 OpenMP 文件交接。

- [x64](D:/MMD-Next/MMS/profiles/x64)。
- [conanfile.py](D:/MMD-Next/MMS/conanfile.py)。
- `D:/MMD-Next/MMS/CMakePresets.json`（预定新增）。
- [mmd_manifest_x64.xml](D:/MMD-Next/MMS/res/mmd_manifest_x64.xml)。
- [build.ps1](D:/MMD-Next/MMV4MMS/scripts/build.ps1)。
- [mmv_distribution.cmake](D:/MMD-Next/MMV4MMS/cmake/mmv_distribution.cmake)。

验收及边界：保留当前兼容性；OpenMP交接前不修改公共CMake。分别固定开发构建条件和部署运行库；生成res与最终PE由主进程串行生成、检查。

J：加载与交付条件。负责人：主进程协调，单一加载器任务。前置：G：计算与显示设备分离；F：仿真状态清理；H：构建与运行库基线及清单。

- [menu.inl](D:/MMD-Next/MMS/src/mmv4mms/menu.inl)。
- [deformer_engine.cpp](D:/MMD-Next/MMV4MMS/src/mmv/deformer_engine.cpp)。
- [mmv_bullet3_effect.cpp](D:/MMD-Next/MMV4MMS/plugins/bullet3/integration/mmv_bullet3_effect.cpp)。
- [mmv_distribution.cmake](D:/MMD-Next/MMV4MMS/cmake/mmv_distribution.cmake)。

验收及边界：承接menu、engine、distribution文件所有权后实施。目标系统或分发策略未定时，仅完成依赖检测和方案，不擅自放弃Win7。

K：Alembic 局部处理。负责人：子进程。前置：E：烘焙属性与 v3 清理。

- [alembic_export.cpp](D:/MMD-Next/MMS/src/io/alembic_export.cpp)。
- [alembic_export_tests.cpp](D:/MMD-Next/MMS/tests/alembic_export_tests.cpp)。

验收及边界：保留所有输入采样和求值；逐字段比较输出与导出耗时；不同提交分别量化法线和复制优化。

L：透明资源复用。负责人：子进程。前置：D：设备丢失等待。

- [model_renderers.cpp](D:/MMD-Next/MMS/src/render/model_renderers.cpp)。
- [transparent_peeling.cpp](D:/MMD-Next/MMS/src/render/transparent_peeling.cpp)。
- [transparent_peeling.hpp](D:/MMD-Next/MMS/include/mikudancestudio/transparent_peeling.hpp)。
- `D:/MMD-Next/MMS/tests/transparent_resource_pool_tests.cpp`（预定新增）。

验收及边界：排序、剥离层数、画质不变；resize、Reset、场景切换、退出释放；主进程串行接入device_reset及shutdown。

M：Imath 无查表实验。负责人：子进程。前置：H：构建与运行库基线及清单；E：烘焙属性与 v3 清理；K：Alembic 局部处理。

- [conanfile.py](D:/MMD-Next/MMS/conanfile.py)。
- `D:/MMD-Next/MMS/profiles/imath-no-table`（预定新增）。
- `D:/MMD-Next/MMS/tests/imath_half_compatibility_tests.cpp`（预定新增）。

验收及边界：使用独立依赖缓存与包身份；检验表符号消失及净尺寸。65536种half分类核对，NaN规则单列；比较Alembic正确性与速度，不直接替换正式依赖。

N：通用变形与仿真契约。负责人：主进程。前置：B：MMV 输入与材质契约；C：PhysicsInput 正确性；E：烘焙属性与 v3 清理；F：仿真状态清理；G：计算与显示设备分离；J：加载与交付条件。

- 两侧 SDK、公共生命周期与构建接入由主进程统一形成改动；子进程不自行增加公开字段。

验收及边界：设计可先并行，生产接口改动须等前置合入。独占两侧SDK、ABI、菜单持久化、device_reset、shutdown、公共构建；确定原始、未蒙皮和posed阶段、mesh/bone顺序、时间及所有权。

N1：MMS 通用结果接入。负责人：子进程。前置：N：通用变形与仿真契约；D：设备丢失等待。

- [host.cpp](D:/MMD-Next/MMS/src/mmv4mms/host.cpp)。
- [menu.inl](D:/MMD-Next/MMS/src/mmv4mms/menu.inl)。
- [renderer.inl](D:/MMD-Next/MMS/src/mmv4mms/renderer.inl)。
- [physics_input.inl](D:/MMD-Next/MMS/src/mmv4mms/physics_input.inl)。
- [frame_scene.cpp](D:/MMD-Next/MMS/src/render/frame_scene.cpp)。
- [native_mesh.cpp](D:/MMD-Next/MMS/src/render/native_mesh.cpp)。
- [mmv4mms_menu_tests.cpp](D:/MMD-Next/MMS/tests/mmv4mms_menu_tests.cpp)。
- `D:/MMD-Next/MMS/tests/mmv4mms_mesh_pipeline_tests.cpp`（预定新增）。

验收及边界：同一变形模块结果能交原生、MME和MMV；每帧只求值一次；渲染切换不额外推进仿真，bone与mesh组合不重复蒙皮。只按冻结SDK实现。

N2：MMV 求值适配。负责人：子进程。前置：N：通用变形与仿真契约。

- [runtime.cpp](D:/MMD-Next/MMV4MMS/src/mmv/runtime.cpp)。
- [runtime.hpp](D:/MMD-Next/MMV4MMS/src/mmv/runtime.hpp)。
- [runtime_deformer.inl](D:/MMD-Next/MMV4MMS/src/mmv/runtime_deformer.inl)。
- [deformer_snapshot.cpp](D:/MMD-Next/MMV4MMS/src/mmv/deformer_snapshot.cpp)。
- [deformer_engine.cpp](D:/MMD-Next/MMV4MMS/src/mmv/deformer_engine.cpp)。
- [runtime_scene.inl](D:/MMD-Next/MMV4MMS/src/mms/runtime_scene.inl)。
- [mmv_runtime_tests.cpp](D:/MMD-Next/MMV4MMS/tests/mmv_runtime_tests.cpp)。
- [mmv_deformer_engine_tests.cpp](D:/MMD-Next/MMV4MMS/tests/mmv_deformer_engine_tests.cpp)。
- [mmv_deformer_snapshot_tests.cpp](D:/MMD-Next/MMV4MMS/tests/mmv_deformer_snapshot_tests.cpp)。
- [mms_plugin_tests.cpp](D:/MMD-Next/MMV4MMS/tests/mms_plugin_tests.cpp)。

验收及边界：与N1按冻结SDK并行；内部快照、事务和状态不能绑到某个显示后端；接口修订先交主进程，相关双方暂停依赖修改。

O：快照与隐藏编辑器。负责人：子进程。前置：N1：MMS 通用结果接入；N2：MMV 求值适配。

- [host.cpp](D:/MMD-Next/MMS/src/mmv4mms/host.cpp)。
- [renderer.inl](D:/MMD-Next/MMS/src/mmv4mms/renderer.inl)。
- [physics_input.inl](D:/MMD-Next/MMS/src/mmv4mms/physics_input.inl)。
- [runtime.cpp](D:/MMD-Next/MMV4MMS/src/mmv/runtime.cpp)。
- [runtime.hpp](D:/MMD-Next/MMV4MMS/src/mmv/runtime.hpp)。
- [editor.cpp](D:/MMD-Next/MMV4MMS/src/mmv/editor.cpp)。
- [frame_input_gate.hpp](D:/MMD-Next/MMV4MMS/src/mmv/frame_input_gate.hpp)。

验收及边界：版本缓存不漏表情、同数量参数修改及隐藏消费者。计数元数据复制、约束查找、纹理转换，并比较内存与帧时间。

P：空闲求值抑制。负责人：子进程。前置：N1：MMS 通用结果接入；N2：MMV 求值适配；D：设备丢失等待。

- [frame_driver.cpp](D:/MMD-Next/MMS/src/app/frame_driver.cpp)。
- [frame_scene.cpp](D:/MMD-Next/MMS/src/render/frame_scene.cpp)。
- [physics_frame.cpp](D:/MMD-Next/MMS/src/physics/physics_frame.cpp)。
- [physics_scale.cpp](D:/MMD-Next/MMS/src/physics/physics_scale.cpp)。
- [bone_scale.cpp](D:/MMD-Next/MMS/src/model/bone_scale.cpp)。

验收及边界：不跳有状态仿真、程序化mesh、表情、导出预热或采样；与GPU批处理串行。

Q：同帧 GPU 蒙皮批处理。负责人：子进程。前置：P：空闲求值抑制；L：透明资源复用。

- [skinning_backend.cpp](D:/MMD-Next/MMS/src/model/skinning_backend.cpp)。
- [model_skinning.cpp](D:/MMD-Next/MMS/src/model/model_skinning.cpp)。
- [skinning_calibration_cache.hpp](D:/MMD-Next/MMS/src/model/skinning_calibration_cache.hpp)。
- [d3d12_skinning.cpp](D:/MMD-Next/MMS/src/render/d3d12_skinning.cpp)。
- [frame_scene.cpp](D:/MMD-Next/MMS/src/render/frame_scene.cpp)。
- [skinning_backend.hpp](D:/MMD-Next/MMS/include/mikudancestudio/skinning_backend.hpp)。
- [d3d12_skinning.hpp](D:/MMD-Next/MMS/include/mikudancestudio/d3d12_skinning.hpp)。
- [skinning_backend_tests.cpp](D:/MMD-Next/MMS/tests/skinning_backend_tests.cpp)。
- [d3d12_skinning_tests.cpp](D:/MMD-Next/MMS/tests/d3d12_skinning_tests.cpp)。

验收及边界：同帧结果、取消与Reset、Auto校准缓存版本均正确；分别测提交、GPU、等待、回读。

R：LTO 与 Gw 实验。负责人：子进程。前置：H：构建与运行库基线及清单；J：加载与交付条件；M：Imath 无查表实验；N1：MMS 通用结果接入；N2：MMV 求值适配；O：快照与隐藏编辑器；Q：同帧 GPU 蒙皮批处理。

- `D:/MMD-Next/MMS/profiles/lto-experiment`（预定新增）。
- `D:/MMD-Next/MMS/.local/artifacts/lto-experiment/README.md`（预定新增）。

验收及边界：固定同一源码与精确工具版本，分别改变GL/LTCG、Gw，保留MD、alignedNew-与物理数值基线；功能、MME导出符号、体积和性能全部验收。正式选项由主进程单独合入。

主进程按以下顺序处理公共语义和文件交接。

1. 让 B 独占 `runtime_scene.inl`，同时修 F02 与 F07。效果重载后需要更多数据时，重新获取对应快照，不能只改插件内的需求位。
2. C 先保证每次 PhysicsInput 返回当前数据。O 在此基础上做缓存，不让 C 为提前优化而侵入 ModelRecord、编辑器和 SDK。
3. F 完成后将 `deformer_engine.cpp` 交给 J，再交给 N2。G 完成后将 `menu.inl` 交给 J，再交给 N1。每次交接记录已验收提交。
4. E 完成后将 `alembic_export.cpp` 和对应测试交给 K。K 只减少重复计算与复制，不跳过求值、物理推进或 MME 捕获所需绘制。
5. H 完成后将 `conanfile.py` 交给 M。M 的实验配置先保留独立身份，主进程决定是否采纳正式依赖。
6. D 固定设备丢失和退出行为后再合入 L。主进程统一规定停止提交、结束 GPU 借用、释放默认池资源和 Reset 的顺序。后续 Q 必须遵守同一顺序。
7. N 先冻结数据与时间契约。N1 与 N2 各自在一份固定 SDK 上开发，需要修改契约时只暂停受影响部分，交主进程升级两侧快照后继续。
8. O 与 P 只在变更版本、输入可见阶段和有状态求值规则已固定后并行。若两者都需要新增公共 dirty/epoch 字段，主进程先串行完成该公共改动。
9. P 完成后把 `frame_scene.cpp` 交给 Q。Q 必须产出本帧结果；不能把旧帧结果交给 MME、录制或 Alembic 导出。

通用 mesh 接口由 N 定案，至少覆盖下列契约。

- 无状态整体变换、程序化形变与有状态刚体、布料、软体可以声明不同能力，不强制普通 mesh 变换创建物理世界。
- 输入分别说明原始绑定几何、表情后未蒙皮几何和 posed mesh。每个模块声明读取阶段与输出阶段，明确 mesh 与骨骼输出的先后及蒙皮责任。
- 拓扑变化维护稳定对象身份、来源顶点、绑定表和材质来源。粗仿真网格与渲染网格的映射独立表达，新顶点和新材质允许没有原始来源。
- CPU 输入借用期限、结果提交和失败回滚明确。异步持有输入必须有显式生命周期；GPU 交换需设备、资源状态、同步和回退契约，不能假设 DX9 任意零复制。
- 同一逻辑帧只推进一次仿真。暂停、编辑、回退、向前跳转、预热、导出采样和设备恢复分别定义行为。输出缓存不冒充完整求解器 checkpoint。
- 有双向相互作用的刚体、布料和软体明确共享求解域。渲染器切换不重新推进仿真，原生渲染与 MME 都是正常消费端。

维持现有 CMake、MikuMikuDance.exe 名称和 MME 导出约定，保留 `/Zc:alignedNew-`、Bullet 2.75 行为及浮点基线。不把升级 v145、改用 fast-math、替换物理求解器或统一静态 CRT 混入普通缺陷修复。Win7 支持目标、D3DCompiler47 分发方式及运行库部署方式尚未定案时，记录待决项，不把任意选择作为默认产品决定。

每个子任务派发说明采用下列内容。主进程填入真实工作树、提交与文件清单，不向独立会话只发送问题编号。

1. 写明任务目标、对应 F/C/O 编号、两个仓库中涉及的模块，以及用户要求保留的 MME、通用 mesh 与兼容性边界。
2. 写明已创建并核验的绝对 workspace、父提交和配对仓库版本。DSH 返回 cwd 不符时先处理目录问题，不继续项目修改。
3. 逐项复制本包允许修改文件，附主进程独占及外部占用文件清单。
4. 附该包触发条件、验收案例、允许的后台测试和不得重启或占用用户程序的约束。
5. 要求返回变更文件、独立提交、运行过的命令、原始结果路径及未完成项。子进程报告 completed 后，主进程仍须读取正式 result 并验收。
6. 限定不推送、不发布、不覆盖正式 EXE、不清理其他工作树或记录。交付仅包含任务范围内的本地提交。

所有性能采样都独占机器串行运行。独立构建目录只能避免产物覆盖，不能避免 CPU、GPU、磁盘与运行库竞争。因此重型构建、其他会话的 GPU 采样和正式性能测试不同时进行。

验收按仓库要求执行。

1. 每包修复完成后，以集成父提交构建 before，以合入该包后的提交构建 after。两侧保留相同测试入口与构建条件。记录提交、工作树、工具版本、依赖身份和 EXE/DLL 哈希。
2. 先执行真实生产路径的缺陷回归。VMD 用隔离进程处理恶意计数和短读；状态类问题验证错误回滚与借用对象寿命，不能只测试辅助算术或伪实现。
3. 进行后台前后生产 build 对照。保持素材、相机、分辨率、动作、物理、透明设置、MME 和采样帧序列一致。图像从渲染目标或回读接口取得，不靠桌面截图。
4. 验证后台窗口没有因最小化、遮挡、跳过绘制或 Present 而降低工作量。性能优化不得关闭物理、描边、透明层或降低画质。
5. 分别记录进程 CPU、CPU 求值与提交时间、GPU 时间、同步等待、峰值内存和导出耗时。取得 Present 数据才报告该口径 FPS；程序内部计数另列。
6. 交替采样前后版本并保留波动、失败和排除样本。每项优化事先规定判定指标；收益不明确或正确性变化时不采纳正式配置。
7. 主进程逐包合入并复核交叉场景：新进程加载透视效果、动态表情、mesh 拓扑与材质变化、设备丢失、场景清理、MME 绘制及导出取消。
8. 只有相关目标系统镜像实际启动、加载插件和变形器通过后，才宣称该系统兼容性已验收。静态导入检查不替代干净系统运行。

第一批、遗留项与第二批的实际工作树、before/after build、采样证据和本地提交见各批验收记录。基线与实现工作树均保留，不自动删除；后续批次仍须按前置条件重新核验文件所有权。

计数、来源覆盖、依赖顺序、同批文件所有权及本地链接可用以下命令重新核对：`D:\anaconda3\python.exe -X utf8 D:/MMD-Next/MMS/.local/artifacts/size-code-review-20261011/03/validate_plan.py`。检查结果保存在 [plan-validation.json](D:/MMD-Next/MMS/.local/artifacts/size-code-review-20261011/03/plan-validation.json)。该检查只验证方案结构，不能代替修复代码的运行验收。
