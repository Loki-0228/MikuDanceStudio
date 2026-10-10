"""为指定 PMX 生成独立的浅阴影 toon 对照模型。

用途：把某个 PMX 模型的 toon 贴图换成一张新生成的浅阴影查找表，输出一个可以
单独打开对照的模型副本。输入模型与它引用的贴图一律只读，不做任何修改。

命令行::

    python tools/make_cloud_toon_variant.py input.pmx output_dir

输出（写入 output_dir，目录不存在则创建）：

- ``ToonCloud-soft.pmx``：对照模型。除贴图表（textures 段）外，顶点、面、
  材质、骨骼、表情、表示枠、刚体、关节等原始字节与输入模型逐字节相同。
- ``CToon-soft.bmp``：64x64、24 位、无压缩 BMP 数值光照查找表。图像自上而下
  第 0-27 行纯白 (255,255,255)，第 28-35 行（中间 8 行）向浅阴影色线性过渡，
  第 36-63 行为浅阴影色 RGB(235,240,246)。
- ``textures\\tex-<索引><原扩展名>``：贴图表里其余非空贴图的副本，按原始字节
  复制，不做任何格式转换。

贴图表的改写规则：

1. 15 个材质共用的那个外部 toon 纹理索引，条目写成 ``CToon-soft.bmp``；
2. 其余非空贴图，条目写成 ``textures\\tex-<索引><原扩展名>``，对应文件复制到
   ``<输出目录>\\textures\\`` 下；空条目保持为空。

条目一律写成相对模型目录的相对路径。原因：PMX 加载器 src/model/pmx_load.cpp
第 800/831 行用 ``L"%s%s"`` 把模型目录与贴图条目直接拼接，绝对路径条目会拼出
非法路径，模型目录之外的贴图必须先复制进输出目录。

前置条件（不满足即报错退出，不做任何处理）：输入模型恰好 15 个材质，且每个
材质都以外部 toon 贴图（PMX toon 模式 0，即贴图索引）引用同一张贴图；需要复制
的源贴图文件都存在。

其他约定：任一目标文件（输出模型、CToon-soft.bmp、textures 下的副本）已存在
时拒绝覆盖；输出写出前先自检「只有 textures 段变化」，自检不通过则不写任何文件。

退出码：0 成功；2 输入无效、校验不通过或依赖贴图缺失；3 输出文件已存在（拒绝
覆盖）；4 写出前自检失败（未写入）。
"""

from __future__ import annotations

import argparse
import os
import struct
import sys
from pathlib import Path

_TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
if _TOOLS_DIR not in sys.path:
    sys.path.insert(0, _TOOLS_DIR)

from pmxlib import PmxError, PmxModel  # noqa: E402  （需要先把 tools 目录放进 sys.path）


# ---------------------------------------------------------------------------
# 常量
# ---------------------------------------------------------------------------

EXPECTED_MATERIAL_COUNT = 15

BMP_SIZE = 64
TRANSITION_ROWS = 8
WHITE_RGB = (255, 255, 255)
SHADOW_RGB = (235, 240, 246)

TEXTURES_SUBDIR = "textures"

_ZERO_WIDTH_CHARS = "\u200b\u200c\u200d\u200e\u200f\ufeff"


def _strip_zero_width(text: str) -> str:
    """去掉文件名里的零宽字符（含零宽空格、方向标记、BOM）。"""
    return "".join(ch for ch in text if ch not in _ZERO_WIDTH_CHARS)


# 输出文件名：常量源串可能带零宽字符，落盘前统一清掉。
OUTPUT_MODEL_NAME = _strip_zero_width("ToonCloud-soft.pmx")
OUTPUT_TOON_NAME = _strip_zero_width("CToon-soft.bmp")


# ---------------------------------------------------------------------------
# BMP 光照查找表
# ---------------------------------------------------------------------------


def row_rgb(y: int) -> tuple[int, int, int]:
    """返回图像自上而下第 y 行的颜色。

    上 28 行纯白，下 28 行浅阴影色，中间 8 行（第 28-35 行）线性过渡：
    第 27 行是白色端点、第 36 行是阴影端点，两者之间 9 等分，中间 8 行取
    1/9 到 8/9 的中间值。这样上下两半保持纯色，8 行过渡带严格居中对称。
    """
    half = (BMP_SIZE - TRANSITION_ROWS) // 2
    if y < half:
        return WHITE_RGB
    if y >= half + TRANSITION_ROWS:
        return SHADOW_RGB
    t = (y - (half - 1)) / (TRANSITION_ROWS + 1)
    return tuple(round(a + (b - a) * t) for a, b in zip(WHITE_RGB, SHADOW_RGB))


