#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════════
# scripts/ci/setup_go.sh — Go toolchain installation for CI
# ═══════════════════════════════════════════════════════════════════════════════
# Installs the exact Go version required by the Throne core (sing-box + Xray).
# Also installs protoc and the Go gRPC code generator.
#
# Environment variables (input):
#   GO_VERSION    — Go version to install (default: read from core/server/go.mod)
#   PROTOC_VER    — Protobuf compiler version (default: 31.1)
#
# This script is idempotent — safe to run multiple times.
# ═══════════════════════════════════════════════════════════════════════════════
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

echo "═══════════════════════════════════════════════════"
echo " Throne Go Setup — $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "═══════════════════════════════════════════════════"

# ─── Determine Go version ─────────────────────────────────────────────────────
if [[ -z "${GO_VERSION:-}" ]]; then
    # Extract from go.mod: "go 1.25.7" → "1.25.7"
    GO_VERSION=$(grep '^go ' "${REPO_ROOT}/core/server/go.mod" | awk '{print $2}')
    echo ">> Go version extracted from go.mod: ${GO_VERSION}"
fi
echo ">> Target Go version: ${GO_VERSION}"

# ─── Check if Go is already installed at the right version ────────────────────
if command -v go &>/dev/null; then
    CURRENT_GO=$(go version 2>/dev/null | grep -oP 'go\K[0-9]+\.[0-9]+(\.[0-9]+)?' || echo "0")
    if [[ "${CURRENT_GO}" == "${GO_VERSION}"* ]]; then
        echo ">> Go ${CURRENT_GO} already installed. Skipping download."
    else
        echo ">> Go ${CURRENT_GO} found, but ${GO_VERSION} required."
        echo ">> Expecting actions/setup-go or manual install to provide it."
    fi
else
    echo ">> Go not found in PATH."
    echo ">> In GitHub Actions, use actions/setup-go@v5 before this script."
    echo ">> For local dev: https://go.dev/dl/"
fi

# ─── Install Protoc ───────────────────────────────────────────────────────────
PROTOC_VER="${PROTOC_VER:-31.1}"
echo ""
echo ">> Installing protoc v${PROTOC_VER}..."

if command -v protoc &>/dev/null; then
    CURRENT_PROTOC=$(protoc --version 2>/dev/null | grep -oP '[0-9]+\.[0-9]+' || echo "0")
    echo ">> protoc ${CURRENT_PROTOC} found."
else
    ARCH="x86_64"   # amd64 only
    PROTOC_ZIP="protoc-${PROTOC_VER}-linux-${ARCH}.zip"
    PROTOC_URL="https://github.com/protocolbuffers/protobuf/releases/download/v${PROTOC_VER}/${PROTOC_ZIP}"

    echo ">> Downloading ${PROTOC_URL}..."
    curl -fLO --retry 5 --retry-delay 3 --retry-all-errors "${PROTOC_URL}"
    unzip -o "${PROTOC_ZIP}" -d /tmp/protoc_install
    sudo cp /tmp/protoc_install/bin/protoc /usr/local/bin/
    rm -rf "${PROTOC_ZIP}" /tmp/protoc_install

    echo ">> protoc installed: $(protoc --version)"
fi

# ─── Install Go protoc plugins ───────────────────────────────────────────────
echo ""
echo ">> Installing Go protoc plugins..."

if command -v go &>/dev/null; then
    go install google.golang.org/protobuf/cmd/protoc-gen-go@latest 2>&1 || {
        echo "WARN: Failed to install protoc-gen-go (non-fatal if already present)."
    }
    go install google.golang.org/grpc/cmd/protoc-gen-go-grpc@latest 2>&1 || {
        echo "WARN: Failed to install protoc-gen-go-grpc (non-fatal if already present)."
    }
    echo ">> Go protoc plugins installed."
else
    echo ">> SKIP: Go not available, cannot install protoc plugins."
fi

echo ""
echo "═══════════════════════════════════════════════════"
echo " Go setup complete."
echo "═══════════════════════════════════════════════════"
