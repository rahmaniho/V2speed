# Supplied V2Speed Brand Assets

The two user-supplied images are preserved byte-for-byte under `source/` and are the authoritative references. No substitute/AI-generated emblem has been made.

| File | Intended use from visual inspection | Source dimensions / mode | Notes |
|---|---|---|---|
| `source/v2speed-lion-sun-app-icon.png` | Launcher/app icon source: square emblem with rounded dark frame | 1254 × 1254, RGB PNG, 2,234,185 bytes | Keep the whole framed artwork; do not crop/stretch. Current source itself has no alpha channel. |
| `source/v2speed-lion-sun-wordmark.png` | Internal brand logo / splash and about source: Lion and Sun, V2SPEED and Smart VPN wordmark | 1536 × 1024, RGBA PNG, 2,141,543 bytes | Transparent-channel PNG; inspect alpha before using as a dark/light background derivative. Keep full emblem and lettering. |

## Brand tokens

- Neon green `#9FFF00`
- Gold `#D6A443`
- Dark background `#080D18`
- Surface `#111A2B`
- Secondary surface `#18243A`
- Primary text `#F5F7FA`
- Secondary text `#9BA8BA`

## Generated asset pack

| Path | Purpose | Dimensions | Generation notes |
|---|---|---:|---|
| `android/adaptive-foreground.png` | Android adaptive-icon foreground source | 1024 × 1024 RGBA | Entire supplied square icon scaled proportionally to 696 × 696 and centered in transparency; no detail was removed. Use background `#080D18`. |
| `ios/AppIcon-1024.png` | iOS App Store icon source | 1024 × 1024 RGB | Proportional high-quality resize of the full square icon. Generate platform catalog slots from this source using the native build tool. |
| `ui/v2speed-logo-768.png` | High-resolution internal wordmark | 768 × 512 RGBA | Full supplied artwork, proportional resize. |
| `ui/v2speed-logo-512.png` | Compact UI rendering source | 512 × 341 RGBA | Full supplied artwork, proportional resize; no crop. |
| `ui/v2speed-logo-dark-background.png` | Dark-surface preview / fallback | 1536 × 1024 RGB | Original alpha artwork composited over `#080D18`. |
| `ui/v2speed-logo-light-background.png` | Light-surface preview | 1536 × 1024 RGB | Original alpha artwork composited over `#F8FAFC`. |

`python3 scripts/verify_brand_assets.py` validates required source and derivative PNG signatures, dimensions, color types, and original source byte sizes with only the Python standard library. The transparent/dark/light wordmark variants retain the supplied art; no background was keyed out and no vector trace or emblem crop was created.

## Still pending

The raw originals and an initial icon/logo asset pack are now in the repository, but they are **not integrated into Android/iOS launcher, splash, or home screens** because no native platform project exists yet. A complete horizontal lockup and emblem-only mark cannot be safely derived from these assets without cropping/reconstructing the Lion and Sun; retain the original full artwork until a design-approved derivation is made. Validate Android adaptive masks, iOS catalog slots, safe areas, and appearance in actual builds before claiming platform brand integration.
