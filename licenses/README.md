# 第三方依赖许可

本目录保留新增依赖的上游许可文本。分发主程序时应一同分发本目录。

| 依赖 | 本次 Conan 版本 | 许可文本 |
| --- | --- | --- |
| Alembic | 1.8.8 | [BSD-3-Clause](Alembic-LICENSE.txt) |
| Imath | 3.2.2，Alembic 的传递依赖 | [BSD-3-Clause](Imath-LICENSE.md) |
| MinHook | 1.3.4 | [BSD-2-Clause 及附带反汇编器声明](MinHook-LICENSE.txt) |

原有 Bullet、DirectX SDK 与 Windows SDK 的许可不变。实际依赖版本由 `conanfile.py` 和 Conan 生成器记录。
