"""Reader for CyberPest's PEST.ADM "Application Database" (see docs/formats/adm.md)."""
import struct

CONTAINERS = (b"ADDM", b"ADDL")


class Node:
    """One chunk of the ADM tree. Containers have `id`, `ref` and `children`; leaves have `value`."""

    __slots__ = ("tag", "offset", "size", "id", "ref", "children", "value")

    def __init__(self, tag, offset, size):
        self.tag, self.offset, self.size = tag, offset, size
        self.id = self.ref = None
        self.children = []
        self.value = None

    def to_json(self):
        if self.tag in ("ADDM", "ADDL"):
            return {"tag": self.tag, "id": self.id, "ref": self.ref,
                    "children": [c.to_json() for c in self.children]}
        if self.tag == "ADFI":
            return {"tag": "ADFI", "offset": self.offset, "size": self.size}
        return {"tag": self.tag, "value": self.value}


class Adm:
    def __init__(self, path):
        self.data = open(path, "rb").read()
        b = self.data
        if b[:4] != b"RIFF" or b[8:12] != b"ADBM":
            raise ValueError("not a CyberPest ADM file")
        # The file ends with a separate RIFF form 'ADBB' (index); the tree is everything before it.
        tail = b.rfind(b"RIFF")
        self.index_offset = tail if tail > 0 else len(b)
        self.roots = self._parse(12, self.index_offset)

    def _parse(self, o, end):
        b = self.data
        nodes = []
        while o < end:
            tag = b[o:o + 4]
            size, = struct.unpack_from("<I", b, o + 4)
            p = o + 8
            node = Node(tag.decode("latin-1"), p, size)
            if tag in CONTAINERS:
                node.id, node.ref, _count = struct.unpack_from("<iiI", b, p)
                node.children = self._parse(p + 12, p + size)
            elif tag == b"ADVI":
                node.value, = struct.unpack_from("<i", b, p)
            elif tag == b"ADVS":
                n, = struct.unpack_from("<I", b, p)
                node.value = b[p + 4:p + 4 + n].split(b"\0")[0].decode("latin-1")
            elif tag == b"NDVE":
                off, length, n = struct.unpack_from("<III", b, p)
                name = b[p + 12:p + 12 + n].split(b"\0")[0].decode("latin-1")
                node.value = {"name": name, "offset": off, "size": length}
            elif tag != b"ADFI":
                node.value = b[p:p + size].hex()
            nodes.append(node)
            o = p + size
        return nodes

    def walk(self, nodes=None):
        for n in self.roots if nodes is None else nodes:
            yield n
            yield from self.walk(n.children)

    def files(self):
        """Virtual files (NDVE entries), de-duplicated by offset, in file order."""
        seen = {}
        for n in self.walk():
            if n.tag == "NDVE" and n.value["offset"] not in seen:
                seen[n.value["offset"]] = n.value
        return [seen[k] for k in sorted(seen)]

    def read(self, entry):
        return self.data[entry["offset"]:entry["offset"] + entry["size"]]
