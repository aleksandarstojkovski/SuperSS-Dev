#!/usr/bin/env bash
# Start SQL Server, then restore pangya.bak once if the database is missing.
set -euo pipefail

SA_PASSWORD="${MSSQL_SA_PASSWORD:?MSSQL_SA_PASSWORD is required}"
BAK="${PANGYA_BAK:-/var/opt/mssql/backup/pangya.bak}"
BAK_URL="${PANGYA_BAK_URL:-http://51.89.73.141/pangya.bak}"
DATA_DIR="${MSSQL_DATA_DIR:-/var/opt/mssql/data}"
SQLCMD=(/opt/mssql-tools18/bin/sqlcmd -S localhost -U sa -P "${SA_PASSWORD}" -C -b)

sql() {
	"${SQLCMD[@]}" "$@"
}

download_bak() {
	if [[ -f "${BAK}" && -s "${BAK}" ]]; then
		return 0
	fi
	if [[ -z "${BAK_URL}" ]]; then
		echo "[mssql] missing ${BAK}. Put pangya.bak in docker/data/ or set PANGYA_BAK_URL." >&2
		return 1
	fi
	echo "[mssql] downloading ${BAK_URL} -> ${BAK}"
	mkdir -p "$(dirname "${BAK}")"
	if command -v curl >/dev/null 2>&1; then
		curl -fL --retry 4 --retry-delay 4 -o "${BAK}" "${BAK_URL}"
	elif command -v wget >/dev/null 2>&1; then
		wget -O "${BAK}" "${BAK_URL}"
	else
		echo "[mssql] neither curl nor wget is available to fetch ${BAK_URL}" >&2
		return 1
	fi
	[[ -s "${BAK}" ]]
}

wait_for_sql() {
	local i
	for i in $(seq 1 60); do
		if sql -Q "SELECT 1" >/dev/null 2>&1; then
			echo "[mssql] SQL Server is ready"
			return 0
		fi
		sleep 2
	done
	echo "[mssql] SQL Server did not become ready" >&2
	return 1
}

db_exists() {
	sql -h -1 -W -Q "SET NOCOUNT ON; SELECT name FROM sys.databases WHERE name = N'pangya'" 2>/dev/null \
		| tr -d '[:space:]' | grep -qx 'pangya'
}

restore_pangya() {
	if db_exists; then
		echo "[mssql] database pangya already present, skip restore"
		return 0
	fi
	download_bak
	echo "[mssql] restoring ${BAK} -> database pangya"
	sql -Q "RESTORE DATABASE [pangya] FROM DISK = N'${BAK}' WITH
		MOVE N'pangya' TO N'${DATA_DIR}/pangya.mdf',
		MOVE N'pangya_log' TO N'${DATA_DIR}/pangya_log.ldf',
		REPLACE, STATS = 10"
	echo "[mssql] restore finished"
	# Fresh dump uses the original test hash; map it to MD5(123456) for the default bot/login.
	sql -Q "UPDATE pangya.pangya.account SET PASSWORD = N'e10adc3949ba59abbe56e057f20f883e' WHERE ID = N'test'"
	echo "[mssql] set account test password to MD5(123456)"
}

/opt/mssql/bin/sqlservr &
SQL_PID=$!

cleanup() {
	kill "${SQL_PID}" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

if ! wait_for_sql || ! restore_pangya; then
	echo "[mssql] init failed" >&2
	exit 1
fi

trap - EXIT
wait "${SQL_PID}"
