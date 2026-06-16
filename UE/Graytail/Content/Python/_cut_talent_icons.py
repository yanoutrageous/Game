# -*- coding: utf-8 -*-
"""把 assets 里的天赋图标 sheet(菜单图标.png, 1024x1024, 2+3 布局, 透明底)
按连通域切成 5 个独立透明 PNG, 输出到 assets/ui/deploy/ui_icon_talent_*.png。
映射(按质心行列): 上排左→map(邻域感知) 上排右→mine(厚皮);
下排左→monster(威压) 中→extract(抢救条款) 右→event(议价)。
跑: python _cut_talent_icons.py
"""
import glob, os
import numpy as np
from PIL import Image
from scipy import ndimage

ASSETS = r"D:\CODE\NetEase Training\Game\assets"
OUT = os.path.join(ASSETS, "ui", "deploy")


def find_sheet():
    for p in glob.glob(os.path.join(ASSETS, "*.png")):
        b = os.path.basename(p)
        if any(ord(c) > 127 for c in b):
            im = Image.open(p).convert("RGBA")
            if im.size == (1024, 1024) and im.split()[-1].getextrema()[0] == 0:
                return p, im
    raise SystemExit("talent sheet not found")


def square_crop(im, bbox, pad_frac=0.06):
    x0, y0, x1, y1 = bbox
    w, h = x1 - x0, y1 - y0
    side = int(max(w, h) * (1 + pad_frac))
    cx, cy = (x0 + x1) // 2, (y0 + y1) // 2
    nx0, ny0 = cx - side // 2, cy - side // 2
    canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    region = im.crop((nx0, ny0, nx0 + side, ny0 + side))
    canvas.paste(region, (0, 0))
    return canvas.resize((256, 256), Image.LANCZOS)


def main():
    path, im = find_sheet()
    print("sheet:", repr(os.path.basename(path)), im.size)
    arr = np.array(im)
    mask = arr[:, :, 3] > 40
    labels, n = ndimage.label(mask)
    print("raw components:", n)
    comps = []
    for i in range(1, n + 1):
        ys, xs = np.where(labels == i)
        area = len(xs)
        if area < 3000:
            continue
        bbox = (xs.min(), ys.min(), xs.max() + 1, ys.max() + 1)
        cx, cy = (bbox[0] + bbox[2]) / 2, (bbox[1] + bbox[3]) / 2
        comps.append((area, cx, cy, bbox))
    print("kept components:", len(comps))
    # 行分组: 质心 y < 460 = 上排, 否则下排
    top = sorted([c for c in comps if c[2] < 460], key=lambda c: c[1])
    bot = sorted([c for c in comps if c[2] >= 460], key=lambda c: c[1])
    print("top row:", len(top), "bottom row:", len(bot))
    mapping = []
    if len(top) >= 2:
        mapping += [("map", top[0]), ("mine", top[1])]
    if len(bot) >= 3:
        mapping += [("monster", bot[0]), ("extract", bot[1]), ("event", bot[2])]
    os.makedirs(OUT, exist_ok=True)
    for key, comp in mapping:
        icon = square_crop(im, comp[3])
        outp = os.path.join(OUT, "ui_icon_talent_%s.png" % key)
        icon.save(outp)
        print("  saved", outp, "from bbox", comp[3])
    if len(mapping) != 5:
        print("WARN: expected 5 icons, got", len(mapping))


if __name__ == "__main__":
    main()
