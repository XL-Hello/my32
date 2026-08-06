#!/usr/bin/env python3
"""局域网网页文件管理台与 HTTP 下载服务。"""

from __future__ import annotations

import argparse
import hashlib
import json
import mimetypes
import os
import shutil
import threading
import time
from collections import deque
from datetime import datetime, timezone
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path, PurePosixPath
from typing import Any
from urllib.parse import parse_qs, unquote, urlparse

DEFAULT_CONFIG: dict[str, Any] = {
    "host": "0.0.0.0",
    "web_port": 8080,
    "storage_dir": "storage",
    "max_upload_bytes": 8 * 1024 * 1024,
}

class ServiceState:
    def __init__(self, config: dict[str, Any], root: Path, ui_session: str | None,
                 shutdown_grace_seconds: float) -> None:
        self.config = config
        self.root = root.resolve()
        self.logs: deque[str] = deque(maxlen=200)
        self.lock = threading.Lock()
        self.ui_session = ui_session
        self.shutdown_grace_seconds = shutdown_grace_seconds
        self.ui_seen = False
        self.ui_last_heartbeat = 0.0
        self.ui_closed = False
        for directory in (self.root, self.root / "ota", self.root / "resources"):
            directory.mkdir(parents=True, exist_ok=True)
        self.log("服务初始化完成")

    def log(self, message: str) -> None:
        line = f"[{datetime.now().strftime('%H:%M:%S')}] {message}"
        with self.lock:
            self.logs.append(line)
        print(line, flush=True)

    def resolve_file(self, relative_path: str) -> Path:
        normalized = PurePosixPath(relative_path)
        if normalized.is_absolute() or ".." in normalized.parts or str(normalized) in ("", "."):
            raise ValueError("非法文件路径")
        path = (self.root / normalized).resolve()
        if self.root not in path.parents:
            raise ValueError("非法文件路径")
        return path

    @staticmethod
    def sha256(path: Path) -> str:
        digest = hashlib.sha256()
        with path.open("rb") as file:
            for block in iter(lambda: file.read(64 * 1024), b""):
                digest.update(block)
        return digest.hexdigest()

    def manifest(self) -> dict[str, Any]:
        path = self.root / "manifest.json"
        if not path.exists():
            return {"schema_version": 1, "generated_at": None, "ota": None, "resources": []}
        return json.loads(path.read_text(encoding="utf-8"))

    def save_manifest(self, manifest: dict[str, Any]) -> None:
        manifest["schema_version"] = 1
        manifest["generated_at"] = datetime.now(timezone.utc).isoformat()
        temporary = self.root / ".manifest.tmp"
        temporary.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        temporary.replace(self.root / "manifest.json")

    def file_info(self, path: Path) -> dict[str, Any]:
        return {"path": path.relative_to(self.root).as_posix(), "size": path.stat().st_size,
                "sha256": self.sha256(path)}

    def list_files(self) -> list[dict[str, Any]]:
        return [self.file_info(path) for path in sorted(self.root.rglob("*"))
                if path.is_file() and path.name != "manifest.json" and not path.name.startswith(".")]

    def publish_ota(self, path: Path) -> dict[str, Any]:
        info = self.file_info(path)
        manifest = self.manifest()
        manifest["ota"] = info
        self.save_manifest(manifest)
        self.log(f"已发布 OTA：{info['path']}，{info['size']} bytes")
        return info

    def heartbeat(self, session: str) -> bool:
        if self.ui_session is None or session != self.ui_session:
            return False
        self.ui_seen = True
        self.ui_last_heartbeat = time.monotonic()
        return True

    def close_ui(self, session: str) -> bool:
        if self.ui_session is None or session != self.ui_session:
            return False
        self.ui_closed = True
        return True

    def should_stop(self) -> bool:
        if self.ui_session is None or not self.ui_seen:
            return False
        return self.ui_closed or time.monotonic() - self.ui_last_heartbeat > self.shutdown_grace_seconds


