# syntax=docker/dockerfile:1

FROM ubuntu:24.04 AS dev

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        bash \
        build-essential \
        ca-certificates \
        clang \
        clang-tidy \
        cmake \
        cppcheck \
        gdb \
        gcovr \
        git \
        libgtest-dev \
        libssl-dev \
        libzstd-dev \
        ninja-build \
        pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
CMD ["bash"]

FROM dev AS test

COPY . /workspace
RUN CMAKE_GENERATOR=Ninja ./scripts/test.sh
RUN CMAKE_GENERATOR=Ninja ./scripts/demo_local_backup.sh
RUN CMAKE_GENERATOR=Ninja ./scripts/demo_crash_recovery.sh
RUN DPC_BENCH_SIZE=8M CMAKE_GENERATOR=Ninja ./scripts/bench.sh
CMD ["bash", "-lc", "./scripts/test.sh && ./scripts/demo_local_backup.sh && ./scripts/demo_crash_recovery.sh"]

FROM dev AS build

COPY . /workspace
RUN cmake -S /workspace -B /workspace/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DDPC_BUILD_TESTS=OFF \
    && cmake --build /workspace/build -j"$(nproc)" \
    && cmake --install /workspace/build --prefix /opt/dpc

FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        bash \
        ca-certificates \
        coreutils \
        diffutils \
        libssl3 \
        libzstd1 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /opt/dpc/bin/backupctl /usr/local/bin/backupctl
COPY --from=build /opt/dpc/bin/backup-client /usr/local/bin/backup-client
COPY --from=build /opt/dpc/bin/backup-server /usr/local/bin/backup-server
COPY --from=build /opt/dpc/bin/backup-bench /usr/local/bin/backup-bench
COPY README.md /opt/dpc/README.md
COPY docs /opt/dpc/docs

WORKDIR /data
CMD ["backupctl", "--help"]
