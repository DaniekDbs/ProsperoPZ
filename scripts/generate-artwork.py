"""Draw ProsperoPZ's original artwork procedurally (no game or third-party art).

Writes assets/generated/prosperopz-icon.png and prosperopz-world.png. Run
scripts/prepare-artwork.py afterwards to encode them for the PS5 loader and
the native UI. The same seed always produces the same images.
"""
from pathlib import Path
import math
import random
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets/generated'
LIME = (159, 211, 107)
BLOOD = (196, 52, 46)


def vertical_gradient(size, top, bottom):
    w, h = size
    image = Image.new('RGB', size)
    draw = ImageDraw.Draw(image)
    for y in range(h):
        t = y / max(1, h - 1)
        draw.line([(0, y), (w, y)], fill=tuple(int(a + (b - a) * t) for a, b in zip(top, bottom)))
    return image


def house(draw, x, y, w, h, wall, roof, window=None):
    """A Kentucky two-storey house silhouette with a gabled roof."""
    draw.rectangle([x, y, x + w, y + h], fill=wall)
    draw.polygon([(x - w * 0.08, y), (x + w / 2, y - h * 0.55), (x + w * 1.08, y)], fill=roof)
    draw.rectangle([x + w * 0.70, y - h * 0.42, x + w * 0.82, y - h * 0.12], fill=roof)  # chimney
    if window:
        for wx, wy in ((0.18, 0.18), (0.62, 0.18), (0.18, 0.58)):
            draw.rectangle([x + w * wx, y + h * wy, x + w * (wx + 0.18), y + h * (wy + 0.22)], fill=window)
        draw.rectangle([x + w * 0.60, y + h * 0.55, x + w * 0.78, y + h], fill=wall)


def pine(draw, x, base, height, color):
    for layer in range(4):
        top = base - height + layer * height * 0.2
        half = height * (0.18 + layer * 0.07)
        draw.polygon([(x, top), (x - half, top + height * 0.35), (x + half, top + height * 0.35)], fill=color)
    draw.rectangle([x - height * 0.03, base - height * 0.12, x + height * 0.03, base], fill=color)


def water_tower(draw, x, base, scale, color):
    tank_w, tank_h = 140 * scale, 90 * scale
    top = base - 330 * scale
    for leg in (-0.4, -0.13, 0.13, 0.4):
        draw.line([(x + leg * tank_w * 0.7, top + tank_h), (x + leg * tank_w * 1.2, base)], fill=color, width=max(2, int(8 * scale)))
    draw.line([(x - tank_w * 0.5, top + tank_h + 90 * scale), (x + tank_w * 0.5, top + tank_h + 90 * scale)], fill=color, width=max(2, int(6 * scale)))
    draw.rounded_rectangle([x - tank_w / 2, top, x + tank_w / 2, top + tank_h], radius=int(30 * scale), fill=color)
    draw.polygon([(x - tank_w * 0.55, top + 4), (x, top - 45 * scale), (x + tank_w * 0.55, top + 4)], fill=color)


def world(width=1672, height=941, seed=1993):
    rng = random.Random(seed)
    image = vertical_gradient((width, height), (10, 16, 13), (38, 46, 34))
    draw = ImageDraw.Draw(image, 'RGBA')
    # A low, hazy blood moon behind the town.
    moon = Image.new('RGBA', (width, height), (0, 0, 0, 0))
    mdraw = ImageDraw.Draw(moon)
    cx, cy, r = int(width * 0.74), int(height * 0.30), int(height * 0.13)
    for k in range(8, 0, -1):
        mdraw.ellipse([cx - r * k / 3, cy - r * k / 3, cx + r * k / 3, cy + r * k / 3], fill=(150, 50, 40, 10))
    mdraw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(212, 120, 92, 255))
    mdraw.ellipse([cx - r * 0.5, cy - r * 0.3, cx - r * 0.2, cy], fill=(190, 100, 80, 255))
    image.paste(moon, (0, 0), moon)
    for _ in range(140):
        x, y = rng.randrange(width), rng.randrange(int(height * 0.55))
        draw.point((x, y), fill=(200, 210, 190, rng.randrange(60, 160)))
    # Far tree line, town, water tower and near trees: three depth layers.
    far = (24, 33, 26)
    for x in range(-20, width + 40, 26):
        pine(draw, x + rng.randrange(-8, 8), int(height * 0.72), rng.randrange(90, 150), far)
    water_tower(draw, int(width * 0.86), int(height * 0.74), 0.9, (20, 27, 22))
    mid_wall, mid_roof = (16, 22, 18), (13, 18, 15)
    for i, x in enumerate(range(int(width * 0.42), int(width * 0.98), 150)):
        h = rng.randrange(95, 140)
        house(draw, x, int(height * 0.80) - h, rng.randrange(100, 130), h, mid_wall, mid_roof,
              window=(222, 180, 90) if i == 2 else (30, 38, 31))
    # Ground with a road vanishing to the right.
    draw.rectangle([0, int(height * 0.80), width, height], fill=(14, 19, 15))
    draw.polygon([(int(width * 0.30), height), (int(width * 0.62), int(height * 0.80)),
                  (int(width * 0.66), int(height * 0.80)), (int(width * 0.55), height)], fill=(28, 30, 27))
    for k in range(6):
        y0 = int(height * 0.82) + k * 26
        draw.line([(int(width * 0.58 - k * 18), y0), (int(width * 0.575 - k * 22), y0 + 12)], fill=(150, 140, 90, 120), width=3)
    near = (8, 11, 9)
    for x in (40, 160, 300, 1480, 1600):
        pine(draw, x, height, rng.randrange(330, 430), near)
    # Ground fog.
    fog = Image.new('RGBA', (width, height), (0, 0, 0, 0))
    fdraw = ImageDraw.Draw(fog)
    for _ in range(26):
        x, y = rng.randrange(-200, width), rng.randrange(int(height * 0.68), int(height * 0.92))
        fdraw.ellipse([x, y, x + rng.randrange(300, 600), y + rng.randrange(40, 90)], fill=(140, 170, 140, 22))
    fog = fog.filter(ImageFilter.GaussianBlur(24))
    image.paste(fog, (0, 0), fog)
    return image


