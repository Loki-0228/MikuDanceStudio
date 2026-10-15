# H.264 导入与后台视频预览

2026-10-15，完整构建档。

## 行为

MP4、MOV 背景在独立线程解码。时间轴拖动和实时播放立即返回上一张可用画面，后台完成后更新背景。连续拖动只保留一个最新目标，不累积逐帧任务。停止拖动后，主循环继续刷新已解码的画面。

预览长边最多为 1024 像素，与现有背景纹理的分辨率上限一致。高分辨率、长关键帧间隔的视频仍需解码时间，背景可以滞后。该改动消除了界面线程中的视频解码等待，不保证整个场景的渲染帧率。

录制及图片序列输出等待目标帧解码完成，避免把滞后的预览帧写入输出。原有 AVI/VFW 播放路径保持不变。

## H.264 兼容性

Windows 解码器首次输出 H.264 画面时可能报告格式变更，更新步幅、裁切等信息。旧代码把该通知直接视为错误，导致普通 H.264 文件报「Video dimensions changed」。现在重新读取并验证 RGB32 格式，按实际步幅复制画面。定位时也处理首帧时间偏移与片尾边界。

Windows 无法解码的格式，例如本次样本中的 10-bit H.264，改用本机 FFmpeg。程序依次查找可执行文件目录下的 `tools/ffmpeg.exe`、同目录 `ffmpeg.exe` 和 `PATH`。FFmpeg 是可选依赖，未捆绑进主程序；常用 8-bit H.264 继续通过 Windows 媒体组件解码。

FFmpeg 以隐藏进程运行，只继承专用管道句柄。解码后的 RGB 画面直接经内存管道传递，不生成图片序列或视频转码缓存。关闭或替换视频时结束本次解码进程。后备路径按媒体声明的帧率采样；高位深输入转换为现有背景所需的 8-bit RGB32，不保留 HDR/10-bit 显示精度。

## 验证

`tests/make_native_media_fixtures.ps1` 生成音频、H.264 Baseline/Main/High/High 10、MP4/MOV 和 4K 长关键帧间隔素材。`native_media_tests <素材目录>` 检查颜色与上下方向、前后定位、片尾、尺寸上限、重新载入、后台画面内存稳定性、最新请求追赶和音视频输出回归。

`native_media_tests --video <文件>` 检查单个视频的导入、正向/反向定位、连续 300 次预览请求、最后请求与同步准确帧的一致性，以及解码未完成时关闭。输出中的请求耗时只测取帧接口，不代表模型渲染或整机帧率。

桌面的 5 个原文件均通过单文件测试，包含 1920×1080 的普通 H.264、1920×1080 的 High 10 MOV、HEVC 和两段不同尺寸的 H.264。

`media_render_tests <素材目录> --background` 在隐藏的正式窗口检查背景开关、暂停后的后台画面更新、GPU 像素和透明输出排除背景。`--media-only` 继续检查半透明 PMX、GPU 回读及 24、30、60 fps 的实际录制输出。以上媒体用例及 `background_overlay_tests` 的 BMP/AVI 回归均已通过。

首次验收时，默认模式附带的窗口光标测试在「header zoom/pan starts a captured drag」处失败，因此先用独立媒体模式完成视频验收。后续背景操作修复将测试坐标改为实际自适应布局的按钮位置，默认模式已通过。

实现依据包括 Microsoft 的 [Source Reader 数据处理说明](https://learn.microsoft.com/en-us/windows/win32/medfound/processing-media-data-with-the-source-reader) 和 [H.264 解码器支持范围](https://learn.microsoft.com/en-us/windows/win32/medfound/h-264-video-decoder)。

## 背景移动与缩放

2026-10-15，操作入口见[背景媒体操作](../../guides/background-media.md)。

`frame_modes.cpp` 原先只把两个按钮的拖动量应用到相机，现在按 Ctrl 状态选择背景。移动量按视口比例换算，并累计小数，避免缩放后的视口吞掉慢速拖动。背景位置和比例沿用 PMM 的既有字段，临时拖动状态放在兼容布局之外。

`AviBgPlacementRefresh` 只更新背景顶点，不读取或解码视频帧。图片沿用 `PicBgOverlayRefresh`。拖动标记当前画面需要重绘。

完整构建档的 `media_render_tests <素材目录>` 默认模式通过。检查包含图片、视频的 Ctrl 平移和缩放，三种视口比例下的一像素拖动，缩放中心，相机参数隔离，松开 Ctrl、取消捕获、无背景及视频优先的行为，以及实际背景顶点更新。原有相机拖动、MP4 预览、透明输出、24、30、60 fps 录制回归也通过。

`background_overlay_tests` 通过 BMP/AVI 加载与 GPU 像素检查，并验证视频移动、缩放时保留已显示帧。窗口光标测试改为依据实际自适应布局定位按钮，修正此前使用旧按钮坐标造成的失败。
