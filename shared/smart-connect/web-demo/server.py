#!/usr/bin/env python3
"""Local browser UI that runs the experimental V2Speed CSV ranker."""
from __future__ import annotations

import base64
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

ROOT = Path(__file__).resolve().parents[3]
SMART_CONNECT = ROOT / "shared" / "smart-connect"
WEB_ROOT = Path(__file__).resolve().parent
MAX_UPLOAD_BYTES = 1_000_000
BUILD_DIR = Path(tempfile.mkdtemp(prefix="v2speed-csv-web-"))
RANKER = BUILD_DIR / "v2speed-csv-rank"
SUBSCRIPTION_SOURCES = {
    "iboxz": {
        "name": "Free V2ray Collector · Mix",
        "url": "https://raw.githubusercontent.com/iboxz/free-v2ray-collector/main/main/mix",
        "api_url": "https://api.github.com/repos/iboxz/free-v2ray-collector/contents/main/mix?ref=main",
    },
    "thomas": {
        "name": "ThomasJasperthecat · Sublist 1",
        "url": "https://raw.githubusercontent.com/ThomasJasperthecat/sub/main/sublist1.txt",
        "api_url": "https://api.github.com/repos/ThomasJasperthecat/sub/contents/sublist1.txt?ref=main",
    },
    "v2collector": {
        "name": "V2rayCollector · Mixed Iran",
        "url": "https://raw.githubusercontent.com/youfoundamin/V2rayCollector/refs/heads/main/mixed_iran.txt",
        "api_url": "https://api.github.com/repos/youfoundamin/V2rayCollector/contents/mixed_iran.txt?ref=main",
    },
}
SUBSCRIPTION_CACHE: dict[str, dict] = {}
SUBSCRIPTION_CACHE_LOCK = threading.Lock()
SUBSCRIPTION_CACHE_SECONDS = 300
MAX_SUBSCRIPTION_BYTES = 1_000_000
SUPPORTED_SCHEMES = {"vless", "vmess", "trojan", "ss"}


def build_ranker() -> None:
    sources = [
        SMART_CONNECT / "src" / "smart_connect.c",
        SMART_CONNECT / "src" / "smart_connect_measurements.c",
        SMART_CONNECT / "src" / "smart_connect_probe_plan.c",
        SMART_CONNECT / "tools" / "v2speed_csv_rank.c",
    ]
    command = [
        os.environ.get("CC", "cc"), "-std=c11", "-O2", "-Wall", "-Wextra",
        "-Werror", "-pedantic", "-I", str(SMART_CONNECT / "include"),
        *(str(source) for source in sources), "-lm", "-o", str(RANKER),
    ]
    subprocess.run(command, check=True, cwd=ROOT, timeout=60)


def decode_subscription_payload(raw_text: str) -> str:
    text = raw_text.lstrip("\ufeff").strip()
    if any(line.strip().lower().startswith(tuple(scheme + "://" for scheme in SUPPORTED_SCHEMES))
           for line in text.splitlines()):
        return text
    compact = "".join(text.split())
    compact += "=" * ((4 - len(compact) % 4) % 4)
    try:
        decoded = base64.urlsafe_b64decode(compact).decode("utf-8-sig").strip()
    except (ValueError, UnicodeDecodeError):
        return text
    if any(line.strip().lower().startswith(tuple(scheme + "://" for scheme in SUPPORTED_SCHEMES))
           for line in decoded.splitlines()):
        return decoded
    return text


def decode_vmess(uri: str) -> tuple[str, str, str] | None:
    payload = uri[len("vmess://"):].split("#", 1)[0]
    payload += "=" * ((4 - len(payload) % 4) % 4)
    try:
        data = json.loads(base64.urlsafe_b64decode(payload).decode("utf-8"))
        host = str(data.get("add", ""))
        port = str(data.get("port", ""))
        name = urllib.parse.unquote(str(data.get("ps", "")))
    except (ValueError, UnicodeDecodeError, json.JSONDecodeError):
        return None
    return name, f"{host}:{port}" if host and port else host, "vmess"


