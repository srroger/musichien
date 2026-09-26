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
# 1. Required tools
#
# The check is done on the COMMANDS, not on package names.
#
# Reason: a command can be provided by several packages, and hard coding a package name that does not
# exist makes the whole script fail for a reason that has nothing to do with the project. That is
# exactly what happened with 'clang-tools', which is not a package on Arch: clang-format, clang-tidy
# and clangd all come from the 'clang' package.
#
# Format: <command>:<package that provides it>
# ---------------------------------------------------------------------------------------------------------------------
REQUIRED_TOOLS=(
    "clang:clang"                # the compiler used by every preset
    "clang-format:clang"         # formatting, driven by .clang-format
    "clang-tidy:clang"           # naming rules and static analysis
    "cmake:cmake"
    "ninja:ninja"
    "git:git"
)

echo
echo "--- Step 1/4: required tools -----------------------------------------------------------"

MISSING_PACKAGES=()

for toolEntry in "${REQUIRED_TOOLS[@]}"; do

    toolCommand="${toolEntry%%:*}"
    toolPackage="${toolEntry##*:}"

    if command -v "${toolCommand}" >/dev/null 2>&1; then
        printf '  %-14s OK\n' "${toolCommand}"
    else
        printf '  %-14s MISSING (package: %s)\n' "${toolCommand}" "${toolPackage}"

        # The same package may provide several missing commands: it is added only once.
        if [[ ! " ${MISSING_PACKAGES[*]:-} " =~ " ${toolPackage} " ]]; then
            MISSING_PACKAGES+=("${toolPackage}")
        fi
    fi
done

if [ ${#MISSING_PACKAGES[@]} -eq 0 ]; then
    echo "  every required tool is already available"
else
    echo
    echo "  Packages to install: ${MISSING_PACKAGES[*]}"
    echo "  The next command asks for your administrator password (sudo)."
    echo

    if ! sudo pacman -S --needed --noconfirm "${MISSING_PACKAGES[@]}"; then
        echo
        echo "  ERROR: the installation failed. Run it by hand to see the details:"
        echo "         sudo pacman -S --needed ${MISSING_PACKAGES[*]}"
        exit 1
    fi
fi


if [ "${WITH_ANDROID}" = "ON" ]; then

    echo
    echo "  Android host tooling (only needed to build for Android)..."

    # jdk21-openjdk belongs here and not in the general list: a desktop only build needs no JDK at all.
    ANDROID_PACKAGES=(
        android-tools        # adb, fastboot
        android-udev         # so that adb sees the phone without sudo
        jdk21-openjdk        # required by the Android toolchain of Qt: JDK 21, never the JDK 26 of the system
    )

    echo "  Packages to install: ${ANDROID_PACKAGES[*]}"
    echo "  The next command asks for your administrator password (sudo)."
    echo

    if ! sudo pacman -S --needed --noconfirm "${ANDROID_PACKAGES[@]}"; then
        echo
        echo "  ERROR: the installation failed. Run it by hand to see the details:"
        echo "         sudo pacman -S --needed ${ANDROID_PACKAGES[*]}"
        exit 1
    fi

    echo
    echo "  NOTE: JDK 21 is required by the Android toolchain of Qt. The JDK 26 of the system is too"
    echo "        recent for Gradle and AGP, and produces obscure build failures."
    echo "  NOTE: add your user to the kvm group to be able to use the emulator, then log out:"
    echo "          sudo gpasswd -a ${USER} kvm"
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

# aqt writes an "aqtinstall.log" file in the current directory. Running it from a temporary directory
# keeps the project tree clean.
run_aqt()
{
    ( cd /tmp && "${AQT}" "$@" )
}


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

# Qt 6.11 or newer is a hard requirement: earlier versions forward to the draft name
# std::saturate_cast, which the final C++26 standard renamed to std::saturating_cast.
# See docs/BUILD_AND_SETUP.md, section "Why Qt 6.11 is required".
if [ "$(printf '%s\n' "6.11" "${QT_VERSION}" | sort -V | head -n 1)" != "6.11" ]; then
    echo "  ERROR: Qt ${QT_VERSION} is too old: Qt 6.11 or newer is required to build in C++26."
    echo "         Set MUSICHIEN_QT_VERSION to a newer version and re-run."
    exit 1
fi


# Qt modules the project needs, ON TOP OF the base installation.
#
# aqt only installs qtbase and qtdeclarative by default. Everything else is a separate module, and a
# missing one makes find_package(Qt6 COMPONENTS ...) fail with a message that does not say what to do.
#
#   qtmultimedia  -> QAudioSink / QAudioSource: the whole audio output of the application
#   qtshadertools -> required to compile QML shader effects at run time
#   qt5compat     -> Qt5Compat.GraphicalEffects: blur, glow, drop shadow - the "juice" of the game
QT_MODULES=(
    qtmultimedia
    qtshadertools
    qt5compat
)

echo "  selected Qt version: ${QT_VERSION}"
echo "  modules: ${QT_MODULES[*]}"
echo "  installing Qt for the desktop (linux_gcc_64)..."
run_aqt install-qt linux desktop "${QT_VERSION}" linux_gcc_64 \
    -m "${QT_MODULES[@]}" \
    -O "${MUSICHIEN_EXTERNAL_DIR}/Qt"

if [ "${WITH_ANDROID}" = "ON" ]; then

    # IMPORTANT: since Qt 6.8, the Android packages are NO LONGER in the "linux_x64/android"
    # repository. That one stopped at Qt 6.7.3, in September 2024. They are published in the cross
    # platform repository "all_os/android", which is why the host is "all_os" here and not "linux".
    #
    # Symptom of getting this wrong: 'aqt list-qt linux android' shows nothing newer than 6.7.3, and
    # one could wrongly conclude that Qt dropped Android support. It did not.
    #
    # --autodesktop is not needed: the desktop Qt was installed just above, and that is all aqt
    # requires. It is used for the host tools, androiddeployqt among them.
    echo "  installing Qt for Android (android_arm64_v8a) with the same modules..."
    run_aqt install-qt all_os android "${QT_VERSION}" android_arm64_v8a \
        -m "${QT_MODULES[@]}" \
        -O "${MUSICHIEN_EXTERNAL_DIR}/Qt"
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

