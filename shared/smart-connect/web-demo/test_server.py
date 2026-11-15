import base64
import importlib.util
import json
from pathlib import Path
import unittest

SERVER_PATH = Path(__file__).with_name("server.py")
SPEC = importlib.util.spec_from_file_location("v2speed_web_demo_server", SERVER_PATH)
SERVER = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(SERVER)


class SubscriptionParserTests(unittest.TestCase):
    def test_plain_mixed_subscription_is_kept_as_text(self):
        text = "vless://id@example.com:443?security=tls#edge\nss://user@example.net:8388#fast"
        self.assertEqual(SERVER.decode_subscription_payload(text), text)

    def test_base64_subscription_list_is_decoded(self):
        plain = "ss://user@example.com:443#node-a\nss://user@example.net:8443#node-b"
        encoded = base64.b64encode(plain.encode()).decode()
        self.assertEqual(SERVER.decode_subscription_payload(encoded), plain)

    def test_vless_and_trojan_rows_expose_endpoint_without_reformatting_config(self):
        for scheme in ("vless", "trojan"):
            uri = f"{scheme}://credential@example.org:8443?security=tls#Frankfurt"
            node = SERVER.parse_subscription_row(uri)
            self.assertIsNotNone(node)
            self.assertEqual(node["endpoint"], "example.org:8443")
            self.assertEqual(node["name"], "Frankfurt")
            self.assertEqual(node["uri"], uri)

    def test_vmess_json_row_is_decoded(self):
        payload = {"add": "vmess.example.org", "port": "443", "ps": "VMess test"}
        encoded = base64.b64encode(json.dumps(payload).encode()).decode().rstrip("=")
        node = SERVER.parse_subscription_row(f"vmess://{encoded}")
        self.assertIsNotNone(node)
        self.assertEqual(node["protocol"], "vmess")
        self.assertEqual(node["endpoint"], "vmess.example.org:443")
        self.assertEqual(node["name"], "VMess test")

    def test_legacy_whole_authority_shadowsocks_is_parsed(self):
        credentials = base64.b64encode(b"aes-128-gcm:secret@192.0.2.1:8388").decode().rstrip("=")
        node = SERVER.parse_subscription_row(f"ss://{credentials}#Example")
        self.assertIsNotNone(node)
        self.assertEqual(node["protocol"], "shadowsocks")
        self.assertEqual(node["endpoint"], "192.0.2.1:8388")

    def test_unknown_protocol_and_invalid_vmess_are_skipped(self):
        self.assertIsNone(SERVER.parse_subscription_row("http://example.com/"))
        self.assertIsNone(SERVER.parse_subscription_row("vmess://not-valid-base64"))

    def test_only_requested_fixed_github_api_sources_are_configured(self):
        self.assertEqual(set(SERVER.SUBSCRIPTION_SOURCES), {"iboxz", "thomas", "v2collector"})
        for source in SERVER.SUBSCRIPTION_SOURCES.values():
            self.assertTrue(source["api_url"].startswith("https://api.github.com/repos/"))
            self.assertTrue(source["url"].startswith("https://raw.githubusercontent.com/"))

    def test_pwa_manifest_uses_existing_brand_icon_derivatives(self):
        manifest = json.loads((SERVER.WEB_ROOT / "manifest.webmanifest").read_text())
        self.assertEqual(manifest["display"], "standalone")
        self.assertEqual({icon["sizes"] for icon in manifest["icons"]}, {"192x192", "512x512"})
        for icon in manifest["icons"]:
            filename = Path(icon["src"]).name.replace("app-icon-", "v2speed-app-icon-")
            self.assertTrue((SERVER.ROOT / "docs" / "brand-assets" / "ui" / filename).exists())

    def test_service_worker_does_not_cache_subscription_apis(self):
        worker = (SERVER.WEB_ROOT / "sw.js").read_text()
        self.assertIn("url.pathname.startsWith('/api/')", worker)
        self.assertNotIn("/api/subscriptions", worker)
        self.assertNotIn("/api/config/", worker)

    def test_manual_refresh_has_a_cooldown(self):
        previous = SERVER.LAST_FORCED_REFRESH
        try:
            SERVER.LAST_FORCED_REFRESH = None
            self.assertTrue(SERVER.claim_forced_refresh(now=1000.0))
            self.assertFalse(SERVER.claim_forced_refresh(now=1179.0))
            self.assertTrue(SERVER.claim_forced_refresh(now=1180.0))
        finally:
            SERVER.LAST_FORCED_REFRESH = previous

    def test_ui_offers_copyable_source_subscription_links(self):
        page = (SERVER.WEB_ROOT / "app.html").read_text()
        self.assertIn("کپی لینک اشتراک", page)
        self.assertIn("copyText(source.url)", page)


if __name__ == "__main__":


    unittest.main()
