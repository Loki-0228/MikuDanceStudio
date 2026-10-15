# NVIDIA 双面绘制修复插件

`MMSNvidiaAAFix.dll` 为独立可选下载，完整包与最小包均不捆绑。不需要此修复时，无需下载或安装。

## 中文

此插件用于 NVIDIA 显卡的双面材质绘制兼容设置，供 MMS 完整版使用。AMD、Intel 无需安装。

1. 下载独立的 `MMSNvidiaAAFix.dll`，放到完整版的 `MikuMikuDance.exe` 同目录。
2. 启动 MMS，在「帮助」菜单选择「安装 NVIDIA 双面绘制补丁…」。
3. 确认管理员授权，安装完成后重新启动 MMS。

下载或放置 DLL 不会自动安装驱动设置。只有明确执行菜单中的安装操作才会修改设置。

需要移除修复时，保留或重新放入 DLL，在「帮助」菜单选择卸载，再重新启动 MMS。直接删除 DLL 只移除菜单入口，不撤销已安装的设置。

## English

`MMSNvidiaAAFix.dll` is a separate, optional download for the full build. Neither application ZIP includes it. AMD and Intel users do not need it.

1. Download the DLL and place it beside `MikuMikuDance.exe` in the full build's directory.
2. Start MMS. In **Help**, select **Install NVIDIA double-sided rendering fix…**.
3. Approve the Windows administrator prompt, then restart MMS after installation.

Downloading or copying the DLL does not install the driver setting. Only the explicit installation command changes the setting.

To remove the fix, keep or restore the DLL, select **Uninstall** in Help, and restart MMS. Deleting the DLL only hides the menu entry; it does not undo an installed setting.

## 日本語

`MMSNvidiaAAFix.dll` は完全版向けの任意の独立ダウンロードです。両方のアプリ ZIP に含まれません。AMD・Intel では不要です。

1. DLL をダウンロードし、完全版の `MikuMikuDance.exe` と同じフォルダーに置きます。
2. MMS を起動し、ヘルプメニューから「NVIDIA 両面描画の修正をインストール…」を選択します。
3. 管理者権限を承認し、インストール完了後に MMS を再起動します。

DLL をダウンロードまたは配置しただけでは、ドライバー設定は変更されません。メニューで明示的にインストールした場合のみ変更されます。

解除するには DLL を残すか再配置し、ヘルプメニューからアンインストールして MMS を再起動します。DLL の削除だけでは、適用済みの設定は解除されません。

## 许可证与源码 / License and source / ライセンスとソース

项目许可证随独立下载附件提供为 `LICENSE`。源码与许可证也可在 [MikuMikuStudio 仓库](https://github.com/Loki-0228/MikuMikuStudio) 查看。
