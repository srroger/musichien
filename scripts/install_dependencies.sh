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
echo "--- Step 1/5: required tools -----------------------------------------------------------"

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

    # A rolling release does not support partial upgrades, and this is not theoretical: installing one
    # package on a machine that has not been upgraded for a while can produce a binary whose library
    # dependencies are newer than the ones installed. The symptom appears at run time, in a tool that
    # has nothing to do with this project, as 'error while loading shared libraries'. 'adb' was broken
    # exactly that way here, by an android-tools built against a libprotobuf newer than the installed
    # one - hence this warning.
    PENDING_UPDATE_COUNT="$(pacman -Qu 2>/dev/null | wc -l || echo 0)"

    if [ "${PENDING_UPDATE_COUNT}" -gt 50 ]; then
        echo "  WARNING: ${PENDING_UPDATE_COUNT} packages are waiting to be upgraded on this machine."
        echo "           Installing a single package now may leave one of them unusable until the whole"
        echo "           system is upgraded. Consider running 'sudo pacman -Syu' first."
        echo
    fi

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
echo "--- Step 2/5: aqtinstall ----------------------------------------------------------------"

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
echo "--- Step 3/5: Qt ------------------------------------------------------------------------"

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
# 4. Android SDK and NDK
#
# The SDK command line tools are enough: no Android Studio, no account, no interactive step. They
# bring 'sdkmanager', which then installs everything else.
#
# Nothing here is guessed:
#
#   * the NDK revision is the one Qt itself was compiled against. It is read from Qt's own toolchain
#     file, which records the exact path used when Qt was built, rather than being assumed;
#   * the compile platform and the build tools follow the same rule: Qt's CMake takes the newest
#     platform installed as its compileSdk, and the Android Gradle Plugin 9.x requires build tools 36.
#
# Note that the SDK brings its OWN adb, in 'platform-tools'. scripts/setup_env.sh puts it first on the
# PATH on purpose: the 'android-tools' package of the distribution can be broken by a partial upgrade,
# and then every call fails with a missing libprotobuf that has nothing to do with this project.
# ---------------------------------------------------------------------------------------------------------------------
if [ "${WITH_ANDROID}" = "ON" ]; then

    echo
    echo "--- Step 4/5: Android SDK and NDK ------------------------------------------------------------"

    ANDROID_SDK_DIR="${MUSICHIEN_EXTERNAL_DIR}/android-sdk"
    ANDROID_SDK_MANAGER="${ANDROID_SDK_DIR}/cmdline-tools/latest/bin/sdkmanager"

    # The build number of the command line tools, as published by Google. Validated against the
    # download server; see docs/BUILD_AND_SETUP.md for how to find a newer one.
    ANDROID_COMMAND_LINE_TOOLS_BUILD="13114758"

    # API 36 (Android 16) and build tools 36.0.0.
    #
    # API 37 does NOT exist in the stable channel yet: Qt 6.12 would like to target it, which is why
    # cmake/musichienAndroid.cmake pins the target SDK to 36 to stay coherent with what can actually
    # be compiled. Asking sdkmanager for android-37 fails with 'Failed to find package'.
    ANDROID_PLATFORM_PACKAGE="platforms;android-36"
    ANDROID_BUILD_TOOLS_PACKAGE="build-tools;36.0.0"

    # Android NDK r27c, the revision Qt 6.12 was built with.
    #   read from <Qt>/android_arm64_v8a/lib/cmake/Qt6/qt.toolchain.cmake
    ANDROID_NDK_REVISION="27.2.12479018"

    mkdir -p "${ANDROID_SDK_DIR}"

    if [ -x "${ANDROID_SDK_MANAGER}" ]; then
        echo "  SDK command line tools already installed"
    else
        echo "  installing the SDK command line tools..."

        ANDROID_TOOLS_ARCHIVE="$(mktemp -t musichien-android-tools-XXXXXX.zip)"

        if ! curl --location --fail --silent --show-error --output "${ANDROID_TOOLS_ARCHIVE}" \
             "https://dl.google.com/android/repository/commandlinetools-linux-${ANDROID_COMMAND_LINE_TOOLS_BUILD}_latest.zip"; then
            echo "  ERROR: could not download the SDK command line tools. Check the network connection."
            rm -f "${ANDROID_TOOLS_ARCHIVE}"
            exit 1
        fi

        # The archive holds a single 'cmdline-tools' directory, and sdkmanager only works from a path
        # named 'cmdline-tools/latest': that layout is imposed by the SDK, not chosen here.
        ANDROID_TOOLS_EXTRACT_DIRECTORY="$(mktemp -d -t musichien-android-tools-XXXXXX)"

        unzip -q "${ANDROID_TOOLS_ARCHIVE}" -d "${ANDROID_TOOLS_EXTRACT_DIRECTORY}"

        mkdir -p "${ANDROID_SDK_DIR}/cmdline-tools"
        rm -rf "${ANDROID_SDK_DIR}/cmdline-tools/latest"
        mv "${ANDROID_TOOLS_EXTRACT_DIRECTORY}/cmdline-tools" "${ANDROID_SDK_DIR}/cmdline-tools/latest"

        rm -rf "${ANDROID_TOOLS_EXTRACT_DIRECTORY}" "${ANDROID_TOOLS_ARCHIVE}"

        echo "  installed into ${ANDROID_SDK_DIR}/cmdline-tools/latest"
    fi

    # sdkmanager is a Java program, and Gradle and the Android Gradle Plugin do not support the JDK 26
    # installed on this machine. JDK 21 is used here exactly as scripts/setup_env.sh does.
    export JAVA_HOME="/usr/lib/jvm/java-21-openjdk"

    if [ ! -x "${JAVA_HOME}/bin/java" ]; then
        echo "  ERROR: JDK 21 was not found in ${JAVA_HOME}."
        echo "         Install it first: sudo pacman -S jdk21-openjdk"
        exit 1
    fi

    echo "  accepting the SDK licences (required before anything can be downloaded)..."
    # 'yes' receives SIGPIPE once sdkmanager closes the pipe, which would abort the script under
    # 'set -o pipefail'. Hence the explicit '|| true'.
    yes | "${ANDROID_SDK_MANAGER}" --sdk_root="${ANDROID_SDK_DIR}" --licenses >/dev/null 2>&1 || true

    echo "  installing: platform-tools, ${ANDROID_PLATFORM_PACKAGE}, ${ANDROID_BUILD_TOOLS_PACKAGE}, ndk;${ANDROID_NDK_REVISION}"
    echo "  (the NDK alone is about 1 GB: this takes a few minutes the first time)"

    "${ANDROID_SDK_MANAGER}" --sdk_root="${ANDROID_SDK_DIR}" \
        "platform-tools" \
        "${ANDROID_PLATFORM_PACKAGE}" \
        "${ANDROID_BUILD_TOOLS_PACKAGE}" \
        "ndk;${ANDROID_NDK_REVISION}"

    echo
    echo "  Android SDK installed into: ${ANDROID_SDK_DIR}"
    echo "  NOTE: add your user to the kvm group to be able to use an emulator, then log out:"
    echo "          sudo gpasswd -a ${USER} kvm"
fi


# ---------------------------------------------------------------------------------------------------------------------
# 5. Dependencies built from source
#
# GoogleTest and nlohmann/json, through the superbuild presets of the project itself.
# ---------------------------------------------------------------------------------------------------------------------
echo
echo "--- Step 5/5: source built dependencies -------------------------------------------------"

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

