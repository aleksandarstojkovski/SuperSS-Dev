# SuperSS Linux servers + Microsoft ODBC Driver 18.
# `docker compose up --build` compiles from source when the image is missing.
FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        pkg-config \
        libglib2.0-dev \
        libzip-dev \
        default-libmysqlclient-dev \
        unixodbc-dev \
        libssl-dev \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . /src

WORKDIR /src/Server Lib/Linux Builds
RUN set -eux; \
    for mk in Makefile-ggsrvlib.mk Makefile-smart.mk Makefile-ggauth70.mk \
              Makefile-as.mk Makefile-ls.mk Makefile-ms.mk Makefile-rs.mk \
              Makefile-iff.mk Makefile-gs.mk; do \
        echo "==> ${mk}"; \
        make -f "${mk}" -j"$(nproc)"; \
    done

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        curl ca-certificates gnupg \
        libglib2.0-0 libzip4 libmysqlclient21 unixodbc libssl3 \
    && curl -fsSL https://packages.microsoft.com/keys/microsoft.asc \
        | gpg --dearmor -o /usr/share/keyrings/microsoft-prod.gpg \
    && echo "deb [arch=amd64 signed-by=/usr/share/keyrings/microsoft-prod.gpg] https://packages.microsoft.com/ubuntu/24.04/prod noble main" \
        > /etc/apt/sources.list.d/mssql-release.list \
    && apt-get update \
    && ACCEPT_EULA=Y apt-get install -y --no-install-recommends msodbcsql18 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/superss/Linux Builds

COPY --from=build /src/Server\ Lib/Linux\ Builds/GGSrvLib26-1 ./GGSrvLib26-1
COPY --from=build /src/Server\ Lib/Linux\ Builds/Auth\ Server ./Auth Server
COPY --from=build /src/Server\ Lib/Linux\ Builds/Login\ Server ./Login Server
COPY --from=build /src/Server\ Lib/Linux\ Builds/Message\ Server ./Message Server
COPY --from=build /src/Server\ Lib/Linux\ Builds/Rank\ Server ./Rank Server
COPY --from=build /src/Server\ Lib/Linux\ Builds/Game\ Server ./Game Server
COPY --from=build /src/Server\ Lib/Linux\ Builds/IFF\ Manager ./IFF Manager

COPY docker/odbc.ini /etc/odbc.ini

RUN for d in "Auth Server" "Login Server" "Message Server" "Rank Server" "Game Server" "IFF Manager"; do \
        mkdir -p "$d/Log" "$d/data"; \
        chmod +x "$d/"* 2>/dev/null || true; \
    done
