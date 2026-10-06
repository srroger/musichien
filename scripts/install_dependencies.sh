#!/bin/bash
# =====================================================================================================================
# Musichien - installs every external dependency, pinned by version.
#
#   Usage:  scripts/install_dependencies.sh [--with-android] [--with-ios] [--without-qt] [--with-superbuild]
#
# Runs on Linux (Arch / Manjaro) and on macOS. Which package manager is used, which Qt binary is
# downloaded and where the pinned JDK lives all follow from the host, detected by scripts/platform.sh.
#
# What this script installs:
#   1. the system packages needed to build a Qt application (pacman on Linux, Homebrew on macOS)
#   2. aqtinstall, the command line Qt installer that does NOT require a Qt account
#   3. the pinned Qt itself: the desktop Qt always; Android with --with-android; iOS with --with-ios
#   4. the dependencies built from source (GoogleTest, nlohmann/json), with --with-superbuild
#
# --without-qt skips steps 2 and 3 entirely. It is for the developer who already has a Qt
# installation (from the official installer, for example) and copied it into MUSICHIEN_EXTERNAL_DIR
# by hand: scripts/setup_env.sh detects that Qt exactly like one downloaded by aqt.
#
# Every dependency lands in MUSICHIEN_EXTERNAL_DIR, and NOT in the system directories: the machine
# can be upgraded freely without ever breaking the project.
# =====================================================================================================================

set -euo pipefail

# ---------------------------------------------------------------------------------------------------------------------
# Arguments
# ---------------------------------------------------------------------------------------------------------------------
WITH_ANDROID=OFF
WITH_IOS=OFF
WITH_QT=ON
WITH_SUPERBUILD=OFF

for argument in "$@"; do
    case "${argument}" in
        --with-android)    WITH_ANDROID=ON ;;
        --with-ios)        WITH_IOS=ON ;;
        --without-qt)      WITH_QT=OFF ;;
        --with-superbuild) WITH_SUPERBUILD=ON ;;
        *) echo "Unknown argument: ${argument}"; exit 1 ;;
    esac
done

# ---------------------------------------------------------------------------------------------------------------------
# Environment
# ---------------------------------------------------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Host helpers: OS detection, package installation, and the version-aware directory pickers. Nothing
# here assumes a GNU tool, so the very same script runs on macOS.
# shellcheck source=platform.sh
source "${SCRIPT_DIR}/platform.sh"

# On macOS the Xcode command line tools bring 'clang' and 'clang++', but clang-format and clang-tidy
# come from Homebrew's 'llvm', which is keg-only: it is NOT on the PATH by default. Prepending its bin
# directory here is what makes the tool check below find them on a second run, and what makes CMake
# find the modern clang instead of Apple's older one.
if [ "${MUSICHIEN_HOST_OS}" = "macos" ]; then
    MUSICHIEN_BREW_LLVM_BIN="$(musichien_brew_prefix llvm)/bin"
    if [ -x "${MUSICHIEN_BREW_LLVM_BIN}/clang-format" ]; then
        export PATH="${MUSICHIEN_BREW_LLVM_BIN}:${PATH}"
    fi
fi

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
echo "  host                  : ${MUSICHIEN_HOST_OS}"
echo "  Android support       : ${WITH_ANDROID}"
echo "  iOS support           : ${WITH_IOS}"
echo "  Qt (via aqtinstall)   : ${WITH_QT}"
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
# The mapping depends on the host:
#
#   Linux (Arch)  clang, clang-format, clang-tidy and clangd all come from the 'clang' package;
#   macOS         'clang' and 'clang++' come from the Xcode command line tools (already present by the
#                 time this runs), clang-format and clang-tidy from Homebrew's 'llvm', the rest too.
#
# Format: <command>:<package that provides it>
# ---------------------------------------------------------------------------------------------------------------------
if [ "${MUSICHIEN_HOST_OS}" = "macos" ]; then
    REQUIRED_TOOLS=(
        "clang:llvm"                # the compiler used by every preset (Apple's clang stays a fallback)
        "clang-format:llvm"         # formatting, driven by .clang-format
        "clang-tidy:llvm"           # naming rules and static analysis
        "cmake:cmake"
        "ninja:ninja"
        "git:git"
    )
else
    REQUIRED_TOOLS=(
        "clang:clang"                # the compiler used by every preset
        "clang-format:clang"         # formatting, driven by .clang-format
        "clang-tidy:clang"           # naming rules and static analysis
        "cmake:cmake"
        "ninja:ninja"
        "git:git"
    )
