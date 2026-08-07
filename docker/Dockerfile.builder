FROM alpine:3.22

ENV VCPKG_FORCE_SYSTEM_BINARIES=1
ENV JAVA_HOME=/usr/lib/jvm/java-21-openjdk

RUN apk add --no-cache \
    build-base cmake ninja git perl zip unzip curl tar pkgconfig linux-headers bash python3 py3-pip autoconf \
    autoconf-archive automake libtool m4 make gettext-dev patch pkgconf ncurses-dev ncurses-terminfo-base \
    coreutils openssl-dev zlib-dev bzip2-dev readline-dev sqlite-dev xz-dev libffi-dev util-linux-dev musl-dev \
    openjdk21-jdk ca-certificates

# Alpine's packaged cmake (< 4.2) builds awsmock/vcpkg: CMake >= 4 breaks vcpkg's
# mongo-c-driver port (its TRY_COMPILE CMAKE_FLAGS "-Werror ..." trips CMake 4's
# stricter -W<category> parsing: "warning category error is not known").
# awsmock-lrt itself requires >= 4.2, so install that separately and use it only
# for the final awsmock-lrt configure/build below.
RUN python3 -m venv /opt/cmake-latest && \
    /opt/cmake-latest/bin/pip install cmake

WORKDIR /build

# Pin the vcpkg tool itself to the same commit as vcpkg.json's builtin-baseline.
# An unpinned (HEAD) vcpkg tool can drift to scripts requiring newer CMake
# features (e.g. string(JSON STRING_ENCODE)) than Alpine's packaged cmake
# provides, breaking the vcpkg install step below.
RUN git clone https://github.com/microsoft/vcpkg.git && \
    git -C vcpkg checkout c3867e714dd3a51c272826eea77267876517ed99 && \
    ./vcpkg/bootstrap-vcpkg.sh -disableMetrics

# Cache-bust when awsmock changes by passing its HEAD SHA as a build arg
ARG AWSMOCK_REV=HEAD
RUN git clone https://github.com/jensvogt/awsmock.git awsmock && \
    if [ "$AWSMOCK_REV" != "HEAD" ]; then git -C awsmock checkout "$AWSMOCK_REV"; fi

# awsmock's own vcpkg.json can pin a builtin-baseline newer than what was
# present when the vcpkg tool clone above was cached (that layer's git
# history is frozen at cache time, not re-fetched on later builds). Fetch
# that specific commit so vcpkg can resolve it even from a stale cache,
# without forcing a full vcpkg re-clone/rebuild.
RUN baseline=$(grep -oE '"builtin-baseline"[[:space:]]*:[[:space:]]*"[a-f0-9]{40}"' awsmock/vcpkg.json | grep -oE '[a-f0-9]{40}') && \
    git -C vcpkg fetch origin "$baseline"

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

RUN /opt/cmake-latest/bin/cmake -B awsmock-lrt/cmake-build-release -S awsmock-lrt \
        -DCMAKE_BUILD_TYPE=Release \
        -DAWSMOCK_VCPKG_DIR=/build/awsmock/cmake-build-release/vcpkg_installed/x64-linux-release \
        -DJAVA_HOME=/usr/lib/jvm/java-21-openjdk \
        -G Ninja && \
    /opt/cmake-latest/bin/cmake --build awsmock-lrt/cmake-build-release --parallel

