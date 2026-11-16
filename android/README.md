# V2Speed Android preview

Open-source native Android VPN-client prototype. The current minimum target is Android 10+ (API 29+). This is not a production release and has not been verified on a physical device.

## Build a debug APK

Requirements: JDK 17, Android SDK platform/build tools 35, Gradle 8.11.1, internet access to GitHub and Google/Maven repositories.

```sh
cd android
./fetch-xray-core.sh
gradle --no-daemon :app:assembleDebug
```

The verified AAR is a third-party release from AndroidLibXrayLite; the script checks its SHA-256 before use. The local AAR is ignored by Git. CI runs the same fetch/check and publishes a temporary debug APK artifact. Debug signing is for personal testing only, not a production release.

## Current implementation and limitations

- The app uses Android `VpnService`, a pinned Xray gomobile binding, in-memory feed-node selection, a Persian-first native Views interface, and the supplied Lion and Sun V2Speed artwork on the home screen.
- Smart Connect fetches the three configured public feeds, discards entries that cannot be converted to an Xray config, and samples at most eight nodes per source for a bounded, parallel TCP endpoint preflight. It then tries up to five reachable candidates one at a time using loopback-only Xray SOCKS inbounds, and makes small HTTPS requests to Instagram and YouTube. A candidate is eligible only if both HTTPS requests succeed; eligible candidates are ranked by the slower of the two response times (then their average), and failed candidates are removed from the current in-memory list. If all candidates fail, the app reports that and does not claim success. This is a lightweight reachability/latency check, not a throughput, video playback, service quality, or Android TUN end-to-end guarantee; untested feed entries are not classified as invalid.
- The connection announcement is optional (on by default) in Settings. On the app's connected-state transition, Android's installed TextToSpeech engine is asked to speak `پاینده ایران، تا ابد جاوید شاه.` using Persian locale, reduced speech rate/pitch and reduced utterance volume. No audio file is downloaded or saved. Voice quality and Persian-language availability depend on the device's TTS engine; if no Persian voice is available, nothing is spoken. The user can disable the announcement locally in Settings.
- Compile success does not prove the VPN tunnel carries traffic. The Xray binding's TUN descriptor behavior, real connectivity, DNS/leak behavior, network changes, stop/revoke lifecycle and physical-device compatibility still require verification.
- Speed measurement, DNS leak testing, Kill Switch, persistent personal profiles, officially managed infrastructure, backend, iOS app and production release signing are not implemented.
- Do not enter sensitive personal profiles into a release candidate until local encrypted storage, logging, routing, DNS, lifecycle and device tests are complete. Third-party notices and bundled license texts are preliminary, not a completed SBOM or legal clearance.
