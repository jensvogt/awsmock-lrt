FROM alpine:3.22

ENV VCPKG_FORCE_SYSTEM_BINARIES=1
ENV JAVA_HOME=/usr/lib/jvm/java-21-openjdk

RUN apk add --no-cache \
    build-base ninja git perl zip unzip curl tar pkgconfig linux-headers bash python3 py3-pip autoconf \
    autoconf-archive automake libtool m4 make gettext-dev patch pkgconf ncurses-dev ncurses-terminfo-base \
    coreutils openssl-dev zlib-dev bzip2-dev readline-dev sqlite-dev xz-dev libffi-dev util-linux-dev musl-dev \
    openjdk21-jdk ca-certificates libgoogle-perftools-dev

# Alpine ships CMake < 4.2; get the current release via pip
RUN pip3 install cmake --break-system-packages

WORKDIR /build

RUN git clone https://github.com/microsoft/vcpkg.git && \
    ./vcpkg/bootstrap-vcpkg.sh -disableMetrics

# Cache-bust when awsmock changes by passing its HEAD SHA as a build arg
ARG AWSMOCK_REV=HEAD
RUN git clone https://github.com/jensvogt/awsmock.git awsmock && \
    if [ "$AWSMOCK_REV" != "HEAD" ]; then git -C awsmock checkout "$AWSMOCK_REV"; fi

RUN cmake -B awsmock/cmake-build-release -S awsmock \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=/build/vcpkg/scripts/buildsystems/vcpkg.cmake \
        -DVCPKG_TARGET_TRIPLET=x64-linux-release \
        -DBUILD_TESTING=OFF \
        -G Ninja && \
    cmake --build awsmock/cmake-build-release \
        --target awsmockcore awsmockdb awsmockdto --parallel

COPY . /build/awsmock-lrt

RUN mkdir awsmock-lrt/lib/ && \
    cp awsmock/cmake-build-release/libawsmockcore.a awsmock-lrt/lib/ && \
    cp awsmock/cmake-build-release/libawsmockdb.a   awsmock-lrt/lib/ && \
    cp awsmock/cmake-build-release/libawsmockdto.a  awsmock-lrt/lib/

RUN cmake -B awsmock-lrt/cmake-build-release -S awsmock-lrt \
        -DCMAKE_BUILD_TYPE=Release \
        -DAWSMOCK_VCPKG_DIR=/build/awsmock/cmake-build-release/vcpkg_installed/x64-linux-release \
        -DJAVA_HOME=/usr/lib/jvm/java-21-openjdk \
        -G Ninja && \
    cmake --build awsmock-lrt/cmake-build-release --parallel
