FROM ubuntu:24.04
ARG http_proxy
ARG https_proxy
ARG HTTP_PROXY
ARG HTTPS_PROXY
ENV DEBIAN_FRONTEND=noninteractive \
    http_proxy=$http_proxy \
    https_proxy=$https_proxy \
    HTTP_PROXY=$HTTP_PROXY \
    HTTPS_PROXY=$HTTPS_PROXY
# tuna: proxy to ports.ubuntu.com 502s; Pipeline-Depth 0 avoids flaky proxy pipelining
RUN arch="$(dpkg --print-architecture)" \
    && if [ "$arch" = "arm64" ]; then \
         sed -i 's|http://ports.ubuntu.com/ubuntu-ports/|http://mirrors.tuna.tsinghua.edu.cn/ubuntu-ports/|g' /etc/apt/sources.list.d/ubuntu.sources; \
       else \
         sed -i 's|http://archive.ubuntu.com/ubuntu/|http://mirrors.tuna.tsinghua.edu.cn/ubuntu/|g; s|http://security.ubuntu.com/ubuntu/|http://mirrors.tuna.tsinghua.edu.cn/ubuntu/|g' /etc/apt/sources.list.d/ubuntu.sources; \
       fi \
    && printf 'Acquire::Retries "5";\nAcquire::http::Pipeline-Depth "0";\n' > /etc/apt/apt.conf.d/80-retries \
    && for _ in 1 2 3 4 5; do apt-get update && break; sleep 10; done \
    && for _ in 1 2 3 4 5; do apt-get install -y --no-install-recommends \
    git cmake ninja-build g++ python3 ca-certificates && break; sleep 15; done \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
