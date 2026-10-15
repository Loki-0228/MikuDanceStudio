# 整理预发布包

使用 PowerShell 7、CMake 和已配置的 Windows x64 Release 构建目录。完整包的构建档为 `full`，最小包为 `mmd932`。两个目录的源码都必须指向当前仓库。

1. 检查工作区改动，运行本轮相关测试。保留失败日志，对照基线，并在发布说明中记录尚未解决的问题。
2. 在[发布记录](../records/releases.md)补充三语说明，用 `prerelease-notes:<版本号>:begin` 和 `prerelease-notes:<版本号>:end` HTML 注释标记该版本的范围。
3. 提交源码和说明。打包脚本要求暂存区与工作区干净，本地日志、截图、模型和构建目录应由 `.git/info/exclude` 排除。
4. 运行打包脚本，将参数替换为实际路径。输出目录必须尚不存在。

```powershell
	./scripts/package_prerelease.ps1 -Version v2026.10.15.1-pre.1 -FullBuild D:/builds/mms-full -MinimalBuild D:/builds/mms-minimal -OutputDirectory D:/releases/v2026.10.15.1-pre.1
```

脚本重新构建两个档位，确认源码未变化，然后只复制程序、许可证和发布说明。两个主程序 ZIP 均不含 NVIDIA 辅助 DLL；`MMSNvidiaAAFix.dll`、三语安装说明与许可证作为独立附件输出。SHA-256 清单包含主程序包和独立附件，构建清单单独列出可选组件。不包含模型、设置、MME、MMV、FFmpeg 或调试符号。

5. 检查 ZIP 条目与包内清单，核对源码提交、构建档、程序及许可证的哈希。
6. 为通过检查的提交创建本地预发布标签。上传 GitHub 前，确认发布说明中的验证范围和已知问题。

打包脚本不推送提交、不创建 GitHub Release，也不修改现有安装。`Uploaded: false` 表示生成时尚未上传。

普通 `cmake --install` 不安装 NVIDIA 辅助 DLL。需要单独安装该 DLL 时，显式指定 `--component NvidiaAAFix`；这只复制文件，不执行驱动设置安装。
