# =====================================================================================================================
# Superbuild configuration
#
# Goal: make the build independent from the libraries installed on the developer machine.
#
# What the superbuild builds: the dependencies we compile from source (GoogleTest, nlohmann/json).
#
# What the superbuild does NOT build: Qt. Deliberate choice, see docs/BUILD_AND_SETUP.md:
#   * Qt's own prebuilt binaries are reproducible and identical on every machine;
#   * a source build of Qt takes hours and would have to be repeated for every target;
#   * scripts/install_dependencies.sh pins the exact version instead.
# =====================================================================================================================

# ---------------------------------------------------------------------------------------------------------------------
# Compiler flags for the superbuild
#
# Dependencies are third party code: the warnings of the project must NOT be applied to them,
# otherwise their build would fail for reasons outside of our control.
# ---------------------------------------------------------------------------------------------------------------------
set(CMAKE_CXX_FLAGS "" CACHE STRING "C++ compiler flags used to build the external dependencies")

# ---------------------------------------------------------------------------------------------------------------------
# Location of the external dependencies
# ---------------------------------------------------------------------------------------------------------------------
if(NOT MUSICHIEN_EXTERNAL_DIR)
    if(DEFINED ENV{MUSICHIEN_EXTERNAL_DIR})
        set(MUSICHIEN_EXTERNAL_DIR $ENV{MUSICHIEN_EXTERNAL_DIR})
    else()
        get_filename_component(MUSICHIEN_EXTERNAL_DIR ${CMAKE_SOURCE_DIR}/../Roger-externals ABSOLUTE)
    endif()
endif()

set(SUPERBUILD_DEPENDENCIES_INSTALL_PATH "${MUSICHIEN_EXTERNAL_DIR}"
    CACHE PATH "Directory where the external dependencies are installed" FORCE)

message(STATUS "Musichien superbuild: dependencies will be installed into '${SUPERBUILD_DEPENDENCIES_INSTALL_PATH}'")

if(NOT EXISTS "${SUPERBUILD_DEPENDENCIES_INSTALL_PATH}")
    message(FATAL_ERROR
        "The external dependencies directory '${SUPERBUILD_DEPENDENCIES_INSTALL_PATH}' does not exist.\n"
        "Create it, or set MUSICHIEN_EXTERNAL_DIR to an existing directory.")
endif()

# ---------------------------------------------------------------------------------------------------------------------
# Registration
#
# Each call requires a matching superbuild/CMakeExternals/<name>.cmake file.
# The order matters: a dependency must be registered after the dependencies it needs.
# ---------------------------------------------------------------------------------------------------------------------
set(MUSICHIEN_EXTERNAL_PROJECTS "")

include(musichienFunctionAddExternalProject)
include(musichienFunctionDeleteDirectoryOnSuccess)


musichienFunction_AddExternalProject("GoogleTest-${MUSICHIEN_GOOGLETEST_VERSION}")
musichienFunction_AddExternalProject("NlohmannJson-${MUSICHIEN_NLOHMANN_JSON_VERSION}")

# ---------------------------------------------------------------------------------------------------------------------
# Actual build of the registered dependencies
# ---------------------------------------------------------------------------------------------------------------------
include("${CMAKE_SOURCE_DIR}/superbuild/includeSuperbuildProjects.cmake")
