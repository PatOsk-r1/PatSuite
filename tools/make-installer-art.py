# Builds the installer look: neon-green wizard images and the license page (Pat Osk banner,
# a plain-language "is this legal?" note, then the full AGPL text, unchanged).
# Run: python tools/make-installer-art.py   (needs Pillow). Output goes to installer/art.
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "installer", "art")
LICENSE = os.path.join(os.path.dirname(ROOT), "PatWave", "LICENSE")
FONTS = os.path.join(os.environ.get("WINDIR", r"C:\Windows"), "Fonts")

NEON = (57, 255, 20)
MINT = (168, 255, 176)
DEEP = (3, 10, 6)


def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), size)


def glow(base, layer, radii=(18, 8, 3)):
    """Adds layer to base with a blurred halo, like a neon tube."""
    for r in radii:
        base.alpha_composite(layer.filter(ImageFilter.GaussianBlur(r)))
    base.alpha_composite(layer)


def grid(img, horizon, colour, alpha, spacing=36):
    """Retro perspective floor grid below the horizon line."""
    w, h = img.size
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    cx = w / 2
    for i in range(-30, 31):
        x = cx + i * spacing
        d.line([(cx + i * 4, horizon), (cx + (x - cx) * 6, h)], fill=colour + (alpha,), width=1)
    y, step = horizon, 3.0
    while y < h:
        d.line([(0, y), (w, y)], fill=colour + (alpha,), width=1)
        step *= 1.28
        y += step
    glow(img, layer, radii=(4,))


def scanlines(img, alpha=26):
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for y in range(0, img.size[1], 3):
        d.line([(0, y), (img.size[0], y)], fill=(0, 0, 0, alpha))
    img.alpha_composite(layer)


def wave(d, x0, x1, y, amp, colour, width=3, seed=1):
    rnd = random.Random(seed)
    pts = []
    for x in range(x0, x1):
        t = (x - x0) / max(1, x1 - x0)
        env = math.sin(math.pi * t)
        v = math.sin(t * 38) * 0.6 + math.sin(t * 91 + 1.3) * 0.3 + rnd.uniform(-0.1, 0.1)
        pts.append((x, y + v * amp * env))
    d.line(pts, fill=colour, width=width)


def wizard_image():
    w, h = 328, 628
    img = Image.new("RGBA", (w, h), DEEP + (255,))
    grid(img, int(h * 0.62), NEON, 60, spacing=22)
    sun = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(sun).ellipse([w / 2 - 90, h * 0.62 - 90, w / 2 + 90, h * 0.62 + 90],
                                outline=NEON + (200,), width=3)
    glow(img, sun)
    cover = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(cover).rectangle([0, h * 0.62 + 1, w, h], fill=DEEP + (170,))
    img.alpha_composite(cover)
    grid(img, int(h * 0.62), NEON, 70, spacing=22)

    text = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(text)
    big = font("bahnschrift.ttf", 92)
    for i, ch in enumerate("PAT"):
        bw = d.textlength(ch, font=big)
        d.text(((w - bw) / 2, 40 + i * 92), ch, font=big, fill=NEON + (255,))
    small = font("consolab.ttf", 30)
    s = "S U I T E"
    # Centred in the circle, sitting on the edge of the floor.
    d.text((w / 2, int(h * 0.62) - 8), s, font=small, fill=MINT + (255,), anchor="mb")
    wave(d, 30, w - 30, 560, 26, NEON + (255,), width=3)
    glow(img, text)
    tag = Image.new("RGBA", img.size, (0, 0, 0, 0))
    td = ImageDraw.Draw(tag)
    f = font("consola.ttf", 17)
    s = "// by PAT OSK"
    td.text(((w - td.textlength(s, font=f)) / 2, 600), s, font=f, fill=MINT + (230,))
    img.alpha_composite(tag)
    scanlines(img)
    img.convert("RGB").save(os.path.join(ART, "wizard.png"))


def small_image():
    s = 110
    img = Image.new("RGBA", (s, s), DEEP + (255,))
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    d.ellipse([12, 12, s - 12, s - 12], outline=NEON + (255,), width=4)
    f = font("bahnschrift.ttf", 58)
    d.text(((s - d.textlength("P", font=f)) / 2, 18), "P", font=f, fill=NEON + (255,))
    glow(img, layer, radii=(8, 3))
    img.convert("RGB").save(os.path.join(ART, "small.png"))


