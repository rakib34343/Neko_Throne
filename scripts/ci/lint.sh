#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════════
# scripts/ci/lint.sh — C++ static analysis, clang-tidy, and formatting check
# ═══════════════════════════════════════════════════════════════════════════════
# Runs cppcheck, clang-tidy (basic pass), and clang-format diff.
# Produces a lint-report/ artifact. The job is continue-on-error in CI.
#
# Usage: bash scripts/ci/lint.sh [--fix]
#   --fix   Apply clang-format fixes in-place (for local dev, NOT CI)
# ═══════════════════════════════════════════════════════════════════════════════
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LINT_DIR="${REPO_ROOT}/lint-report"
mkdir -p "${LINT_DIR}"
LINT_LOG="${LINT_DIR}/lint-report.log"
EXIT_CODE=0

echo "═══════════════════════════════════════════════════"
echo " Throne C++ Lint — $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "═══════════════════════════════════════════════════"

# ─── Install dependencies ─────────────────────────────────────────────────────
echo ">> Installing lint tools..."
if command -v apt-get &>/dev/null; then
    sudo apt-get update -qq
    sudo apt-get install -y -qq \
        cmake ninja-build \
        cppcheck \
        clang-format \
        clang-tidy \
        libxml2-utils \
        >/dev/null 2>&1
elif command -v brew &>/dev/null; then
    brew install cppcheck clang-format clang-tidy 2>/dev/null || true
else
    echo "WARN: Unsupported package manager. Assuming tools are pre-installed."
fi

echo ">> cppcheck version:     $(cppcheck --version)"
echo ">> clang-format version: $(clang-format --version)"
echo ">> clang-tidy version:   $(clang-tidy --version 2>/dev/null | head -1 || echo 'not found')"

# ─── cppcheck — static analysis ──────────────────────────────────────────────
echo ""
echo ">> Running cppcheck on src/ and include/..."
CPPCHECK_REPORT="${LINT_DIR}/cppcheck-report.xml"

cppcheck \
    --enable=warning,performance,portability,style \
    --std=c++20 \
    --suppress=missingInclude \
    --suppress=unmatchedSuppression \
    --suppress=unusedFunction \
    --suppress=unknownMacro \
    --suppress=useInitializationList \
    --inline-suppr \
    --error-exitcode=0 \
    --xml \
    --output-file="${CPPCHECK_REPORT}" \
    -I "${REPO_ROOT}/include" \
    -I "${REPO_ROOT}/3rdparty" \
    "${REPO_ROOT}/src/" \
    "${REPO_ROOT}/include/" \
    2>&1 | tee -a "${LINT_LOG}" || true

# Count and categorise findings
ISSUE_COUNT=$(grep -c '<error ' "${CPPCHECK_REPORT}" 2>/dev/null || echo "0")
CRITICAL_COUNT=$(grep -c 'severity="error"' "${CPPCHECK_REPORT}" 2>/dev/null || echo "0")
echo ">> cppcheck: ${ISSUE_COUNT} finding(s), ${CRITICAL_COUNT} severity=error."

if [[ "${ISSUE_COUNT}" -gt 0 ]]; then
    grep '<error ' "${CPPCHECK_REPORT}" | head -50 >> "${LINT_LOG}" 2>/dev/null || true
fi

# ─── clang-tidy — static analysis (advisory, no compile DB needed) ───────────
echo ""
echo ">> Running clang-tidy (header-only, advisory pass)..."
TIDY_REPORT="${LINT_DIR}/clang-tidy-report.txt"

if command -v clang-tidy &>/dev/null; then
    # Collect project sources (exclude 3rdparty) as an array for batch invocation
    mapfile -d '' TIDY_FILES < <(
      find "${REPO_ROOT}/src" -name '*.cpp' ! -path '*/3rdparty/*' -print0 | sort -z
    )

    if [[ ${#TIDY_FILES[@]} -eq 0 ]]; then
        echo ">> No C++ sources found for clang-tidy."
    else
        TIDY_ARGS=(
            "--config-file=${REPO_ROOT}/.clang-tidy"
            "--header-filter=${REPO_ROOT}/(src|include)/.*"
            "--"
            "-std=c++20"
            "-I${REPO_ROOT}/include"
            "-I${REPO_ROOT}/3rdparty"
            "-Wno-error"
            "-DQT_NO_KEYWORDS"
        )
        # Single batch invocation (much faster than per-file loop)
        clang-tidy "${TIDY_FILES[@]}" "${TIDY_ARGS[@]}" \
            > "${TIDY_REPORT}" 2>&1 || true
    fi

    if [[ -s "${TIDY_REPORT}" ]]; then
        TIDY_ISSUES=$(grep -c ': warning:\|: error:' "${TIDY_REPORT}" || echo 0)
    else
        TIDY_ISSUES=0
    fi
    echo ">> clang-tidy: ${TIDY_ISSUES} diagnostic(s). See clang-tidy-report.txt."
    cat "${TIDY_REPORT}" >> "${LINT_LOG}" 2>/dev/null || true
else
    echo ">> SKIP: clang-tidy not available"
fi

# ─── clang-format — style conformance ────────────────────────────────────────
echo ""
echo ">> Checking clang-format conformance..."

cd "${REPO_ROOT}"

# Find project source files (exclude 3rdparty)
SOURCES=$(find src/ include/ -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) \
    ! -path '*/3rdparty/*' 2>/dev/null || true)

if [[ -n "${SOURCES}" ]]; then
    if [[ "${1:-}" == "--fix" ]]; then
        echo ">> Applying clang-format fixes in-place..."
        echo "${SOURCES}" | xargs -r clang-format -i
        echo ">> Fixes applied."
    else
        # Dry-run: detect formatting violations
        FORMAT_DIFF=$(echo "${SOURCES}" | xargs -r clang-format --dry-run 2>&1 || true)
        if [[ -n "${FORMAT_DIFF}" ]]; then
            echo ">> Formatting violations detected:" | tee -a "${LINT_LOG}"
            echo "${FORMAT_DIFF}" | head -100 >> "${LINT_LOG}" 2>/dev/null || true
            echo ">> WARN: clang-format violations found (non-blocking). See lint-report.log"
        else
            echo ">> All files conform to clang-format style."
        fi
    fi
else
    echo ">> No source files found to format-check."
fi

# ─── XML validation (.qrc and .ui files) ─────────────────────────────────────
echo ""
echo ">> Validating .qrc and .ui XML files..."
XML_ERRORS=0
while IFS= read -r xmlfile; do
    if ! xmllint --noout "$xmlfile" 2>>"${LINT_LOG}"; then
        echo ">> XML error in: $xmlfile" | tee -a "${LINT_LOG}"
        XML_ERRORS=$((XML_ERRORS + 1))
    fi
done < <(find "${REPO_ROOT}" -type f \( -name '*.qrc' -o -name '*.ui' \) ! -path '*/build/*' 2>/dev/null)
if [[ "${XML_ERRORS}" -gt 0 ]]; then
    echo ">> XML validation: ${XML_ERRORS} error(s) found."
else
    echo ">> XML validation: OK"
fi

# ─── Summary ──────────────────────────────────────────────────────────────────
echo ""
echo "═══════════════════════════════════════════════════"
echo " Lint complete. Report dir: ${LINT_DIR}"
echo " Artifacts:  lint-report.log  cppcheck-report.xml  clang-tidy-report.txt"
echo " Exit code:  ${EXIT_CODE}"
echo "═══════════════════════════════════════════════════"

exit "${EXIT_CODE}"
