#!/usr/bin/env python3
# Generates the HYPERSENSEGLOBAL app icon (multi-size .ico, pure stdlib).
import struct, zlib, os, math

OUT = os.path.join(os.path.dirname(__file__), "..", "res", "icon.ico")

BG_TOP = (13, 21, 34)
BG_BOT = (6, 10, 16)
CYAN = (34, 211, 238)
CYAN_DIM = (20, 120, 138)

def rounded_rect(x, y, w, h, r):
    # signed distance style inside test for rounded rectangle in normalized coords
    cx = min(max(x, r), w - r)
    cy = min(max(y, r), h - r)
    dx = x - cx
    dy = y - cy
    return dx * dx + dy * dy <= r * r or (r <= x <= w - r) or (r <= y <= h - r)

def hex_point(cx, cy, rad, i):
    ang = math.pi / 3.0 * i - math.pi / 2.0
    return (cx + rad * math.cos(ang), cy + rad * math.sin(ang))

def seg_dist(px, py, ax, ay, bx, by):
    vx, vy = bx - ax, by - ay
    wx, wy = px - ax, py - ay
    t = max(0.0, min(1.0, (wx * vx + wy * vy) / (vx * vx + vy * vy)))
    dx, dy = px - (ax + t * vx), py - (ay + t * vy)
    return math.hypot(dx, dy)

def render(size):
    ss = 4
    n = size * ss
    # background layer
    img = [[(0, 0, 0, 0)] * size for _ in range(size)]
    margin = n * 0.03
    r = n * 0.22
    cx, cy = n / 2.0, n / 2.0
    hexr = n * 0.335
    pts = [hex_point(cx, cy, hexr, i) for i in range(6)]
    thick = n * 0.045
    # pulse waveform points (relative to center)
    wave = [(-0.19, 0.0), (-0.10, 0.0), (-0.055, -0.105), (0.0, 0.115), (0.055, -0.055),
            (0.10, 0.0), (0.19, 0.0)]
    wpts = [(cx + wx * n, cy + wy * n) for wx, wy in wave]
    wt = n * 0.030

    acc = [[[0, 0, 0, 0] for _ in range(size)] for _ in range(size)]
    for sy in range(n):
        for sx in range(n):
            x = sx + 0.5
            y = sy + 0.5
            inside = False
            if margin <= x <= n - margin and margin <= y <= n - margin:
                if rounded_rect(x - margin, y - margin, n - 2 * margin, n - 2 * margin, r):
                    inside = True
            if not inside:
                continue
            # gradient
            t = y / n
            col = [int(BG_TOP[i] + (BG_BOT[i] - BG_TOP[i]) * t) for i in range(3)]

            # hexagon ring
            ring = False
            for i in range(6):
                ax, ay = pts[i]
                bx, by = pts[(i + 1) % 6]
                if seg_dist(x, y, ax, ay, bx, by) <= thick:
                    ring = True
                    break
            # pulse
            pulse = False
            for i in range(len(wpts) - 1):
                ax, ay = wpts[i]
                bx, by = wpts[i + 1]
                if seg_dist(x, y, ax, ay, bx, by) <= wt:
                    pulse = True
                    break

            if ring or pulse:
                col = list(CYAN)
            # soft inner glow near ring
            elif any(seg_dist(x, y, pts[i][0], pts[i][1], pts[(i + 1) % 6][0], pts[(i + 1) % 6][1]) <= thick * 2.6 for i in range(6)):
                col = [int(col[j] * 0.72 + CYAN_DIM[j] * 0.28) for j in range(3)]

            ox, oy = sx // ss, sy // ss
            a = acc[oy][ox]
            a[0] += col[0]; a[1] += col[1]; a[2] += col[2]; a[3] += 255

    samples = ss * ss
    for oy in range(size):
        for ox in range(size):
            a = acc[oy][ox]
            img[oy][ox] = (a[0] // samples, a[1] // samples, a[2] // samples, a[3] // samples)
    return img

def png_bytes(img):
    size = len(img)
    raw = bytearray()
    for y in range(size):
        raw.append(0)
        for x in range(size):
            raw.extend(img[y][x])
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        c += struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        return c
    ihdr = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")

def main():
    sizes = [16, 32, 48, 64, 128, 256]
    blobs = []
    for s in sizes:
        blobs.append((s, png_bytes(render(s))))
    header = struct.pack("<HHH", 0, 1, len(blobs))
    offset = 6 + 16 * len(blobs)
    entries = b""
    data = b""
    for s, blob in blobs:
        w = 0 if s >= 256 else s
        h = 0 if s >= 256 else s
        entries += struct.pack("<BBBBHHII", w, h, 0, 0, 1, 32, len(blob), offset)
        data += blob
        offset += len(blob)
    with open(OUT, "wb") as f:
        f.write(header + entries + data)
    print("wrote", os.path.abspath(OUT))

if __name__ == "__main__":
    main()
