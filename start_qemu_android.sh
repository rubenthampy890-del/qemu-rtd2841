#!/usr/bin/env bash
# Boot the Android image with the host display and a serial console.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${root}/run.sh" --serial "$@"
