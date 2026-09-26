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
# Guard: this file only does anything useful when it is SOURCED.
#
# Executed instead of sourced, it would still print the summary at the end, but every export would be
# lost the moment the script terminates. The user would see nothing wrong, while the build quietly
# used the Qt of the machine. Failing loudly is the only safe behaviour here.
# ---------------------------------------------------------------------------------------------------------------------
if [ -n "${BASH_VERSION:-}" ] && [ "${BASH_SOURCE[0]:-}" = "${0:-}" ]; then
    echo "Musichien: ERROR - this script must be SOURCED, not executed."
    echo ""
    echo "           Wrong:  scripts/setup_env.sh"
    echo "           Right:  source scripts/setup_env.sh"
    echo ""
    return 1 2>/dev/null || exit 1
fi

# ---------------------------------------------------------------------------------------------------------------------
# This file is sourced into arbitrary shells, including shells running with 'set -u'. Every reference
# to a variable that may not be defined yet therefore uses the ${VAR:-} form. Without that precaution,
# 'set -u' inherited from a caller script aborts the whole environment setup on the first line.
# ---------------------------------------------------------------------------------------------------------------------

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
if [ -z "${MUSICHIEN_EXTERNAL_DIR:-}" ]; then
    MUSICHIEN_EXTERNAL_DIR="$(cd "${MUSICHIEN_PROJECT_DIR}/../Roger-externals" 2>/dev/null && pwd || true)"
fi

if [ -z "${MUSICHIEN_EXTERNAL_DIR:-}" ]; then
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
if [ -z "${CLANG_DIR:-}" ]; then
    export CLANG_DIR="/usr/bin"
fi


# ---------------------------------------------------------------------------------------------------------------------
# CMake and Ninja
#
# The other projects on this machine pin their own CMake and Ninja in the external directory. Using
# them as well means the build does not change when the system versions are upgraded.
# The newest pinned version wins; the system tool is used only if none is pinned.
# ---------------------------------------------------------------------------------------------------------------------
if [ -n "${MUSICHIEN_EXTERNAL_DIR:-}" ]; then

    PINNED_CMAKE_DIR="$(find "${MUSICHIEN_EXTERNAL_DIR}" -maxdepth 1 -mindepth 1 -type d -name 'cmake-*' \
                        2>/dev/null | sort -V | tail -n 1)"


    if [ -n "${PINNED_CMAKE_DIR}" ] && [ -x "${PINNED_CMAKE_DIR}/bin/cmake" ]; then
        export PATH="${PINNED_CMAKE_DIR}/bin:${PATH}"
        export MUSICHIEN_PINNED_CMAKE_DIR="${PINNED_CMAKE_DIR}"
    fi

    PINNED_NINJA_DIR="$(find "${MUSICHIEN_EXTERNAL_DIR}" -maxdepth 1 -mindepth 1 -type d -name 'ninja-*' \
                        2>/dev/null | sort -V | tail -n 1)"

    if [ -n "${PINNED_NINJA_DIR}" ] && [ -x "${PINNED_NINJA_DIR}/ninja" ]; then
        export PATH="${PINNED_NINJA_DIR}:${PATH}"
        export MUSICHIEN_PINNED_NINJA_DIR="${PINNED_NINJA_DIR}"
    fi

fi


# ---------------------------------------------------------------------------------------------------------------------
# Qt
#
# Two layouts are supported, because both are legitimately used on this machine:
#
#   1. the layout produced by aqtinstall (see scripts/install_dependencies.sh)
#          <externals>/Qt/<version>/gcc_64
#
#   2. the flat layout used by the other personal projects
#          <externals>/Qt-<version>
#
# The newest version that actually contains a Qt6 installation wins. If nothing is found, the script
# warns loudly and CMake falls back to the Qt of the machine, which breaks reproducibility on purpose.
# ---------------------------------------------------------------------------------------------------------------------

# Prints "<version><TAB><directory>" for every pinned Qt installation found.
musichien_list_pinned_qt_installations()
{
    [ -n "${MUSICHIEN_EXTERNAL_DIR:-}" ] || return 0

    if [ -d "${MUSICHIEN_EXTERNAL_DIR}/Qt" ]; then

        find "${MUSICHIEN_EXTERNAL_DIR}/Qt" -maxdepth 1 -mindepth 1 -type d -name '6.*' 2>/dev/null |
        while IFS= read -r versionDirectory; do
            printf '%s\t%s\n' "$(basename "${versionDirectory}")" "${versionDirectory}/gcc_64"
        done
    fi

    find "${MUSICHIEN_EXTERNAL_DIR}" -maxdepth 1 -mindepth 1 -type d -name 'Qt-6.*' 2>/dev/null |
    while IFS= read -r versionDirectory; do
        printf '%s\t%s\n' "${versionDirectory##*Qt-}" "${versionDirectory}"
    done
}

