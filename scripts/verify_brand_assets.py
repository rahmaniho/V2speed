#!/usr/bin/env python3
"""Validate the checked-in V2Speed source and derivative PNG asset pack (stdlib only)."""
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "docs" / "brand-assets"
EXPECTED = {
    "source/v2speed-lion-sun-app-icon.png": ((1254, 1254), 2, 2_234_185),
    "source/v2speed-lion-sun-wordmark.png": ((1536, 1024), 6, 2_141_543),
    "android/adaptive-foreground.png": ((1024, 1024), 6, None),
    "ios/AppIcon-1024.png": ((1024, 1024), 2, None),
    "ui/v2speed-logo-768.png": ((768, 512), 6, None),
    "ui/v2speed-logo-512.png": ((512, 341), 6, None),
    "ui/v2speed-logo-dark-background.png": ((1536, 1024), 2, None),
    "ui/v2speed-logo-light-background.png": ((1536, 1024), 2, None),
    "ui/v2speed-app-icon-192.png": ((192, 192), 2, None),
    "ui/v2speed-app-icon-512.png": ((512, 512), 2, None),
}
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def inspect(path: Path):
    data = path.read_bytes()
    if len(data) < 26 or data[:8] != PNG_SIGNATURE:
        raise ValueError(f"{path.relative_to(ROOT)} is not a valid PNG header")
    width, height = struct.unpack(">II", data[16:24])
    return (width, height), data[25], len(data)


def main() -> int:
    errors = []
    for relative, (dimensions, color_type, source_size) in EXPECTED.items():
        path = ASSETS / relative
        try:
            actual_dimensions, actual_color_type, actual_size = inspect(path)
            if actual_dimensions != dimensions:
                errors.append(f"{relative}: expected {dimensions}, got {actual_dimensions}")
            if actual_color_type != color_type:
                errors.append(f"{relative}: expected PNG color type {color_type}, got {actual_color_type}")
            if source_size is not None and actual_size != source_size:
                errors.append(f"{relative}: original byte size changed ({actual_size} != {source_size})")
            if not errors or not any(item.startswith(relative + ":") for item in errors):
                print(f"OK {relative}: {actual_dimensions[0]}x{actual_dimensions[1]}, {actual_size} bytes")
        except (OSError, ValueError, struct.error) as exc:
            errors.append(f"{relative}: {exc}")
    if errors:
        print("Brand asset validation failed:", file=sys.stderr)
        print("\n".join(f"- {error}" for error in errors), file=sys.stderr)
        return 1
    print(f"Validated {len(EXPECTED)} V2Speed PNG assets.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
