# syntax=docker/dockerfile:1
#
# Containerized Linux build for the Superss-Dev (PangYa) server.
#
# Stage 1 (builder) compiles every Linux server component from the
# "Server Lib/Linux Builds" Makefiles. Stage 2 (runtime) is a small image
# that carries the compiled binaries together with just their runtime
# shared libraries.
#
#   docker build -t superss-dev-linux .
#   docker run --rm superss-dev-linux

############################################
# Stage 1 - builder: compile all components
############################################
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Toolchain + libraries required by the servers. These mirror the
# CXXFLAGS / CXXLINK flags in Server Lib/Linux Builds/Makefile-*.mk
# (glib-2.0, libzip, the MySQL client, unixODBC and OpenSSL).
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        pkg-config \
        libglib2.0-dev \
        libzip-dev \
        default-libmysqlclient-dev \
        unixodbc-dev \
        libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Only the server sources are needed to build.
COPY ["Server Lib", "Server Lib"]

WORKDIR "/src/Server Lib/Linux Builds"

# Build order matters: the GGSrvLib26-1 static library and the Smart
# Calculator / GG Auth 7.0 shared libraries must exist before the Game
# Server, which links / consumes them.
RUN set -eux; \
    for mk in ggsrvlib smart ggauth70 as ls ms rs iff gs; do \
        echo "==> Building Makefile-$mk.mk"; \
        make -f "Makefile-$mk.mk" -j"$(nproc)"; \
    done

# Fail the build if any of the 9 expected artifacts is missing.
RUN set -eux; \
    for a in \
        "GGSrvLib26-1/GGSrvLib26-1.a" \
        "Auth Server/auth" \
        "Login Server/login" \
        "Message Server/msn" \
        "Rank Server/rank" \
        "IFF Manager/iff" \
        "Game Server/game" \
        "Game Server/SMARTCALCULATORLIB.so" \
        "Game Server/ggauth701.so"; do \
        test -e "$a" || { echo "MISSING ARTIFACT: $a"; exit 1; }; \
    done; \
    echo "All 9 components built successfully."

############################################
# Stage 2 - runtime: binaries + runtime libs
############################################
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# Runtime shared libraries the binaries link against, plus tmux (used by
# the bundled run-as-to-rs.sh launcher).
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        libglib2.0-0 \
        libzip4 \
        libmysqlclient21 \
        unixodbc \
        libssl3 \
        gawk \
        tmux \
    && rm -rf /var/lib/apt/lists/*

# Entrypoint that rewrites server.ini for the Docker network at startup.
COPY docker/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

WORKDIR "/opt/superss/Linux Builds"

# Copy the built server folders (binaries + config files + shared libs).
COPY --from=builder ["/src/Server Lib/Linux Builds/GGSrvLib26-1", "GGSrvLib26-1"]
COPY --from=builder ["/src/Server Lib/Linux Builds/Auth Server", "Auth Server"]
COPY --from=builder ["/src/Server Lib/Linux Builds/Login Server", "Login Server"]
COPY --from=builder ["/src/Server Lib/Linux Builds/Message Server", "Message Server"]
COPY --from=builder ["/src/Server Lib/Linux Builds/Rank Server", "Rank Server"]
COPY --from=builder ["/src/Server Lib/Linux Builds/Game Server", "Game Server"]
COPY --from=builder ["/src/Server Lib/Linux Builds/IFF Manager", "IFF Manager"]
COPY --from=builder ["/src/Server Lib/Linux Builds/run-as-to-rs.sh", "run-as-to-rs.sh"]
COPY --from=builder ["/src/Server Lib/Linux Builds/odbc.ini", "odbc.ini"]

# Each server reads/writes a Log directory relative to its working dir.
RUN for d in "Auth Server" "Login Server" "Message Server" "Rank Server" "Game Server" "IFF Manager"; do \
        mkdir -p "$d/Log"; \
    done

# By default, show the compiled artifacts. To run a server, start it from
# its folder (a reachable MySQL database is required), e.g.:
#   docker run --rm -it superss-dev-linux bash
#   cd "Auth Server" && ./auth
CMD ["bash", "-lc", "echo 'Superss-Dev - Linux server components:'; ls -la 'Auth Server/auth' 'Login Server/login' 'Message Server/msn' 'Rank Server/rank' 'Game Server/game' 'IFF Manager/iff' 'Game Server/SMARTCALCULATORLIB.so' 'Game Server/ggauth701.so' 'GGSrvLib26-1/GGSrvLib26-1.a'"]