if [ -z "${MUSICHIEN_QT_DIR:-}" ] || [ "${MUSICHIEN_QT_IS_FALLBACK:-0}" = "1" ]; then


    TAB_CHARACTER="$(printf '\t')"

    # Keep only the directories holding a real Qt6 installation, then take the newest version.
    BEST_QT_ENTRY="$(musichien_list_pinned_qt_installations |
                     while IFS="${TAB_CHARACTER}" read -r qtVersion qtDirectory; do
                         if [ -f "${qtDirectory}/lib/cmake/Qt6/Qt6Config.cmake" ]; then
                             printf '%s\t%s\n' "${qtVersion}" "${qtDirectory}"
                         fi
                     done |
                     sort -V | tail -n 1)"

    if [ -n "${BEST_QT_ENTRY}" ]; then
        export MUSICHIEN_QT_VERSION="${BEST_QT_ENTRY%%${TAB_CHARACTER}*}"
        export MUSICHIEN_QT_DIR="${BEST_QT_ENTRY#*${TAB_CHARACTER}}"
        export MUSICHIEN_QT_IS_FALLBACK=0
    fi

fi

if [ -z "${MUSICHIEN_QT_DIR:-}" ]; then
    echo "Musichien: WARNING - no pinned Qt found in '${MUSICHIEN_EXTERNAL_DIR:-}'."

    echo "           Falling back to the Qt installed on this machine (/usr):"
    echo "           the build is then NOT reproducible. See docs/BUILD_AND_SETUP.md"
    export MUSICHIEN_QT_DIR="/usr"

    # Remember that this value is our own fallback and not a deliberate choice of the user.
    # Without this flag, the fallback would stick for the whole session: installing a pinned Qt
    # afterwards would have no effect, because this script respects an already defined value.
    export MUSICHIEN_QT_IS_FALLBACK=1
fi



# ---------------------------------------------------------------------------------------------------------------------
# Android
#
# Nothing here is needed by a desktop build: a machine that only runs the application on the computer
# never has to install any of it. Everything is exported from the pinned external directory, never
# from the system.
#
# ANDROID_SDK_ROOT and ANDROID_NDK_ROOT are the two variables the NDK toolchain, Qt's toolchain file
# and androiddeployqt all look for. Exporting them is what makes the cross compilation work with no
# interactive step and no Qt Creator.
# ---------------------------------------------------------------------------------------------------------------------
if [ -n "${MUSICHIEN_EXTERNAL_DIR:-}" ]; then

    MUSICHIEN_ANDROID_SDK_CANDIDATE="${MUSICHIEN_EXTERNAL_DIR}/android-sdk"

    if [ -d "${MUSICHIEN_ANDROID_SDK_CANDIDATE}" ]; then

        export MUSICHIEN_ANDROID_SDK_DIR="${MUSICHIEN_ANDROID_SDK_CANDIDATE}"
        export ANDROID_SDK_ROOT="${MUSICHIEN_ANDROID_SDK_DIR}"
        export ANDROID_HOME="${MUSICHIEN_ANDROID_SDK_DIR}"

        # The newest NDK wins, exactly like the pinned CMake and Ninja above. Several NDKs can live
        # side by side, and Qt was compiled against one precise revision: r27c here, which is the
        # only one this project is expected to use.
        MUSICHIEN_ANDROID_NDK_DIR="$(find "${MUSICHIEN_ANDROID_SDK_DIR}/ndk" -maxdepth 1 -mindepth 1 -type d \
                                    2>/dev/null | sort -V | tail -n 1)"

        if [ -n "${MUSICHIEN_ANDROID_NDK_DIR}" ]; then
            export MUSICHIEN_ANDROID_NDK_DIR
            export ANDROID_NDK_ROOT="${MUSICHIEN_ANDROID_NDK_DIR}"
        fi

        # The platform-tools of the SDK goes FIRST on the PATH, on purpose.
        #
        # The 'android-tools' package of the distribution can be broken by a partial upgrade: its
        # binary then asks for a libprotobuf that is newer than the installed one, and every call
        # fails with 'cannot open shared object file'. The adb shipped inside the SDK has no such
        # dependency. Putting it first means the project always talks to a working adb, whatever
        # state the system packages are in.
        export PATH="${MUSICHIEN_ANDROID_SDK_DIR}/platform-tools:${PATH}"

        if [ -x "${MUSICHIEN_ANDROID_SDK_DIR}/cmdline-tools/latest/bin/sdkmanager" ]; then
            export PATH="${MUSICHIEN_ANDROID_SDK_DIR}/cmdline-tools/latest/bin:${PATH}"
        fi
    fi

    # Qt for Android lives next to the desktop Qt chosen above, in the same version directory:
    #   .../Qt/<version>/gcc_64              <- the desktop build
    #   .../Qt/<version>/android_arm64_v8a   <- the phone build
    # Deriving one from the other guarantees that both always share the same Qt version.
    if [ -d "${MUSICHIEN_QT_DIR:-/nonexistent}/lib/cmake/Qt6" ] &&
       [ -d "${MUSICHIEN_QT_DIR%/gcc_64}/android_arm64_v8a/lib/cmake/Qt6" ]; then
        export MUSICHIEN_QT_ANDROID_DIR="${MUSICHIEN_QT_DIR%/gcc_64}/android_arm64_v8a"
    fi

