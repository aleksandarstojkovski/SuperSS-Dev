#!/bin/sh
# Copy compose-downloaded IFF to the hardcoded data/pangya_jp.iff path, then exec the server.
set -eu

IFF_SRC="${PANGYA_IFF:-/data/pangya_jp.iff}"
IFF_DST="${IFF_DST:-data/pangya_jp.iff}"

if [ -f "${IFF_SRC}" ]; then
	mkdir -p "$(dirname "${IFF_DST}")"
	if [ -d "${IFF_DST}" ]; then
		echo "[server] removing mistaken directory ${IFF_DST}"
		rm -rf "${IFF_DST}"
	fi
	cp "${IFF_SRC}" "${IFF_DST}"
	echo "[server] installed ${IFF_DST} from ${IFF_SRC}"
fi

exec "$@"
