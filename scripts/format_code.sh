#!/bin/bash
# =====================================================================================================================
# Musichien - formats every source file in place.
#
#   Usage:  scripts/format_code.sh
#
# C++  -> clang-format, driven by .clang-format
# QML  -> qmlformat, shipped with Qt
#
# IMPORTANT: CMake files are deliberately NOT formatted.
# clang-format knows how to format CMake, and does it badly for the banner comments used in this
# project. CMake files are written by hand, following the style of the existing ones.
# =====================================================================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${PROJECT_DIR}"

# ---------------------------------------------------------------------------------------------------------------------
# C++
# ---------------------------------------------------------------------------------------------------------------------
echo "--- clang-format: C++ sources -----------------------------------------------------------------"

mapfile -d '' CPP_FILES < <(find source -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) -print0)

if [ ${#CPP_FILES[@]} -eq 0 ]; then
    echo "  no C++ file found"
else
    clang-format --style=file -i "${CPP_FILES[@]}"
    echo "  formatted ${#CPP_FILES[@]} C++ file(s)"
fi

# ---------------------------------------------------------------------------------------------------------------------
# QML
# ---------------------------------------------------------------------------------------------------------------------
echo "--- qmlformat: QML sources --------------------------------------------------------------------"

QML_FORMAT="$(command -v qmlformat || echo "")"

if [ -z "${QML_FORMAT}" ]; then
    echo "  qmlformat not found: install Qt tools, or run scripts/install_dependencies.sh"
else

    mapfile -d '' QML_FILES < <(find source -type f -name '*.qml' -print0)

    if [ ${#QML_FILES[@]} -eq 0 ]; then
        echo "  no QML file found"
    else
        "${QML_FORMAT}" --inplace --normalize "${QML_FILES[@]}"
        echo "  formatted ${#QML_FILES[@]} QML file(s)"
    fi

fi

echo
echo "Done. Review the diff before committing: formatting and logic must never share a commit."
