#!/bin/sh
# Download pangya.bak + pangya_jp.iff into /data when they are missing.
set -eu

DATA="${DATA_DIR:-/data}"
BAK_URL="${PANGYA_BAK_URL:-http://51.89.73.141/pangya.bak}"
IFF_URL="${PANGYA_IFF_URL:-http://51.89.73.141/pangya_jp.iff}"

mkdir -p "${DATA}"

fetch() {
	dest=$1
	url=$2
	label=$3
	if [ -d "${dest}" ]; then
		echo "[assets] removing mistaken directory ${dest}"
		rm -rf "${dest}"
	fi
	if [ -f "${dest}" ] && [ -s "${dest}" ]; then
		echo "[assets] ${label} already present ($(wc -c < "${dest}") bytes)"
		return 0
	fi
	echo "[assets] downloading ${label} from ${url}"
	tmp="${dest}.part"
	curl -fL --retry 4 --retry-delay 4 -o "${tmp}" "${url}"
	mv "${tmp}" "${dest}"
	echo "[assets] saved ${dest} ($(wc -c < "${dest}") bytes)"
}

fetch "${DATA}/pangya.bak" "${BAK_URL}" "pangya.bak"
fetch "${DATA}/pangya_jp.iff" "${IFF_URL}" "pangya_jp.iff"
echo "[assets] ready"
