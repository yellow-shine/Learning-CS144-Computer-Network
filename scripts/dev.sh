#!/usr/bin/env bash
set -euo pipefail
IMAGE="${CS144_IMAGE:-cs144-minnow}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_ARGS=(
  --build-arg "http_proxy=${http_proxy:-}"
  --build-arg "https_proxy=${https_proxy:-}"
  --build-arg "HTTP_PROXY=${HTTP_PROXY:-}"
  --build-arg "HTTPS_PROXY=${HTTPS_PROXY:-}"
)
RUN_ENV=(
  -e "http_proxy=${http_proxy:-}"
  -e "https_proxy=${https_proxy:-}"
  -e "HTTP_PROXY=${HTTP_PROXY:-}"
  -e "HTTPS_PROXY=${HTTPS_PROXY:-}"
)
if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
  if ! docker build "${BUILD_ARGS[@]}" -t "$IMAGE" "$ROOT"; then
    IMAGE=jenkins-agent:task2-fixes
  fi
fi
CAPS=()
if [[ "${CS144_NET:-}" == "1" ]]; then
  CAPS+=(--device /dev/net/tun --cap-add NET_ADMIN)
fi
docker run --rm "${CAPS[@]}" "${RUN_ENV[@]}" -v "$ROOT":/src -w /src "$IMAGE" "$@"
