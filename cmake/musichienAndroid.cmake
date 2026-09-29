# =====================================================================================================================
# Android packaging
#
# Musichien claims to be technically unable to reach the network. That claim is not a promise written
# in a README: it is a property of the installed package, and this file is what makes it true.
#
# ---------------------------------------------------------------------------------------------------------------------
# The problem: Qt requests INTERNET on our behalf
#
# Qt6::Core declares two permissions in its link interface:
#
#     INTERFACE_QT_ANDROID_PERMISSIONS = android.permission.WRITE_EXTERNAL_STORAGE
#                                        android.permission.INTERNET
#
# Because the property is transitive, EVERY target that links Qt6::Core inherits both, and Qt writes
# them into the generated AndroidManifest.xml as <uses-permission> elements. A build left to itself
# therefore produces an APK that asks for Internet access.
#
# The permission is not cosmetic, and this is the whole point of the exercise: on Android, an
# application that does not hold android.permission.INTERNET is not placed in the 'inet' group, and
# its socket() calls fail. The absence of the declaration IS the guarantee - a stronger one than any
# runtime check we could write, since it is enforced by the kernel and not by our own code.
#
# ---------------------------------------------------------------------------------------------------------------------
# The fix
#
# The transitive property is emptied on every Qt module that publishes one, before the manifest is
# generated. Qt then finds nothing to insert, and the manifest requests nothing at all.
#
# scripts/build_android.sh verifies the result a second time, on the finished APK, with the tools of
# the Android SDK. Prevention AND verification, deliberately: a Qt upgrade could otherwise put the
# permission back without a single warning being printed.
# =====================================================================================================================

# ---------------------------------------------------------------------------------------------------------------------
# Identity of the package
#
# Left alone, Qt derives the package name from the CMake target, which would give the unreadable
# 'org.qtproject.example.musichien_app', and a label taken from the target name as well. Both are
# visible to the user: the label on the launcher, the package name in the system settings.
#
# The package name is an identifier, not an address: it does not have to point at a domain that
# exists. The convention used here is the one GitHub suggests for projects without their own domain.
# ---------------------------------------------------------------------------------------------------------------------
set(MUSICHIEN_ANDROID_PACKAGE_NAME "io.github.srroger.musichien")

# ---------------------------------------------------------------------------------------------------------------------
# Removing the permissions requested by the Qt modules
#
# The targets are enumerated instead of hard coded: only Qt6::Core publishes the property today, but
# a future Qt module could start doing so, and the rule has to survive that. Enumerating the imported
# targets of this directory is the cheap way to be certain of catching all of them.
# ---------------------------------------------------------------------------------------------------------------------
# The Qt targets are created by find_package() in the directory that includes
# musichienFindDependencies.cmake, which is the TOP LEVEL one - not the directory of the application
# target that includes this file.
#
# Querying CMAKE_SOURCE_DIR explicitly is what makes the enumeration below actually see them.
# Asking for the imported targets of the current directory would silently return an empty list, and
# the permissions would then be inserted without a single warning being printed.
get_property(musichienImportedTargets DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY IMPORTED_TARGETS)

foreach(musichienImportedTarget IN LISTS musichienImportedTargets)

    if(NOT musichienImportedTarget MATCHES "^Qt6::")
        continue()
    endif()

    get_target_property(musichienInheritedPermissions ${musichienImportedTarget}
                        INTERFACE_QT_ANDROID_PERMISSIONS)

    if(musichienInheritedPermissions)
        message(STATUS "Musichien: dropping the Android permissions requested by "
                       "${musichienImportedTarget}: ${musichienInheritedPermissions}")

        set_property(TARGET ${musichienImportedTarget} PROPERTY INTERFACE_QT_ANDROID_PERMISSIONS "")
    endif()

endforeach()

# ---------------------------------------------------------------------------------------------------------------------
# Package source directory
#
# This is where the AndroidManifest.xml of the project lives, and supplying it is what takes the
# permissions out of Qt's hands.
#
# androiddeployqt reads the '<module>-android-dependencies.xml' files of every Qt library it packages
# and merges the permissions they declare into the manifest - it does NOT ask CMake for them. The only
# place it can write them is the '%%INSERT_PERMISSIONS%%' marker, and the manifest of this project
# deliberately does not contain that marker. See source/android/AndroidManifest.xml.
#
# Emptying the CMake property above is therefore necessary but NOT sufficient: both channels have to
# be closed. scripts/build_android.sh verifies the outcome on the finished package.
# ---------------------------------------------------------------------------------------------------------------------
set_target_properties(musichien_app PROPERTIES
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/source/android"
)

# ---------------------------------------------------------------------------------------------------------------------
# Identity shown by the system
#
# The version code is an integer that Android compares when it installs an update over an existing
# application: it MUST increase, or the installation is refused with INSTALL_FAILED_VERSION_DOWNGRADE.
#
# The three numbers are therefore given WEIGHTS rather than being concatenated. Concatenation looked
# simpler and was wrong: it turned 0.10.3 into 103, and 1.0.0 into 100 - a release that the phone would
# have refused to install, at the exact moment the project reached its first major version.
#
#   major x 10000 + minor x 100 + patch     ->   0.10.3 = 1003, 1.0.0 = 10000
# ---------------------------------------------------------------------------------------------------------------------
math(EXPR MUSICHIEN_ANDROID_VERSION_CODE
     "${PROJECT_VERSION_MAJOR} * 10000 + ${PROJECT_VERSION_MINOR} * 100 + ${PROJECT_VERSION_PATCH}")

set_target_properties(musichien_app PROPERTIES
    QT_ANDROID_PACKAGE_NAME "${MUSICHIEN_ANDROID_PACKAGE_NAME}"
    QT_ANDROID_APP_NAME     "Musichien"
    QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
    QT_ANDROID_VERSION_CODE "${MUSICHIEN_ANDROID_VERSION_CODE}"
)

# ---------------------------------------------------------------------------------------------------------------------
# Supported Android versions
#
# The minimum is API 28, that is Android 9. This is the floor imposed by Qt 6.12 itself: below it,
# the Qt libraries refuse to load.
#
# The target is pinned to 36 rather than left at the default of 37, for two reasons:
#   * API 37 (Android 17) has no stable SDK platform yet, so nothing could compile against it;
#   * a targetSdkVersion higher than compileSdkVersion is an error for the Android Gradle Plugin,
#     and Qt detects the compile SDK as the newest platform actually installed in the SDK.
#
# The compile SDK is deliberately NOT set here: Qt's auto detection is what keeps the build working
# on a machine where a different platform is installed. See docs/BUILD_AND_SETUP.md.
# ---------------------------------------------------------------------------------------------------------------------
set_target_properties(musichien_app PROPERTIES
    QT_ANDROID_MIN_SDK_VERSION    28
    QT_ANDROID_TARGET_SDK_VERSION 36
)

# ---------------------------------------------------------------------------------------------------------------------
# Summary
#
# Printed loudly on purpose: the promise of the project is verified here for the first time, and the
# second verification (on the APK) is done by scripts/build_android.sh.
# ---------------------------------------------------------------------------------------------------------------------
message(STATUS "Musichien: Android package '${MUSICHIEN_ANDROID_PACKAGE_NAME}' "
               "(version ${PROJECT_VERSION}, code ${MUSICHIEN_ANDROID_VERSION_CODE})")
message(STATUS "Musichien: Android API 28 minimum, 36 targeted "
               "(the compile SDK is the newest platform installed in the SDK)")
message(STATUS "Musichien: no Android permission is requested - the application cannot use the network")