fi

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

    if [ "${MUSICHIEN_HOST_OS}" = "macos" ]; then
        echo "  Homebrew installs them; no administrator password is needed."
        echo
    else
        echo "  The next command asks for your administrator password (sudo)."
        echo

        # A rolling release does not support partial upgrades, and this is not theoretical: installing
        # one package on a machine that has not been upgraded for a while can produce a binary whose
        # library dependencies are newer than the ones installed. The symptom appears at run time, in a
        # tool that has nothing to do with this project, as 'error while loading shared libraries'.
        # 'adb' was broken exactly that way here, by an android-tools built against a libprotobuf newer
        # than the installed one - hence this warning.
        PENDING_UPDATE_COUNT="$(pacman -Qu 2>/dev/null | wc -l || echo 0)"

        if [ "${PENDING_UPDATE_COUNT}" -gt 50 ]; then
            echo "  WARNING: ${PENDING_UPDATE_COUNT} packages are waiting to be upgraded on this machine."
            echo "           Installing a single package now may leave one of them unusable until the whole"
            echo "           system is upgraded. Consider running 'sudo pacman -Syu' first."
            echo
        fi
    fi

    if ! musichien_install_packages "${MISSING_PACKAGES[@]}"; then
        echo
        echo "  ERROR: the installation failed. Install by hand to see the details:"
        if [ "${MUSICHIEN_HOST_OS}" = "macos" ]; then
            echo "         brew install ${MISSING_PACKAGES[*]}"
        else
            echo "         sudo pacman -S --needed ${MISSING_PACKAGES[*]}"
        fi
        exit 1
    fi
fi


if [ "${WITH_ANDROID}" = "ON" ]; then

    echo
    echo "  Android host tooling (only needed to build for Android)..."

    # The JDK belongs here and not in the general list: a desktop-only build needs no JDK at all. JDK
    # 21 is what the Android toolchain of Qt expects; a newer one is too recent for Gradle and AGP.
    if [ "${MUSICHIEN_HOST_OS}" = "macos" ]; then
        ANDROID_PACKAGES=(
            android-platform-tools   # adb, fastboot (the SDK also ships its own adb)
            openjdk@21               # required by the Android toolchain of Qt: JDK 21
        )
    else
        ANDROID_PACKAGES=(
            android-tools        # adb, fastboot
            android-udev         # so that adb sees the phone without sudo
            jdk21-openjdk        # required by the Android toolchain of Qt: JDK 21, never the JDK 26 of the system
        )
    fi

    echo "  Packages to install: ${ANDROID_PACKAGES[*]}"

    if ! musichien_install_packages "${ANDROID_PACKAGES[@]}"; then
        echo
        echo "  ERROR: the installation failed."
        exit 1
    fi

    echo
    echo "  NOTE: JDK 21 is required by the Android toolchain of Qt. A newer JDK is too"
    echo "        recent for Gradle and AGP, and produces obscure build failures."

    if [ "${MUSICHIEN_HOST_OS}" = "macos" ]; then
        echo "  NOTE: Homebrew's openjdk is keg-only, so it is not on the PATH by default; the"
        echo "        environment script points JAVA_HOME at it directly, which is what Gradle reads."
    else
        echo "  NOTE: add your user to the kvm group to be able to use the emulator, then log out:"
        echo "          sudo gpasswd -a ${USER} kvm"
    fi
fi


# ---------------------------------------------------------------------------------------------------------------------
# 2. aqtinstall
#
# The official Qt installer requires creating a Qt account. aqtinstall (MIT, unofficial) downloads
# the very same official Qt binaries from the Qt mirror, without any account and without any
# interactive step. It is therefore the right tool for a reproducible, scriptable setup.
# ---------------------------------------------------------------------------------------------------------------------
# '--without-qt' skips everything below: an existing Qt installation, copied by hand into
# MUSICHIEN_EXTERNAL_DIR, is detected by scripts/setup_env.sh exactly like one installed by aqt.
if [ "${WITH_QT}" = "ON" ]; then

echo
echo "--- Step 2/5: aqtinstall ----------------------------------------------------------------"

if [ "${MUSICHIEN_HOST_OS}" = "macos" ]; then
    command -v pipx >/dev/null 2>&1 || brew install pipx
else
    pacman -Q python-pipx >/dev/null 2>&1 || musichien_install_packages python-pipx
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

    if [ "${MUSICHIEN_HOST_OS}" = "macos" ]; then
        QT_LIST_HOST="mac"
    else
        QT_LIST_HOST="linux"
    fi

    AVAILABLE_QT_VERSIONS="$("${AQT}" list-qt "${QT_LIST_HOST}" desktop 2>/dev/null | tr ' ' '\n')"
    QT_VERSION="$(musichien_greatest_version ${AVAILABLE_QT_VERSIONS})"
fi

if [ -z "${QT_VERSION}" ]; then
    echo "  ERROR: could not determine a Qt version. Check the network connection, then re-run."
    exit 1
fi

# Qt 6.11 or newer is a hard requirement: earlier versions forward to the draft name
# std::saturate_cast, which the final C++26 standard renamed to std::saturating_cast.
# See docs/BUILD_AND_SETUP.md, section "Why Qt 6.11 is required".
#
# musichien_greatest_version returns the greater of the two: if it is not our version, ours is older.
if [ "$(musichien_greatest_version "6.11" "${QT_VERSION}")" != "${QT_VERSION}" ]; then
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

