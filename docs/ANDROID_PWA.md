# Android access for the V2Speed prototype

## What is available now

The browser prototype is an installable Progressive Web App (PWA), not a native Android APK. On a supported Android browser, it can be added to the home screen and opened in a standalone app-like window. When Chrome offers the install prompt, the page shows **«نصب روی گوشی»**. Otherwise, use Chrome's three-dot menu and choose **Install app** or **Add to Home screen**.

The installed item is only a shortcut to the hosted web app. It is not a VPN client: it does not create a tunnel, connect to a server, probe endpoints, or measure speed. Subscription and config APIs require network access; the service worker caches only the static shell and does not persist those API responses.

## Install on an Android phone for testing

1. Open the HTTPS V2Speed web preview in Chrome on the phone. If the preview is open in an embedded browser, use its **Open in browser** action first.
2. Wait for the page to load, then tap **«نصب روی گوشی»** if shown.
3. If that button is not shown, open Chrome's three-dot menu and choose **Install app** or **Add to Home screen**. Menu wording differs by Chrome version.
4. Open the V2Speed icon from the home screen. Internet access is required to load subscription data.

The Arena preview is temporary and intended for review only; an installed shortcut to it is not a production release. It may stop working when the preview environment is stopped or its address changes. A public, stable HTTPS deployment is needed before distributing a usable PWA.

## Native Android APK remains future work

No APK is built or represented as tested. This workspace has no Android SDK or Java runtime, and a catalog-only WebView wrapper would not create VPN capability. A genuine Android VPN client still needs a selected and legally reviewed engine, Android `VpnService` integration, consent and lifecycle handling, secure configuration handling, Android device testing, signing, and a stable backend/hosting and privacy review. See `PLATFORM_COMPATIBILITY.md` and `LICENSE_COMPLIANCE.md` before planning distribution.
