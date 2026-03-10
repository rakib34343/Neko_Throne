#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════════
# scripts/ci/release_pack.sh — Pack Throne release archives (amd64 only)
# ═══════════════════════════════════════════════════════════════════════════════
# Downloads all build artifacts, assembles release archives, extracts debug
# symbols and optionally publishes via ghr.
#
# Usage:
#   INPUT_VERSION=v1.2.3 ./scripts/ci/release_pack.sh
#   INPUT_VERSION=v1.2.3 PUBLISH_MODE=r ./scripts/ci/release_pack.sh   # release
#   INPUT_VERSION=v1.2.3 PUBLISH_MODE=p ./scripts/ci/release_pack.sh   # prerelease
#
# Environment variables (input):
#   INPUT_VERSION  — Release tag / version string (required)
#   PUBLISH_MODE   — "r" = release, "p" = prerelease, empty = no publish
#   GITHUB_TOKEN   — GitHub token (required if PUBLISH_MODE is set)
#
# Expects:
#   deployment/ directory already populated with extracted artifacts from
#   earlier build jobs (linux-amd64/, windows64/).
# ═══════════════════════════════════════════════════════════════════════════════
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${REPO_ROOT}"

echo "═══════════════════════════════════════════════════"
echo " Throne Release Pack — $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "═══════════════════════════════════════════════════"

if [[ -z "${INPUT_VERSION:-}" ]]; then
    echo "ERROR: INPUT_VERSION is not set."
    exit 1
fi

# ─── Install ghr ──────────────────────────────────────────────────────────────
GHR_VERSION="0.17.0"
if ! command -v ./ghr &>/dev/null; then
    echo ">> Installing ghr v${GHR_VERSION}..."
    curl -Lo - "https://github.com/tcnksm/ghr/releases/download/v${GHR_VERSION}/ghr_v${GHR_VERSION}_linux_amd64.tar.gz" | tar xzv
    mv ghr*linux_amd64/ghr .
fi

# ─── Resolve env ──────────────────────────────────────────────────────────────
source script/env_deploy.sh

# Assemble deployment/ from artifacts downloaded by actions/download-artifact.
# Structure after download: download-artifact/NekoThrone-<os>-amd64/<dirs>/...
echo ">> Assembling deployment from downloaded artifacts..."
mkdir -p deployment
for _adir in download-artifact/NekoThrone-*/; do
    if [[ -d "${_adir}" ]]; then
        echo ">> Merging ${_adir} -> deployment/"
        cp -r "${_adir}." deployment/
    fi
done

version_standalone="NekoThrone-${INPUT_VERSION}"

cd deployment
mkdir -p debug

# ─── Debian package ───────────────────────────────────────────────────────────
echo ""
echo ">> Building Debian package..."
bash "${REPO_ROOT}/script/pack_debian.sh" "${INPUT_VERSION}"
mv Throne.deb "${version_standalone}-debian-x64.deb"
rm -rf Throne

# ─── Linux amd64 archive ─────────────────────────────────────────────────────
echo ""
echo ">> Packing Linux amd64..."
if [[ -d linux-amd64 ]]; then
    mv linux-amd64 Throne
    mv Throne/Neko_Throne.debug "debug/${version_standalone}-linux-amd64.debug" 2>/dev/null || \
        mv Throne/Throne.debug "debug/${version_standalone}-linux-amd64.debug" 2>/dev/null || true
    # Ensure NO debug files remain in archive
    rm -f Throne/*.debug 2>/dev/null || true
    zip -9 -r "${version_standalone}-linux-amd64.zip" Throne
    rm -rf Throne
else
    echo "WARN: linux-amd64 directory not found, skipping."
fi

# ─── Windows 64-bit archive ──────────────────────────────────────────────────
echo ""
echo ">> Packing Windows 64-bit..."
if [[ -d windows64 ]]; then
    mv windows64 Throne
    # Support both old (Throne.pdb) and new (Neko_Throne.pdb) executable names
    find Throne -maxdepth 1 -name "*.pdb" | head -1 | \
        xargs -I{} mv {} "debug/${version_standalone}-windows64.pdb" 2>/dev/null || true
    # Ensure NO .pdb files remain in archive
    rm -f Throne/*.pdb 2>/dev/null || true
    zip -9 -r "${version_standalone}-windows64.zip" Throne
    rm -rf Throne
else
    echo "WARN: windows64 directory not found, skipping."
fi

# ─── Debug symbols bundle (separate artifact, not in release) ────────────────
echo ""
echo ">> Packing debug symbols..."
zip -9 -r debug-symbols.zip debug

# ─── Cleanup staging ─────────────────────────────────────────────────────────
# Remove debug dir and leftover symbols
rm -rf linux-amd64 windows64 debug *.pdb
# Move debug-symbols.zip out of deployment so ghr won't publish it
mv debug-symbols.zip "${REPO_ROOT}/debug-symbols.zip" 2>/dev/null || true

cd "${REPO_ROOT}"

# ─── Publish ──────────────────────────────────────────────────────────────────
PUBLISH_MODE="${PUBLISH_MODE:-}"

if [[ "${PUBLISH_MODE}" == "p" ]]; then
    echo ""
    echo ">> Publishing as PRE-release: ${INPUT_VERSION}"
    ./ghr -prerelease -delete -t "${GITHUB_TOKEN}" -n "${INPUT_VERSION}" "${INPUT_VERSION}" deployment
elif [[ "${PUBLISH_MODE}" == "r" ]]; then
    echo ""
    echo ">> Publishing as RELEASE: ${INPUT_VERSION}"
    ./ghr -delete -t "${GITHUB_TOKEN}" -n "${INPUT_VERSION}" "${INPUT_VERSION}" deployment
else
    echo ""
    echo ">> Skipping publish (PUBLISH_MODE not set)."
fi

echo ""
echo "═══════════════════════════════════════════════════"
echo " Release packaging complete."
echo "═══════════════════════════════════════════════════"
echo ""
echo ">> Contents of deployment/:"
ls -la deployment/
