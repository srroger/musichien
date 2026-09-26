#!/bin/bash
# =====================================================================================================================
# Musichien - installs every external dependency, pinned by version.
#
#   Usage:  scripts/install_dependencies.sh [--with-android] [--with-superbuild]
#
# What this script installs:
#   1. the system packages needed to build a Qt application (Arch / Manjaro)
#   2. aqtinstall, the command line Qt installer that does NOT require a Qt account
#   3. the pinned Qt itself: desktop always, Android only with --with-android
#   4. the dependencies built from source (GoogleTest, nlohmann/json), with --with-superbuild
#
# Every dependency lands in MUSICHIEN_EXTERNAL_DIR, and NOT in the system directories: the machine
# can be upgraded freely without ever breaking the project.
# =====================================================================================================================

set -euo pipefail

# ---------------------------------------------------------------------------------------------------------------------
# Arguments
# ---------------------------------------------------------------------------------------------------------------------
WITH_ANDROID=OFF
WITH_SUPERBUILD=OFF

for argument in "$@"; do
    case "${argument}" in
        --with-android)    WITH_ANDROID=ON ;;
        --with-superbuild) WITH_SUPERBUILD=ON ;;
        *) echo "Unknown argument: ${argument}"; exit 1 ;;
    esac
done

# ---------------------------------------------------------------------------------------------------------------------
# Environment
# ---------------------------------------------------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

MUSICHIEN_EXTERNAL_DIR="${MUSICHIEN_EXTERNAL_DIR:-$(cd "${PROJECT_DIR}/../Roger-externals" 2>/dev/null && pwd || true)}"

if [ -z "${MUSICHIEN_EXTERNAL_DIR}" ]; then
    MUSICHIEN_EXTERNAL_DIR="${PROJECT_DIR}/../Roger-externals"
    mkdir -p "${MUSICHIEN_EXTERNAL_DIR}"
    MUSICHIEN_EXTERNAL_DIR="$(cd "${MUSICHIEN_EXTERNAL_DIR}" && pwd)"
fi

echo "====================================================================================================="
echo " Musichien - dependency installation"
echo "====================================================================================================="
echo "  external dependencies : ${MUSICHIEN_EXTERNAL_DIR}"
echo "  Android support       : ${WITH_ANDROID}"
echo "  source built deps     : ${WITH_SUPERBUILD}"
echo "====================================================================================================="

# ---------------------------------------------------------------------------------------------------------------------
# 1. System packages
#
# Only the generic tooling is taken from the host. Qt, GoogleTest and the JSON library are pinned in
# the external directory, never taken from pacman.
# ---------------------------------------------------------------------------------------------------------------------
echo
echo "--- Step 1/4: system packages ------------------------------------------------------------"

SYSTEM_PACKAGES=(
    clang                # the compiler used by every preset
    clang-tools          # clang-format, clang-tidy and clangd
    cmake
    ninja
    git
    jdk21-openjdk        # required by the Android toolchain of Qt (JDK 21, not 26)
    coreutils            # provides the find and sort used to detect the pinned Qt
)

MISSING_PACKAGES=()
for package in "${SYSTEM_PACKAGES[@]}"; do
    if ! pacman -Q "${package}" >/dev/null 2>&1; then
        MISSING_PACKAGES+=("${package}")
    fi
done

