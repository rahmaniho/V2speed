#!/usr/bin/env python3
"""Local browser UI that runs the experimental V2Speed CSV ranker."""
from __future__ import annotations

import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

ROOT = Path(__file__).resolve().parents[3]
SMART_CONNECT = ROOT / "shared" / "smart-connect"
WEB_ROOT = Path(__file__).resolve().parent
MAX_UPLOAD_BYTES = 1_000_000
BUILD_DIR = Path(tempfile.mkdtemp(prefix="v2speed-csv-web-"))
RANKER = BUILD_DIR / "v2speed-csv-rank"


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
            content = (WEB_ROOT / "index.html").read_bytes()
            content_type = "text/html; charset=utf-8"
        elif path == "/demo.csv":
            content = (SMART_CONNECT / "tests" / "fixtures" / "demo_observations.csv").read_bytes()
            content_type = "text/csv; charset=utf-8"
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
