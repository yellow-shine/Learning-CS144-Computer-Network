# 运行

镜像基于 `ubuntu:24.04`（g++ 13、CMake ≥ 3.24、Ninja）。Docker 构建会把 `http_proxy` / `https_proxy` / `HTTP_PROXY` / `HTTPS_PROXY` 传给 `apt-get`。

    export http_proxy=http://192.168.71.40:7890
    export https_proxy=http://192.168.71.40:7890
    export HTTP_PROXY=http://192.168.71.40:7890
    export HTTPS_PROXY=http://192.168.71.40:7890
    ./scripts/dev.sh bash

空构建：

    ./scripts/dev.sh bash -lc 'g++ --version; cmake --version; cmake -S . -B build -G Ninja && cmake --build build'

默认 `test` 目标排除 webget 与 speed。需要分别跑：

    cmake --build build --target check0
    cmake --build build --target check_webget
    cmake --build build --target check1
    cmake --build build --target check2
    cmake --build build --target check3
    cmake --build build --target check5
    cmake --build build --target check6
    cmake --build build --target speed

TUN/TAP（Checkpoint 3/7 应用路径）：

    CS144_NET=1 ./scripts/dev.sh bash