# The desktop Qt: the reference version, and the HOST Qt that the cross compilation of Android and
# iOS both need for their host tools (androiddeployqt, moc, and so on).
if [ "${MUSICHIEN_HOST_OS}" = "macos" ]; then
    QT_DESKTOP_HOST="mac"
    QT_DESKTOP_ARCH="clang_64"
else
    QT_DESKTOP_HOST="linux"
    QT_DESKTOP_ARCH="linux_gcc_64"
fi

echo "  selected Qt version: ${QT_VERSION}"
echo "  modules: ${QT_MODULES[*]}"
echo "  installing Qt for the desktop (${QT_DESKTOP_ARCH})..."
run_aqt install-qt "${QT_DESKTOP_HOST}" desktop "${QT_VERSION}" "${QT_DESKTOP_ARCH}" \
    -m "${QT_MODULES[@]}" \
    -O "${MUSICHIEN_EXTERNAL_DIR}/Qt"

if [ "${WITH_ANDROID}" = "ON" ]; then

    # IMPORTANT: since Qt 6.8, the Android packages are NO LONGER in the "<host>_x64/android"
    # repository. That one stopped at Qt 6.7.3, in September 2024. They are published in the cross
    # platform repository "all_os/android", which is why the host is "all_os" here and not the host
    # operating system.
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

if [ "${WITH_IOS}" = "ON" ]; then

    if [ "${MUSICHIEN_HOST_OS}" != "macos" ]; then
        echo
        echo "  ERROR: iOS can only be built on macOS: Qt for iOS is a macOS-only package."
        exit 1
    fi

    # Qt for iOS is published for the mac host, with the architecture literally named 'ios'. The
    # desktop Qt installed just above is the HOST Qt that Qt's tools need while cross compiling - it
    # is found at build time through QT_HOST_PATH, which the iOS preset sets.
    echo "  installing Qt for iOS (ios) with the same modules..."
    run_aqt install-qt mac ios "${QT_VERSION}" ios \
        -m "${QT_MODULES[@]}" \
        -O "${MUSICHIEN_EXTERNAL_DIR}/Qt"
fi

echo
echo "  Qt installed into: ${MUSICHIEN_EXTERNAL_DIR}/Qt/${QT_VERSION}"

else
    echo
    echo "--- Steps 2/5 and 3/5 skipped (--without-qt) -----------------------------------------"
    echo "  Using the Qt already present in '${MUSICHIEN_EXTERNAL_DIR}'."
    echo "  scripts/setup_env.sh must find a Qt6Config.cmake there: see its detection rules."
fi

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

    # Google publishes the same command line tools revision for every host; only the platform tag in
    # the file name changes: 'linux' on Linux, 'mac' on macOS.
    if [ "${MUSICHIEN_HOST_OS}" = "macos" ]; then
        ANDROID_CMDLINE_TOOLS_OS_TAG="mac"
    else
        ANDROID_CMDLINE_TOOLS_OS_TAG="linux"
    fi

    mkdir -p "${ANDROID_SDK_DIR}"

    if [ -x "${ANDROID_SDK_MANAGER}" ]; then
        echo "  SDK command line tools already installed"
    else
        echo "  installing the SDK command line tools..."

        ANDROID_TOOLS_ARCHIVE="$(mktemp -t musichien-android-tools-XXXXXX.zip)"

        if ! curl --location --fail --silent --show-error --output "${ANDROID_TOOLS_ARCHIVE}" \
             "https://dl.google.com/android/repository/commandlinetools-${ANDROID_CMDLINE_TOOLS_OS_TAG}-${ANDROID_COMMAND_LINE_TOOLS_BUILD}_latest.zip"; then
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
    # installed by default on the reference machine. JDK 21 is used here exactly as scripts/setup_env.sh
    # does; its location is host dependent, hence musichien_jdk_home.
    JAVA_HOME="$(musichien_jdk_home)"
    export JAVA_HOME

    if [ ! -x "${JAVA_HOME}/bin/java" ]; then
        echo "  ERROR: JDK 21 was not found (looked in '${JAVA_HOME}')."
        if [ "${MUSICHIEN_HOST_OS}" = "macos" ]; then
            echo "         Install it first: brew install openjdk@21"
        else
            echo "         Install it first: sudo pacman -S jdk21-openjdk"
        fi
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

    if [ "${MUSICHIEN_HOST_OS}" = "linux" ]; then
        echo "  NOTE: add your user to the kvm group to be able to use an emulator, then log out:"
        echo "          sudo gpasswd -a ${USER} kvm"
    fi
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

if [ "${MUSICHIEN_HOST_OS}" = "macos" ]; then
echo "   cmake --preset \"macOS-Release Musichien\"          # native macOS build (Xcode or Ninja)"
fi

if [ "${WITH_IOS}" = "ON" ]; then
echo "   scripts/build_ios.sh                               # build, archive and sign the iPhone package"
fi

echo "====================================================================================================="

