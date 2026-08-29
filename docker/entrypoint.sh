#!/usr/bin/env bash
#
# Container entrypoint for the Superss-Dev Linux servers.
#
# The committed server.ini files point at a developer LAN (DBIP=localhost,
# bind IP 192.168.15.20, AUTHSERVER 127.0.0.1). This script rewrites those
# values from environment variables at startup so the server works inside the
# Docker network, WITHOUT modifying the repository defaults.
#
# Recognised environment variables (all optional):
#   BIND_IP        -> [SERVERINFO]   IP   (default 0.0.0.0 so the published
#                                          port is reachable)
#   DB_HOST        -> [NORMAL_DB]    DBIP
#   DB_PORT        -> [NORMAL_DB]    DBPORT
#   AUTH_HOST      -> [AUTHSERVER]   IP
#   GGAUTH_HOST    -> [GGAUTHSERVER] IP
#   GAMEGUARDAUTH  -> [SERVERINFO]   GAMEGUARDAUTH
#
# Usage (from a server's working directory):
#   entrypoint.sh ./auth
set -euo pipefail

BIND_IP="${BIND_IP:-0.0.0.0}"
DB_HOST="${DB_HOST:-}"
DB_PORT="${DB_PORT:-}"
AUTH_HOST="${AUTH_HOST:-}"
GGAUTH_HOST="${GGAUTH_HOST:-}"
GAMEGUARDAUTH="${GAMEGUARDAUTH:-}"

cfg="server.ini"

if [ -f "$cfg" ]; then
    awk \
        -v bind="$BIND_IP" \
        -v dbip="$DB_HOST" \
        -v dbport="$DB_PORT" \
        -v authip="$AUTH_HOST" \
        -v ggip="$GGAUTH_HOST" \
        -v gga="$GAMEGUARDAUTH" '
        function setval(line, val) { sub(/=.*/, "= " val, line); return line }
        /^[[:space:]]*\[/ { sec=$0; gsub(/[[:space:]]/, "", sec); print; next }
        {
            l=$0
            if (l ~ /^[[:space:]]*IP[[:space:]]*=/) {
                if (sec=="[SERVERINFO]"   && bind   != "") { print setval(l, bind);   next }
                if (sec=="[AUTHSERVER]"   && authip != "") { print setval(l, authip); next }
                if (sec=="[GGAUTHSERVER]" && ggip   != "") { print setval(l, ggip);   next }
            }
            if (sec=="[NORMAL_DB]" && l ~ /^[[:space:]]*DBIP[[:space:]]*=/   && dbip   != "") { print setval(l, dbip);   next }
            if (sec=="[NORMAL_DB]" && l ~ /^[[:space:]]*DBPORT[[:space:]]*=/ && dbport != "") { print setval(l, dbport); next }
            if (sec=="[SERVERINFO]" && l ~ /^[[:space:]]*GAMEGUARDAUTH[[:space:]]*=/ && gga != "") { print setval(l, gga); next }
            print l
        }
    ' "$cfg" > "$cfg.tmp" && mv "$cfg.tmp" "$cfg"

    echo "[entrypoint] Patched $cfg (bind=${BIND_IP} db=${DB_HOST:-unchanged}:${DB_PORT:-unchanged} auth=${AUTH_HOST:-unchanged})"
    echo "[entrypoint] Effective connection settings:"
    grep -nE "^[[:space:]]*(IP|PORT|DBIP|DBPORT|DBNAME|GAMEGUARDAUTH)[[:space:]]*=" "$cfg" | sed 's/^/    /' || true
else
    echo "[entrypoint] WARNING: $cfg not found in $(pwd); starting without patching"
fi

echo "[entrypoint] exec: $*"
exec "$@"
