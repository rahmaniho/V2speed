# V2Speed native Android spike

This is an open-source, Android 10+ (API 29+) VPN-client prototype. It is not an all-Android-version app and is not yet device-verified.

## Build a debug APK

Requirements: JDK 17, Android SDK platform/build tools 35, Gradle 8.11.1, internet access to GitHub and Google/Maven repositories.

```sh
cd android
./fetch-xray-core.sh
gradle --no-daemon :app:assembleDebug
```

The verified AAR is a third-party release from AndroidLibXrayLite; the script checks its SHA-256 before use. The local AAR is ignored by Git. CI runs the same fetch/check and publishes a temporary debug APK artifact. Debug signing is for personal testing only, not a production release.

## Important status

The UI, feed parsing, `VpnService` TUN setup and Xray binding adapter are an initial spike. Compile success alone is not evidence of a working tunnel. The adapter currently probes the generated gomobile API reflectively; it must be verified against the pinned AAR. Do not enter sensitive personal profiles into a release candidate until storage, logs, TLS, routing, DNS, lifecycle, and physical-device tests are complete. See `../docs/ANDROID_NATIVE_MVP.md`.
