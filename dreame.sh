#!/usr/bin/env bash

# 本地 OTA 测试入口。实际构建流程复用 build.sh，避免两份脚本配置漂移。
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${SCRIPT_DIR}/build.sh" "$@"
