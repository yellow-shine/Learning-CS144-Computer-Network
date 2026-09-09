# 运行

镜像基于 `ubuntu:24.04`。apt 使用清华源：ARM 为 `https://mirrors.tuna.tsinghua.edu.cn/ubuntu-ports`，amd64 为 `https://mirrors.tuna.tsinghua.edu.cn/ubuntu`。

Intended path：从本仓库 `Dockerfile` 构建 `cs144-minnow`，再进入容器：

    ./scripts/dev.sh bash

当前 Docker Hub / apt 从 Docker VM 不可达，不要 `docker build` `ubuntu:24.04`。空构建改用本机已有镜像：

    CS144_IMAGE=jenkins-agent:task2-fixes ./scripts/dev.sh bash

`scripts/dev.sh` 在 `cs144-minnow` 不存在时也会回退到 `jenkins-agent:task2-fixes`。空构建：

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
