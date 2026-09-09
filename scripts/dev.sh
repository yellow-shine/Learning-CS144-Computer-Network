#!/usr/bin/env bash
set -euo pipefail
IMAGE="${CS144_IMAGE:-cs144-minnow}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
  IMAGE=jenkins-agent:task2-fixes
fi
CAPS=()
if [[ "${CS144_NET:-}" == "1" ]]; then
  CAPS+=(--device /dev/net/tun --cap-add NET_ADMIN)
fi
docker run --rm "${CAPS[@]}" -v "$ROOT":/src -w /src "$IMAGE" "$@"
