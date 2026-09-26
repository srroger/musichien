# =====================================================================================================================
# Musichien - the application itself
#
# This target only assembles: the entry point, the QML resources and the modules. It contains no
# logic of its own. See docs/ARCHITECTURE.md.
# =====================================================================================================================

# qt_add_executable, and NOT the plain add_executable from CMake.
#
# This is not a stylistic preference. qt_add_executable marks the target as one Qt expects to
# finalize, and that finalization is what creates the Android deployment targets. With a plain
# add_executable, the 'apk' target is created but stays EMPTY: the build reports success and produces
# no package at all, which is a very expensive kind of silence.
#
# On the desktop the two calls are equivalent for this project: qt_add_executable only adds the
# platform entry point handling that WIN32_EXECUTABLE / MACOSX_BUNDLE set explicitly below.
qt_add_executable(musichien_app
    main.cpp
    # The QML resources, declared by the ui module. Referencing the .qrc file from here is what makes
    # CMake compile the QML into the executable.
    ${MUSICHIEN_QML_RESOURCE_FILE}
)

musichienFunction_ConfigureTarget(musichien_app)

# The union of the QML file list and the resource file makes CMake and the IDE aware of every
# displayed file, so that a change in a .qml file triggers a rebuild of the resources.
target_sources(musichien_app PRIVATE ${MUSICHIEN_QML_FILES})

# ---------------------------------------------------------------------------------------------------------------------
# QML import scanning - what makes the Android package able to start
#
# Qt EMBEDS, inside the Android package, the QML plugins the application actually imports: the
# QtQuick controls, their Material style, the layouts... To decide which ones, it runs
# 'qmlimportscanner' over the directories declared in the deployment settings.
#
# Those directories come from the QT_QML_ROOT_PATH property. When it is NOT set, Qt falls back to the
# SOURCE_DIR of the TARGET - see _qt_internal_android_get_qml_root_paths in Qt6AndroidGradleHelpers.cmake.
# Here that is 'source/application', which contains NO QML AT ALL: the QML files live in the ui
# module. The scanner therefore finds nothing, bundles no plugin, and the application dies on the
# phone, right after a successful install, with:
#
#     QQmlApplicationEngine failed to load component
#     qrc:/qml/Main.qml:16:1: module "QtQuick.Controls" plugin
#     "qml_QtQuick_Controls_qtquickcontrols2plugin" not found
#
# Nothing warns at build time, and the desktop build is unaffected, because there the plugins are
# taken from the Qt installation on the machine. Only the phone reveals it.
#
# Naming the directory that holds the QML files is the whole fix. It is exported by the ui module,
# which owns them, so that moving a QML file only means updating one line there.
# ---------------------------------------------------------------------------------------------------------------------
if(ANDROID)
    set_target_properties(musichien_app PROPERTIES
        QT_QML_ROOT_PATH "${MUSICHIEN_QML_ROOT_PATH}"
    )
endif()


# The version comes from the single project() call of the top level CMakeLists.txt.
target_compile_definitions(musichien_app PRIVATE MUSICHIEN_VERSION="${PROJECT_VERSION}")

# -------------------------------------------------------------------------------------------------------------------
# Identifier of the build
#
# The version says what the code is SUPPOSED to be; this says WHICH commit was actually compiled. Until the
# first release, the second is what a player needs: an application installed on a phone can be weeks old, and
# "version 0.5.0" alone cannot tell two builds of it apart.
#
# The header is written by a target that runs on every build and rewrites the file only when the identifier
# changed, so being exact costs nothing after the first build. See cmake/musichienWriteBuildId.cmake.
# -------------------------------------------------------------------------------------------------------------------
set(MUSICHIEN_BUILD_ID_HEADER "${CMAKE_BINARY_DIR}/generated/musichienBuildId.h")

# The files git rewrites when the repository moves: HEAD, and the branch it points at - which is the file a
# commit actually updates. Depending on BOTH is what makes the identifier exact: HEAD alone does not change
# when a commit is made on a branch.
#
# A file that does not exist is left out rather than depended on: a missing dependency would look like a
# permanent change, and the identifier would be regenerated on every single build.
set(MUSICHIEN_GIT_STATE_FILES "")

foreach(musichienGitStateFile IN ITEMS
        "${CMAKE_SOURCE_DIR}/.git/HEAD"
        "${CMAKE_SOURCE_DIR}/.git/packed-refs"
        "${CMAKE_SOURCE_DIR}/.git/refs/heads/${MUSICHIEN_GIT_BRANCH}")
    if(EXISTS "${musichienGitStateFile}")
        list(APPEND MUSICHIEN_GIT_STATE_FILES "${musichienGitStateFile}")
    endif()
endforeach()

execute_process(
    COMMAND git symbolic-ref --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE MUSICHIEN_GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE MUSICHIEN_GIT_BRANCH_RESULT
)

if(MUSICHIEN_GIT_BRANCH_RESULT EQUAL 0)
    list(APPEND MUSICHIEN_GIT_STATE_FILES "${CMAKE_SOURCE_DIR}/.git/refs/heads/${MUSICHIEN_GIT_BRANCH}")
endif()

# A real build artifact, produced by a rule CMake knows about: that is what lets the compiler be ordered after
# it - main.cpp includes the header, and the dependency the compiler itself records is what makes the ordering
# exact. A file written by an 'always runs' target would be one build behind, because the build tool decides
# what to recompile before that target has had a chance to run.
add_custom_command(
    OUTPUT ${MUSICHIEN_BUILD_ID_HEADER}
    COMMAND ${CMAKE_COMMAND}
            -D MUSICHIEN_SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -D MUSICHIEN_OUTPUT_FILE=${MUSICHIEN_BUILD_ID_HEADER}
            -P ${CMAKE_SOURCE_DIR}/cmake/musichienWriteBuildId.cmake
    DEPENDS ${MUSICHIEN_GIT_STATE_FILES}
    COMMENT "Musichien: identifying the build"
    VERBATIM
)

add_custom_target(musichien_build_id ALL DEPENDS ${MUSICHIEN_BUILD_ID_HEADER})

target_include_directories(musichien_app PRIVATE ${CMAKE_BINARY_DIR}/generated)

target_link_libraries(musichien_app PRIVATE
    musichien_domain
    musichien_ui
    musichien_infrastructure

    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
)


# The file is named "musichien" while the target keeps the explicit "musichien_app" name: the binary
# and the target do not have to share a name.
set_target_properties(musichien_app PROPERTIES
    OUTPUT_NAME "musichien"
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

set_target_properties(musichien_app PROPERTIES FOLDER "application")

# ---------------------------------------------------------------------------------------------------------------------
# Android packaging
#
# Only meaningful when cross compiling for a phone. A desktop build has no manifest, no package and
# no permission to argue about, so this is deliberately kept out of the way.
#
# See cmake/musichienAndroid.cmake: it is what turns the privacy promise of the project into a
# property of the installed package rather than an intention.
# ---------------------------------------------------------------------------------------------------------------------
if(ANDROID)
    include(musichienAndroid)
endif()

