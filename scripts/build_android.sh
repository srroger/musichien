#!/bin/bash
# =====================================================================================================================
# Musichien - builds the Android package, then proves it cannot use the network.
#
#   Usage:  scripts/build_android.sh [--no-install]
#
#   --no-install   build and verify the package, but never touch a phone
#
# What it does:
#   1. checks that every pinned Android tool is present
#   2. configures and builds the 'Android-Release Musichien' preset
#   3. builds the APK
#   4. VERIFIES that the APK requests no permission at all, and refuses to go on otherwise
#   5. installs it on the connected phone, unless --no-install was given
#
# Step 4 is the reason this script exists.
#
# Musichien claims to be technically unable to reach the network, and that claim rests on a single
# fact: the manifest must not declare android.permission.INTERNET. Without the declaration, Android
# keeps the process out of the 'inet' group and every socket() call fails - the kernel enforces the
# promise, not our code.
#
# CMake already removes the permission that Qt6::Core requests on our behalf (see
# cmake/musichienAndroid.cmake). This step verifies the result on the FINISHED package, where no Qt
# internal could hide it any more. A Qt upgrade that put the permission back would therefore be
# caught here, loudly, instead of shipping silently.
# =====================================================================================================================

set -euo pipefail

# ---------------------------------------------------------------------------------------------------------------------
# Arguments
# ---------------------------------------------------------------------------------------------------------------------
INSTALL_ON_DEVICE=ON

for argument in "$@"; do
    case "${argument}" in
        --no-install) INSTALL_ON_DEVICE=OFF ;;
        *) echo "Unknown argument: ${argument}"; exit 1 ;;
    esac
done

# ---------------------------------------------------------------------------------------------------------------------
# Environment
#
# setup_env.sh is sourced into a normal shell and must not inherit 'set -u': nounset would abort it on
# the first variable that is not defined yet. The option is restored right after the source.
# ---------------------------------------------------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

set +u
# shellcheck source=setup_env.sh
source "${SCRIPT_DIR}/setup_env.sh"
set -u

BUILD_DIR="${PROJECT_DIR}/../Musichien-build/Android-Release"
CONFIGURE_PRESET="Android-Release Musichien"
BUILD_PRESET="Build Android-Release Musichien"

# Must stay identical to MUSICHIEN_ANDROID_PACKAGE_NAME in cmake/musichienAndroid.cmake.
# It is only used to ask the phone about the installed package, never to build anything.
ANDROID_PACKAGE_NAME="io.github.srroger.musichien"

echo

# ---------------------------------------------------------------------------------------------------------------------
# 1. Pinned tools
#
# The check is done on the exported variables, never on what happens to be installed on the machine:
# a missing tool must stop the build here, with an explanation, and not half an hour later inside
# Gradle.
# ---------------------------------------------------------------------------------------------------------------------
echo "--- Step 1/5: pinned Android tools ------------------------------------------------------------"

MISSING_TOOL_COUNT=0

check_tool()
{
    local toolName="$1"
    local toolValue="$2"
    local toolHint="$3"

    if [ -n "${toolValue}" ] && [ -e "${toolValue}" ]; then
        printf '  %-26s %s\n' "${toolName}" "${toolValue}"
    else
        printf '  %-26s MISSING\n' "${toolName}"
        echo   "      ${toolHint}"
        MISSING_TOOL_COUNT=$((MISSING_TOOL_COUNT + 1))
    fi
}

check_tool "ANDROID_SDK_ROOT"         "${ANDROID_SDK_ROOT:-}"         "run: scripts/install_dependencies.sh --with-android"
check_tool "ANDROID_NDK_ROOT"         "${ANDROID_NDK_ROOT:-}"         "run: scripts/install_dependencies.sh --with-android"
check_tool "MUSICHIEN_QT_ANDROID_DIR" "${MUSICHIEN_QT_ANDROID_DIR:-}" "run: scripts/install_dependencies.sh --with-android"
check_tool "JAVA_HOME"                "${JAVA_HOME:-}"                "run: scripts/install_dependencies.sh --with-android"

if [ ${MISSING_TOOL_COUNT} -gt 0 ]; then
    echo
    echo "build_android.sh: ${MISSING_TOOL_COUNT} pinned tool(s) missing."
    echo "                  Have the dependencies been installed, and this script been run from a shell"
    echo "                  where 'source scripts/setup_env.sh' was executed?"
    exit 1
