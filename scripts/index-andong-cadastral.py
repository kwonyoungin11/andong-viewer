#!/usr/bin/env python3
"""Build a shapefile .qix next to Andong LSMD_CONT via ogrinfo. No API key."""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path


def find_shp() -> Path | None:
    home = Path.home()
    cands = [
        home / "Desktop" / "안동시",
        home / "바탕 화면" / "안동시",
        Path(os.environ.get("USERPROFILE", "")) / "Desktop" / "안동시",
        Path(r"D:/hgis/dist/andong-portable/data/andong/cad"),
        Path(r"D:/hgis/data/andong/cad"),
    ]
    extra = os.environ.get("ANDONG_DIR")
    if extra:
        cands.insert(0, Path(extra))
    for root in cands:
        if not root or not root.exists():
            continue
        hits = list(root.rglob("LSMD_CONT*.shp"))
        for p in hits:
            if "47170" in p.name:
                return p
        if hits:
            return hits[0]
    return None


def ogrinfo_exe() -> Path | None:
    root = Path(os.environ.get("OSGEO4W_ROOT", r"D:/OSGeo4W"))
    cands = [
        root / "apps" / "gdal-dev" / "bin" / "ogrinfo.exe",
        root / "bin" / "ogrinfo.exe",
        Path(r"D:/OSGeo4W/apps/gdal-dev/bin/ogrinfo.exe"),
    ]
    for p in cands:
        if p.exists():
            return p
    return None


def main() -> int:
    shp = find_shp()
    if shp is None:
        print("no LSMD_CONT shapefile")
        return 1
    ogr = ogrinfo_exe()
    if ogr is None:
        print("ogrinfo.exe not found")
        return 1
    qix = shp.with_suffix(".qix")
    print(f"shp={shp}")
    print(f"ogr={ogr}")
    if qix.exists() and qix.stat().st_size > 1000:
        print(f"keep qix bytes={qix.stat().st_size}")
        return 0
    layer = shp.stem
    cmd = [str(ogr), "-sql", f"CREATE SPATIAL INDEX ON {layer}", str(shp)]
    print("run", cmd)
    r = subprocess.run(cmd, check=False)
    print(f"ogrinfo_exit={r.returncode} qix_exists={qix.exists()} bytes={qix.stat().st_size if qix.exists() else 0}")
    return 0 if qix.exists() else 1


if __name__ == "__main__":
    raise SystemExit(main())
