#!/usr/bin/env python3
"""双击启动本地文件服务并在网页关闭后自动退出。"""
from __future__ import annotations

import secrets
import json
import subprocess
import sys
import time
import webbrowser
from pathlib import Path
from urllib.request import urlopen

ROOT = Path(__file__).parent
CONFIG = ROOT / "config.json"

if not CONFIG.exists():
    CONFIG.write_text((ROOT / "config.example.json").read_text(encoding="utf-8"), encoding="utf-8")
    print("已创建 config.json，请按需修改 host 或 web_port。")

config = json.loads(CONFIG.read_text(encoding="utf-8"))
web_port = int(config.get("web_port", 8080))

session = secrets.token_urlsafe(24)
command = [sys.executable, str(ROOT / "server.py"), "--config", str(CONFIG), "--ui-session", session]
process = subprocess.Popen(command, cwd=ROOT)
web_url = f"http://127.0.0.1:{web_port}/?session={session}"
for _ in range(50):
    if process.poll() is not None:
        raise RuntimeError("本地服务启动失败，请检查 config.json 中的 host 和 web_port")
    try:
        with urlopen(f"http://127.0.0.1:{web_port}/api/status", timeout=0.2):
            break
    except OSError:
        time.sleep(0.1)
else:
    process.terminate()
    raise RuntimeError("等待本地服务启动超时")

webbrowser.open(web_url)
try:
    process.wait()
except KeyboardInterrupt:
    process.terminate()
    process.wait()