def build_toon_bmp() -> bytes:
    """生成 64x64、24 位、无压缩 BMP，返回完整文件字节。

    每行颜色相同，整张图就是一条自上而下的亮度查找表。BMP 按行自下而上
    存储，所以写入顺序是第 63 行到第 0 行；像素按 BGR 三字节排列。
    """
    row_bytes = BMP_SIZE * 3  # 192，正好是 4 的倍数，不需要行填充
    pixel_bytes = row_bytes * BMP_SIZE
    pixel_offset = 14 + 40
    out = bytearray()
    # BITMAPFILEHEADER
    out += struct.pack("<2sIHHI", b"BM", pixel_offset + pixel_bytes, 0, 0, pixel_offset)
    # BITMAPINFOHEADER（biHeight 为正，逐行自下而上；BI_RGB 无压缩）
    out += struct.pack(
        "<IiiHHIIiiII",
        40,
        BMP_SIZE,
        BMP_SIZE,
        1,  # planes
        24,  # bit count
        0,  # BI_RGB
        pixel_bytes,
        2835,  # 72 DPI
        2835,
        0,
        0,
    )
    for y in range(BMP_SIZE - 1, -1, -1):
        r, g, b = row_rgb(y)
        out += struct.pack("<3B", b, g, r) * BMP_SIZE
    return bytes(out)


# ---------------------------------------------------------------------------
# 贴图表改写
# ---------------------------------------------------------------------------


def to_absolute(model_dir: str, texture_path: str) -> str:
    """把贴图条目解析成源文件的绝对路径（用来定位要复制的文件）。

    空路径（PMX 中表示「无贴图」）与已经是绝对路径（含盘符）的条目原样返回。
    """
    if not texture_path:
        return texture_path
    if os.path.isabs(texture_path) or os.path.splitdrive(texture_path)[0]:
        return texture_path
    return os.path.abspath(os.path.join(model_dir, texture_path))


def build_textures_section(model: PmxModel, textures: list[str]) -> bytes:
    """按 PMX 布局重排贴图表原始字节：条目数 + 逐条长度前缀文本。"""
    out = bytearray()
    out += struct.pack("<i", len(textures))
    for path in textures:
        out += model.encode_text(path)
    return bytes(out)


def find_shared_toon(model: PmxModel) -> tuple[int, str]:
    """校验 15 个材质共用同一张外部 toon 纹理，返回 (纹理索引, 原路径)。

    不满足条件时抛 ValueError，说明具体是哪个材质、哪条要求没过。
    """
    count = model.material_count
    if count != EXPECTED_MATERIAL_COUNT or len(model.materials) != count:
        raise ValueError(
            f"材质数为 {count}（解析到 {len(model.materials)} 条），"
            f"本工具只处理恰好 {EXPECTED_MATERIAL_COUNT} 个材质共用一张外部 toon 纹理的模型"
        )
    shared_index: int | None = None
    for i, mat in enumerate(model.materials):
        if mat.toon_internal:
            raise ValueError(f"材质 {i}（{mat.name_local}）用的是内置 toon，不是外部 toon 纹理")
        if mat.toon_index < 0:
            raise ValueError(f"材质 {i}（{mat.name_local}）没有外部 toon 纹理索引")
        if shared_index is None:
            shared_index = mat.toon_index
        elif mat.toon_index != shared_index:
            raise ValueError(
                f"材质 {i}（{mat.name_local}）的 toon 纹理索引 {mat.toon_index} "
                f"与材质 0 的 {shared_index} 不同，15 个材质没有共用同一张 toon 纹理"
            )
    if shared_index is None or shared_index >= len(model.textures):
        raise ValueError(
            f"toon 纹理索引 {shared_index} 超出贴图表范围（共 {len(model.textures)} 项）"
        )
    source = model.textures[shared_index]
    if not source:
        raise ValueError(f"贴图表第 {shared_index} 项路径为空，不是外部 toon 纹理")
    return shared_index, source


# ---------------------------------------------------------------------------
# 输出自检
# ---------------------------------------------------------------------------


def verify_preserved_bytes(out: PmxModel, snapshot: dict) -> None:
    """确认只有 textures 段变化，其余原始字节逐字节不变。

    ``snapshot`` 是改写前抓下的副本：``raw_sections`` 全量、表情与表示枠的
    原始字节。不一致就抛 ValueError，调用方据此中止写盘。
    """
    before_raw: dict[str, bytes] = snapshot["raw_sections"]
    for key, raw in before_raw.items():
        if key == "textures":
            continue
        if out.raw_sections.get(key) != raw:
            raise ValueError(f"输出的 {key} 段与输入不一致")
    if out.raw_sections.get("textures") == before_raw.get("textures"):
        raise ValueError("textures 段没有任何变化")
    if [m.raw for m in out.morphs] != snapshot["morphs"]:
        raise ValueError("表情（morph）段与输入不一致")
    if [f.raw for f in out.frames] != snapshot["frames"]:
        raise ValueError("表示枠段与输入不一致")


