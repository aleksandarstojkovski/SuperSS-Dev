#!/usr/bin/env bash
#
# Cloud Agent install script for Superss-Dev (PangYa server emulator).
#
# Installs the system libraries required to compile the C++ server
# components on Linux and then builds every component from the
# "Server Lib/Linux Builds" Makefiles.
#
# The script is idempotent: it can be run repeatedly and will simply
# refresh dependencies and rebuild.
set -euo pipefail

# Resolve the repository root as the parent of the directory holding this
# script (.cursor/), so the script works regardless of the checkout path.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/Server Lib/Linux Builds"

echo "==> Repository root: ${REPO_ROOT}"

# ---------------------------------------------------------------------------
# 1. System build dependencies
# ---------------------------------------------------------------------------
# g++ (>= 13, for C++20), make and pkg-config come with build-essential.
# The servers link against glib-2.0, libzip, the MySQL client, unixODBC and
# OpenSSL (see the CXXLINK flags in the Makefile-*.mk files).
echo "==> Installing system build dependencies"
export DEBIAN_FRONTEND=noninteractive
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
    build-essential \
    pkg-config \
    libglib2.0-dev \
    libzip-dev \
    default-libmysqlclient-dev \
    unixodbc-dev \
    libssl-dev

# ---------------------------------------------------------------------------
# 2. Build every server component
# ---------------------------------------------------------------------------
cd "${BUILD_DIR}"

JOBS="$(nproc)"

# Build order matters:
#   * GGSrvLib26-1.a is a static library linked by the Game Server.
#   * SMARTCALCULATORLIB.so and ggauth701.so are shared libraries copied
#     into the Game/Message Server folders and must exist before "gs".
# The remaining executables (auth, login, msn, rank, iff) are independent.
COMPONENTS=(
    "ggsrvlib:GGSrvLib26-1 static library"
    "smart:Smart Calculator shared library"
    "ggauth70:GG Auth 7.0 shared library"
    "as:Auth Server"
    "ls:Login Server"
    "ms:Message Server"
    "rs:Rank Server"
    "iff:IFF Manager"
    "gs:Game Server"
)

for entry in "${COMPONENTS[@]}"; do
    mk="${entry%%:*}"
    desc="${entry#*:}"
    echo "==> Building ${desc} (Makefile-${mk}.mk)"
    make -f "Makefile-${mk}.mk" -j"${JOBS}"
done

echo "==> Build complete. Produced binaries:"
ls -la \
    "GGSrvLib26-1/GGSrvLib26-1.a" \
    "Auth Server/auth" \
    "Login Server/login" \
    "Message Server/msn" \
    "Rank Server/rank" \
    "IFF Manager/iff" \
    "Game Server/game" \
    "Game Server/SMARTCALCULATORLIB.so" \
    "Game Server/ggauth701.so" 2>/dev/null || true