fi

# ---------------------------------------------------------------------------------------------------------------------
# JDK
#
# Gradle and the Android Gradle Plugin do not support the JDK 26 installed on this machine: the build
# fails with obscure messages about unsupported class file versions. JDK 21 is what the Android
# toolchain of Qt expects, so JAVA_HOME is pinned to it.
#
# JAVA_HOME is left untouched when it already designates a 21, so that a deliberate choice wins.
# ---------------------------------------------------------------------------------------------------------------------
if [ -d "/usr/lib/jvm/java-21-openjdk" ]; then
    case "${JAVA_HOME:-}" in
        *java-21*) ;;
        *) export JAVA_HOME="/usr/lib/jvm/java-21-openjdk" ;;
    esac
fi

# ---------------------------------------------------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------------------------------------------------
echo "-----------------------------------------------------------------------------------------------------"
echo " Musichien - development environment"
echo "-----------------------------------------------------------------------------------------------------"
echo "  MUSICHIEN_PROJECT_DIR   = ${MUSICHIEN_PROJECT_DIR}"
echo "  MUSICHIEN_EXTERNAL_DIR  = ${MUSICHIEN_EXTERNAL_DIR:-<not set>}"
echo "  MUSICHIEN_QT_VERSION    = ${MUSICHIEN_QT_VERSION:-<not found>}"
echo "  MUSICHIEN_QT_DIR        = ${MUSICHIEN_QT_DIR:-<not set>}"
echo "  CLANG_DIR               = ${CLANG_DIR}"
echo "  cmake                   = $(command -v cmake) ($(cmake --version 2>/dev/null | head -1))"
echo "  ninja                   = $(command -v ninja)"
echo "-----------------------------------------------------------------------------------------------------"

if [ -n "${MUSICHIEN_ANDROID_SDK_DIR:-}" ]; then
echo "  Android (only needed by the Android presets)"
echo "    MUSICHIEN_ANDROID_SDK_DIR  = ${MUSICHIEN_ANDROID_SDK_DIR}"
echo "    ANDROID_NDK_ROOT           = ${ANDROID_NDK_ROOT:-<not installed>}"
echo "    MUSICHIEN_QT_ANDROID_DIR   = ${MUSICHIEN_QT_ANDROID_DIR:-<not installed>}"
echo "    JAVA_HOME                  = ${JAVA_HOME:-<not set>}"
echo "    adb                        = $(command -v adb || echo '<not found>')"
echo "-----------------------------------------------------------------------------------------------------"
fi

echo "  Useful commands:"
echo "    cmake --preset \"Clang-Debug Musichien\""
echo "    cmake --build --preset \"Build Clang-Debug Musichien\""
echo "    ctest --preset \"CTest Clang-Debug Musichien\""
echo "    ./Musichien-build/Clang-Debug/bin/musichien"
if [ -n "${MUSICHIEN_QT_ANDROID_DIR:-}" ]; then
echo "    scripts/build_android.sh"
fi

echo "-----------------------------------------------------------------------------------------------------"