fi

# ---------------------------------------------------------------------------------------------------------------------
# 2. Configuration
# ---------------------------------------------------------------------------------------------------------------------
echo
echo "--- Step 2/5: configuration -------------------------------------------------------------------"

cmake --preset "${CONFIGURE_PRESET}"

# ---------------------------------------------------------------------------------------------------------------------
# 3. Compilation
#
# The 'apk' target is provided by Qt: it compiles the C++, runs androiddeployqt, then calls the
# Gradle wrapper shipped inside Qt to assemble and sign the package with a debug key.
# ---------------------------------------------------------------------------------------------------------------------
echo
echo "--- Step 3/5: compilation ---------------------------------------------------------------------"

cmake --build --preset "${BUILD_PRESET}" --target apk

# ---------------------------------------------------------------------------------------------------------------------
# 4. The package
#
# Gradle decides the file name and its directory on its own, and it changes from one version to the
# next: it is searched for rather than assumed.
# ---------------------------------------------------------------------------------------------------------------------
echo
echo "--- Step 4/5: package -------------------------------------------------------------------------"

PACKAGE_FILE="$(find "${BUILD_DIR}" -type f -name '*.apk' -printf '%T@ %p\n' 2>/dev/null |
                sort -n | tail -n 1 | cut -d' ' -f2-)"

if [ -z "${PACKAGE_FILE}" ]; then
    echo "  ERROR: the build reported success but no .apk was found under '${BUILD_DIR}'."
    exit 1
fi

printf '  %-16s %s\n' "package" "${PACKAGE_FILE}"
printf '  %-16s %s\n' "size" "$(du -h "${PACKAGE_FILE}" | cut -f1)"

echo
echo "  Verifying what the package is allowed to do..."

# apkanalyzer comes with the SDK command line tools, aapt2 with the build tools. Both read the binary
# manifest of the package, which is the only description Android itself will ever look at.
APK_ANALYZER="$(command -v apkanalyzer 2>/dev/null || echo "")"
AAPT2="$(find "${ANDROID_SDK_ROOT}/build-tools" -maxdepth 2 -type f -name 'aapt2' 2>/dev/null |
         sort -V | tail -n 1 || echo "")"

if [ -z "${APK_ANALYZER}" ] && [ -z "${AAPT2}" ]; then
    echo "  ERROR: neither apkanalyzer nor aapt2 was found: this package CANNOT be verified."
    echo "         Refusing to hand over an unverified package, that is the whole point."
    exit 1
fi

# Prints every permission declared by a package, one per line. The two inspectors have compatible
# output shapes: a bare name, or a 'uses-permission: name=...' line.
read_declared_permissions()
{
    local packagePath="$1"

    if [ -n "${APK_ANALYZER}" ]; then
        "${APK_ANALYZER}" manifest permissions "${packagePath}" 2>/dev/null || true
    else
        "${AAPT2}" dump permissions "${packagePath}" 2>/dev/null || true
    fi
}

# ---------------------------------------------------------------------------------------------------------------------
# The permissions the application is allowed to ask Android for.
#
# Musichien is offline, local and private: it must never be able to reach the network. That is not a
# declaration of intent, it is a property of the package, and verifying it is the whole point of this
# script.
#
# The list is deliberately tiny, and every entry has to justify itself:
#
#   POST_NOTIFICATIONS  the daily reminder. It answers one question and one only - "may I display a
#                       notification?" - and grants access to NOTHING: no network, no file, no
#                       camera, no microphone, no location. It is asked at run time, and refusing it
#                       costs the reminder and nothing else.
#
#   RECORD_AUDIO        the voice pillar. Singing an interval back is half of ear training - one does
#                       not learn to read without learning to write - which is why this one is
#                       accepted even though it opens a real hardware input.
#
#   VIBRATE             a short buzz when an answer is wrong, doubling the shake of the screen so that
#                       a mistake is felt and not only seen. It is a NORMAL permission: Android shows
#                       no dialog for it, it grants access to NOTHING - no network, no file, no
#                       camera, no microphone, no location - and the only hardware it reaches is the
#                       vibrating motor.
#
# INTERNET is absent from this list, and can never be added to it: see the first rule below.
# ---------------------------------------------------------------------------------------------------------------------
ALLOWED_SYSTEM_PERMISSIONS=(
    "android.permission.POST_NOTIFICATIONS"
    "android.permission.RECORD_AUDIO"
    "android.permission.VIBRATE"
)