def parse_subscription_row(uri: str) -> dict | None:
    stripped = uri.strip()
    if "://" not in stripped:
        return None
    scheme = stripped.split("://", 1)[0].lower()
    if scheme not in SUPPORTED_SCHEMES:
        return None
    if scheme == "vmess":
        parsed = decode_vmess(stripped)
        if parsed is None:
            return None
        name, endpoint, protocol = parsed
    else:
        try:
            parsed_uri = urllib.parse.urlsplit(stripped)
            host = parsed_uri.hostname or ""
            port = parsed_uri.port
        except ValueError:
            parsed_uri = urllib.parse.urlsplit("")
            host, port = "", None
        if scheme == "ss" and (not host or port is None):
            authority = stripped[len("ss://"):].split("#", 1)[0].split("?", 1)[0]
            token = authority.split("/", 1)[0]
            padded = token + "=" * ((4 - len(token) % 4) % 4)
            try:
                decoded = base64.urlsafe_b64decode(padded).decode("utf-8")
                address = decoded.rsplit("@", 1)[1]
                parsed_authority = urllib.parse.urlsplit("//" + address)
                host, port = parsed_authority.hostname or "", parsed_authority.port
            except (ValueError, UnicodeDecodeError, IndexError):
                host, port = "", None
        if not host or port is None:
            return None
        name = urllib.parse.unquote(parsed_uri.fragment).strip()
        endpoint = f"[{host}]:{port}" if ":" in host else f"{host}:{port}"
        protocol = "shadowsocks" if scheme == "ss" else scheme
    if not name:
        name = endpoint or protocol.upper()
    name = " ".join(name.replace("\x00", "").split())[:120]
    key = hashlib.sha256(stripped.encode("utf-8")).hexdigest()[:16]
    return {"id": key, "name": name, "endpoint": endpoint,
            "protocol": protocol, "uri": stripped}


def fetch_source(source_id: str, force: bool = False) -> dict:
    now = time.monotonic()
    with SUBSCRIPTION_CACHE_LOCK:
        cached = SUBSCRIPTION_CACHE.get(source_id)
        if not force and cached and now - cached["cached_at"] < SUBSCRIPTION_CACHE_SECONDS:
            return cached
    source = SUBSCRIPTION_SOURCES[source_id]
    request = urllib.request.Request(
        source["api_url"],
        headers={"User-Agent": "V2Speed-Prototype", "Accept": "application/vnd.github+json"},
    )
    with urllib.request.urlopen(request, timeout=20) as response:
        payload = json.loads(response.read(MAX_SUBSCRIPTION_BYTES * 2).decode("utf-8"))
    if payload.get("type") != "file" or int(payload.get("size", 0)) > MAX_SUBSCRIPTION_BYTES:
        raise ValueError("subscription file is not a supported size/type")
    encoded = payload.get("content", "")
    raw = base64.b64decode(encoded, validate=False)
    if len(raw) > MAX_SUBSCRIPTION_BYTES:
        raise ValueError("subscription file exceeds size limit")
    text = decode_subscription_payload(raw.decode("utf-8-sig", errors="replace"))
    items = []
    seen = set()
    for line in text.splitlines():
        row = line.strip()
        if not row or row.startswith("#"):
            continue
        parsed = parse_subscription_row(row)
        if parsed is None or parsed["id"] in seen:
            continue
        seen.add(parsed["id"])
        items.append(parsed)
    protocols: dict[str, int] = {}
    for item in items:
        protocols[item["protocol"]] = protocols.get(item["protocol"], 0) + 1
    result = {
        "source_id": source_id,
        "name": source["name"],
        "url": source["url"],
        "items": items,
        "protocols": protocols,
        "cached_at": now,
    }
    with SUBSCRIPTION_CACHE_LOCK:
        SUBSCRIPTION_CACHE[source_id] = result
    return result


def json_response(handler: BaseHTTPRequestHandler, status: int, payload: dict) -> None:
    body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    handler.send_response(status)
    handler.send_header("Content-Type", "application/json; charset=utf-8")
    handler.send_header("Content-Length", str(len(body)))
    handler.send_header("Cache-Control", "no-store")
    handler.end_headers()
    handler.wfile.write(body)


