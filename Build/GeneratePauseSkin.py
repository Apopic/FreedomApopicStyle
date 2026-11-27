"""Generate the default pause-menu skin PNGs. Run with Pillow installed."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "Android/app/src/main/assets/Default/Image/Playing/PauseMenu"
FONT = ROOT / "Android/app/src/main/assets/Default/Font/MPLUS1p-Medium.ttf"
if not FONT.is_file():
    FONT = ROOT / "x64/Release/FreedomApopicStyle/Skins/Default/Font/MPLUS1p-Medium.ttf"
SCALE = 2


def image(size):
    canvas = Image.new("RGBA", (size[0] * SCALE, size[1] * SCALE))
    return canvas, ImageDraw.Draw(canvas)


def box(draw, rect, radius, fill, outline=None, width=1):
    draw.rounded_rectangle(tuple(int(v * SCALE) for v in rect), radius * SCALE,
                           fill=fill, outline=outline, width=width * SCALE)


def text(draw, position, value, size, fill):
    draw.text(tuple(v * SCALE for v in position), value,
              font=ImageFont.truetype(str(FONT), size * SCALE), fill=fill, anchor="mm")


def save(canvas, name):
    canvas.resize((canvas.width // SCALE, canvas.height // SCALE), Image.Resampling.LANCZOS).save(DEST / name)


def main():
    DEST.mkdir(parents=True, exist_ok=True)
    Image.new("RGBA", (16, 16), (14, 20, 31, 170)).save(DEST / "Shade.png")
    canvas, draw = image((720, 480))
    box(draw, (4, 4, 716, 476), 32, "#303030", "#858585", 4)
    text(draw, (360, 80), "PAUSE", 44, "#ffffff")
    text(draw, (360, 440), "↑↓  選択     Enter  決定     TAB / Esc  再開", 16, "#cccccc")
    save(canvas, "Panel.png")
    for name, label in [("Resume.png", "続ける"), ("Restart.png", "最初から"), ("Exit.png", "選曲へ戻る")]:
        canvas, draw = image((420, 64))
        box(draw, (2, 2, 418, 62), 20, "#454545", "#747474", 2)
        text(draw, (210, 33), label, 28, "#ffffff")
        save(canvas, name)
    canvas, draw = image((444, 80))
    box(draw, (3, 3, 441, 77), 25, None, "#eeeeee", 3)
    save(canvas, "Cursor.png")


if __name__ == "__main__":
    main()
