"""Extract an InstallShield 3 DATA.Z archive (PKWARE DCL implode, via zlib's blast algorithm)."""
import os
import struct
import sys

LENLEN = [2, 35, 36, 53, 38, 23]
DISTLEN = [2, 20, 53, 230, 247, 151, 248]
BASE = [3, 2, 4, 5, 6, 7, 8, 9, 10, 12, 16, 24, 40, 72, 136, 264]
EXTRA = [0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8]


def construct(rep):
    lengths = []
    for b in rep:
        lengths += [b & 15] * ((b >> 4) + 1)
    count = [0] * 16
    for ln in lengths:
        count[ln] += 1
    offs = [0] * 16
    for i in range(1, 15):
        offs[i + 1] = offs[i] + count[i]
    symbol = [0] * len(lengths)
    for s, ln in enumerate(lengths):
        if ln:
            symbol[offs[ln]] = s
            offs[ln] += 1
    # Map (length, code) -> symbol, with canonical codes as blast assigns them.
    table = {}
    code = first = index = 0
    for ln in range(1, 16):
        for k in range(count[ln]):
            table[(ln, first + k)] = symbol[index + k]
        index += count[ln]
        first = (first + count[ln]) << 1
    return table


LENCODE = construct(LENLEN)
DISTCODE = construct(DISTLEN)


def blast(data):
    pos = 0
    bitbuf = 0
    bitcnt = 0
    n = len(data)

    def bits(need):
        nonlocal pos, bitbuf, bitcnt
        while bitcnt < need:
            if pos >= n:
                raise EOFError("compressed stream ended early")
            bitbuf |= data[pos] << bitcnt
            pos += 1
            bitcnt += 8
        v = bitbuf & ((1 << need) - 1)
        bitbuf >>= need
        bitcnt -= need
        return v

    def decode(table):
        code = 0
        for ln in range(1, 16):
            code = (code << 1) | (bits(1) ^ 1)
            sym = table.get((ln, code))
            if sym is not None:
                return sym
        raise ValueError("bad huffman code")

    lit = bits(8)
    if lit != 0:
        raise ValueError("coded literals not supported")
    dictbits = bits(8)
    if not 4 <= dictbits <= 6:
        raise ValueError("bad dictionary size %d" % dictbits)
    out = bytearray()
    while True:
        if bits(1):
            sym = decode(LENCODE)
            length = BASE[sym] + bits(EXTRA[sym])
            if length == 519:
                break
            sh = 2 if length == 2 else dictbits
            dist = (decode(DISTCODE) << sh) + bits(sh) + 1
            if dist > len(out):
                raise ValueError("distance too far back")
            start = len(out) - dist
            for i in range(length):
                out.append(out[start + i])
        else:
            out.append(bits(8))
    return bytes(out)


def main(archive, outdir):
    raw = open(archive, "rb").read()
    assert raw[:4] == b"\x13\x5d\x65\x8c", "not an InstallShield 3 archive"
    file_count, = struct.unpack_from("<H", raw, 0x0C)
    toc, = struct.unpack_from("<I", raw, 0x29)
    dir_count, = struct.unpack_from("<H", raw, 0x31)
    p = toc
    dirs = []
    for _ in range(dir_count):
        count, chunk, nlen = struct.unpack_from("<HHH", raw, p)
        dirs.append((raw[p + 6:p + 6 + nlen].decode("ascii"), count))
        p += chunk
    order = [d for d, c in dirs for _ in range(c)]
    assert len(order) == file_count
    for d in order:
        usize, csize, off = struct.unpack_from("<III", raw, p + 3)
        chunk, = struct.unpack_from("<H", raw, p + 0x17)
        nlen = raw[p + 0x1D]
        name = raw[p + 0x1E:p + 0x1E + nlen].decode("ascii")
        p += chunk
        data = blast(raw[off:off + csize])
        status = "ok" if len(data) == usize else "SIZE MISMATCH %d" % len(data)
        target = os.path.join(outdir, *d.split("\\"), name)
        os.makedirs(os.path.dirname(target), exist_ok=True)
        with open(target, "wb") as f:
            f.write(data)
        print(f"{d}\\{name}\t{usize}\t{status}", flush=True)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