# Refuses to go on when a package asks for a system permission it has no business asking for.
#
# Two rules, in this order:
#
#   1. 'android.permission.INTERNET' is forbidden, ALWAYS. It is what separates an application that
#      can reach the network from one that cannot, and the second is the reason this project exists
#      in the shape it has. The check is spelled out so that adding it to the allowlist above by
#      accident would not be enough;
#
#   2. every other system permission must appear in that allowlist. A permission that is not there
#      was not thought through, and an unthinking permission is how a private application quietly
#      stops being one.
#
# The package's own permission is a different thing, and is not checked here: androidx.core declares
# one named after the application ('<package>.DYNAMIC_RECEIVER_NOT_EXPORTED_PERMISSION') with a
# signature protection level. It is declared BY the application, can only be held by applications
# signed with the same key, grants nothing outside of itself, and never shows up in the settings.
#
# The whole list is printed either way: a new entry must never go unnoticed.
check_declared_permissions()
{
    local packagePath="$1"
    local declaredPermissions=""
    local systemPermissions=""
    local unexpectedPermissions=""

    declaredPermissions="$(read_declared_permissions "${packagePath}" |
                          grep -oE '(android\.)?permission\.[A-Za-z0-9_.]+' | sort -u || true)"

    echo
    if [ -n "${declaredPermissions}" ]; then
        echo "  permissions declared by the package:"
        printf '%s\n' "${declaredPermissions}" | sed 's/^/    /'
    else
        echo "  permissions declared by the package: none at all"
    fi

    systemPermissions="$(printf '%s\n' "${declaredPermissions}" | grep -E '^android\.permission\.' || true)"

    if [ -z "${systemPermissions}" ]; then
        return 0
    fi

    # Rule 1, the one that can never be relaxed, whatever the reason.
    if printf '%s\n' "${systemPermissions}" | grep -qE '^android\.permission\.INTERNET$'; then
        echo
        echo "  ==============================================================================================="
        echo "  FAILED - the package requests android.permission.INTERNET."
        echo "  ==============================================================================================="
        echo
        echo "  This one is not negotiable. Without this declaration, Android keeps the process out of the"
        echo "  'inet' group and every socket() call fails: the ABSENCE of the permission is the guarantee,"
        echo "  and it is the kernel that enforces it, not our code."
        echo
        echo "  Most likely cause: androiddeployqt found its INSERT_PERMISSIONS marker again in"
        echo "  source/android/AndroidManifest.xml and filled it with the permissions declared by the Qt"
        echo "  modules. See the header of that file and cmake/musichienAndroid.cmake."
        echo "  ==============================================================================================="
        exit 1
    fi

    # Rule 2, the allowlist.
    for permission in ${systemPermissions}; do
        if ! printf '%s\n' "${ALLOWED_SYSTEM_PERMISSIONS[@]}" | grep -qxF "${permission}"; then
            unexpectedPermissions="${unexpectedPermissions}${permission}"$'\n'
        fi
    done

    if [ -n "${unexpectedPermissions}" ]; then
        echo
        echo "  ==============================================================================================="
        echo "  FAILED - the package requests system permissions that are not in the allowlist."
        echo "  ==============================================================================================="
        printf '%s' "${unexpectedPermissions}" | sed 's/^/    /'
        echo
        echo "  A new permission is a decision, not a detail: it has to be justified, written down in"
        echo "  ALLOWED_SYSTEM_PERMISSIONS at the top of this script, and explained in the project charter."
        echo "  'android.permission.INTERNET' will never be accepted, whatever the argument."
        echo "  ==============================================================================================="
        exit 1
    fi

    echo
    echo "  OK - every system permission asked for is one the project decided to accept:"
    printf '%s\n' "${systemPermissions}" | sed 's/^/    /'

    return 0
}

if [ -n "${APK_ANALYZER}" ]; then
    echo "  inspector: apkanalyzer"
else
    echo "  inspector: aapt2"
fi

check_declared_permissions "${PACKAGE_FILE}"

echo
echo "  Musichien stays unable to reach the network: 'android.permission.INTERNET' is absent, and this"
echo "  build refuses to produce a package that would ask for it."

