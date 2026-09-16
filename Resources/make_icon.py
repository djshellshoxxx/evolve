"""Generates Resources/icon.png - the MUTAGEN application icon.

Kept in the repo as a script rather than as a one-off binary drop so the mark
can be re-cut at another size, and so the next plugin in the range can start
from the same geometry with its own accent and its own colony.

The mark: a petri dish seen from above, a colony of cells inside it, and the
house diagonal notch in the top-left corner. Palette straight from theme.md.
"""
import zlib, struct, math
import numpy as np

S   = 512      # output size
SS  = 4        # supersampling factor
N   = S * SS

def srgb(hexstr):
    h = hexstr.lstrip('#')
    return np.array([int(h[i:i+2], 16) for i in (0, 2, 4)], dtype=np.float64) / 255.0

BG0     = srgb('0E1116')
BG1     = srgb('12161C')
STROKE  = srgb('2A303A')
ACCENT  = srgb('E8532A')
ACCENT2 = srgb('4FB6C4')
SUCCESS = srgb('7BC96F')
VIOLET  = srgb('9B7BD6')
TEXT    = srgb('E6E8EC')

y, x = np.mgrid[0:N, 0:N].astype(np.float64)
# work in 0..1 coordinates so every radius below reads as a fraction of the icon
u = (x + 0.5) / N
v = (y + 0.5) / N

img   = np.zeros((N, N, 3), dtype=np.float64)
alpha = np.zeros((N, N), dtype=np.float64)
px    = 1.0 / N          # one output pixel, in u/v units

def smooth(edge_dist, width=1.5):
    """Coverage from a signed distance: negative inside, positive outside."""
    return np.clip(0.5 - edge_dist / (width * px * SS), 0.0, 1.0)

def over(colour, cov):
    """Source-over composite of a flat colour with the given coverage."""
    global img, alpha
    c = cov[..., None]
    img[:] = img * (1.0 - c) + np.asarray(colour) * c
    alpha[:] = alpha * (1.0 - cov) + cov

def rounded_rect(cx, cy, hw, hh, r):
    dx = np.abs(u - cx) - (hw - r)
    dy = np.abs(v - cy) - (hh - r)
    dx = np.maximum(dx, 0.0)
    dy = np.maximum(dy, 0.0)
    inside = np.minimum(np.maximum(np.abs(u - cx) - hw, np.abs(v - cy) - hh), 0.0)
    return np.hypot(dx, dy) + inside - r * 0.0 - (r - r)  # outer distance
def rrect_sdf(cx, cy, hw, hh, r):
    qx = np.abs(u - cx) - (hw - r)
    qy = np.abs(v - cy) - (hh - r)
    return (np.hypot(np.maximum(qx, 0.0), np.maximum(qy, 0.0))
            + np.minimum(np.maximum(qx, qy), 0.0) - r)

def circle_sdf(cx, cy, r):
    return np.hypot(u - cx, v - cy) - r

# ---------------------------------------------------------------- body
# The tile itself: house window radius (6px on a 512 icon reads as nothing, so
# the icon uses the platform's own proportion instead).
body = rrect_sdf(0.5, 0.5, 0.5 - 2 * px, 0.5 - 2 * px, 0.21)
over(BG0, smooth(body))

# A cold top-left to warm bottom-right wash, so the flat field is not dead.
wash = np.clip((u * 0.6 + v * 0.4), 0.0, 1.0)[..., None]
tint = BG0 * (1.0 - wash) + BG1 * wash
img[:] = np.where((alpha > 0)[..., None], tint, img)

# ---------------------------------------------------------------- dish
DISH_R = 0.335
dish = circle_sdf(0.5, 0.5, DISH_R)
over(BG1, smooth(dish))

# culture medium: a faint radial lift toward the middle
med = np.clip(1.0 - np.hypot(u - 0.5, v - 0.5) / DISH_R, 0.0, 1.0) ** 2.2
inside = smooth(dish)
img[:] = img + (srgb('1F242D') - BG1) * (med * inside)[..., None] * 0.9

