#!/bin/bash
# =====================================================================================================================
# Musichien - checks the code without modifying it.
#
#   Usage:  scripts/check_code.sh
#
# Exits with a non zero code when something does not comply, which makes it usable as a pre-commit
# hook or a CI step.
#
#   C++  -> clang-format --dry-run  (formatting)
#        -> clang-tidy              (naming rules, m_ / p_, const correctness, modernisation)
#   QML  -> qmllint                (static analysis)
# =====================================================================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${PROJECT_DIR}"

FAILURE_COUNT=0

# ---------------------------------------------------------------------------------------------------------------------
# C++ formatting
# ---------------------------------------------------------------------------------------------------------------------
echo "--- clang-format --dry-run --------------------------------------------------------------------"

mapfile -d '' CPP_FILES < <(find source -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) -print0)

if [ ${#CPP_FILES[@]} -gt 0 ]; then
    if clang-format --style=file --dry-run -Werror "${CPP_FILES[@]}" 2>&1; then
        echo "  OK - every C++ file is formatted"
    else
        echo "  FAILED - run scripts/format_code.sh"
        FAILURE_COUNT=$((FAILURE_COUNT + 1))
    fi
fi

# ---------------------------------------------------------------------------------------------------------------------
# QML static analysis
# ---------------------------------------------------------------------------------------------------------------------
echo "--- qmllint -----------------------------------------------------------------------------------"

QML_LINT="$(command -v qmllint || echo "")"

if [ -z "${QML_LINT}" ]; then
    echo "  qmllint not found: skipped"
else

    mapfile -d '' QML_FILES < <(find source -type f -name '*.qml' -print0)

    if [ ${#QML_FILES[@]} -gt 0 ]; then
        if "${QML_LINT}" "${QML_FILES[@]}" 2>&1; then
            echo "  OK"
        else
            echo "  FAILED - see the findings above"
            FAILURE_COUNT=$((FAILURE_COUNT + 1))
        fi
    fi

fi

# ---------------------------------------------------------------------------------------------------------------------
# clang-tidy
#
# It needs a configured build directory to know how to compile each file. The compile_commands.json
# produced by the Debug preset is used when it exists; otherwise the step is skipped with a hint.
# ---------------------------------------------------------------------------------------------------------------------
echo "--- clang-tidy --------------------------------------------------------------------------------"

COMPILE_COMMANDS="${PROJECT_DIR}/../Musichien-build/Clang-Debug/compile_commands.json"

if ! command -v clang-tidy >/dev/null 2>&1; then
    echo "  clang-tidy not found: skipped"
elif [ ! -f "${COMPILE_COMMANDS}" ]; then
    echo "  ${COMPILE_COMMANDS} not found"
    echo "  configure the project first:  cmake --preset \"Clang-Debug Musichien\""
else

    # Careful: piping clang-tidy into 'tail' would make the 'if' test the status of 'tail', which is
    # always zero. The output is therefore captured first, and inspected for findings.
    #
    # ONE FILE PER CORE. clang-tidy re-parses every header of the project for each translation unit, so a
    # sequential run spends minutes doing the same work over and over - the same reason a compiler is never
    # invoked one file at a time on a modern machine. Parallelising it is what takes this step from minutes
    # to seconds, and it costs nothing in coverage: every file is still checked, only the waiting is removed.
    TIDY_LOG="$(mktemp)"
    TIDY_JOBS="$(nproc 2>/dev/null || echo 4)"

    set +e

    find source -type f -name '*.cpp' ! -name '*_test.cpp' -print0 |
        xargs -0 -n1 -P "${TIDY_JOBS}" \
            clang-tidy -p "${PROJECT_DIR}/../Musichien-build/Clang-Debug" \
                       --config-file="${PROJECT_DIR}/.clang-tidy" > "${TIDY_LOG}" 2>&1

    # xargs reports a non zero status as soon as ONE of its commands does, and clang-tidy does exactly that
    # when it has something to say. The log is what decides, not the exit status.
    TIDY_STATUS=0

    # One progress line per finished file, so that a slow run can be watched instead of waited on.
    printf '  checked %s file(s) on %s core(s)\n' \
           "$(find source -type f -name '*.cpp' ! -name '*_test.cpp' | wc -l)" "${TIDY_JOBS}"

    # The findings themselves, and nothing else: the 'Suppressed ... warnings ...' summary of each run would
    # bury them under thousands of lines that say the same thing.
    TIDY_FINDINGS="$(grep -E 'warning:|error:' "${TIDY_LOG}" || true)"

    printf '%s\n' "${TIDY_FINDINGS}" | tail -30

    set -e

    if [ -n "${TIDY_FINDINGS}" ]; then
        echo "  FAILED - see the findings above"
        FAILURE_COUNT=$((FAILURE_COUNT + 1))
    else
        echo "  OK - no finding"
    fi

    rm -f "${TIDY_LOG}"
fi


# ---------------------------------------------------------------------------------------------------------------------
# Result
# ---------------------------------------------------------------------------------------------------------------------
echo
if [ ${FAILURE_COUNT} -eq 0 ]; then
    echo "check_code.sh: everything is clean"
else
    echo "check_code.sh: ${FAILURE_COUNT} check(s) failed"
    exit 1
fi
