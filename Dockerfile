FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
# tuna: ports.ubuntu.com is unreachable from Docker; http until ca-certificates is installed
RUN arch="$(dpkg --print-architecture)" \
    && if [ "$arch" = "arm64" ]; then \
         sed -i 's|http://ports.ubuntu.com/ubuntu-ports/|http://mirrors.tuna.tsinghua.edu.cn/ubuntu-ports/|g' /etc/apt/sources.list.d/ubuntu.sources; \
       else \
         sed -i 's|http://archive.ubuntu.com/ubuntu/|http://mirrors.tuna.tsinghua.edu.cn/ubuntu/|g; s|http://security.ubuntu.com/ubuntu/|http://mirrors.tuna.tsinghua.edu.cn/ubuntu/|g' /etc/apt/sources.list.d/ubuntu.sources; \
       fi \
    && apt-get update && apt-get install -y --no-install-recommends \
    git cmake ninja-build build-essential g++ clang clang-tidy clang-format \
    pkg-config python3 iputils-ping traceroute iproute2 ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