class Handler(BaseHTTPRequestHandler):
    server_version = "V2SpeedCsvTrial/0.1"

    def log_message(self, fmt: str, *args) -> None:
        print("[csv-trial] " + fmt % args, flush=True)

    def do_GET(self) -> None:
        path = urlparse(self.path).path
        if path in ("/", "/index.html"):
            content = (WEB_ROOT / "app.html").read_bytes()
            content_type = "text/html; charset=utf-8"
        elif path in ("/csv", "/csv.html"):
            content = (WEB_ROOT / "index.html").read_bytes()
            content_type = "text/html; charset=utf-8"
        elif path == "/brand.png":
            content = (ROOT / "docs" / "brand-assets" / "ui" / "v2speed-logo-dark-background.png").read_bytes()
            content_type = "image/png"
        elif path == "/demo.csv":
            content = (SMART_CONNECT / "tests" / "fixtures" / "demo_observations.csv").read_bytes()
            content_type = "text/csv; charset=utf-8"
        elif path == "/api/subscriptions":
            results = []
            errors = []
            force_refresh = parse_qs(urlparse(self.path).query).get("refresh") == ["1"]
            with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
                futures = {pool.submit(fetch_source, source_id, force_refresh): source_id
                           for source_id in SUBSCRIPTION_SOURCES}
                for future in concurrent.futures.as_completed(futures):
                    source_id = futures[future]
                    try:
                        source = future.result()
                        results.append({
                            "source_id": source["source_id"],
                            "name": source["name"],
                            "url": source["url"],
                            "count": len(source["items"]),
                            "protocols": source["protocols"],
                            "nodes": [{key: node[key] for key in ("id", "name", "endpoint", "protocol")}
                                      for node in source["items"]],
                        })
                    except (OSError, ValueError, urllib.error.URLError,
                            urllib.error.HTTPError, json.JSONDecodeError):
                        errors.append({"source_id": source_id,
                                       "name": SUBSCRIPTION_SOURCES[source_id]["name"],
                                       "message": "منبع فعلاً در دسترس نیست؛ بعداً دوباره تلاش کنید."})
            results.sort(key=lambda item: item["source_id"])
            json_response(self, 200, {
                "sources": results,
                "errors": errors,
                "warning": "پیکربندی‌های عمومی شخص ثالث هستند؛ فعال‌بودن، امنیت یا مجازبودن آن‌ها تأیید نشده است.",
            })
            return
        elif path.startswith("/api/config/"):
            match = re.fullmatch(r"/api/config/([a-z0-9_-]+)/([0-9a-f]{16})", path)
            if match is None or match.group(1) not in SUBSCRIPTION_SOURCES:
                json_response(self, 404, {"error": "پیکربندی پیدا نشد."})
                return
            try:
                source = fetch_source(match.group(1))
                node = next(item for item in source["items"] if item["id"] == match.group(2))
            except (OSError, ValueError, StopIteration, urllib.error.URLError,
                    urllib.error.HTTPError, json.JSONDecodeError):
                json_response(self, 404, {"error": "پیکربندی در دسترس نیست؛ فهرست را تازه‌سازی کنید."})
                return
            json_response(self, 200, {"uri": node["uri"]})
            return
        elif path == "/healthz":
            content = b"ok\n"
            content_type = "text/plain; charset=utf-8"
        else:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(content)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(content)

    def do_POST(self) -> None:
        if urlparse(self.path).path != "/rank":
            json_response(self, 404, {"error": "مسیر درخواستی پیدا نشد."})
            return
        content_type = self.headers.get("Content-Type", "").split(";", 1)[0].strip().lower()
        if content_type != "text/csv":
            json_response(self, 415, {"error": "فایل باید به‌صورت متن CSV ارسال شود."})
            return
        try:
            length = int(self.headers.get("Content-Length", "-1"))
        except ValueError:
            length = -1
        if length < 0 or length > MAX_UPLOAD_BYTES:
            json_response(self, 413, {"error": "حجم CSV نامعتبر است؛ سقف بارگذاری ۱ مگابایت است."})
            return
        query = parse_qs(urlparse(self.path).query)

        def integer_parameter(name: str) -> int | None:
            values = query.get(name, [])
            if (len(values) != 1 or len(values[0]) > 20 or
                    not re.fullmatch(r"[0-9]+", values[0])):
                return None
            return int(values[0])

        now_ms = integer_parameter("now_ms")
        max_age_ms = integer_parameter("max_age_ms")
        recent_ms = integer_parameter("recent_window_ms")
        if (now_ms is None or max_age_ms is None or max_age_ms == 0 or
                recent_ms is None or recent_ms == 0 or recent_ms > max_age_ms):
            json_response(self, 400, {"error": "زمان مرجع یا بازه‌های زمانی نامعتبر هستند."})
            return
        csv_bytes = self.rfile.read(length)
        if len(csv_bytes) != length:
            json_response(self, 400, {"error": "بارگذاری CSV کامل دریافت نشد."})
            return
        try:
            csv_bytes.decode("utf-8")
        except UnicodeDecodeError:
            json_response(self, 400, {"error": "فایل باید با UTF-8 ذخیره شده باشد."})
            return

        temp_file = tempfile.NamedTemporaryFile(
            prefix="upload-", suffix=".csv", dir=BUILD_DIR, delete=False
        )
        temp_path = Path(temp_file.name)
        try:
            with temp_file:
                temp_file.write(csv_bytes)
            completed = subprocess.run(
                [str(RANKER), str(temp_path), str(now_ms), str(max_age_ms), str(recent_ms)],
                cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace",
                timeout=10, check=False,
            )
        except subprocess.TimeoutExpired:
            json_response(self, 408, {"error": "پردازش CSV بیش از حد طول کشید."})
            return
        finally:
            try:
                temp_path.unlink(missing_ok=True)
            except OSError:
                pass
        if completed.returncode != 0:
            json_response(self, 422, {"error": completed.stderr.strip() or "CSV قابل پردازش نیست."})
            return
        json_response(self, 200, {
            "output": completed.stdout,
            "notice": completed.stderr.strip(),
        })


if __name__ == "__main__":
    build_ranker()
    port = int(os.environ.get("PORT", "8000"))
    server = ThreadingHTTPServer(("0.0.0.0", port), Handler)
    print(f"V2Speed CSV trial ready on 0.0.0.0:{port}", flush=True)
    try:
        server.serve_forever()
    finally:
        server.server_close()
