#!/bin/bash
# =====================================================================================================================
# Musichien - builds and signs the iPhone/iPad application package (.ipa).
#
#   Usage:  scripts/build_ios.sh [--no-export] [--simulator]
#
#   --no-export   configure and archive, but stop before exporting the .ipa
#   --simulator   build for the iOS simulator instead of a device (no signing required)
#
# What it does:
#   1. checks that Xcode, Qt for iOS and the host Qt are all present and pinned
#   2. configures and builds the 'iOS-Release Musichien' preset (Xcode generator)
#   3. archives the application with xcodebuild
#   4. exports a signed .ipa from the archive
#
# Why an archive and not a plain build: an .ipa is an archive wrapped in a payload, and only
# 'xcodebuild archive' + 'xcodebuild -exportArchive' produce one with a provisioning profile embedded
# in the right places. The Xcode generator of the iOS preset is what makes both possible.
#
# Code signing needs a Development Team identifier. Put it in the environment once:
#
#     export MUSICHIEN_IOS_TEAM_ID=ABCDE12345
#
# and Xcode handles the rest (automatic signing, provisioning profile refresh). Without it the archive
# still succeeds, but the export step asks Xcode to resolve the signing interactively.
# =====================================================================================================================

set -euo pipefail

# ---------------------------------------------------------------------------------------------------------------------
# Arguments
# ---------------------------------------------------------------------------------------------------------------------
EXPORT_PACKAGE=ON
BUILD_FOR_SIMULATOR=OFF

for argument in "$@"; do
    case "${argument}" in
        --no-export) EXPORT_PACKAGE=OFF ;;
        --simulator) BUILD_FOR_SIMULATOR=ON ;;
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

CONFIGURE_PRESET="iOS-Release Musichien"
BUILD_PRESET="Build iOS-Release Musichien"
BUILD_DIR="${PROJECT_DIR}/../Musichien-build/iOS-Release"
ARCHIVE_PATH="${BUILD_DIR}/Musichien.xcarchive"
EXPORT_DIRECTORY="${BUILD_DIR}/ipa"

# The export method decides what the .ipa is good for:
#   development  install on a registered device, or hand it to a tester (the default)
#   ad-hoc       install on registered devices outside a development setup
#   app-store    send to TestFlight / the App Store
#   enterprise   in-house distribution (requires an Enterprise account)
EXPORT_METHOD="${MUSICHIEN_IOS_EXPORT_METHOD:-development}"

echo

# ---------------------------------------------------------------------------------------------------------------------
# 1. Pinned tools
#
# The check is done on the exported variables, never on what happens to be installed on the machine: a
# missing tool must stop the build here, with an explanation, and not half an hour later inside Xcode.
# ---------------------------------------------------------------------------------------------------------------------
echo "--- Step 1/5: pinned iOS tools ------------------------------------------------"

MISSING_TOOL_COUNT=0

check_tool()
{
    local description="$1"
    local value="$2"
    local explanation="$3"

    if [ -n "${value}" ] && [ -e "${value}" ]; then
        printf '  %-26s %s\n' "${description}" "${value}"
    else
        printf '  %-26s MISSING (%s)\n' "${description}" "${explanation}"
        MISSING_TOOL_COUNT=$((MISSING_TOOL_COUNT + 1))
    fi
}

check_tool "xcodebuild"     "$(command -v xcodebuild || true)" "install Xcode from the App Store"
check_tool "MUSICHIEN_QT_DIR"        "${MUSICHIEN_QT_DIR:-}"        "run: scripts/install_dependencies.sh"
check_tool "MUSICHIEN_QT_IOS_DIR"    "${MUSICHIEN_QT_IOS_DIR:-}"    "run: scripts/install_dependencies.sh --with-ios"
check_tool "MUSICHIEN_EXTERNAL_DIR"  "${MUSICHIEN_EXTERNAL_DIR:-}"  "run: scripts/install_dependencies.sh --with-superbuild"

if [ "${MISSING_TOOL_COUNT}" -gt 0 ]; then
    echo
    echo "build_ios.sh: ${MISSING_TOOL_COUNT} pinned tool(s) missing, stopping before doing any work."
    exit 1
fi

echo

# ---------------------------------------------------------------------------------------------------------------------
# 2. Configuration
#
# Code signing is left to Xcode in Automatic mode. When a development team is known it is passed
# straight to the generated Xcode project, so that xcodebuild never has to guess it.
# ---------------------------------------------------------------------------------------------------------------------
echo "--- Step 2/5: configuration --------------------------------------------------"

CONFIGURE_ARGUMENTS=(
    "-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE=Automatic"
)

if [ -n "${MUSICHIEN_IOS_TEAM_ID:-}" ]; then
    CONFIGURE_ARGUMENTS+=( "-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=${MUSICHIEN_IOS_TEAM_ID}" )
    echo "  development team: ${MUSICHIEN_IOS_TEAM_ID}"
else
    echo "  NOTE: MUSICHIEN_IOS_TEAM_ID is not set. Xcode resolves the signing team itself, and may ask"
    echo "        for it interactively during the export step. See the header of this script."
fi

