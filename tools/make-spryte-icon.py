#!/usr/bin/env python3
"""Builds Spryte's app icon, and only Spryte's.

Deliberately NOT tools/make-app-icons.py. That script builds the whole
family's icon from one template -- the Retro script lifted from the Retro
Recompilation logo, the machine's name under it, the machine's mark below
that -- which is the same three-part composition on every app this account
publishes. Guideline 4.3(a) names exactly that: "the same source code or
assets as other apps". So this app's icon shares no element with it: no
wordmark, no Retro script, a different palette and a different subject.

The subject is the thing the machine's graphics hardware actually does. A
"spryte" is the 3DO's own name for its drawing primitive, and what made it
unusual in 1993 is that it is not a blitted rectangle: the CEL engine maps a
rectangular source into an *arbitrary four-corner quadrilateral*, corner by
corner, which is how 3DO games scaled and rotated sprites without a 3D
pipeline. So the icon is one texture being mapped into a quad -- the
operation the app is named after, drawn by doing it rather than illustrating
it.

    python3 tools/make-spryte-icon.py

Overwrites the icon-*.png files in assets/apple/Assets.xcassets/AppIcon.appiconset.
Contents.json is untouched, so the filenames must not change.
"""

from __future__ import annotations

import os

from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ICONSET = os.path.join(HERE, "assets", "apple", "Assets.xcassets", "AppIcon.appiconset")

SIZE = 1024
SS = 2  # supersample, then downsample once at the end

# A plum/indigo ground, chosen to sit away from the rest of the family: the
# others are navy with a blue mark, DOSDeck is amber on near-black. Never flat
# black -- a mark on pure #000 looks pasted on rather than lit.
BG_TOP = (36, 18, 54)
BG_BOTTOM = (12, 6, 22)

# The 3DO's mark was a spectrum sweep, and the app's own interface uses the
# same three hues. Carried here so the icon and the shelf agree.
SPECTRUM = [(250, 79, 120), (253, 158, 61), (79, 209, 232)]


def lerp(a, b, t):
    return tuple(round(x + (y - x) * t) for x, y in zip(a, b))


def spectrum_at(t):
    """Three-stop sweep: pink -> amber -> cyan, interpolated by hue.

    Not a straight RGB blend. Amber and cyan are close to opposite, so the
    midpoint between them in RGB is a desaturated grey-green -- the sweep came
    out khaki through the middle and looked like dirt rather than a spectrum.
    Rotating the hue instead keeps every intermediate colour fully saturated,
    which is what a spectrum is.
    """
    import colorsys

    t = min(max(t, 0.0), 1.0)
    lo, hi, local = (0, 1, t * 2) if t < 0.5 else (1, 2, (t - 0.5) * 2)
    h1, s1, v1 = colorsys.rgb_to_hsv(*[c / 255 for c in SPECTRUM[lo]])
    h2, s2, v2 = colorsys.rgb_to_hsv(*[c / 255 for c in SPECTRUM[hi]])

    # Take the short way round the wheel, so pink -> amber runs through red
    # rather than all the way back through green.
    dh = h2 - h1
    if dh > 0.5:
        dh -= 1.0
    elif dh < -0.5:
        dh += 1.0

    h = (h1 + dh * local) % 1.0
    s = s1 + (s2 - s1) * local
    v = v1 + (v2 - v1) * local
    return tuple(round(c * 255) for c in colorsys.hsv_to_rgb(h, s, v))


def ground(size):
    img = Image.new("RGB", (size, size))
    d = ImageDraw.Draw(img)
    for y in range(size):
        d.line([(0, y), (size, y)], fill=lerp(BG_TOP, BG_BOTTOM, y / (size - 1)))
    # A soft off-centre glow, so the quad reads as lit from its own surface.
    glow = Image.new("L", (size, size), 0)
    ImageDraw.Draw(glow).ellipse(
        [size * 0.12, size * 0.06, size * 0.88, size * 0.82], fill=70)
    glow = glow.filter(ImageFilter.GaussianBlur(size * 0.17))
    img.paste(Image.new("RGB", (size, size), (120, 70, 180)), (0, 0), glow)
    return img


