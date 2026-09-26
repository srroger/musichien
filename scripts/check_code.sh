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
    echo "  running clang-tidy over ${COMPILE_COMMANDS}"
    if clang-tidy -p "${PROJECT_DIR}/../Musichien-build/Clang-Debug" \
                  --config-file="${PROJECT_DIR}/.clang-tidy" \
                  $(find source -type f -name '*.cpp' ! -name '*_test.cpp') 2>&1 | tail -40; then
        echo "  OK"
    else
        echo "  FAILED - see the findings above"
        FAILURE_COUNT=$((FAILURE_COUNT + 1))
    fi
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
