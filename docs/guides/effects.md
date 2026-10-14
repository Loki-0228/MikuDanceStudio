# MME 特效

MME 需要另行安装。将对应的 `d3d9.dll`、`MMEffect.dll`、`MMHack.dll` 及其配置放在 `MikuMikuDance.exe` 所在目录，保留插件要求的程序文件名。安装与构建条件见[仓库的 MME 说明](../../README.md#mikumikueffectmme支持)。

## 分配与保存效果

通过 MME 的效果分配窗口选择对象或材质，再加载对应 FX。PMM 保存 MMS 工程，EMM 保存 MME 的效果分配；交换工程时同时保留效果引用的素材与贴图。PMM 扩展设置不写入 EMM。

重新分配后出现模型消失、黑色材质或地面阴影异常时，先记录工程、效果名称和 MME 版本，再对照[MME 兼容记录](../records/effects/compatibility.md)与[材质诊断](../records/rendering/materials.md)。同名 FX 的共享状态和第三方效果行为不能仅凭 DLL 加载成功来判断。

## 自动重载 FX

在 MME 中启用「自动重载」后，保存已加载的根 FX 文件即可触发原有重新编译。关闭该选项时不自动编译，重新开启后会检查期间发生的修改。

完整包和最小包对已验证版本的 MME 使用 Windows 文件变更通知缓存时间戳。没有文件变化时避免反复打开文件，变更时仍由 MME 执行重载。无法建立监听或无法识别版本时使用原始查询。该优化不扩大 MME 原有的 include 依赖重载范围。

实现边界、路径编码检查和响应时间记录见[预览与自动重载优化](../records/performance/preview.md#record-mme-preview-optimization)。

## 缩小预览中的阴影

完整包仅缩放已确认用途的 SoftShadow、ray 固定阴影资源，并同步相关采样参数。粒子数据纹理、查找表和未知资源不缩放。100% 预览与导出使用原尺寸，FX 源文件保持不变。

最小包没有预览比例功能，但包含模型结果缓存和自动重载通知优化。缩放与设备重置的检查记录见[预览性能专题](../records/performance/preview.md)。