if [ ${#MISSING_PACKAGES[@]} -eq 0 ]; then
    echo "  every system package is already installed"
else
    echo "  missing: ${MISSING_PACKAGES[*]}"
    echo "  installing (sudo required)..."
    sudo pacman -S --needed --noconfirm "${MISSING_PACKAGES[@]}"
fi

if [ "${WITH_ANDROID}" = "ON" ]; then
    echo "  Android host tooling..."
    sudo pacman -S --needed --noconfirm android-tools android-udev
    echo "  NOTE: add your user to the kvm group to be able to use the emulator:"
    echo "        sudo gpasswd -a ${USER} kvm"
fi

# ---------------------------------------------------------------------------------------------------------------------
# 2. aqtinstall
#
# The official Qt installer requires creating a Qt account. aqtinstall (MIT, unofficial) downloads
# the very same official Qt binaries from the Qt mirror, without any account and without any
# interactive step. It is therefore the right tool for a reproducible, scriptable setup.
# ---------------------------------------------------------------------------------------------------------------------
echo
echo "--- Step 2/4: aqtinstall ----------------------------------------------------------------"

if ! pacman -Q python-pipx >/dev/null 2>&1; then
    sudo pacman -S --needed --noconfirm python-pipx
fi

if command -v aqt >/dev/null 2>&1; then
    echo "  aqtinstall already available: $(command -v aqt)"
else
    echo "  installing aqtinstall with pipx (isolated, no sudo)..."
    pipx install aqtinstall
    pipx ensurepath
    echo "  NOTE: open a new shell (or re-source your profile) so that 'aqt' is on the PATH."
fi

AQT="$(command -v aqt || echo "${HOME}/.local/bin/aqt")"

# ---------------------------------------------------------------------------------------------------------------------
# 3. Qt
#
# The exact version is discovered rather than hard coded: asking the mirror is always right, while
# guessing a version number is always a risk of a confusing failure.
# ---------------------------------------------------------------------------------------------------------------------
echo
echo "--- Step 3/4: Qt ------------------------------------------------------------------------"

# The newest 6.x version offered for the desktop, used as the reference version.
QT_VERSION="${MUSICHIEN_QT_VERSION:-}"

if [ -z "${QT_VERSION}" ]; then
    echo "  asking the Qt mirror for the available desktop versions..."
    QT_VERSION="$("${AQT}" list-qt linux desktop 2>/dev/null | tr ' ' '\n' | sort -V | tail -n 1)"
fi

if [ -z "${QT_VERSION}" ]; then
    echo "  ERROR: could not determine a Qt version. Check the network connection, then re-run."
    exit 1
fi

echo "  selected Qt version: ${QT_VERSION}"
echo "  installing Qt for the desktop (linux_gcc_64)..."
"${AQT}" install-qt linux desktop "${QT_VERSION}" linux_gcc_64 -O "${MUSICHIEN_EXTERNAL_DIR}/Qt"

if [ "${WITH_ANDROID}" = "ON" ]; then
    echo "  installing Qt for Android (android_arm64_v8a)..."
    "${AQT}" install-qt linux android "${QT_VERSION}" android_arm64_v8a \
        --autodesktop -O "${MUSICHIEN_EXTERNAL_DIR}/Qt"
fi

echo
echo "  Qt installed into: ${MUSICHIEN_EXTERNAL_DIR}/Qt/${QT_VERSION}"

# ---------------------------------------------------------------------------------------------------------------------
# 4. Dependencies built from source
#
# GoogleTest and nlohmann/json, through the superbuild presets of the project itself.
# ---------------------------------------------------------------------------------------------------------------------
echo
echo "--- Step 4/4: source built dependencies -------------------------------------------------"

if [ "${WITH_SUPERBUILD}" = "ON" ]; then

    # setup_env.sh must not inherit 'set -u': nounset would abort it on the first undefined variable.
    set +u
    source "${SCRIPT_DIR}/setup_env.sh"
    set -u

    cmake --preset "Superbuild Musichien"
    cmake --build --preset "Build Superbuild Musichien"


    echo "  dependencies installed into ${MUSICHIEN_EXTERNAL_DIR}"

else
    echo "  skipped (add --with-superbuild to build GoogleTest and nlohmann/json)"
    echo "  NOTE: without them, the build falls back to the versions installed on the machine."
fi

echo
echo "====================================================================================================="
echo " Done. Next steps:"
echo "   source scripts/setup_env.sh"
echo "   cmake --preset \"Clang-Debug Musichien\""
echo "   cmake --build --preset \"Build Clang-Debug Musichien\""
echo "   ctest --preset \"CTest Clang-Debug Musichien\""
echo "   ./Musichien-build/Clang-Debug/bin/musichien"
echo "====================================================================================================="

