#!/usr/bin/env python3
"""Extract the largest image from an .ico file to a .png for preview (stdlib only)."""
import struct, sys, zlib, os


def read_ico(path):
    data = open(path, "rb").read()
    _, _, count = struct.unpack("<HHH", data[:6])
    entries = []
    for i in range(count):
        w, h, colors, res, planes, bpp, size, off = struct.unpack("<BBBBHHII", data[6 + 16 * i:22 + 16 * i])
        entries.append({"w": w or 256, "h": h or 256, "bpp": bpp, "size": size, "off": off, "data": data[off:off + size]})
    return entries


def dib_to_rgba(blob, w, h):
    # BITMAPINFOHEADER/V4/V5 + bottom-up BGRA pixels
    hdr = struct.unpack("<IiiHHIIiiII", blob[:40])
    bw, bh = hdr[1], abs(hdr[2])
    bh //= 2  # ICO DIB heights are doubled to include the AND mask
    bpp = hdr[4]
    header_size = hdr[0]
    compression = hdr[5]
    palette = 0
    if bpp <= 8:
        palette = (1 << bpp) * 4
    masks = 12 if (compression == 3 and header_size == 40) else 0
    pixel_off = header_size + palette + masks
    pixels = blob[pixel_off:]
    row = bw * 4
    out = bytearray()
    for y in range(h - 1, -1, -1):
        start = y * row
        line = pixels[start:start + bw * 4]
        if len(line) < bw * 4:
            raise ValueError(f"truncated pixel data: need {bw*4} got {len(line)}")
        for x in range(bw):
            b, g, r, a = line[x * 4:x * 4 + 4]
            out += bytes((r, g, b, a if a else 255))
    return bytes(out), bw, bh


def png_bytes(rgba, w, h):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw += rgba[y * w * 4:(y + 1) * w * 4]

    def chunk(tag, payload):
        return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")


def main():
    src = sys.argv[1]
    dst = sys.argv[2] if len(sys.argv) > 2 else os.path.splitext(src)[0] + "_preview.png"
    want = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    entries = read_ico(src)
    for e in entries:
        print(f"  {e['w']}x{e['h']} {e['bpp']}bpp {e['size']} bytes")
    if want:
        match = [x for x in entries if x["w"] == want]
        if not match:
            raise SystemExit(f"no {want}px entry")
        e = match[0]
    else:
        e = max(entries, key=lambda x: x["w"] * x["h"])
    if e["data"][:8] == b"\x89PNG\r\n\x1a\n":
        open(dst, "wb").write(e["data"])
    else:
        rgba, w, h = dib_to_rgba(e["data"], e["w"], e["h"])
        open(dst, "wb").write(png_bytes(rgba, w, h))
    print("wrote", dst)


if __name__ == "__main__":
    main()