def icon(size=1024):
    image = vertical_gradient((size, size), (22, 30, 22), (8, 11, 9))
    draw = ImageDraw.Draw(image, 'RGBA')
    c = size / 2
    # Lime ring: the launcher's accent and the "safe house" light.
    for k in range(18, 0, -1):
        draw.ellipse([c - size * 0.02 * k, c - size * 0.02 * k, c + size * 0.02 * k, c + size * 0.02 * k],
                     fill=(LIME[0], LIME[1], LIME[2], 6))
    draw.ellipse([size * 0.13, size * 0.13, size * 0.87, size * 0.87], outline=LIME, width=int(size * 0.035))
    # House silhouette with a lit window.
    house(draw, size * 0.32, size * 0.50, size * 0.36, size * 0.24, (231, 224, 205), (150, 58, 50),
          window=(240, 196, 92))
    draw.rectangle([size * 0.32 + size * 0.36 * 0.60, size * 0.50 + size * 0.24 * 0.55,
                    size * 0.32 + size * 0.36 * 0.78, size * 0.74], fill=(70, 50, 36))
    # A fire axe leaning across the doorway.
    angle = math.radians(-38)
    hx, hy = size * 0.30, size * 0.83
    length = size * 0.55
    ex, ey = hx + math.cos(angle) * length, hy + math.sin(angle) * length
    draw.line([(hx, hy), (ex, ey)], fill=(120, 78, 44), width=int(size * 0.035))
    head = [(ex - size * 0.02, ey - size * 0.06), (ex + size * 0.12, ey - size * 0.02),
            (ex + size * 0.10, ey + size * 0.10), (ex + size * 0.005, ey + size * 0.05)]
    draw.polygon(head, fill=BLOOD)
    draw.polygon([(ex + size * 0.09, ey - size * 0.015), (ex + size * 0.12, ey - size * 0.02),
                  (ex + size * 0.10, ey + size * 0.10), (ex + size * 0.075, ey + size * 0.09)], fill=(220, 224, 226))
    # A zombie hand rising from the ground line.
    ground = size * 0.80
    draw.rectangle([size * 0.18, ground, size * 0.82, ground + size * 0.02], fill=(48, 62, 40))
    hand = (118, 150, 92)
    draw.rectangle([size * 0.67, ground - size * 0.12, size * 0.72, ground], fill=hand)
    for f, lift in enumerate((0.16, 0.19, 0.18, 0.15)):
        x0 = size * (0.655 + f * 0.0185)
        draw.rounded_rectangle([x0, ground - size * lift, x0 + size * 0.016, ground - size * 0.10],
                               radius=int(size * 0.006), fill=hand)
    draw.polygon([(size * 0.66, ground - size * 0.10), (size * 0.62, ground - size * 0.13),
                  (size * 0.625, ground - size * 0.145), (size * 0.675, ground - size * 0.115)], fill=hand)
    return image


if __name__ == '__main__':
    OUT.mkdir(parents=True, exist_ok=True)
    icon().save(OUT / 'prosperopz-icon.png')
    world().save(OUT / 'prosperopz-world.png')
    print('Generated', OUT / 'prosperopz-icon.png', 'and', OUT / 'prosperopz-world.png')
