#!/usr/bin/env bash
# 对应 SPEC §3 Phase 1.4 —— 初始化 OJ 数据库
# 用法:bash scripts/init_db.sh
#   环境变量:OJ_DB_USER(默认 root)、OJ_DB_HOST、OJ_DB_PORT、OJ_ADMIN_USER、OJ_ADMIN_PASS
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

DB_USER="${OJ_DB_USER:-root}"
DB_HOST="${OJ_DB_HOST:-localhost}"
DB_PORT="${OJ_DB_PORT:-3306}"
ADMIN_USER="${OJ_ADMIN_USER:-admin}"
ADMIN_PASS="${OJ_ADMIN_PASS:-admin123}"

# 选择 mysql 调用方式。若以普通用户身份尝试通过 unix socket 以 'root'
# 登录(Ubuntu 默认 auth_socket),则改用 `sudo`。
MYSQL_CMD=(mysql)
if [[ "${DB_USER}" == "root" && -z "${OJ_DB_PASS:-}" ]]; then
    if ! mysql -u"${DB_USER}" -h"${DB_HOST}" -P"${DB_PORT}" -e "SELECT 1" >/dev/null 2>&1; then
        if command -v sudo >/dev/null 2>&1 && sudo -n true 2>/dev/null; then
            MYSQL_CMD=(sudo mysql)
        fi
    fi
fi

echo "[init_db] applying schema on ${DB_USER}@${DB_HOST}:${DB_PORT}"
"${MYSQL_CMD[@]}" -u"${DB_USER}" -h"${DB_HOST}" -P"${DB_PORT}" \
    < "${ROOT_DIR}/sql/schema.sql"

echo "[init_db] building seed_admin"
cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/build" >/dev/null
cmake --build "${ROOT_DIR}/build" --target seed_admin >/dev/null

SEED_BIN="${ROOT_DIR}/build/seed_admin"
if [[ ! -x "${SEED_BIN}" ]]; then
    echo "[init_db] FATAL: seed_admin binary missing at ${SEED_BIN}" >&2
    exit 1
fi

echo "[init_db] seeding admin user: ${ADMIN_USER}"
READ_SQL="$("${SEED_BIN}" "${ADMIN_USER}" "${ADMIN_PASS}" admin)"
"${MYSQL_CMD[@]}" -u"${DB_USER}" -h"${DB_HOST}" -P"${DB_PORT}" oj -e "${READ_SQL}"

echo "[init_db] done. admin user: ${ADMIN_USER} / ${ADMIN_PASS}"
