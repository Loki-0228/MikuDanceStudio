# MMV 外置 UI 描述参考

`.uifx` 使用 FX 变量声明、注解、注释、宏和 `include` 语法。它描述 GUI 与参数绑定，不包含效果算法；加载器只解析元数据，不执行其中的 shader。原 `.fx` 和其 include 文件不被改写。

## 文件选择与匹配

默认读取与选中 FX 同目录、同主文件名的 `.uifx`，例如 `Face.fx` 对应 `Face.uifx`。加载器不把旧扩展名 `.mmvui` 当作默认名，也不在 `.uifx` 缺失时回退到它；旧文件需要改成 `.uifx`，或在 `MMVConfig` 中按原名显式指定。这是对已经显式分配的 FX 查找界面，不是为 `.x` 自动搜索或分配同名 FX。

存在外置描述时，以其中的变量列表为界面暴露清单。没有描述文件时，不推测旧 FX 哪些内部变量适合给用户调整；已有 MMV 内联暴露注解仍作为兼容入口。自有 FX 可在 `MMVConfig` 中用 `string MMVUI="ui/face.uifx";` 指定路径；显式路径按写下的后缀读取，加载器不附加也不校验后缀。为禁止二改的 FX 配界面时只需创建默认同名文件。

外置文件为 UTF-8，主文件不超过 1 MiB。`include` 路径按 FX 包含规则相对于所在文件解析。修改现有描述或其 include 会参与热重载；新建首个描述文件后，点击重新加载。

## 参数声明

除保留的界面声明外，每个声明都必须匹配原 FX 中一个可写全局变量。支持 `float`、`float2/3/4`、`int`、`bool`；不支持数组、矩阵、纹理对象、函数或 `static/const` 变量。类型不匹配、重复声明和未知变量均报错。

没有外置初始值时读取原变量的字面量初始值；支持数字、布尔、向量构造和大括号列表。复杂表达式需要在描述文件提供显式默认值。宏在读取元数据前展开。

| 注解 | 类型 | 默认值或作用 |
| --- | --- | --- |
| `MMVExpose` | bool | 外置声明默认 `true`；`false` 不显示 |
| `MMVId` | string | 默认是原变量名；稳定存档身份 |
| `UIName` | string | 通用显示名称 |
| `UIName_zh_CN`、`_en`、`_ja` | string | 中、英、日名称 |
| `UIHelp` 及语言后缀 | string | 参数帮助文字 |
| `MMVKeyable` | bool | 默认 `true`；允许记录 K |
| `UIMin` / `UIMax` | 数值 | 默认 −1000000 / 1000000；布尔为 0 / 1 |
| `UIStep` | 数值 | 浮点默认 0.01，整数和布尔为 1 |
| `UIWidget` | string | `auto`、`slider`、`number`、`color`、`checkbox`、`enum` |
| `MMVEnum` 及语言后缀 | string | 整数值与标签，例如 `0=关闭;1=开启` |
| `UIGroup` | string | 所属参数分组 ID |
| `UIRow` | string | 同一分组中优先同行的 ID |
| `UIOrder` | int | 非负排序值，相同时保持声明顺序 |

显示文字按当前语言、通用名称、英文、变量 ID 的顺序回退。标签使用 `zh-CN`、`en`、`ja`，注解中的连字符写成下划线。旧 `MMVLabel`/`MMVHelp` 名称仍可读取，外置描述优先使用 `UIName`/`UIHelp`。

一个效果最多暴露 256 个变量。ID 和 HLSL 名称分别唯一；所有数值有限且可由对应类型表示。范围、步长、初始值和枚举候选都必须有效。Color 用于 float3/4，Checkbox 用于 bool，Enum 用于 int，Slider 用于单个 float。

## 界面声明

文件可包含一个可选标题声明：

```hlsl
float MMVUI <int Version=1;string UIName_zh_CN="参数";>;
```

省略声明时按版本 1 读取。未知版本报错。`MMVUI` 不绑定原 FX 变量。

分组声明名称以 `MMVUIGroup_` 开头，后缀就是分组 ID：

```hlsl
float MMVUIGroup_camera <
    string UIName_zh_CN="相机";
    string UIName_en="Camera";
    int UIColumns=0;
    int UIOrder=0;
>;
```

`UIColumns=0` 为自动；1–4 指定最大列数，窄窗口仍会换行。变量只写 `UIGroup="camera"` 而没有分组声明时，自动创建以 ID 为名称的分组。没有分组或排列注解的变量使用自动布局。作者不填写 Win32 像素坐标。

`UIRow` 只合并同一组的字段。不同组不会因同名行合并。分组与行描述的是参数面板，不是 FX 目标页或场景骨骼层级。描述文件不接受 `technique`、`pass`、函数定义或脚本。

## 关键帧

关键帧以效果分配实例和稳定变量 ID 定位。整数帧范围为 0–2147483647；同帧重新 K 替换该帧。运行时可以在小数动画帧求值，使用 MMD 每秒 30 帧的时间基准。

| 插值 | 两个关键帧之间的值 |
| --- | --- |
| Step | 保持前一关键帧 |
| Linear | `a + (b-a)t` |
| Smooth | `a + (b-a)t²(3−2t)` |

`t` 是归一化时间，插值模式属于前一关键帧。整数、枚举、布尔使用 Step；向量各分量共享时间和模式。没有关键帧时使用基础值；首键前保持首键，末键后保持末键。`MMVKeyable=false` 的变量不能包含轨道。

采样不修改基础值或关键帧。暂停、跳帧及重复绘制按当前动画时间重新采样。K 轨道保存在 `.vmm/.vem` 的 `Parameters.<效果实例ID>` 分段，格式 `Version=1`。未知变量字段保留；已知非法数据导致整个加载事务失败。

## 后端与原文件

旧 DX9/MME FX 保持原源文本交给 D3DX Effect 编译，运行时按原变量名称提交参数。原生 DX12 FX 的内存编译副本自动生成 `MMVUserParameters : register(b1)`，每个参数占 16 字节寄存器；不向磁盘写回转换后的源码。作者不另行声明 b1。内置 `Perspective1` 由参数适配器生成它的既有常量块。

GUI 描述不改变 FX 后端。纯 DX12 版保留旧 DX9 FX 的分配与参数，但不能执行旧 shader。具体控件、后端写入与宿主验收状态以实现报告为准。