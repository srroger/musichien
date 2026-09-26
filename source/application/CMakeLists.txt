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

