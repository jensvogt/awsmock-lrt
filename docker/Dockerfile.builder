FROM amazonlinux:2023

ENV JAVA_HOME=/usr/lib/jvm/java-21-amazon-corretto

RUN dnf install -y \
    gcc gcc-c++ gcc14 gcc14-c++ make cmake ninja-build git perl zip unzip tar pkgconf-pkg-config kernel-headers bash \
    python3 python3-pip autoconf autoconf271 autoconf-archive automake libtool m4 gettext-devel patch \
    ncurses-devel openssl-devel zlib-devel bzip2-devel readline-devel sqlite-devel \
    xz-devel libffi-devel libuuid-devel java-21-amazon-corretto-devel ca-certificates \
    diffutils findutils which && \
    dnf install -y --allowerasing curl && \
    dnf clean all

# vcpkg's python3 port requires autoconf >= 2.71, but Amazon Linux 2023's base
# "autoconf" package is pinned at 2.69 (older ports/tooling still depend on that
# version). autoconf271 installs side-by-side under versioned binary names,
# so symlink the unversioned names vcpkg's portfiles call (autoreconf, aclocal,
# ...) into /usr/local/bin, which sorts ahead of /usr/bin on PATH.
RUN for t in autoconf autoheader autom4te autoreconf autoscan autoupdate ifnames; do \
        ln -sf "/usr/bin/${t}2.71" "/usr/local/bin/${t}"; \
    done

# awsmock/awsmock-lrt use C++23 std::format and std::chrono::zoned_time /
# current_zone (timezone-aware chrono, GCC >= 14). AL2023's default "gcc"
# package is GCC 11. Alias the unversioned compiler names to gcc14 so both
# vcpkg's own builds and awsmock's build pick it up uniformly.
RUN ln -sf /usr/bin/gcc14-gcc /usr/local/bin/gcc && \
    ln -sf /usr/bin/gcc14-gcc /usr/local/bin/cc && \
    ln -sf /usr/bin/gcc14-g++ /usr/local/bin/g++ && \
    ln -sf /usr/bin/gcc14-g++ /usr/local/bin/c++
ENV CC=/usr/local/bin/gcc
ENV CXX=/usr/local/bin/g++

# amazonlinux:2023's packaged cmake is too old for both awsmock-lrt (requires
# >= 4.2) and current vcpkg registry commits, whose scripts use
# string(JSON ... STRING_ENCODE) — a mode CMake only gained after 3.31.
# Install a recent cmake via pip and put it first on PATH for every cmake and
# vcpkg invocation below.
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
# scripts executing the install come from the same consistent snapshot.
RUN baseline=$(grep -oE '"builtin-baseline"[[:space:]]*:[[:space:]]*"[a-f0-9]{40}"' awsmock/vcpkg.json | grep -oE '[a-f0-9]{40}') && \
    git clone https://github.com/microsoft/vcpkg.git && \
    git -C vcpkg checkout "$baseline" && \
    ./vcpkg/bootstrap-vcpkg.sh -disableMetrics

RUN cmake -B awsmock/cmake-build-release -S awsmock \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=/build/vcpkg/scripts/buildsystems/vcpkg.cmake \
        -DVCPKG_TARGET_TRIPLET=x64-linux-release \
        -DBUILD_TESTING=OFF \
        -G Ninja \
    || (find /build/vcpkg/buildtrees -name '*-err.log' -exec sh -c 'echo ===== {} =====; cat {}' \; ; exit 1)
RUN cmake --build awsmock/cmake-build-release \
        --target awsmockcore awsmockdb awsmockdto --parallel

COPY . /build/awsmock-lrt

RUN mkdir -p awsmock-lrt/lib/ && \
    cp awsmock/cmake-build-release/libawsmockcore.a awsmock-lrt/lib/ && \
    cp awsmock/cmake-build-release/libawsmockdb.a   awsmock-lrt/lib/ && \
    cp awsmock/cmake-build-release/libawsmockdto.a  awsmock-lrt/lib/

RUN cmake -B awsmock-lrt/cmake-build-release -S awsmock-lrt \
        -DCMAKE_BUILD_TYPE=Release \
        -DAWSMOCK_VCPKG_DIR=/build/awsmock/cmake-build-release/vcpkg_installed/x64-linux-release \
        -DJAVA_HOME=/usr/lib/jvm/java-21-amazon-corretto \
        -G Ninja && \
    cmake --build awsmock-lrt/cmake-build-release --parallel
