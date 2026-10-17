#!/bin/bash
# Build, run the remake headlessly on Windows (via WSL interop) and convert the frame to PNG.
# Usage: tools/snap.sh [--fresh] [extra cyberpest.exe args]   -> work/test/snap.png
# Snapshot runs keep their own save in %TEMP%\CyberPest-test (never the player's pest in
# %APPDATA%\CyberPest). --fresh deletes that test save first (newborn pest); it is deleted
# from the Windows side because WSL's /mnt/c view may not show it.
set -euo pipefail
cd "$(dirname "$0")/.."
if [ "${1:-}" = "--fresh" ]; then
  shift
  (cd /mnt/c && cmd.exe /c 'if exist %TEMP%\CyberPest-test\PEST.CFG del %TEMP%\CyberPest-test\PEST.CFG' > /dev/null 2>&1) || true
fi
source ~/tools/env.sh
mkdir -p work/test
if ! cmake --build build -j16 > work/build.log 2>&1; then grep -E "error" work/build.log | head -20; exit 1; fi
grep -E "warning" work/build.log | head -10 || true
./build/cyberpest.exe --adm "$(wslpath -w original/extracted/APP/SLOW/PEST.ADM)" \
  --snapshot "$(wslpath -w work/test)\\snap.bmp" "$@"
python3 - <<'PY'
import struct, sys
sys.path.insert(0, "tools")
from png import write_indexed, write_rgb
b = open("work/test/snap.bmp", "rb").read()
w, h = struct.unpack_from("<ii", b, 18)
off = struct.unpack_from("<I", b, 10)[0]
bits = struct.unpack_from("<H", b, 28)[0]
if bits == 8:
    pal = [(b[54 + 4 * i + 2], b[54 + 4 * i + 1], b[54 + 4 * i]) for i in range(256)]
    stride = (w + 3) & ~3
    pixels = b"".join(b[off + y * stride:off + y * stride + w] for y in reversed(range(h)))
    write_indexed("work/test/snap.png", w, h, pixels, pal)
elif bits in (24, 32):
    pixel_size = bits // 8
    stride = (w * pixel_size + 3) & ~3
    pixels = bytearray(w * h * 3)
    for row in range(h):
        source = b[off + (h - 1 - row) * stride:off + (h - row) * stride]
        target = memoryview(pixels)[row * w * 3:(row + 1) * w * 3]
        target[0::3] = source[2:w * pixel_size:pixel_size]
        target[1::3] = source[1:w * pixel_size:pixel_size]
        target[2::3] = source[0:w * pixel_size:pixel_size]
    write_rgb("work/test/snap.png", w, h, pixels)
else:
    raise ValueError(f"unsupported snapshot BMP depth: {bits}")
print("work/test/snap.png", w, h)
PY