cmake --preset "${CONFIGURE_PRESET}" "${CONFIGURE_ARGUMENTS[@]}"

# ---------------------------------------------------------------------------------------------------------------------
# 3. Archive
#
# The project file is SEARCHED for rather than assumed: its name follows the target name, and guessing
# it is how a script breaks the day a target is renamed.
# ---------------------------------------------------------------------------------------------------------------------
echo
echo "--- Step 3/5: archive --------------------------------------------------------"

XCODE_PROJECT=""
for candidate in "${BUILD_DIR}"/*.xcodeproj; do
    if [ -d "${candidate}" ]; then
        XCODE_PROJECT="${candidate}"
        break
    fi
done

if [ -z "${XCODE_PROJECT}" ]; then
    echo "  ERROR: no .xcodeproj was found under '${BUILD_DIR}'."
    exit 1
fi

SCHEME="$(xcodebuild -list -project "${XCODE_PROJECT}" 2>/dev/null |
          awk '/Schemes:/ { found = 1; next } found && NF { print $1; exit }' || true)"

[ -n "${SCHEME}" ] || SCHEME="musichien_app"

if [ "${BUILD_FOR_SIMULATOR}" = "ON" ]; then
    XCODE_SDK="iphonesimulator"
    XCODE_DESTINATION="generic/platform=iOS Simulator"
else
    XCODE_SDK="iphoneos"
    XCODE_DESTINATION="generic/platform=iOS"
fi

printf '  %-16s %s\n' "project" "${XCODE_PROJECT}"
printf '  %-16s %s\n' "scheme" "${SCHEME}"
printf '  %-16s %s\n' "sdk" "${XCODE_SDK}"

rm -rf "${ARCHIVE_PATH}"

xcodebuild \
    -project "${XCODE_PROJECT}" \
    -scheme "${SCHEME}" \
    -configuration Release \
    -sdk "${XCODE_SDK}" \
    -destination "${XCODE_DESTINATION}" \
    -archivePath "${ARCHIVE_PATH}" \
    -allowProvisioningUpdates \
    archive

# ---------------------------------------------------------------------------------------------------------------------
# 4. Export of the .ipa
#
# An archive is not an installable package: it holds the application with its symbols. The export step
# wraps it into a payload and embeds the provisioning profile, which is what makes an .ipa.
# ---------------------------------------------------------------------------------------------------------------------
echo
echo "--- Step 4/5: export of the .ipa ---------------------------------------------"

PACKAGE_FILE=""

if [ "${EXPORT_PACKAGE}" = "OFF" ]; then
    echo "  skipped (--no-export)"
elif [ "${BUILD_FOR_SIMULATOR}" = "ON" ]; then
    echo "  skipped: a simulator build is not packaged as an .ipa."
else
    EXPORT_OPTIONS_PLIST="${BUILD_DIR}/ExportOptions.plist"

    cat > "${EXPORT_OPTIONS_PLIST}" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>teamID</key>
    <string>${MUSICHIEN_IOS_TEAM_ID:-}</string>
    <key>method</key>
    <string>${EXPORT_METHOD}</string>
    <key>signingStyle</key>
    <string>automatic</string>
    <key>stripSwiftSymbols</key>
    <true/>
    <key>compileBitcode</key>
    <false/>
</dict>
</plist>
PLIST

    rm -rf "${EXPORT_DIRECTORY}"

    xcodebuild \
        -exportArchive \
        -archivePath "${ARCHIVE_PATH}" \
        -exportOptionsPlist "${EXPORT_OPTIONS_PLIST}" \
        -exportPath "${EXPORT_DIRECTORY}" \
        -allowProvisioningUpdates

    for candidate in "${EXPORT_DIRECTORY}"/*.ipa; do
        if [ -f "${candidate}" ]; then
            PACKAGE_FILE="${candidate}"
            break
        fi
    done
fi

# ---------------------------------------------------------------------------------------------------------------------
# 5. Result
# ---------------------------------------------------------------------------------------------------------------------
echo
echo "--- Step 5/5: result ---------------------------------------------------------"

printf '  %-16s %s\n' "archive" "${ARCHIVE_PATH}"

if [ -n "${PACKAGE_FILE}" ]; then
    printf '  %-16s %s\n' "package" "${PACKAGE_FILE}"
    printf '  %-16s %s\n' "size" "$(du -h "${PACKAGE_FILE}" | cut -f1)"
else
    echo "  No .ipa exported. The archive can be opened in Xcode (Window > Organizer)."
fi

echo
echo "====================================================================================================="
echo " Done."
echo "   install : Apple Configurator, or 'xcrun devicectl device install app'"
echo "   tester  : TestFlight, once the project belongs to a paid Apple Developer account"
echo "====================================================================================================="

# A sound, once the package is built - same reason as scripts/build_android.sh: the build runs in the
# background, the human is elsewhere, and silence must mean "still working". See AGENTS.md, section 7.
if command -v paplay >/dev/null 2>&1; then
    paplay /usr/share/sounds/freedesktop/stereo/complete.oga 2>/dev/null || true
elif command -v afplay >/dev/null 2>&1; then
    afplay /System/Library/Sounds/Glass.aiff 2>/dev/null || true
fi