def source_texture(n):
    """The rectangular source a spryte is mapped from: a cel of pixel cells.

    Six cells, not sixteen. The icon has to survive 60px on a home screen, and
    a fine grid turns to mush there -- worse, it starts reading as a chart.
    """
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cells = 6
    step = n / cells
    for row in range(cells):
        for col in range(cells):
            # Hue across the columns, light across the rows. Running the hue
            # diagonally and then darkening toward the plum ground dragged
            # amber through it and came out olive; keeping the two axes
            # separate holds every cell on the sweep.
            c = spectrum_at(col / (cells - 1))
            shade = row / (cells - 1)
            c = lerp(c, (255, 255, 255), 0.22 * (1 - shade))
            c = lerp(c, (90, 40, 120), 0.20 * shade)
            x0, y0 = col * step, row * step
            pad = step * 0.045
            d.rectangle([x0 + pad, y0 + pad, x0 + step - pad, y0 + step - pad],
                        fill=c + (255,))
    return img


def perspective_coeffs(dst, src):
    """Coefficients for Image.transform(PERSPECTIVE), which maps dst -> src."""
    import math

    m = []
    rhs = []
    for (dx, dy), (sx, sy) in zip(dst, src):
        m.append([dx, dy, 1, 0, 0, 0, -sx * dx, -sx * dy])
        rhs.append(sx)
        m.append([0, 0, 0, dx, dy, 1, -sy * dx, -sy * dy])
        rhs.append(sy)

    # Plain Gaussian elimination with partial pivoting -- eight equations, and
    # no reason to pull in numpy for it.
    n = 8
    for i in range(n):
        p = max(range(i, n), key=lambda r: abs(m[r][i]))
        if abs(m[p][i]) < 1e-12:
            raise ValueError("degenerate quad")
        m[i], m[p] = m[p], m[i]
        rhs[i], rhs[p] = rhs[p], rhs[i]
        for r in range(i + 1, n):
            f = m[r][i] / m[i][i]
            if f:
                for c in range(i, n):
                    m[r][c] -= f * m[i][c]
                rhs[r] -= f * rhs[i]
    out = [0.0] * n
    for i in reversed(range(n)):
        s = rhs[i] - sum(m[i][c] * out[c] for c in range(i + 1, n))
        out[i] = s / m[i][i]
    assert all(math.isfinite(v) for v in out)
    return out


def build(size):
    s = size * SS
    img = ground(s).convert("RGBA")

    tex_n = int(s * 0.62)
    tex = source_texture(tex_n)

    # The four corners the source is mapped onto. Not a rotation and not a
    # scale: each corner moves independently, which is the whole point of the
    # primitive -- a rectangle could not produce this. The top edge is pulled
    # well in and the bottom pushed out, so the mapping is unmistakable at a
    # glance instead of looking like a tilt.
    quad = [
        (s * 0.365, s * 0.210),   # top-left, pulled in
        (s * 0.672, s * 0.272),   # top-right, pulled in and dropped
        (s * 0.845, s * 0.782),   # bottom-right, thrown wide
        (s * 0.168, s * 0.688),   # bottom-left, thrown wide and lifted
    ]
    src = [(0, 0), (tex_n, 0), (tex_n, tex_n), (0, tex_n)]
    coeffs = perspective_coeffs(quad, src)
    warped = tex.transform((s, s), Image.PERSPECTIVE, coeffs,
                           resample=Image.BICUBIC)

    # Drop the quad onto the ground with a shadow under it, so it sits in the
    # icon rather than floating on it.
    shadow = warped.getchannel("A").filter(ImageFilter.GaussianBlur(s * 0.035))
    img.paste(Image.new("RGBA", (s, s), (0, 0, 0, 255)),
              (int(s * 0.012), int(s * 0.022)), shadow.point(lambda v: v * 0.55))
    img.alpha_composite(warped)

    # The quad's own outline, which is what makes the four corners readable at
    # 60px as a deliberate shape rather than a smear.
    edge = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    ImageDraw.Draw(edge).polygon(quad, outline=(255, 255, 255, 205),
                                 width=max(2, int(s * 0.009)))
    img.alpha_composite(edge)

    return img.convert("RGB").resize((size, size), Image.LANCZOS)


def main():
    icon = build(SIZE)
    written = []
    for name, px in [
        ("icon-ios-1024.png", 1024),
        ("icon-mac-1024.png", 1024),
        ("icon-mac-512.png", 512),
        ("icon-mac-256.png", 256),
        ("icon-mac-128.png", 128),
        ("icon-mac-64.png", 64),
        ("icon-mac-32.png", 32),
        ("icon-mac-16.png", 16),
    ]:
        out = os.path.join(ICONSET, name)
        (icon if px == 1024 else icon.resize((px, px), Image.LANCZOS)).save(out)
        written.append(f"{name} ({px}x{px})")
    print("wrote into " + ICONSET + ":")
    for w in written:
        print("  " + w)


if __name__ == "__main__":
    main()
