# =====================================================================================================================
# Apple packaging (macOS and iOS)
#
# The same binary identity for both Apple targets. Nothing here is about permissions - there is no
# manifest and no permission list on Apple platforms - but about WHO the application is: its bundle
# identifier, its display name, and the version numbers the system and the App Store read.
#
# Qt already builds the bundle (qt_add_executable with MACOSX_BUNDLE ON, see the application module),
# and on iOS it generates an Info.plist suited to a device rather than to a desktop. What it cannot
# guess is the identity of THIS project, so it is written once, here.
# =====================================================================================================================

# ---------------------------------------------------------------------------------------------------------------------
# Identity
#
# The bundle identifier is an identifier, not an address: it does not have to point at a domain that
# exists. The convention used here is the one GitHub suggests for projects without their own domain,
# and it is deliberately the same string as the Android package name
# (cmake/musichienAndroid.cmake) so that the application is one thing on every store.
#
# NOTE: changing this string changes the identity of the application. On iOS an application installed
# under a new identifier is a NEW application: its local save is not carried over automatically.
# ---------------------------------------------------------------------------------------------------------------------
set(MUSICHIEN_APPLE_BUNDLE_IDENTIFIER "io.github.srroger.musichien")

set_target_properties(musichien_app PROPERTIES

    # The bundle itself. Qt sets MACOSX_BUNDLE already; repeating it here documents the intent and
    # keeps this file readable on its own.
    MACOSX_BUNDLE              TRUE

    # What the user sees under the icon.
    MACOSX_BUNDLE_BUNDLE_NAME  "Musichien"

    # Who the system and the App Store think this is.
    MACOSX_BUNDLE_GUI_IDENTIFIER              "${MUSICHIEN_APPLE_BUNDLE_IDENTIFIER}"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${MUSICHIEN_APPLE_BUNDLE_IDENTIFIER}"

    # Version numbers. CFBundleShortVersionString is the marketing version the user reads;
    # CFBundleVersion is the build number the store compares between two uploads. Both follow the one
    # single source of truth, the project() call of the top level CMakeLists.txt.
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION       "${PROJECT_VERSION}"
    XCODE_ATTRIBUTE_MARKETING_VERSION  "${PROJECT_VERSION}"
    XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION "${PROJECT_VERSION}"
)

message(STATUS "Musichien: Apple bundle '${MUSICHIEN_APPLE_BUNDLE_IDENTIFIER}' (version ${PROJECT_VERSION})")
