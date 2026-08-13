"""Generate Windows and Qt raster derivatives for the editable SVG icon sources.

The SVG files under resources/icons remain the design sources.  This script keeps
the current sidebar geometry reproducible without adding an SVG runtime dependency
to the Qt application.  Export the app SVG to its 1024 px PNG before running this
script when the app mark itself changes.
"""

from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
ICON_ROOT = ROOT / "resources" / "icons"
SIDEBAR_ROOT = ICON_ROOT / "sidebar"
PREVIEW_PATH = ROOT / ".ref" / "ui" / "selected-icon-set-v2.png"

CANVAS_SIZE = 384
OUTPUT_SIZE = 96
SCALE = CANVAS_SIZE / 24
WHITE = (255, 255, 255, 255)
NAVY = (29, 30, 55, 255)
MUTED = (200, 202, 213, 255)
ORANGE = (243, 115, 33, 255)
STROKE = round(1.8 * SCALE)


def xy(values):
    return tuple(round(value * SCALE) for value in values)


def new_icon():
    return Image.new("RGBA", (CANVAS_SIZE, CANVAS_SIZE), (0, 0, 0, 0))


def save_icon(image, name):
    white = image.resize((OUTPUT_SIZE, OUTPUT_SIZE), Image.Resampling.LANCZOS)
    white.save(SIDEBAR_ROOT / f"{name}.png")

    muted = Image.new("RGBA", white.size, MUTED)
    muted.putalpha(white.getchannel("A"))
    muted.save(SIDEBAR_ROOT / f"{name}-muted.png")


def draw_live():
    image = new_icon()
    draw = ImageDraw.Draw(image)
    for left, top in ((3.5, 3.5), (13.5, 3.5), (3.5, 13.5), (13.5, 13.5)):
        draw.rounded_rectangle(
            xy((left, top, left + 7, top + 7)),
            radius=round(1.4 * SCALE),
            outline=WHITE,
            width=STROKE,
        )
    save_icon(image, "live")


def draw_playback():
    image = new_icon()
    draw = ImageDraw.Draw(image)
    draw.ellipse(xy((5, 3.5, 19, 17.5)), outline=WHITE, width=STROKE)
    draw.polygon([xy((10.3, 7.6)), xy((14.8, 10.5)), xy((10.3, 13.4))], fill=WHITE)
    draw.line([xy((4, 20)), xy((20, 20))], fill=WHITE, width=STROKE)
    draw.line([xy((7, 18.5)), xy((7, 21.5))], fill=WHITE, width=STROKE)
    draw.line([xy((17, 18.5)), xy((17, 21.5))], fill=WHITE, width=STROKE)
    save_icon(image, "playback")


def draw_events():
    image = new_icon()
    draw = ImageDraw.Draw(image)
    points = [
        xy((4.5, 16.4)),
        xy((6.2, 13.9)),
        xy((6.2, 9.8)),
        xy((6.5, 6.7)),
        xy((8.9, 4.0)),
        xy((12, 4.0)),
        xy((15.1, 4.0)),
        xy((17.5, 6.7)),
        xy((17.8, 9.8)),
        xy((17.8, 13.9)),
        xy((19.5, 16.4)),
        xy((4.5, 16.4)),
    ]
    draw.line(points, fill=WHITE, width=STROKE, joint="curve")
    draw.arc(xy((9.3, 16.1, 14.7, 21.5)), start=10, end=170, fill=WHITE, width=STROKE)
    draw.line([xy((12, 3)), xy((12, 1.8))], fill=WHITE, width=STROKE)
    save_icon(image, "events")


def draw_devices():
    image = new_icon()
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle(xy((3.5, 4, 10.5, 10)), radius=round(1.4 * SCALE), outline=WHITE, width=STROKE)
    draw.rounded_rectangle(xy((13.5, 14, 20.5, 20)), radius=round(1.4 * SCALE), outline=WHITE, width=STROKE)
    draw.line([xy((10.5, 7)), xy((13.5, 7)), xy((17, 10.5)), xy((17, 14))], fill=WHITE, width=STROKE, joint="curve")
    draw.line([xy((7, 10)), xy((7, 17)), xy((9, 19)), xy((13.5, 19))], fill=WHITE, width=STROKE, joint="curve")
    draw.ellipse(xy((6.2, 6.2, 7.8, 7.8)), fill=WHITE)
    draw.ellipse(xy((16.2, 16.2, 17.8, 17.8)), fill=WHITE)
    save_icon(image, "devices")


def draw_settings():
    image = new_icon()
    draw = ImageDraw.Draw(image)
    for y, knob_x in ((6, 13), (12, 9), (18, 16)):
        draw.line([xy((4, y)), xy((20, y))], fill=WHITE, width=STROKE)
        draw.ellipse(xy((knob_x - 2, y - 2, knob_x + 2, y + 2)), fill=NAVY, outline=WHITE, width=STROKE)
    save_icon(image, "settings")


def generate_app_ico():
    app_png = ICON_ROOT / "veda-vms-app-icon-v2.png"
    if not app_png.exists():
        raise FileNotFoundError(f"Export the app SVG first: {app_png}")
    with Image.open(app_png) as source:
        source.convert("RGBA").save(
            ICON_ROOT / "veda-vms-app-icon-v2.ico",
            format="ICO",
            sizes=[(16, 16), (20, 20), (24, 24), (32, 32), (40, 40), (48, 48), (64, 64), (128, 128), (256, 256)],
        )


def create_preview():
    preview = Image.new("RGBA", (1120, 360), (245, 246, 248, 255))
    app = Image.open(ICON_ROOT / "veda-vms-app-icon-v2.png").convert("RGBA").resize((240, 240), Image.Resampling.LANCZOS)
    preview.alpha_composite(app, (56, 60))

    sidebar = Image.new("RGBA", (720, 280), NAVY)
    for index, name in enumerate(("live", "playback", "events", "devices", "settings")):
        icon = Image.open(SIDEBAR_ROOT / f"{name}.png").convert("RGBA").resize((64, 64), Image.Resampling.LANCZOS)
        if index != 0:
            icon = Image.open(SIDEBAR_ROOT / f"{name}-muted.png").convert("RGBA").resize((64, 64), Image.Resampling.LANCZOS)
        sidebar.alpha_composite(icon, (38, 22 + index * 50))
    draw = ImageDraw.Draw(sidebar)
    draw.rectangle((0, 18, 4, 82), fill=ORANGE)
    preview.alpha_composite(sidebar, (344, 40))
    PREVIEW_PATH.parent.mkdir(parents=True, exist_ok=True)
    preview.convert("RGB").save(PREVIEW_PATH, quality=94)


def main():
    SIDEBAR_ROOT.mkdir(parents=True, exist_ok=True)
    draw_live()
    draw_playback()
    draw_events()
    draw_devices()
    draw_settings()
    generate_app_ico()
    create_preview()


if __name__ == "__main__":
    main()
