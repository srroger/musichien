# =====================================================================================================================
# Musichien - the application itself
#
# This target only assembles: the entry point, the QML resources and the modules. It contains no
# logic of its own. See docs/ARCHITECTURE.md.
# =====================================================================================================================

add_executable(musichien_app
    main.cpp
    # The QML resources, declared by the ui module. Referencing the .qrc file from here is what makes
    # CMake compile the QML into the executable.
    ${MUSICHIEN_QML_RESOURCE_FILE}
)

musichienFunction_ConfigureTarget(musichien_app)

# The union of the QML file list and the resource file makes CMake and the IDE aware of every
# displayed file, so that a change in a .qml file triggers a rebuild of the resources.
target_sources(musichien_app PRIVATE ${MUSICHIEN_QML_FILES})

# The version comes from the single project() call of the top level CMakeLists.txt.
target_compile_definitions(musichien_app PRIVATE MUSICHIEN_VERSION="${PROJECT_VERSION}")

target_link_libraries(musichien_app PRIVATE
    musichien_domain

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
