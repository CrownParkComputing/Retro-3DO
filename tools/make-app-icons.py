#!/usr/bin/env python3
"""Builds the launcher icon and every size Android, iOS and macOS ask for.

The same three-part icon as the rest of the family - the Retro script cut from
the Retro Recompilation logo, the machine's name under it, and the machine's
own mark below that. The 3DO's mark is three blocks stacked into a tower,
drawn here from numbers, in the family's blue, and owing nothing to the
console maker's own emblem.

    python3 tools/make-app-icons.py

Run from anywhere. Overwrites assets/branding/retro3do-icon-1024.png, the
Apple asset catalogue, the Android mipmaps and the Play store icon.
"""

from __future__ import annotations

import os
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOGO = os.path.join(HERE, "assets", "branding", "retro_recomp_logo.png")
FONT_CANDIDATES = [
    "/usr/share/fonts/liberation/LiberationSans-Bold.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
    "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
    "/Library/Fonts/Arial Bold.ttf",
]
FONT = next((f for f in FONT_CANDIDATES if os.path.exists(f)), None)
if FONT is None:
    raise SystemExit(
        "no usable bold sans font found; looked for:\n  "
        + "\n  ".join(FONT_CANDIDATES))

SIZE = 1024

BG_TOP = (8, 12, 26)
BG_BOTTOM = (3, 4, 9)

CHROME = [
    (232, 244, 255),
    (150, 205, 250),
    (56, 120, 220),
    (26, 60, 160),
    (120, 180, 240),
]

# The three faces of each block, light on top, mid on the left, deep on the
# right, so the tower reads as solid without a single line drawn on it.
FACE_TOP = (214, 232, 255)
FACE_LEFT = (61, 139, 255)
FACE_RIGHT = (28, 86, 190)
BLOCK_DARK = (10, 22, 48)


def vertical_gradient(size, colours):
    width, height = size
    grad = Image.new("RGB", (1, height))
    pixels = grad.load()
    steps = len(colours) - 1
    for y in range(height):
        position = y / max(1, height - 1) * steps
        index = min(int(position), steps - 1)
        blend = position - index
        start, end = colours[index], colours[index + 1]
        pixels[0, y] = tuple(
            int(start[c] + (end[c] - start[c]) * blend) for c in range(3)
        )
    return grad.resize((width, height))


def background():
    canvas = vertical_gradient((SIZE, SIZE), [BG_TOP, BG_BOTTOM]).convert("RGBA")
    glow = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(glow)
    draw.ellipse((60, 250, SIZE - 60, SIZE - 120), fill=(30, 80, 200, 110))
    draw.ellipse((200, 520, SIZE - 200, SIZE - 60), fill=(60, 140, 235, 95))
    glow = glow.filter(ImageFilter.GaussianBlur(120))
    return Image.alpha_composite(canvas, glow)


def retro_script(width):
    """The Retro script, cut out of the logo rather than redrawn."""
    logo = Image.open(LOGO).convert("RGBA")
    script = logo.crop((168, 0, 578, 92))
    pixels = script.load()
    for y in range(script.height):
        for x in range(script.width):
            r, g, b, a = pixels[x, y]
            if a and b > r:
                pixels[x, y] = (r, g, b, 0)
    height = round(script.height * width / script.width)
    return script.resize((width, height), Image.LANCZOS)


def chrome_text(text, width, height):
    size = 10
    font = ImageFont.truetype(FONT, size)
    while True:
        probe = ImageFont.truetype(FONT, size + 4)
        box = probe.getbbox(text)
        if box[2] - box[0] > width or box[3] - box[1] > height:
            break
        size += 4
        font = probe

    box = font.getbbox(text)
    pad = 18
    layer = Image.new("RGBA", (box[2] - box[0] + pad * 2, box[3] - box[1] + pad * 2))
    ImageDraw.Draw(layer).text(
        (pad - box[0], pad - box[1]), text, font=font, fill=(255, 255, 255, 255)
    )

    mask = layer.split()[3]
    fill = vertical_gradient(layer.size, CHROME).convert("RGBA")
    fill.putalpha(mask)

    outline = Image.new("RGBA", layer.size, (0, 0, 0, 0))
    outline.paste((12, 20, 48, 255), (0, 0), mask.filter(ImageFilter.MaxFilter(9)))
    return Image.alpha_composite(outline, fill)


def _cube(draw, cx, base_y, half, depth):
    """One isometric block: a rhombus lid over two side faces.

    cx is the centre column, base_y the y of the bottom vertex, half the
    half-width, depth the height of the vertical faces.
    """
    lid = half * 0.5  # the lid's half-height: a 2:1 rhombus
    front = (cx, base_y)
    left = (cx - half, base_y - lid)
    right = (cx + half, base_y - lid)
    back = (cx, base_y - 2 * lid)
    up = lambda p: (p[0], p[1] - depth)
    draw.polygon([front, left, up(left), up(front)], fill=FACE_LEFT)
    draw.polygon([front, right, up(right), up(front)], fill=FACE_RIGHT)
    draw.polygon([up(front), up(left), up(back), up(right)], fill=FACE_TOP)


