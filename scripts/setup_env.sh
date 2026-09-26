#!/bin/bash
# =====================================================================================================================
# Musichien - development environment
#
#   Usage:  source scripts/setup_env.sh
#
# Exports everything the build needs, and nothing else. The point is that the compiler, Qt and CMake
# used by the project are always the same, whatever has been installed or upgraded on the machine in
# the meantime.
#
# This script must be SOURCED, not executed:
#   source scripts/setup_env.sh        (bash, zsh)
# =====================================================================================================================

# ---------------------------------------------------------------------------------------------------------------------
# Root of the project
# ---------------------------------------------------------------------------------------------------------------------
if [ -n "${BASH_SOURCE[0]}" ]; then
    MUSICHIEN_PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
else
    MUSICHIEN_PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
fi
export MUSICHIEN_PROJECT_DIR

# ---------------------------------------------------------------------------------------------------------------------
# External dependencies
#
# Follows the convention of the other projects: a sibling folder holding every pinned dependency.
# ---------------------------------------------------------------------------------------------------------------------
if [ -z "${MUSICHIEN_EXTERNAL_DIR}" ]; then
    MUSICHIEN_EXTERNAL_DIR="$(cd "${MUSICHIEN_PROJECT_DIR}/../Roger-externals" 2>/dev/null && pwd)"
fi

if [ -z "${MUSICHIEN_EXTERNAL_DIR}" ]; then
    echo "Musichien: WARNING - no external dependencies directory found."
    echo "           Create it and re-source this script:"
    echo "             mkdir -p ${MUSICHIEN_PROJECT_DIR}/../Roger-externals"
else
    export MUSICHIEN_EXTERNAL_DIR
fi

# ---------------------------------------------------------------------------------------------------------------------
# Compiler
#
# CLANG_DIR is expected by CMakePresets.json.
# ---------------------------------------------------------------------------------------------------------------------
if [ -z "${CLANG_DIR}" ]; then
    export CLANG_DIR="/usr/bin"
fi

# ---------------------------------------------------------------------------------------------------------------------
# Qt
#
# The pinned Qt installed by scripts/install_dependencies.sh lives in:
#     ${MUSICHIEN_EXTERNAL_DIR}/Qt/<version>/gcc_64
#
# The newest version found is used. If none is found, the script warns loudly and CMake falls back to
# the Qt of the machine, which breaks the reproducibility guarantee on purpose.
# ---------------------------------------------------------------------------------------------------------------------
if [ -z "${MUSICHIEN_QT_DIR}" ] && [ -n "${MUSICHIEN_EXTERNAL_DIR}" ]; then

    QT_DESKTOP_DIR="${MUSICHIEN_EXTERNAL_DIR}/Qt"

    if [ -d "${QT_DESKTOP_DIR}" ]; then
        # Newest version first, then the desktop build of that version.
        LATEST_QT_VERSION="$(find "${QT_DESKTOP_DIR}" -maxdepth 1 -mindepth 1 -type d -name '6.*' \
                             -printf '%f\n' 2>/dev/null | sort -V | tail -n 1)"

        if [ -n "${LATEST_QT_VERSION}" ] && [ -d "${QT_DESKTOP_DIR}/${LATEST_QT_VERSION}/gcc_64" ]; then
            export MUSICHIEN_QT_DIR="${QT_DESKTOP_DIR}/${LATEST_QT_VERSION}/gcc_64"
        fi
    fi
fi

if [ -z "${MUSICHIEN_QT_DIR}" ]; then
    export MUSICHIEN_QT_DIR="/usr"
fi

# ---------------------------------------------------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------------------------------------------------
echo "-----------------------------------------------------------------------------------------------------"
echo " Musichien - development environment"
echo "-----------------------------------------------------------------------------------------------------"
echo "  MUSICHIEN_PROJECT_DIR   = ${MUSICHIEN_PROJECT_DIR}"
echo "  MUSICHIEN_EXTERNAL_DIR  = ${MUSICHIEN_EXTERNAL_DIR:-<not set>}"
echo "  MUSICHIEN_QT_DIR        = ${MUSICHIEN_QT_DIR}"
echo "  CLANG_DIR               = ${CLANG_DIR}"
echo "-----------------------------------------------------------------------------------------------------"
echo "  Useful commands:"
echo "    cmake --preset \"Clang-Debug Musichien\""
echo "    cmake --build --preset \"Build Clang-Debug Musichien\""
echo "    ctest --preset \"CTest Clang-Debug Musichien\""
echo "    ./Musichien-build/Clang-Debug/bin/musichien"
echo "-----------------------------------------------------------------------------------------------------"
