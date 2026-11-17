#!/bin/sh
set -eu

VERSION=v26.11.9
SHA256=cdfef146218233a31a8b52c8b1016557730d3b63fc121d4c84cacea50e669155
DEST="$(dirname "$0")/app/libs/libv2ray.aar"
URL="https://github.com/2dust/AndroidLibXrayLite/releases/download/${VERSION}/libv2ray.aar"
mkdir -p "$(dirname "$DEST")"
curl --fail --location --retry 3 --connect-timeout 30 --max-time 300 "$URL" --output "$DEST"
printf '%s  %s\n' "$SHA256" "$DEST" | sha256sum --check --status || {
  rm -f "$DEST"
  echo 'Pinned Xray AAR digest mismatch.' >&2
  exit 1
}
echo "Verified AndroidLibXrayLite ${VERSION} AAR: ${SHA256}"
