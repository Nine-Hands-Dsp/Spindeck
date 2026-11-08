# Spindeck - illustrations de la notice, tirees des captures reelles du plugin (outil tools/render).
# Usage : python3 tools/manual_assets_gen.py <dossier des captures>
import os, sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter

SRC = sys.argv[1]
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "tools", "manual_assets")
FONT = os.path.join(OUT, "fonts", "Poppins-Bold.ttf")


def load(name): return Image.open(os.path.join(SRC, name)).convert("RGB")


big = load("manual_ui_2x.png")                     # 3200 x 1800 (interface 1600 x 900 en 2x)
S = big.width / 1600.0


def crop(x0, y0, x1, y1, name, maxw=None):
    im = big.crop((int(x0 * S), int(y0 * S), int(x1 * S), int(y1 * S)))
    if maxw and im.width > maxw: im = im.resize((maxw, int(im.height * maxw / im.width)), Image.LANCZOS)
    im.save(os.path.join(OUT, name))
    return im


# couverture : interface entiere, coins arrondis, reflet sur sol noir, halo cyan tres doux qui retombe a zero
ui = big.resize((1800, int(1800 * big.height / big.width)), Image.LANCZOS)
pad = 120
W, H = ui.width + 2 * pad, ui.height + 2 * pad + int(ui.height * 0.28)
cov = Image.new("RGB", (W, H), (0, 0, 0))
halo = Image.new("L", (W, H), 0)
ImageDraw.Draw(halo).rounded_rectangle((pad + 60, pad + 60, pad + ui.width - 60, pad + ui.height - 60), 40, fill=90)
halo = halo.filter(ImageFilter.GaussianBlur(70))
cov.paste(Image.new("RGB", (W, H), (54, 197, 240)), (0, 0), halo)
mask = Image.new("L", ui.size, 0)
ImageDraw.Draw(mask).rounded_rectangle((0, 0, ui.width - 1, ui.height - 1), 22, fill=255)
cov.paste(ui, (pad, pad), mask)
refl = ui.transpose(Image.FLIP_TOP_BOTTOM).crop((0, 0, ui.width, int(ui.height * 0.28)))
fade = Image.linear_gradient("L").resize(refl.size).point(lambda v: int((255 - v) * 0.28))
fade = Image.composite(fade, Image.new("L", refl.size, 0), mask.transpose(Image.FLIP_TOP_BOTTOM).crop((0, 0, refl.width, refl.height)))
cov.paste(refl, (pad, pad + ui.height + 6), fade)
cov.save(os.path.join(OUT, "cover.png"))

# vue d'ensemble annotee
ann = load("ui_final.png")
d = ImageDraw.Draw(ann)
f = ImageFont.truetype(FONT, 15)
marks = [
    (1, 258, 26), (2, 1238, 47), (3, 1338, 47), (4, 970, 32), (5, 1538, 47),
    (6, 400, 104), (7, 240, 284), (8, 300, 342), (9, 150, 425), (10, 54, 455),
    (11, 54, 630), (12, 492, 398), (13, 492, 448), (14, 492, 512), (15, 492, 578),
    (16, 132, 590), (17, 1470, 588), (18, 682, 392), (19, 705, 500), (20, 826, 470),
    (21, 800, 570), (22, 852, 607), (23, 752, 272), (24, 836, 268), (25, 800, 790),
    (26, 1438, 47), (27, 468, 700), (28, 712, 47),
]
for n, x, y in marks:
    r = 12
    d.ellipse((x - r, y - r, x + r, y + r), fill=(255, 194, 61), outline=(0, 0, 0), width=2)
    d.text((x, y + 1), str(n), fill=(0, 0, 0), font=f, anchor="mm")
ann.save(os.path.join(OUT, "annotated.png"))

# details
crop(12, 242, 602, 632, "deck_a.png", 1400)
crop(12, 242, 602, 370, "deck_header.png", 1400)
crop(82, 368, 372, 628, "platter.png", 900)
crop(395, 372, 592, 625, "deck_buttons.png", 700)
crop(612, 242, 988, 632, "mixer.png", 1000)
crop(0, 0, 1600, 52, "topbar.png", 1800)
crop(10, 56, 1590, 234, "waveforms.png", 1800)
crop(12, 642, 1588, 892, "library.png", 1800)
crop(78, 520, 198, 630, "start_lamp.png", 500)
crop(285, 572, 355, 630, "scratch_hand.png", 400)
crop(14, 360, 80, 630, "pitch.png", 300)

# skins (captures 1x de chaque skin)
names = ["Midnight", "Neon Club", "Amber Mono", "Pro Red / Blue"]
tiles = []
for i in range(4):
    im = load("ui_theme_%d.png" % i).resize((800, 450), Image.LANCZOS)
    t = Image.new("RGB", (800, 500), (255, 255, 255))
    t.paste(im, (0, 50))
    ImageDraw.Draw(t).text((0, 8), names[i], fill=(27, 31, 42), font=ImageFont.truetype(FONT, 28))
    tiles.append(t)
sk = Image.new("RGB", (1640, 1040), (255, 255, 255))
for i, t in enumerate(tiles): sk.paste(t, ((i % 2) * 840, (i // 2) * 540))
sk.save(os.path.join(OUT, "skins.png"))
print("assets ok")
