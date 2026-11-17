# Android MVP device acceptance

This checklist is intentionally not marked passed by CI. A successful Gradle build and unit tests do not establish that an Android device routes real traffic through the VPN.

## Install and connect

1. Build locally with the instructions in `README.md`, or download the `v2speed-android-debug-apk` artifact from the Android native prototype GitHub Actions run. Install only on a test device; this is a debug build.
2. Open the app and allow the subscription feeds to load. The app fetches three public feeds; each response is limited to 1 MB. Feed availability and the public nodes' safety/reliability are not guaranteed.
3. Tap the home connect control. Confirm the app reports feed retrieval, then TCP preflight, then HTTPS progress for each TCP-responsive config. It must not report connected while probes are still running.
4. Confirm the final server list contains only configs that answered both Instagram and YouTube HTTPS checks. Entries should be ordered by the slower service's measured latency and show Instagram, YouTube and TCP timings. The list should remain usable after briefly backgrounding and returning to the app.
5. If all checks fail, confirm the app reports failure and leaves no config marked as eligible. If a candidate passes the HTTPS checks but the Android VPN cannot start, confirm it is removed and the next ranked candidate is tried.

## VPN lifecycle and honesty checks

- Deny Android VPN consent: no tunnel may be reported as connected.
- While probing, use the stop control. The scan should stop at the next safe point, and untested entries must not be presented as verified.
- During a successful session, stop using the in-app control and the foreground-notification action. Also revoke VPN permission in Android settings and confirm cleanup.
- With the VPN active, test ordinary browser traffic and compare the observed public IP with a known baseline. This is a manual device check only; it does not certify DNS privacy, leak protection, throughput, streaming quality, or a Kill Switch.
- Test with Wi-Fi, mobile data, airplane mode, and one service unavailable. The app must show failures instead of synthetic success or latency values.
- Toggle the optional Persian connect announcement and verify both enabled and disabled behavior on the device's installed TTS engine.

## Release gate

Record device model, Android version, build SHA, network type, outcome, and sanitized logs for each case. Do not record credential-bearing subscription URIs. This prototype remains unverified for production until real-device traffic, network-change, lifecycle, privacy, license/SBOM, and release-signing reviews are complete.
