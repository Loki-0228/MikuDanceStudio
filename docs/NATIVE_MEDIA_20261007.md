# MMS 原生媒体导入与导出

## 导入

MMS 使用 Windows Media Foundation 解码本机支持的音视频格式。MP4/MOV 视频逐帧解码到背景纹理，并支持时间轴定位。实际可用的编码取决于 Windows 安装的媒体组件。

## 导出

透明 MOV 使用 Windows Imaging Component（WIC）逐帧编码 PNG，由 MMS 内置的 MOV 封装器写入文件。导出保留 Alpha 通道，支持自定义帧率和奇数宽高。当前 WIC PNG/MOV 路径不写入音轨，也暂不支持 3D Vision。

MP4/H.264 继续使用 Windows Media Foundation 编码器；其他 AVI 压缩器仍通过 DirectShow 提供。

透明录制将模型和配件渲染到独立的 A8R8G8B8 目标，隐藏背景图片与视频。回读时翻转图像方向并解除颜色预乘，再编码为 PNG。每帧同时显示到窗口后缓冲区，保持录制预览可见。
