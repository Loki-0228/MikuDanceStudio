# MMDModern 汉化适配包与 MMS 崩溃风险核对

## 结论

**尚不能确认用户提到的旧版崩溃已在 MMS 中修复。** 用户未掌握触发操作，两份包也没有提供对应的崩溃修复记录。

新包唯一变化的配置值是 `DrawOrderTable=1` 改为 `0`。新旧 DLL 中已定位的顺序表、纹理索引和当前对象缓存核心代码相同。顺序表的宿主补丁地址经过适配，但默认不安装该功能。现有证据支持“默认禁用顺序表”，不能据此认定算法已修复，也不能确定禁用是为规避用户提到的崩溃。

MMS 参考旧包的机制编写源码，没有载入 `dsound.dll`。MMS 仍启用自己的顺序表实现，因此没有继承新包的禁用配置。MMS 的边界检查、缓存失效及对象注册检查已存在，但这些保护不能替代对未知崩溃的复现。

本次保留现有生产实现，只提交调查报告。未重建程序，未执行运行测试，也没有新的帧率结果。

## 文件身份

“新包”和“旧包”表示用户提供的两份本地文件，不表示已经确认的上游版本先后。

| 项目 | 旧包 | 新包 |
| --- | --- | --- |
| 目录 | `D:/MMD-Next/refs/MMDModern` | `D:/MMD-Next/refs/MMDModern-适配MikuMikuDance932汉化版` |
| DLL | `dsound.dll` | `dsound.dll` |
| 大小 | 785920 字节 | 785920 字节 |
| PE 时间戳 | `0x6ac80290` | `0x6ac80290` |
| 映像大小 | `0xd2000` | `0xd2000` |
| SHA-256 | `b91d627d0d245d49a93d845278464c708ee7dd3e34686fa4cfcf58a6a4e1f9e1` | `fc07ea2a1cb039b57a54f766901cbeac49a932ec9a8557a1b528d3592758709f` |

两份 `README.md` 和 `LICENSE` 相同。新包 `README-cn.md` 增加 MikuMikuDancePlus 兼容说明，目标 EXE 时间戳为 `0x5A26BB1D`，原官方目标为 `0x5DEC0538`。相同 DLL 布局、核心代码及该说明支持“基于同一构建的适配包”这一判断，不能仅凭目录名称认定它包含新版算法修复。

## 二进制差异

全文件共有 5112 字节不同：

| 区域 | 不同字节数 | 核对结果 |
| --- | ---: | --- |
| `.text` | 214 | 对应 204 条发生变化的指令，均保持原指令助记符及长度，操作数有变化 |
| `.rdata` | 4895 | 包含中文字符串替换、宿主补丁地址及预期机器码变化 |
| `.data` | 1 | RVA `0xb79e0` 的值从 `0x274f0` 改为 `0x274e0` |
| PE 头 | 2 | `MajorOperatingSystemVersion` 从 `0x6` 改为 `0x3d7c` |

`.pdata`、`.fptable`、`.rsrc` 和 `.reloc` 相同。逐指令比较覆盖 `.text` 的全部 214 个不同字节。助记符不变不等于行为不变，地址和常量也可以影响正确性。

### 三项移植机制

下表地址均为 DLL RVA，范围右端不包含在内。比较范围包含当前对象入口的全部展开信息分段，避免只比较函数开头。

| 已定位代码 | 范围 | 新旧结果 |
| --- | --- | --- |
| 顺序表构建 | `0x6730..0x67c0` | 字节相同 |
| 顺序表查询 | `0x67c0..0x6993` | 字节相同 |
| 纹理索引辅助函数 | `0x73c0..0x7849`、`0x8080..0x80eb` | 字节相同 |
| 当前对象查询 | `0xda80..0xdc0a` | 字节相同 |
| 当前对象缓存入口 | `0xdc10..0xdcaf` | 字节相同 |
| 当前对象缓存重建 | `0xddd0..0xdef1` | 字节相同 |

当前对象缓存的安装代码把宿主函数地址 `0xa80a0` 改为 `0xa8090`，位于 DLL RVA `0xdcfe`、`0xdd0e`、`0xdd71`、`0xdd81`、`0xdd9e`。这是目标 EXE 的地址适配，MMS 的源码调用不使用这些补丁地址。

渲染补丁表位于 `0x80240..0x81370`，共 25 项。其中 15 项属于纹理查询，10 项属于顺序表。新包把 13 个纹理目标地址及全部 10 个顺序表目标地址减去 `0x10`，另两个纹理目标地址不变。顺序表并非完全未做地址适配；然而新包仍通过配置关闭它。该选择的原因没有说明。