def back_image():
    w, h = 1200, 920
    img = Image.new("RGBA", (w, h), DEEP + (255,))
    grid(img, int(h * 0.70), NEON, 34, spacing=60)
    halo = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(halo).ellipse([w * 0.55, -h * 0.4, w * 1.4, h * 0.45], fill=NEON + (22,))
    img.alpha_composite(halo.filter(ImageFilter.GaussianBlur(120)))
    scanlines(img, alpha=34)
    img.convert("RGB").save(os.path.join(ART, "back.png"))


# Plain ASCII on purpose: box-drawing glyphs fall back to another font in the license box.
LETTERS = {
    "P": [r" ____  ", r"|  _ \ ", r"| |_) |", r"|  __/ ", r"|_|    "],
    "A": [r"    _    ", r"   / \   ", r"  / _ \  ", r" / ___ \ ", "/_/   \\_\\"],
    "T": [r" _____ ", r"|_   _|", r"  | |  ", r"  | |  ", r"  |_|  "],
    " ": ["   "] * 5,
    "O": [r"  ___  ", r" / _ \ ", r"| | | |", r"| |_| |", r" \___/ "],
    "S": [r" ____  ", r"/ ___| ", r"\___ \ ", r" ___) |", r"|____/ "],
    "K": [r" _  __", r"| |/ /", r"| ' / ", r"| . \ ", "|_|\\_\\"],
}
BANNER = ["".join(LETTERS[c][row] for c in "PAT OSK") for row in range(5)]

NOTE_ES = [
    ("b", "¡GRACIAS POR DESCARGAR PATSUITE!"),
    ("", "De verdad, gracias por bajarlo. Ojalá te sirva para hacer mucha música."),
    ("", "PatSuite es código abierto: si querés aportar ideas, arreglos o mejoras, "
         "el código está abierto para todos y cualquier aporte es bienvenido."),
    ("", "- Pat Osk"),
]
NOTE_EN = [
    ("b", "THANKS FOR DOWNLOADING PATSUITE!"),
    ("", "PatSuite is open source: ideas, fixes and improvements are always welcome."),
]


def rtf_text(s):
    out = []
    for ch in s:
        o = ord(ch)
        if ch in "\\{}":
            out.append("\\" + ch)
        elif o > 127:
            out.append("\\u%d?" % (o if o < 32768 else o - 65536))
        else:
            out.append(ch)
    return "".join(out)


def license_rtf():
    rtf = [r"{\rtf1\ansi\deff1",
           r"{\fonttbl{\f0\fmodern Consolas;}{\f1\fswiss Segoe UI;}}",
           r"{\colortbl;\red57\green255\blue20;\red168\green255\blue176;}",
           r"\f0\fs18\cf1 "]
    for line in BANNER:
        rtf.append(rtf_text(line) + r"\line ")
    rtf.append(r"\cf2\fs16 " + rtf_text("        P A T S U I T E   //   plugins libres") + r"\par\par ")
    for style, line in NOTE_ES + [("", "")] + NOTE_EN:
        if style == "b":
            rtf.append(r"\f1\fs22\b\cf1 " + rtf_text(line) + r"\b0\par ")
        else:
            rtf.append(r"\f1\fs17\cf2 " + rtf_text(line) + r"\par ")
    rtf.append(r"\par\f1\fs18\b\cf1 " + rtf_text("--- LICENCIA COMPLETA / FULL LICENSE ---") + r"\b0\par\par ")
    rtf.append(r"\f0\fs15\cf0 ")
    with open(LICENSE, encoding="utf-8") as f:
        for line in f.read().splitlines():
            rtf.append(rtf_text(line) + r"\line ")
    rtf.append("}")
    with open(os.path.join(ART, "license.rtf"), "w", encoding="ascii") as f:
        f.write("\n".join(rtf))


if __name__ == "__main__":
    os.makedirs(ART, exist_ok=True)
    wizard_image()
    small_image()
    back_image()
    license_rtf()
    print("ok")
