#!/usr/bin/env bash
# ==============================================================================
# OpenDTV RTD2841 / TCL RT41 Unified Turnkey Runner
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="${SCRIPT_DIR}"

# Locate Python 3 interpreter
PYTHON_BIN=""
for cand in \
    "${PYTHON_BIN:-}" \
    "$(command -v python3 2>/dev/null || true)" \
    "/opt/homebrew/bin/python3" \
    "/usr/local/bin/python3" \
    "/usr/bin/python3" \
    "$(command -v python 2>/dev/null || true)"; do
    if [ -n "${cand}" ] && [ -x "${cand}" ]; then
        # Check python version >= 3.8
        if "${cand}" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)' 2>/dev/null; then
            PYTHON_BIN="${cand}"
            break
        fi
    fi
done

if [ -z "${PYTHON_BIN}" ]; then
    echo "[-] Error: Python 3 (>= 3.8) is required but was not found in PATH." >&2
    exit 1
fi

# Forward execution to unified Python CLI engine
exec "${PYTHON_BIN}" "${REPO_ROOT}/tools/rtd2841_cli.py" "$@"