两份 INI 按节与键解析后，只有 `[Render] DrawOrderTable` 的值不同。`TextureLookupHash` 和 `CurrentObjectCache` 均保持为 `1`。

### 新包另有两处需要确认的改动

这两处不属于 MMS 的三项移植，不能当作 MMS 需要照搬的修复。

- **相机撤销状态写入目标变化。** DLL RVA `0x126b1` 从 `mov [rip+0xa80a0], rdx` 改成 `mov [rip+0xa8090], rdx`，实际写入的 DLL 内部地址由 `0xba758` 变为 `0xba748`。这里的 `0xa80a0` 是 RIP 相对位移，并非宿主 EXE 函数地址。旧目标由 `0x1553` 初始化为 30，在 `0x12713` 被用作撤销数量的比较值。新目标在 `0x13735`、`0x13783` 参与撤销容器的索引运算，`0x137c8` 会在容器清空时将其归零。这是可疑的内部状态改写；尚未通过运行复现确定影响，不能宣称它必然导致崩溃或解释旧包崩溃。
- **PE 操作系统版本字段异常变化。** 文件偏移 `0x168` 的 `MajorOperatingSystemVersion` 从 6 变为 `0x3d7c`。没有配套说明。本次未测试该字段对加载器的实际影响。

## MMS 源码核对

本次核对时最新提交为 `6368b3b`。移植原提交为 `f1aa12f2930489c18d6f583d6f1d4d9d7c931344`。

| 机制 | 现有保护 | 与 DLL 的区别 |
| --- | --- | --- |
| `BuildDrawOrder` | 固定容量数组初始化为空，跳过空对象、负数顺序及超界顺序，重复顺序保留首项 | 每次遍历重新构建，不使用 DLL 的跨调用顺序表缓存或机器码补丁 |
| `FindTextureResource` | 索引拥有路径字符串副本，按资源代次重建，返回资源项的实时字段 | 使用宿主资源生命周期失效，不扫描原版 EXE 的固定地址 |
| `ExpGetCurrentObject` | 缓存仅在渲染线程且场景未变更时命中，核对注册代次、通道、分割顺序、槽位指针及对象顺序 | 使用完整对象指针和宿主槽位访问，不安装 DLL 跳转补丁 |

源码位置：

- [顺序表实现](../include/mikudancestudio/draw_order.hpp)，调用位于 `src/render/model_renderers.cpp` 和 `src/render/accessory.cpp`。
- [纹理索引实现](../include/mikudancestudio/texture_lookup.hpp)，失效调用位于 `src/unported/stubs.cpp`、`src/model/path_resolve.cpp`、`src/window/enhance_model_io.cpp` 和 `src/app/shutdown_cleanup.cpp`。
- [当前对象接口](../src/exports/effect_api.cpp)，场景变更失效位于 `src/app/scene_ownership.cpp`。

`ExpGetCurrentObject` 已有“对象未被效果引擎注册则返回 0”的保护。源码说明该保护针对 MMHack 查询未注册对象状态时的空指针访问。没有证据表明这就是用户提到的 MMDModern 旧版崩溃，因此不能把这项已有保护列为本次新修复。

正式目录 `build/Release/` 中没有 `dsound.dll`。本次回读的正式 `MikuMikuDance.exe` SHA-256 为 `90cf474dcc0f004f757828369483979d9d00775e490fd5543c6e9a6dc7d500d6`，与前次实测版本一致。

## 证据与验证范围

静态分析产物保存在 `.local/artifacts/mmdmodern/20261010/new-dll-audit/`：

- `compare.py`：读取 PE、反汇编差异、比较完整核心代码范围、解析 INI 与渲染补丁表。
- `comparison.json`：文件哈希、各区域统计、函数范围、逐指令差异、配置和 PE 头差异。
- `code-changes.txt`：逐指令变化。
- `render-patch-table.json`：25 项渲染补丁的目标地址与变化。
- `summary.txt`：比较脚本输出。

本机可在 `D:/MMD-Next/MMS` 下运行以下命令重新生成静态统计，依赖已安装的 `pefile` 和 `capstone`：

```powershell
python -X utf8 .local/artifacts/mmdmodern/20261010/new-dll-audit/compare.py
```

该脚本读取 DLL 字节，不载入 DLL。本次未启动 MMD 或 MMS，未运行回归、压力或性能测试。此前的 `Stage.pmm` 对照见[移植与测量报告](MMDMODERN_PORT_20261010.md)，不能作为这次未知崩溃的复现或修复证据。