def tower(width):
    """Three blocks stacked, each sitting on the lid of the one below."""
    scale = 4
    w = width * scale
    layer = Image.new("RGBA", (w, w), (0, 0, 0, 0))
    faces = ImageDraw.Draw(layer)

    half = w * 0.30
    depth = w * 0.19
    lid = half * 0.5
    gap = w * 0.012
    stack = 3 * depth + 2 * lid + 2 * gap
    base_y = (w + stack) / 2
    cx = w / 2
    for i in range(3):
        _cube(faces, cx, base_y - i * (depth + gap), half, depth)

    shape = layer.split()[3]
    rim = Image.new("RGBA", (w, w), (0, 0, 0, 0))
    rim.paste(BLOCK_DARK + (255,), (0, 0), shape.filter(ImageFilter.MaxFilter(9)))

    # A hairline of the rim colour between the faces, so the blocks keep their
    # edges once they are shrunk to a home-screen tile.
    edges = Image.new("RGBA", (w, w), (0, 0, 0, 0))
    pen = ImageDraw.Draw(edges)
    for i in range(3):
        by = base_y - i * (depth + gap)
        front = (cx, by)
        left = (cx - half, by - lid)
        right = (cx + half, by - lid)
        back = (cx, by - 2 * lid)
        up = lambda p: (p[0], p[1] - depth)
        pen.line([front, up(front)], fill=BLOCK_DARK + (255,), width=6)
        pen.line([up(left), up(front), up(right)], fill=BLOCK_DARK + (255,), width=6)
        pen.line([up(left), up(back), up(right)], fill=BLOCK_DARK + (255,), width=6)
    stamped = Image.alpha_composite(rim, layer)
    stamped = Image.alpha_composite(stamped, edges)
    return stamped.resize((width, width), Image.LANCZOS)


def artwork(width):
    layer = Image.new("RGBA", (width, width), (0, 0, 0, 0))

    script = retro_script(round(width * 0.80))
    name = chrome_text("3DO", round(width * 0.72), round(width * 0.19))
    mark = tower(round(width * 0.46))

    stack = script.height + name.height + mark.height + round(width * 0.05)
    top = max(0, (width - stack) // 2)

    layer.alpha_composite(script, ((width - script.width) // 2, top))
    top += script.height + round(width * 0.015)
    layer.alpha_composite(name, ((width - name.width) // 2, top))
    top += name.height + round(width * 0.030)
    layer.alpha_composite(mark, ((width - mark.width) // 2, top))
    return layer


def master():
    canvas = background()
    art = artwork(round(SIZE * 0.86))
    canvas.alpha_composite(art, ((SIZE - art.width) // 2, (SIZE - art.width) // 2))
    return canvas


def foreground():
    layer = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    art = artwork(round(SIZE * 0.62))
    layer.alpha_composite(art, ((SIZE - art.width) // 2, (SIZE - art.width) // 2))
    return layer


def rounded(image, radius_fraction=0.22):
    mask = Image.new("L", image.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        (0, 0, image.width - 1, image.height - 1),
        radius=round(image.width * radius_fraction),
        fill=255,
    )
    out = image.copy()
    out.putalpha(mask)
    return out


def main():
    icon = master()
    fore = foreground()
    flat = icon.convert("RGB")

    flat.save(os.path.join(HERE, "assets", "branding", "retro3do-icon-1024.png"))

    # Apple: one alpha-free 1024 for iOS, the 16..1024 ladder for macOS.
    apple = os.path.join(HERE, "assets", "apple", "Assets.xcassets", "AppIcon.appiconset")
    flat.save(os.path.join(apple, "icon-ios-1024.png"))
    for px in (16, 32, 64, 128, 256, 512, 1024):
        flat.resize((px, px), Image.LANCZOS).save(os.path.join(apple, f"icon-mac-{px}.png"))

    res = os.path.join(HERE, "android", "app", "src", "main", "res")
    legacy = {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}
    layers = {"mdpi": 108, "hdpi": 162, "xhdpi": 216, "xxhdpi": 324, "xxxhdpi": 432}
    for density, px in legacy.items():
        folder = os.path.join(res, f"mipmap-{density}")
        os.makedirs(folder, exist_ok=True)
        icon.resize((px, px), Image.LANCZOS).save(os.path.join(folder, "ic_launcher.png"))
        rounded(icon.resize((px, px), Image.LANCZOS), 0.5).save(
            os.path.join(folder, "ic_launcher_round.png"))
        fore.resize((layers[density],) * 2, Image.LANCZOS).save(
            os.path.join(folder, "ic_launcher_foreground.png"))

    plate = "#%02X%02X%02X" % BG_TOP
    path = os.path.join(res, "values", "ic_launcher_background.xml")
    with open(path, "w") as handle:
        handle.write(
            '<?xml version="1.0" encoding="utf-8"?>\n<resources>\n'
            '    <color name="ic_launcher_background">' + plate + "</color>\n"
            "</resources>\n")

    store = os.path.join(HERE, "play", "graphics", "store")
    if os.path.isdir(store):
        flat.resize((512, 512), Image.LANCZOS).save(os.path.join(store, "icon.png"))

    print("icons written")


if __name__ == "__main__":
    main()