# dish rim: two concentric strokes, the outer one catching the accent at the top
rim_outer = np.abs(dish) - 0.0075
over(STROKE, smooth(rim_outer))
rim_inner = np.abs(circle_sdf(0.5, 0.5, DISH_R - 0.028)) - 0.0022
over(srgb('232833'), smooth(rim_inner) * 0.85)

# ---------------------------------------------------------------- colony
# Seven cells: the three species of the engine, sized and placed by hand so the
# cluster reads as one organism at 16px rather than as scattered dots.
cells = [
    # (cx,      cy,      r,      colour,  halo)
    (0.498,  0.464,  0.103,  ACCENT,  1.00),
    (0.378,  0.572,  0.064,  ACCENT2, 0.85),
    (0.614,  0.590,  0.054,  SUCCESS, 0.80),
    (0.592,  0.372,  0.040,  VIOLET,  0.70),
    (0.405,  0.386,  0.030,  ACCENT2, 0.60),
    (0.498,  0.664,  0.025,  ACCENT,  0.55),
    (0.683,  0.474,  0.021,  SUCCESS, 0.50),
]

# haloes first, so the cluster glows through the medium
for cx, cy, r, col, strength in cells:
    d = np.hypot(u - cx, v - cy)
    halo = np.clip(1.0 - d / (r * 3.4), 0.0, 1.0) ** 2.6
    img[:] = img + np.asarray(col)[None, None, :] * (halo * inside * 0.30 * strength)[..., None]

for cx, cy, r, col, strength in cells:
    c = circle_sdf(cx, cy, r)
    # body: darker at the rim, bright at the top-left, i.e. lit from above
    lift = np.clip(1.0 - np.hypot(u - (cx - r * 0.3), v - (cy - r * 0.35)) / (r * 1.7), 0, 1) ** 1.5
    fill = np.asarray(col)[None, None, :] * (0.62 + 0.55 * lift)[..., None]
    cov = smooth(c)
    img[:] = img * (1.0 - cov[..., None]) + fill * cov[..., None]
    alpha[:] = np.maximum(alpha, cov)
    # nucleus
    nuc = smooth(circle_sdf(cx, cy, r * 0.30))
    over(np.clip(col * 0.35, 0, 1), nuc * 0.75)

# ---------------------------------------------------------------- notch
# The signature: a 45-degree accent stroke in the top-left, the same mark the
# plugin window draws.
def segment_sdf(ax, ay, bx, by, half):
    pax, pay = u - ax, v - ay
    bax, bay = bx - ax, by - ay
    h = np.clip((pax * bax + pay * bay) / (bax * bax + bay * bay), 0.0, 1.0)
    return np.hypot(pax - bax * h, pay - bay * h) - half

notch = segment_sdf(0.105, 0.105, 0.205, 0.205, 0.0095)
over(ACCENT, smooth(notch) * smooth(body))

# ---------------------------------------------------------------- vignette
edge = np.clip((np.hypot(u - 0.5, v - 0.5) - 0.34) / 0.22, 0.0, 1.0) ** 1.6
img[:] = img * (1.0 - 0.25 * edge)[..., None]

# ---------------------------------------------------------------- downsample
img = np.clip(img, 0.0, 1.0)
rgba = np.dstack([img, np.clip(alpha, 0.0, 1.0)])
rgba = rgba.reshape(S, SS, S, SS, 4).mean(axis=(1, 3))
# un-premultiply nothing: everything above was composited straight, and the
# only partial alpha is the rounded corner, where the colour is already BG0.
out = (np.clip(rgba, 0, 1) * 255.0 + 0.5).astype(np.uint8)

raw = b''.join(b'\x00' + out[r].tobytes() for r in range(S))
def chunk(tag, data):
    return (struct.pack('>I', len(data)) + tag + data
            + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff))

png = (b'\x89PNG\r\n\x1a\n'
       + chunk(b'IHDR', struct.pack('>IIBBBBB', S, S, 8, 6, 0, 0, 0))
       + chunk(b'IDAT', zlib.compress(raw, 9))
       + chunk(b'IEND', b''))

import os, sys
dest = sys.argv[1]
open(dest, 'wb').write(png)
print('wrote', dest, len(png), 'bytes')
