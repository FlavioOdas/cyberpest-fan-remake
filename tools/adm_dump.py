"""Dump PEST.ADM: tree.json, every virtual file, and FLC animations as PNG frame strips.

Usage: python3 tools/adm_dump.py original/extracted/APP/SLOW/PEST.ADM work/adm [--no-png]
"""
import collections
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
from admlib import Adm  # noqa: E402
from flc import Flc  # noqa: E402
from png import write_indexed  # noqa: E402


def bmp_palette(data):
    header_size, = struct.unpack_from("<I", data, 14)
    colors, = struct.unpack_from("<I", data, 46)
    p = 14 + header_size
    return [(data[p + 4 * i + 2], data[p + 4 * i + 1], data[p + 4 * i]) for i in range(colors or 256)]


def main(adm_path, out, png=True):
    adm = Adm(adm_path)
    os.makedirs(os.path.join(out, "files"), exist_ok=True)
    with open(os.path.join(out, "tree.json"), "w") as f:
        json.dump([n.to_json() for n in adm.roots], f, indent=1)

    files = adm.files()
    palette = next(bmp_palette(adm.read(e)) for e in files if e["name"] == "PALETTE.BMP")
    names = collections.Counter()
    stats = collections.Counter()
    manifest = []
    for e in files:
        names[e["name"]] += 1
        stem, ext = os.path.splitext(e["name"])
        fname = e["name"] if names[e["name"]] == 1 else f"{stem}~{names[e['name']]}{ext}"
        data = adm.read(e)
        with open(os.path.join(out, "files", fname), "wb") as f:
            f.write(data)
        item = {"file": fname, "offset": e["offset"], "size": e["size"]}
        if ext == ".FLC":
            flc = Flc(data)
            frames = list(flc.frames())
            item.update(frames=flc.frame_count, width=flc.width, height=flc.height, speed_ms=flc.speed,
                        own_palette=flc.palette_set, chunk_types=sorted(flc.chunk_types))
            stats.update(f"chunk{t}" for t in flc.chunk_types)
            if png and frames:
                w, h, n = flc.width, flc.height, flc.frame_count
                strip = bytearray(w * n * h)
                for i, (px, _) in enumerate(frames[:n]):
                    for y in range(h):
                        strip[y * w * n + i * w:y * w * n + (i + 1) * w] = px[y * w:(y + 1) * w]
                pal = frames[0][1] if flc.palette_set else palette
                write_indexed(os.path.join(out, "files", fname + ".png"), w * n, h, strip, pal)
        manifest.append(item)
        stats[ext] += 1
    with open(os.path.join(out, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=1)
    print(dict(stats))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], "--no-png" not in sys.argv)
