FROM alpine:3.22

ENV VCPKG_FORCE_SYSTEM_BINARIES=1
ENV JAVA_HOME=/usr/lib/jvm/java-21-openjdk

RUN apk add --no-cache \
    build-base cmake ninja git perl zip unzip curl tar pkgconfig linux-headers bash python3 py3-pip autoconf \
    autoconf-archive automake libtool m4 make gettext-dev patch pkgconf ncurses-dev ncurses-terminfo-base \
    coreutils openssl-dev zlib-dev bzip2-dev readline-dev sqlite-dev xz-dev libffi-dev util-linux-dev musl-dev \
    openjdk21-jdk ca-certificates

# Alpine's packaged cmake (3.31) is too old for both awsmock-lrt (requires
# >= 4.2) and current vcpkg registry commits, whose scripts use
# string(JSON ... STRING_ENCODE) — a mode CMake only gained after 3.31.
# Install a recent cmake via pip and put it first on PATH for every cmake and
# vcpkg invocation below. (vcpkg's mongo-c-driver port used to break under
# CMake >= 4's stricter TRY_COMPILE flag parsing, but that's fixed upstream
# via a cmake-4.4.patch as of the baseline this project tracks.)
RUN python3 -m venv /opt/cmake-latest && \
    /opt/cmake-latest/bin/pip install cmake
ENV PATH="/opt/cmake-latest/bin:${PATH}"

WORKDIR /build

# Cache-bust when awsmock changes by passing its HEAD SHA as a build arg
ARG AWSMOCK_REV=HEAD
RUN git clone https://github.com/jensvogt/awsmock.git awsmock && \
    if [ "$AWSMOCK_REV" != "HEAD" ]; then git -C awsmock checkout "$AWSMOCK_REV"; fi

# Pin the vcpkg tool to the exact commit awsmock's own vcpkg.json declares as
# its builtin-baseline, so the ports/versions registry data and the vcpkg
# scripts executing the install come from the same consistent snapshot. A
# fixed, hardcoded commit here drifts behind upstream awsmock's baseline and
# fails with "no version database entry for <port> at <version>" once awsmock
# requests package versions newer than that commit's registry knows about.
RUN baseline=$(grep -oE '"builtin-baseline"[[:space:]]*:[[:space:]]*"[a-f0-9]{40}"' awsmock/vcpkg.json | grep -oE '[a-f0-9]{40}') && \
    git clone https://github.com/microsoft/vcpkg.git && \
    git -C vcpkg checkout "$baseline" && \
    ./vcpkg/bootstrap-vcpkg.sh -disableMetrics

RUN cmake -B awsmock/cmake-build-release -S awsmock \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=/build/vcpkg/scripts/buildsystems/vcpkg.cmake \
        -DVCPKG_TARGET_TRIPLET=x64-linux-release \
        -DBUILD_TESTING=OFF \
        -G Ninja && \
    cmake --build awsmock/cmake-build-release \
        --target awsmockcore awsmockdb awsmockdto --parallel

COPY . /build/awsmock-lrt

RUN mkdir -p awsmock-lrt/lib/ && \
    cp awsmock/cmake-build-release/libawsmockcore.a awsmock-lrt/lib/ && \
    cp awsmock/cmake-build-release/libawsmockdb.a   awsmock-lrt/lib/ && \
    cp awsmock/cmake-build-release/libawsmockdto.a  awsmock-lrt/lib/

RUN cmake -B awsmock-lrt/cmake-build-release -S awsmock-lrt \
        -DCMAKE_BUILD_TYPE=Release \
        -DAWSMOCK_VCPKG_DIR=/build/awsmock/cmake-build-release/vcpkg_installed/x64-linux-release \
        -DJAVA_HOME=/usr/lib/jvm/java-21-openjdk \
        -G Ninja && \
    cmake --build awsmock-lrt/cmake-build-release --parallel

