# Original procedural textures (numpy + PIL). No game art is used or copied.
# Run: python make_textures.py   -> textures/*.png
import numpy as np, pathlib
from PIL import Image
out = pathlib.Path(__file__).parent / 'textures'
rng = np.random.default_rng(7)

def blur(a, s):
    f = np.fft.fft2(a); h, w = a.shape
    fy = np.fft.fftfreq(h)[:, None]; fx = np.fft.fftfreq(w)[None, :]
    return np.real(np.fft.ifft2(f * np.exp(-2 * (np.pi * s) ** 2 * (fx ** 2 + fy ** 2))))

def norm(a):
    a = a - a.mean(); return a / (a.std() + 1e-9)

def stone(base, mortar_k, size=128, row=32, contrast=1.0):
    low = norm(blur(rng.standard_normal((size, size)), 6)) * 0.05 * contrast
    mid = norm(blur(rng.standard_normal((size, size)), 1.6)) * 0.035 * contrast
    grain = norm(rng.standard_normal((size, size))) * 0.015 * contrast
    lum = 1 + low + mid + grain
    block = np.ones((size, size))
    bw = size // 2
    for r in range(size // row):
        off = (r % 2) * (bw // 2)
        for c in range(2):
            x0 = (c * bw + off) % size
            v = 1 + rng.uniform(-0.05, 0.05) * contrast
            ys = slice(r * row, (r + 1) * row)
            xs = [(x0 + i) % size for i in range(bw)]
            block[ys, xs] = v
    lum = lum * block
    img = np.clip(np.array(base)[None, None, :] * lum[..., None], 0, 255)
    mort = np.zeros((size, size), bool)
    for r in range(size // row):
        mort[r * row:r * row + 2, :] = True
        off = (r % 2) * (bw // 2)
        for c in range(2):
            x = (c * bw + off) % size
            mort[r * row:(r + 1) * row, x:x + 2] = True if x + 2 <= size else mort[r * row:(r + 1) * row, x:x + 2]
            mort[r * row:(r + 1) * row, (x + 1) % size] = True
    img[mort] *= mortar_k
    # 1px highlight below horizontal mortar
    hl = np.roll(mort, 2, axis=0) & ~mort
    img[np.roll(np.zeros_like(mort), 0)] *= 1
    img[hl] = np.clip(img[hl] * 1.12, 0, 255)
    return Image.fromarray(img.astype(np.uint8), 'RGB')

def ring_image(w, h, face_img, outer, gold, hi, sh, inner=None, transparent_center=False, corner=None):
    a = np.zeros((h, w, 4), np.uint8)
    ft = np.array(face_img.resize((w, h))) if face_img is not None else None
    for y in range(h):
        for x in range(w):
            d = min(x, y, w - 1 - x, h - 1 - y)
            if d == 0: c = outer
            elif d == 1: c = gold
            elif d == 2:
                c = hi if (y - 2 < x - 2 and y <= h - 1 - x) or (x - 2 <= y - 2 and x <= h - 1 - y and x < w / 2 and y < h / 2) else sh
                if x > w / 2 or y > h / 2: c = sh
                if x < w / 2 and y < h / 2: c = hi
            else:
                if transparent_center: a[y, x] = (0, 0, 0, 0); continue
                c = tuple(ft[y, x]) if ft is not None else inner
            a[y, x] = (*c, 255)
    # clip the four outer corner pixels for a softly cut corner
    for (x, y) in [(0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1)]:
        a[y, x, 3] = 0
    return Image.fromarray(a, 'RGBA')

def tint(img, k):
    return Image.fromarray(np.clip(np.array(img).astype(float) * k, 0, 255).astype(np.uint8), 'RGB')

def build(theme):
    d = theme == 'dark'
    sfx = '' if d else '-light'
    stone_c = (62, 56, 49) if d else (216, 203, 172)
    bar_c = (44, 39, 35) if d else (210, 197, 165)
    st = stone(stone_c, 0.62 if d else 0.78); st.save(out / f'stone{sfx}.png', optimize=True)
    br = stone(bar_c, 0.55 if d else 0.88, contrast=0.8 if d else 0.4); br.save(out / f'stone-bar{sfx}.png', optimize=True)
    face = stone((84, 76, 66) if d else (230, 218, 188), 0.9, size=24, row=12, contrast=0.6)
    face_h = tint(face, 1.18 if d else 1.04)
    gold_f = stone((100, 82, 40) if d else (234, 204, 118), 0.9, size=24, row=12, contrast=0.6)
    P = dict(outer=(22, 17, 12) if d else (84, 62, 26), hi=(132, 118, 96) if d else (252, 244, 220), sh=(38, 32, 26) if d else (176, 156, 112))
    ring_image(24, 24, face, P['outer'], (150, 120, 58) if d else (140, 98, 24), P['hi'], P['sh']).save(out / f'btn{sfx}.png', optimize=True)
    ring_image(24, 24, face_h, P['outer'], (176, 142, 66) if d else (150, 106, 28), P['hi'], P['sh']).save(out / f'btn-hover{sfx}.png', optimize=True)
    ring_image(24, 24, gold_f, P['outer'], (238, 200, 96) if d else (112, 70, 10), (255, 226, 140) if d else (255, 240, 190), (120, 90, 30) if d else (150, 112, 40)).save(out / f'btn-checked{sfx}.png', optimize=True)
    # dock frame: 4px ring + stud corners, transparent center
    fr = np.zeros((16, 16, 4), np.uint8)
    cols = [(20, 16, 12), (176, 144, 72), (96, 80, 50), (30, 26, 22)] if d else [(70, 52, 20), (150, 108, 30), (204, 178, 112), (124, 104, 62)]
    for y in range(16):
        for x in range(16):
            dd = min(x, y, 15 - x, 15 - y)
            if dd < 4: fr[y, x] = (*cols[dd], 255)
    stud = (236, 200, 100) if d else (110, 70, 10)
    for cx, cy in [(1, 1), (14, 1), (1, 14), (14, 14)]:
        fr[cy, cx] = (*stud, 255)
    for cx, cy in [(0, 0), (15, 0), (0, 15), (15, 15)]:
        fr[cy, cx, 3] = 0
    Image.fromarray(fr, 'RGBA').save(out / f'frame{sfx}.png', optimize=True)

build('dark'); build('light')
for p in sorted(out.glob('*.png')): print(p.name, p.stat().st_size)