class WebHandler(BaseHTTPRequestHandler):
    state: ServiceState

    def log_message(self, format: str, *args: object) -> None:
        self.state.log("Web " + format % args)

    def send_json(self, status: HTTPStatus, data: dict[str, Any]) -> None:
        payload = json.dumps(data, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def read_json(self) -> dict[str, Any]:
        length = int(self.headers.get("Content-Length", "0"))
        return json.loads(self.rfile.read(length).decode("utf-8"))

    def do_GET(self) -> None:
        parsed = urlparse(self.path)
        try:
            if parsed.path in ("/", "/index.html", "/style.css", "/app.js"):
                relative = "index.html" if parsed.path in ("/", "/index.html") else parsed.path[1:]
                static_path = Path(__file__).parent / "web" / relative
                payload = static_path.read_bytes()
                content_types = {
                    ".html": "text/html; charset=utf-8",
                    ".css": "text/css; charset=utf-8",
                    ".js": "application/javascript; charset=utf-8",
                }
                self.send_response(HTTPStatus.OK)
                self.send_header("Content-Type", content_types[static_path.suffix])
                self.send_header("Content-Length", str(len(payload))); self.end_headers(); self.wfile.write(payload); return
            if parsed.path == "/api/status":
                host = self.headers.get("Host", "localhost").split(":")[0]
                web_url = f"http://{host}:{self.state.config['web_port']}"
                self.send_json(HTTPStatus.OK, {"web_url": web_url, "manifest_url": f"{web_url}/api/manifest"}); return
            if parsed.path == "/api/manifest": self.send_json(HTTPStatus.OK, self.state.manifest()); return
            if parsed.path == "/api/files": self.send_json(HTTPStatus.OK, {"files": self.state.list_files()}); return
            if parsed.path == "/api/logs": self.send_json(HTTPStatus.OK, {"lines": list(self.state.logs)}); return
            if parsed.path.startswith("/files/"):
                file_path = self.state.resolve_file(unquote(parsed.path.removeprefix("/files/")))
                if not file_path.is_file(): raise FileNotFoundError
                self.send_response(HTTPStatus.OK); self.send_header("Content-Type", mimetypes.guess_type(file_path.name)[0] or "application/octet-stream")
                self.send_header("Content-Length", str(file_path.stat().st_size)); self.end_headers()
                with file_path.open("rb") as file: shutil.copyfileobj(file, self.wfile)
                return
            self.send_json(HTTPStatus.NOT_FOUND, {"error": "接口不存在"})
        except (FileNotFoundError, ValueError): self.send_json(HTTPStatus.NOT_FOUND, {"error": "文件不存在"})
        except Exception as error: self.state.log(f"Web GET 错误：{error}"); self.send_json(HTTPStatus.INTERNAL_SERVER_ERROR, {"error": str(error)})

    def do_POST(self) -> None:
        parsed = urlparse(self.path)
        try:
            session = parse_qs(parsed.query).get("session", [""])[0]
            if parsed.path == "/api/heartbeat":
                self.send_json(HTTPStatus.OK if self.state.heartbeat(session) else HTTPStatus.FORBIDDEN,
                               {"ok": self.state.ui_session is None or session == self.state.ui_session}); return
            if parsed.path == "/api/ui-close":
                self.send_json(HTTPStatus.OK if self.state.close_ui(session) else HTTPStatus.FORBIDDEN,
                               {"ok": self.state.ui_session is None or session == self.state.ui_session}); return
            if parsed.path == "/api/delete":
                path = self.state.resolve_file(self.read_json()["path"])
                if not path.is_file(): raise FileNotFoundError
                path.unlink(); manifest = self.state.manifest()
                if manifest.get("ota", {}).get("path") == path.relative_to(self.state.root).as_posix(): manifest["ota"] = None; self.state.save_manifest(manifest)
                self.state.log(f"已删除文件：{path.name}"); self.send_json(HTTPStatus.OK, {"ok": True}); return
            if parsed.path == "/api/upload": self.handle_upload(); return
            self.send_json(HTTPStatus.NOT_FOUND, {"error": "接口不存在"})
        except (KeyError, ValueError, FileNotFoundError) as error: self.send_json(HTTPStatus.BAD_REQUEST, {"error": str(error) or "请求无效"})
        except Exception as error: self.state.log(f"Web POST 错误：{error}"); self.send_json(HTTPStatus.INTERNAL_SERVER_ERROR, {"error": str(error)})

    def handle_upload(self) -> None:
        content_type = self.headers.get("Content-Type", "")
        if "multipart/form-data" not in content_type or "boundary=" not in content_type: raise ValueError("请求必须是 multipart/form-data")
        length = int(self.headers.get("Content-Length", "0"))
        if length <= 0 or length > self.state.config["max_upload_bytes"] + 8192: raise ValueError("上传文件过大或为空")
        body = self.rfile.read(length); boundary = ("--" + content_type.split("boundary=", 1)[1].strip('"')).encode()
        file_name = None; file_data = None; category = "resources"
        for part in body.split(boundary):
            headers, separator, data = part.partition(b"\r\n\r\n")
            if not separator: continue
            if data.endswith(b"\r\n"):
                data = data[:-2]
            header_text = headers.decode("utf-8", "ignore")
            if 'name="category"' in header_text: category = data.decode("utf-8", "ignore")
            if 'name="file"' in header_text:
                marker = 'filename="'; start = header_text.find(marker)
                if start >= 0: file_name = header_text[start + len(marker):].split('"', 1)[0]
                file_data = data
        if category not in ("ota", "resources") or not file_name or file_data is None: raise ValueError("缺少文件或类型")
        safe_name = Path(file_name).name
        if not safe_name or len(file_data) > self.state.config["max_upload_bytes"]: raise ValueError("文件名或大小无效")
        relative_path = f"{category}/{safe_name}"; target = self.state.resolve_file(relative_path)
        temporary = target.with_suffix(target.suffix + ".uploading")
        temporary.write_bytes(file_data); temporary.replace(target)
        info = self.state.publish_ota(target) if category == "ota" else self.state.file_info(target)
        self.state.log(f"网页上传完成：{relative_path}")
        self.send_json(HTTPStatus.CREATED, info)


def load_config(path: Path) -> dict[str, Any]:
    config = DEFAULT_CONFIG.copy()
    if path.exists(): config.update(json.loads(path.read_text(encoding="utf-8")))
    return config


def main() -> None:
    parser = argparse.ArgumentParser(description="本地固件与资源 HTTP 服务")
    parser.add_argument("--config", default="config.json", help="配置文件路径，默认 config.json")
    parser.add_argument("--ui-session", help="由启动器传入的网页会话标识")
    parser.add_argument("--shutdown-grace-seconds", type=float, default=5.0,
                        help="网页心跳中断后的自动停服等待时间")
    arguments = parser.parse_args(); config_path = Path(arguments.config)
    config = load_config(config_path)
    root = Path(config["storage_dir"])
    if not root.is_absolute(): root = Path(__file__).parent / root
    state = ServiceState(config, root, arguments.ui_session, arguments.shutdown_grace_seconds)
    WebHandler.state = state
    http_server = ThreadingHTTPServer((config["host"], config["web_port"]), WebHandler)
    state.log(f"Web 管理台监听 {config['host']}:{config['web_port']}")
    http_server.timeout = 0.5
    try:
        while not state.should_stop():
            http_server.handle_request()
        state.log("网页已关闭，服务自动停止")
    except KeyboardInterrupt: state.log("服务已停止")
    finally: http_server.server_close()


if __name__ == "__main__": main()
