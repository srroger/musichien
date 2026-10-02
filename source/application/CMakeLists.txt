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

# Les QML eux-memes, par qt_add_resources : AUTORCC ne suit que le .qrc et laisserait un QML modifie hors du build.
# Voir le long commentaire de source/ui/CMakeLists.txt, et celui du test qui charge cet ecran.
set_source_files_properties(${MUSICHIEN_QML_ABSOLUTE_FILES}
    PROPERTIES QT_RESOURCE_ALIAS "${MUSICHIEN_QML_ALIASES}")

qt_add_resources(musichien_app "qml-resources"
    PREFIX "/"
    FILES ${MUSICHIEN_QML_ABSOLUTE_FILES}
)

musichienFunction_ConfigureTarget(musichien_app)

# The union of the QML file list and the resource file makes CMake and the IDE aware of every
# displayed file.
target_sources(musichien_app PRIVATE ${MUSICHIEN_QML_FILES})

# ---------------------------------------------------------------------------------------------------------------------
# A changed QML file must rebuild the RESOURCE, and not only the target
#
# Listing the QML files as sources of the executable - the line above - looks like it should be enough. It is not:
# the resource is a C++ file carrying the CONTENT of the QML, generated once, and CMake has no reason to regenerate
# it because the .qml changed. The build succeeds, the application starts, and the screen is the one from before
# the change.
#
# What fixes it lives in source/ui/CMakeLists.txt, next to the resource itself: a build target that touches the
# .qrc whenever a QML changes, which is the only event AUTORCC reacts to.


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
# The version says what the code is SUPPOSED to be; this says WHEN the binary was made - local date and time, to the
# minute. That is what a player compares between two APKs: an application installed on a phone can be weeks old, and
# « which one is the newer? » is a question a date answers in one look.
#
# It is regenerated on EVERY build rather than when git moves, because the date changes with the build. The header is
# rewritten only when its content changes - the minute, not the second - so two builds in the same minute cost
# nothing. See cmake/musichienWriteBuildId.cmake.
# -------------------------------------------------------------------------------------------------------------------
set(MUSICHIEN_BUILD_ID_HEADER "${CMAKE_BINARY_DIR}/generated/musichienBuildId.h")

# Written ONCE at configure time as well, and that is not a duplicate: at the very first build the file does not
# exist yet when the compiler looks for its headers, and a rule that has not run cannot provide it.
execute_process(
    COMMAND ${CMAKE_COMMAND}
            -D MUSICHIEN_OUTPUT_FILE=${MUSICHIEN_BUILD_ID_HEADER}
            -P ${CMAKE_SOURCE_DIR}/cmake/musichienWriteBuildId.cmake
)

# A target that runs on every build. It is ORDERED BEFORE the compiler by the explicit dependency below, which is
# what makes an always-running target exact here - a file written after the compiler had looked would be one build
# behind, and would name the wrong minute.
add_custom_target(musichien_build_id ALL
    COMMAND ${CMAKE_COMMAND}
            -D MUSICHIEN_OUTPUT_FILE=${MUSICHIEN_BUILD_ID_HEADER}
            -P ${CMAKE_SOURCE_DIR}/cmake/musichienWriteBuildId.cmake
    COMMENT "Musichien: identifying the build"
    VERBATIM
)

# And the application depends on it EXPLICITLY, which 'ALL' does not cover: the Android build asks for a
# precise target - the package - and a target marked ALL is then never built. Without this line the header is
# simply missing on a phone build, and main.cpp fails to compile.
add_dependencies(musichien_app musichien_build_id)

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