# ---------------------------------------------------------------------------
# 命令行
# ---------------------------------------------------------------------------


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="make_cloud_toon_variant.py",
        description="为指定 PMX 生成独立的浅阴影 toon 对照模型（ToonCloud-soft.pmx + CToon-soft.bmp + textures 贴图副本）。",
    )
    parser.add_argument("input_pmx", help="输入 PMX 模型路径（只读，不修改）")
    parser.add_argument("output_dir", help="输出目录（不存在则创建）")
    args = parser.parse_args(argv)

    input_path = Path(args.input_pmx)
    if not input_path.is_file():
        print(f"错误：输入模型不存在或不是文件: {input_path}", file=sys.stderr)
        return 2

    output_dir = Path(args.output_dir)
    out_model = output_dir / OUTPUT_MODEL_NAME
    out_bmp = output_dir / OUTPUT_TOON_NAME

    try:
        model = PmxModel.from_file(input_path)
    except PmxError as exc:
        print(f"错误：无法读取 PMX: {exc}", file=sys.stderr)
        return 2

    snapshot = {
        "raw_sections": dict(model.raw_sections),
        "morphs": [m.raw for m in model.morphs],
        "frames": [f.raw for f in model.frames],
    }

    try:
        toon_index, toon_source = find_shared_toon(model)
    except ValueError as exc:
        print(f"错误：{exc}", file=sys.stderr)
        return 2

    model_dir = os.path.dirname(os.path.abspath(str(input_path)))

    # 贴图表改写计划：(源贴图绝对路径, 输出副本文件, 写进 PMX 的相对条目)
    copies: list[tuple[str, Path, str]] = []
    new_textures: list[str] = []
    for index, entry in enumerate(model.textures):
        if index == toon_index:
            new_textures.append(OUTPUT_TOON_NAME)
            continue
        if not entry:
            new_textures.append(entry)
            continue
        source = to_absolute(model_dir, entry)
        ext = os.path.splitext(source)[1]
        target = output_dir / TEXTURES_SUBDIR / f"tex-{index}{ext}"
        pmx_entry = f"{TEXTURES_SUBDIR}\\{target.name}"
        copies.append((source, target, pmx_entry))
        new_textures.append(pmx_entry)

    # 写盘前先确认依赖齐全、目标不冲突
    missing = [source for source, _, _ in copies if not os.path.isfile(source)]
    if missing:
        print("错误：贴图依赖文件不存在: " + ", ".join(missing), file=sys.stderr)
        return 2
    targets = [out_model, out_bmp] + [target for _, target, _ in copies]
    existing = [p for p in targets if p.exists()]
    if existing:
        print(
            "错误：输出文件已存在，拒绝覆盖: " + ", ".join(str(p) for p in existing),
            file=sys.stderr,
        )
        return 3

    model.textures = new_textures
    model.raw_sections["textures"] = build_textures_section(model, new_textures)

    try:
        out_bytes = model.to_bytes()
        verify_preserved_bytes(PmxModel.from_bytes(out_bytes), snapshot)
    except (PmxError, ValueError) as exc:
        print(f"错误：写出前自检失败，未写入任何文件: {exc}", file=sys.stderr)
        return 4

    bmp_bytes = build_toon_bmp()
    try:
        output_dir.mkdir(parents=True, exist_ok=True)
        if copies:
            (output_dir / TEXTURES_SUBDIR).mkdir(parents=True, exist_ok=True)
        for source, target, _ in copies:
            with open(source, "rb") as f:  # 原始字节复制，不做转换
                data = f.read()
            with open(target, "xb") as f:
                f.write(data)
        with open(out_bmp, "xb") as f:
            f.write(bmp_bytes)
        with open(out_model, "xb") as f:
            f.write(out_bytes)
    except FileExistsError as exc:
        print(f"错误：输出文件已存在，拒绝覆盖: {exc.filename}", file=sys.stderr)
        return 3
    except OSError as exc:
        print(f"错误：写入输出文件失败: {exc}", file=sys.stderr)
        return 1

    print(f"输出模型: {os.path.abspath(str(out_model))}（{len(out_bytes)} 字节）")
    print(
        f"输出 toon 查找表: {os.path.abspath(str(out_bmp))}"
        f"（{len(bmp_bytes)} 字节，24 位 {BMP_SIZE}x{BMP_SIZE}）"
    )
    print(f"toon 纹理索引 {toon_index}: {toon_source} -> 条目 {OUTPUT_TOON_NAME}")
    for source, target, pmx_entry in copies:
        print(f"贴图副本: {source} -> {os.path.abspath(str(target))}（条目 {pmx_entry}）")
    print(f"贴图表共 {len(new_textures)} 项；原模型目录 {model_dir}（输入模型与其贴图未做任何修改）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
