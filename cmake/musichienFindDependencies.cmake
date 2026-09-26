# =====================================================================================================================
# External dependencies lookup
#
# The point of this file is reproducibility: the project must not silently pick up whatever version
# happens to be installed on the developer machine.
#
# Search order for every dependency:
#   1. the pinned installation inside MUSICHIEN_EXTERNAL_DIR
#   2. the Qt indicated by MUSICHIEN_QT_DIR for Qt itself
#   3. the system installation, with an explicit warning
# =====================================================================================================================

# ---------------------------------------------------------------------------------------------------------------------
# Qt 6
#
# Qt is NOT built by the superbuild: official prebuilt binaries are downloaded by
# scripts/install_dependencies.sh. They are reproducible and identical on every machine.
# ---------------------------------------------------------------------------------------------------------------------
if(NOT MUSICHIEN_QT_DIR AND DEFINED ENV{MUSICHIEN_QT_DIR})
    set(MUSICHIEN_QT_DIR $ENV{MUSICHIEN_QT_DIR})
endif()

if(MUSICHIEN_QT_DIR)
    if(NOT EXISTS "${MUSICHIEN_QT_DIR}/lib/cmake/Qt6/Qt6Config.cmake")
        message(FATAL_ERROR
            "MUSICHIEN_QT_DIR is set to '${MUSICHIEN_QT_DIR}' but no Qt6 installation was found there.\n"
            "Expected file: ${MUSICHIEN_QT_DIR}/lib/cmake/Qt6/Qt6Config.cmake\n"
            "Reinstall the pinned Qt with scripts/install_dependencies.sh.")
    endif()

    list(PREPEND CMAKE_PREFIX_PATH ${MUSICHIEN_QT_DIR})
    message(STATUS "Musichien: using the pinned Qt from '${MUSICHIEN_QT_DIR}'")
else()
    message(WARNING
        "MUSICHIEN_QT_DIR is not defined: falling back to the Qt installed on this machine.\n"
        "The build will then depend on the host, which is exactly what the project tries to avoid.\n"
        "Run 'source scripts/setup_env.sh' before configuring.")
endif()

# Core     : QObject, signals, containers
# Gui      : QImage, QColor and the platform integration of Qt Quick
# Qml      : the QML engine and the C++/QML bridge
# Quick    : the Qt Quick scene graph behind every window
# QuickControls2 : the ready made mobile controls (Material style on Android)
# Multimedia : QAudioSink / QAudioSource, used by the audio engine
# Test     : QTest, only needed when a Qt based test is required
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Multimedia
    Test
)

qt_standard_project_setup()

message(STATUS "Musichien: Qt ${Qt6_VERSION} found at '${Qt6_DIR}'")

# ---------------------------------------------------------------------------------------------------------------------
# GoogleTest / GoogleMock
#
# Built from source by the superbuild, so that the version never depends on the host.
# ---------------------------------------------------------------------------------------------------------------------
set(MUSICHIEN_GOOGLETEST_DIR "${MUSICHIEN_EXTERNAL_DIR}/GoogleTest-${MUSICHIEN_GOOGLETEST_VERSION}")

if(EXISTS "${MUSICHIEN_GOOGLETEST_DIR}")
    list(PREPEND CMAKE_PREFIX_PATH ${MUSICHIEN_GOOGLETEST_DIR})
    message(STATUS "Musichien: using the pinned GoogleTest from '${MUSICHIEN_GOOGLETEST_DIR}'")
else()
    message(WARNING
        "Pinned GoogleTest not found in '${MUSICHIEN_GOOGLETEST_DIR}': falling back to the host.\n"
        "Build it once with: cmake --preset \"Superbuild Musichien\" && cmake --build --preset \"Build Superbuild Musichien\"")
endif()

if(MUSICHIEN_BUILD_TESTING)
    find_package(GTest REQUIRED)
    message(STATUS "Musichien: GoogleTest ${GTest_VERSION} found")
    find_package(Qt6 REQUIRED COMPONENTS Test)
endif()

# ---------------------------------------------------------------------------------------------------------------------
# nlohmann/json - header only JSON library, used for the save file and the content files
# ---------------------------------------------------------------------------------------------------------------------
set(MUSICHIEN_NLOHMANN_JSON_DIR "${MUSICHIEN_EXTERNAL_DIR}/NlohmannJson-${MUSICHIEN_NLOHMANN_JSON_VERSION}")

if(EXISTS "${MUSICHIEN_NLOHMANN_JSON_DIR}/include")
    message(STATUS "Musichien: using the pinned nlohmann/json from '${MUSICHIEN_NLOHMANN_JSON_DIR}'")
    add_library(musichien_nlohmann_json INTERFACE)
    target_include_directories(musichien_nlohmann_json SYSTEM INTERFACE "${MUSICHIEN_NLOHMANN_JSON_DIR}/include")
    add_library(nlohmann_json ALIAS musichien_nlohmann_json)
else()
    message(WARNING
        "Pinned nlohmann/json not found in '${MUSICHIEN_NLOHMANN_JSON_DIR}': falling back to the host.\n"
        "Build it once with the superbuild preset.")
    find_package(nlohmann_json QUIET)
endif()