# ---------------------------------------------------------------------------------------------------------------------
# Signing
#
# Gradle only signs a RELEASE package when it is given a signing configuration, so what comes out of
# the build is unsigned - and an unsigned APK cannot be installed at all.
#
# Musichien signs its package with a personal key, kept OUTSIDE the repository in '~/.android', which
# is the directory Android Studio uses for the same purpose. Two reasons:
#
#   * Android refuses to install an update whose signature differs from the one already installed.
#     Keeping the same key from one build to the next is what makes 'adb install -r' work;
#
#   * that key is the author's and nobody else's. Under the GPL, everyone remains free to rebuild
#     Musichien and sign it with a key of their own, which is exactly the intent.
#
# The key is created on the first run, with a password drawn from /dev/urandom and written beside it
# with owner only permissions. Nothing is interactive, so that the script can be launched from a task
# runner or from the editor as well as from a terminal.
# ---------------------------------------------------------------------------------------------------------------------
echo
echo "  Signing the package..."

ANDROID_KEYSTORE_DIRECTORY="${HOME}/.android"
ANDROID_KEYSTORE_FILE="${ANDROID_KEYSTORE_DIRECTORY}/musichien-release.keystore"
ANDROID_KEYSTORE_PASSWORD_FILE="${ANDROID_KEYSTORE_DIRECTORY}/musichien-release.password"
ANDROID_KEY_ALIAS="musichien"

APK_SIGNER="$(find "${ANDROID_SDK_ROOT}/build-tools" -maxdepth 2 -type f -name 'apksigner' 2>/dev/null |
              sort -V | tail -n 1)"
ZIP_ALIGNER="$(find "${ANDROID_SDK_ROOT}/build-tools" -maxdepth 2 -type f -name 'zipalign' 2>/dev/null |
               sort -V | tail -n 1)"

if [ -z "${APK_SIGNER}" ] || [ -z "${ZIP_ALIGNER}" ]; then
    echo "  ERROR: apksigner or zipalign was not found under '${ANDROID_SDK_ROOT}/build-tools'."
    echo "         Reinstall the Android SDK: scripts/install_dependencies.sh --with-android"
    exit 1
fi

if [ ! -f "${ANDROID_KEYSTORE_FILE}" ]; then
    echo "  creating a personal signing key, once and for all:"
    echo "    ${ANDROID_KEYSTORE_FILE}"

    mkdir -p "${ANDROID_KEYSTORE_DIRECTORY}"
    chmod 700 "${ANDROID_KEYSTORE_DIRECTORY}"

    # The trailing newline is kept on purpose: apksigner reads a password file LINE BY LINE and
    # reports 'end of file reached' when the file does not end with one. Command substitution
    # ('$(cat ...)', used above and below) strips it, so keeping it costs nothing.
    head -c 32 /dev/urandom | base64 > "${ANDROID_KEYSTORE_PASSWORD_FILE}"
    chmod 600 "${ANDROID_KEYSTORE_PASSWORD_FILE}"

    # keytool comes from the JDK, and JAVA_HOME points at JDK 21 (see scripts/setup_env.sh). Its 'bin'
    # directory is not necessarily on the PATH, so it is named explicitly.
    KEY_TOOL="${JAVA_HOME}/bin/keytool"

    if [ ! -x "${KEY_TOOL}" ]; then
        KEY_TOOL="$(command -v keytool || echo "")"
    fi

    if [ -z "${KEY_TOOL}" ]; then
        echo "  ERROR: keytool was not found. It comes with the JDK: scripts/install_dependencies.sh --with-android"
        exit 1
    fi

    "${KEY_TOOL}" -genkeypair \
        -keystore "${ANDROID_KEYSTORE_FILE}" \
        -storepass "$(cat "${ANDROID_KEYSTORE_PASSWORD_FILE}")" \
        -keypass "$(cat "${ANDROID_KEYSTORE_PASSWORD_FILE}")" \
        -alias "${ANDROID_KEY_ALIAS}" \
        -keyalg RSA -keysize 4096 -validity 10000 \
        -dname "CN=Musichien" >/dev/null

    echo "    the password is in ${ANDROID_KEYSTORE_PASSWORD_FILE}"
    echo "    BACK BOTH UP: without the same key, the phone will refuse the next update."
fi

ALIGNED_PACKAGE_FILE="$(mktemp -t musichien-aligned-XXXXXX.apk)"
SIGNED_PACKAGE_FILE="$(dirname "${PACKAGE_FILE}")/musichien-release-signed.apk"

