"""Build the deterministic 40-second MWFL showcase loop.

Requires Pillow. Run from the repository root with the bundled Codex Python or
any Python 3 installation that provides Pillow.
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
SHOWCASE = ROOT / "docs" / "images" / "showcase"
OUTPUT = SHOWCASE / "mwfl-showcase-40s.gif"
SIZE = (960, 600)
SECONDS_PER_SCENE = 8

SCENES = [
    (
        SHOWCASE / "markdown-editor-1440x900.png",
        "RESPONSIVE NATIVE UI",
        "Resize confidently  •  Per-Monitor V2 DPI  •  real HWNDs",
        "resize",
    ),
    (
        SHOWCASE / "markdown-preview-1440x900.png",
        "MARKDOWN EDITOR",
        "Edit ↔ Preview  •  Scintilla + offline WebView2  •  atomic saves",
        "cover",
    ),
    (
        SHOWCASE / "explorer-1440x900.png",
        "EXPLORER WORKFLOW",
        "Tree + virtual list  •  tabs  •  splitter  •  native commands",
        "cover",
    ),
    (
        SHOWCASE / "docking-floating-1440x900.png",
        "DOCKING WORKSPACE",
        "Dock  •  float  •  auto-hide  •  persist  •  keyboard operation",
        "cover",
    ),
    (
        SHOWCASE / "desktop-integration-1440x900.png",
        "DESKTOP INTEGRATION",
        "Drop files  •  modern dialogs  •  clipboard  •  window placement",
        "cover",
    ),
]


def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    windows = Path("C:/Windows/Fonts")
    candidate = windows / ("seguisb.ttf" if bold else "segoeui.ttf")
    try:
        return ImageFont.truetype(str(candidate), size)
    except OSError:
        return ImageFont.load_default()


def cover(source: Image.Image, scale: float) -> Image.Image:
    target_ratio = SIZE[0] / SIZE[1]
    source_ratio = source.width / source.height
    if source_ratio > target_ratio:
        height = SIZE[1]
        width = round(height * source_ratio)
    else:
        width = SIZE[0]
        height = round(width / source_ratio)
    width = round(width * scale)
    height = round(height * scale)
    resized = source.resize((width, height), Image.Resampling.LANCZOS)
    left = (width - SIZE[0]) // 2
    top = (height - SIZE[1]) // 2
    return resized.crop((left, top, left + SIZE[0], top + SIZE[1]))


def render_scene(path: Path, eyebrow: str, caption: str, mode: str) -> list[Image.Image]:
    source = Image.open(path).convert("RGB")
    frames: list[Image.Image] = []
    for index in range(SECONDS_PER_SCENE):
        if mode == "resize":
            scale = 0.78 + index * 0.03
            window = source.resize((round(SIZE[0] * scale), round(SIZE[1] * scale)),
                                   Image.Resampling.LANCZOS)
            frame = Image.new("RGB", SIZE, (13, 30, 58))
            frame.paste(window, ((SIZE[0] - window.width) // 2,
                                 (SIZE[1] - window.height) // 2))
        else:
            frame = cover(source, 1.0 + index * 0.003)
        overlay = Image.new("RGBA", SIZE, (0, 0, 0, 0))
        draw = ImageDraw.Draw(overlay)
        draw.rectangle((0, 0, SIZE[0], 54), fill=(7, 18, 42, 220))
        draw.rectangle((0, SIZE[1] - 92, SIZE[0], SIZE[1]), fill=(7, 18, 42, 224))
        draw.text((28, 15), "MWFL 0.1  ·  PUBLIC PREVIEW", font=font(20, True), fill=(121, 212, 255, 255))
        draw.text((28, SIZE[1] - 78), eyebrow, font=font(25, True), fill=(255, 255, 255, 255))
        draw.text((28, SIZE[1] - 42), caption, font=font(19), fill=(210, 227, 247, 255))
        frames.append(Image.alpha_composite(frame.convert("RGBA"), overlay).convert("P", palette=Image.Palette.ADAPTIVE, colors=192))
    return frames


def main() -> None:
    missing = [str(path) for path, _, _, _ in SCENES if not path.is_file()]
    if missing:
        raise SystemExit("Missing showcase inputs:\n" + "\n".join(missing))
    frames = [frame for scene in SCENES for frame in render_scene(*scene)]
    frames[0].save(
        OUTPUT,
        save_all=True,
        append_images=frames[1:],
        duration=1000,
        loop=0,
        optimize=True,
        disposal=2,
    )
    print(f"Wrote {OUTPUT} ({len(frames)} frames, {len(frames)} seconds)")


if __name__ == "__main__":
    main()
