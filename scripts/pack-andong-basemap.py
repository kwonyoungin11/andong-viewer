#!/usr/bin/env python3
"""Download Andong-only satellite tiles into MBTiles. No API key in source."""
from __future__ import annotations

import argparse
import math
import sqlite3
import ssl
import time
import urllib.request
from pathlib import Path

# Andong-si bbox from bundled andong_city.geojson (WGS84).
LON0, LAT0, LON1, LAT1 = 128.4292, 36.2951, 128.9997, 36.8231
URL = "https://xdworld.vworld.kr/2d/Satellite/service/{z}/{x}/{y}.jpeg"


def lonlat_to_tile(lon: float, lat: float, z: int) -> tuple[int, int]:
    n = 2**z
    x = int((lon + 180.0) / 360.0 * n)
    lat_rad = math.radians(lat)
    y = int((1.0 - math.log(math.tan(lat_rad) + 1.0 / math.cos(lat_rad)) / math.pi) / 2.0 * n)
    return max(0, min(n - 1, x)), max(0, min(n - 1, y))


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--min-zoom", type=int, default=12)
    p.add_argument("--max-zoom", type=int, default=15)
    p.add_argument("--append", action="store_true", help="Keep existing tiles; add this zoom range")
    p.add_argument("--out", required=True)
    args = p.parse_args()
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    appending = bool(args.append and out.exists())
    if out.exists() and not appending:
        out.unlink()

    ctx = ssl.create_default_context()
    con = sqlite3.connect(str(out))
    cur = con.cursor()
    cur.execute("CREATE TABLE IF NOT EXISTS metadata (name TEXT, value TEXT)")
    cur.execute(
        "CREATE TABLE IF NOT EXISTS tiles (zoom_level INTEGER, tile_column INTEGER, tile_row INTEGER, tile_data BLOB)"
    )
    cur.execute("CREATE UNIQUE INDEX IF NOT EXISTS tile_index ON tiles (zoom_level, tile_column, tile_row)")
    meta = {
        "name": "andong-satellite",
        "type": "baselayer",
        "version": "1",
        "description": "Andong-si offline satellite",
        "format": "jpg",
        "bounds": f"{LON0},{LAT0},{LON1},{LAT1}",
        "minzoom": str(args.min_zoom),
        "maxzoom": str(args.max_zoom),
        "srs": "EPSG:3857",
    }
    if appending:
        cur.execute("SELECT name, value FROM metadata")
        old = {k: v for k, v in cur.fetchall()}
        try:
            meta["minzoom"] = str(min(int(old.get("minzoom", args.min_zoom)), args.min_zoom))
            meta["maxzoom"] = str(max(int(old.get("maxzoom", args.max_zoom)), args.max_zoom))
        except ValueError:
            pass
        cur.execute("DELETE FROM metadata")
    cur.executemany("INSERT INTO metadata VALUES (?, ?)", list(meta.items()))

    ok = 0
    fail = 0
    for z in range(args.min_zoom, args.max_zoom + 1):
        x0, y1 = lonlat_to_tile(LON0, LAT0, z)
        x1, y0 = lonlat_to_tile(LON1, LAT1, z)
        if x0 > x1:
            x0, x1 = x1, x0
        if y0 > y1:
            y0, y1 = y1, y0
        n = 2**z
        for x in range(x0, x1 + 1):
            for y in range(y0, y1 + 1):
                url = URL.format(z=z, x=x, y=y)
                tms_y = n - 1 - y
                try:
                    req = urllib.request.Request(
                        url,
                        headers={
                            "User-Agent": "andong-viewer/1.0",
                            "Referer": "https://localhost",
                        },
                    )
                    with urllib.request.urlopen(req, timeout=20, context=ctx) as resp:
                        data = resp.read()
                    if not data or len(data) < 80:
                        fail += 1
                        continue
                    cur.execute(
                        "INSERT OR REPLACE INTO tiles VALUES (?, ?, ?, ?)",
                        (z, x, tms_y, data),
                    )
                    ok += 1
                    if ok % 40 == 0:
                        con.commit()
                        print(f"z{z} tiles={ok} fail={fail}", flush=True)
                except Exception:
                    fail += 1
                    time.sleep(0.15)
    con.commit()
    con.close()
    print(f"done ok={ok} fail={fail} -> {out}")
    return 0 if ok > 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