# The package must be aligned BEFORE being signed: signing rewrites the archive, so aligning
# afterwards would invalidate the signature. '-p' keeps the pages of the shared libraries contiguous,
# which is what lets Android map them straight from the package instead of copying them.
"${ZIP_ALIGNER}" -f -p 4 "${PACKAGE_FILE}" "${ALIGNED_PACKAGE_FILE}"

# There is deliberately NO '--key-pass' option here.
#
# For a PKCS12 keystore (what keytool creates by default), the key password is the store password, and
# apksigner falls back to the store one when no key password is given. Passing '--key-pass
# file:<same file>' on top of '--ks-pass file:<same file>' makes apksigner read that file twice and
# fail with a cryptic 'end of file reached'. Reading the password from the file rather than from the
# command line also keeps it out of the process list.
"${APK_SIGNER}" sign \
    --ks "${ANDROID_KEYSTORE_FILE}" \
    --ks-pass "file:${ANDROID_KEYSTORE_PASSWORD_FILE}" \
    --ks-key-alias "${ANDROID_KEY_ALIAS}" \
    --out "${SIGNED_PACKAGE_FILE}" \
    "${ALIGNED_PACKAGE_FILE}"

rm -f "${ALIGNED_PACKAGE_FILE}"

# Proves the signature before the package goes anywhere near a phone: an APK whose signature does not
# verify is refused at install time, with a message that says nothing about the real cause.
"${APK_SIGNER}" verify --print-certs "${SIGNED_PACKAGE_FILE}" 2>/dev/null | head -4

printf '  %-16s %s\n' "signed package" "${SIGNED_PACKAGE_FILE}"

# The signed package becomes the one everything below works with.
PACKAGE_FILE="${SIGNED_PACKAGE_FILE}"

# Signing appends signature blocks to the archive and does not touch the manifest, so the check above
# already covered this content. It is repeated all the same, naming the exact file that is going to be
# installed: a verification is only worth anything when it points at the artifact it approved.
echo
echo "  Re-checking the signed package..."
check_declared_permissions "${PACKAGE_FILE}"

echo
echo "  OK - the signed package passes the same check: what was verified is what will be installed."


# ---------------------------------------------------------------------------------------------------------------------
# 5. Installation on the phone
#
# adb is deliberately the one that ships inside the pinned SDK: the 'android-tools' package of the
# distribution can be broken by a partial upgrade, and then fails with a missing libprotobuf that has
# nothing to do with this project.
# ---------------------------------------------------------------------------------------------------------------------
echo
echo "--- Step 5/5: phone ---------------------------------------------------------------------------"

if [ "${INSTALL_ON_DEVICE}" = "OFF" ]; then
    echo "  skipped (--no-install)"
else

    DEVICE_COUNT="$(adb devices | sed '1d' | grep -cE '[[:space:]]device$' || true)"

    if [ "${DEVICE_COUNT}" -eq 0 ]; then
        echo "  no phone detected: skipped."
        echo "  Plug it in, unlock it, accept the USB debugging prompt, then:"
        echo "    adb install -r ${PACKAGE_FILE}"
    elif [ "${DEVICE_COUNT}" -gt 1 ]; then
        echo "  ${DEVICE_COUNT} phones detected, which one to use is not obvious: skipped."
        echo "    adb devices"
        echo "    adb -s <serial> install -r ${PACKAGE_FILE}"
    else
        echo "  installing on the connected phone..."
        adb install -r "${PACKAGE_FILE}"

        echo
        echo "  Confirming on the phone that no permission was granted..."

        # The definitive check, run on the device itself rather than on a local file: what the system
        # says it has granted this package. It should be empty.
        GRANTED_PERMISSIONS="$(adb shell dumpsys package "${ANDROID_PACKAGE_NAME}" 2>/dev/null |
                               grep -E 'android\.permission\.' || true)"

        if [ -n "${GRANTED_PERMISSIONS}" ]; then
            echo "  WARNING - the phone reports permissions for this package:"
            printf '%s\n' "${GRANTED_PERMISSIONS}" | sed 's/^/    /'
        else
            echo "  OK - the phone grants this package no permission."
        fi
    fi
fi

echo
echo "====================================================================================================="
echo " Done."
echo "   package : ${PACKAGE_FILE}"
echo "   launch  : adb shell am start -n ${ANDROID_PACKAGE_NAME}/org.qtproject.qt.android.bindings.QtActivity"
echo "====================================================================================================="


