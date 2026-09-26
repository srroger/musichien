# =====================================================================================================================
# Project configuration
#
# Sets the global conventions (output directories, Qt code generators, warnings) and declares the
# user options of the project.
# =====================================================================================================================

# ---------------------------------------------------------------------------------------------------------------------
# External dependencies location
#
# Resolution order:
#   1. the CMake cache variable, if the user provided it
#   2. the MUSICHIEN_EXTERNAL_DIR environment variable, exported by scripts/setup_env.sh
#   3. the sibling folder ../Roger-externals, following the convention of the other projects
# ---------------------------------------------------------------------------------------------------------------------
if(NOT MUSICHIEN_EXTERNAL_DIR)
    if(DEFINED ENV{MUSICHIEN_EXTERNAL_DIR})
        set(MUSICHIEN_EXTERNAL_DIR $ENV{MUSICHIEN_EXTERNAL_DIR})
    else()
        get_filename_component(MUSICHIEN_EXTERNAL_DIR ${CMAKE_SOURCE_DIR}/../Roger-externals ABSOLUTE)
        message(WARNING
            "MUSICHIEN_EXTERNAL_DIR is not defined. Falling back to '${MUSICHIEN_EXTERNAL_DIR}'.\n"
            "Run 'source scripts/setup_env.sh' before configuring the project.")
    endif()
endif()

message(STATUS "Musichien: external dependencies directory is '${MUSICHIEN_EXTERNAL_DIR}'")

# =====================================================================================================================
# User options
# =====================================================================================================================
option(MUSICHIEN_BUILD_SHARED_LIBS      "Build the project libraries as shared libraries"     ON)
option(MUSICHIEN_BUILD_TESTING          "Build the unit tests"                                ON)
option(MUSICHIEN_ENABLE_CLANG_TIDY      "Run clang-tidy while compiling"                      ON)
option(MUSICHIEN_WARNINGS_AS_ERRORS     "Treat compiler warnings as errors"                   OFF)
option(MUSICHIEN_ENABLE_QML_INSPECTION  "Enable QML debugging and profiling over a TCP port"  ON)
option(MUSICHIEN_ENABLE_ADDRESS_SANITIZER "Enable the Address sanitizer (Debug builds only)"  OFF)

# ---------------------------------------------------------------------------------------------------------------------
# Qt code generators
#
# AUTOMOC -> turns Q_OBJECT / Q_PROPERTY / Q_INVOKABLE into generated C++ (the moc preprocessor)
# AUTORCC -> embeds .qrc resources into the binary
# AUTOUIC -> turns .ui files into C++ (unused, but harmless and convenient if a .ui appears)
# ---------------------------------------------------------------------------------------------------------------------
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ---------------------------------------------------------------------------------------------------------------------
# Compilation database: required by clangd, clang-tidy and most editors
# ---------------------------------------------------------------------------------------------------------------------
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------------------------------------------------
# Build output
#
# Every executable, library and test lands in one single directory. This is what lets the application
# find the Qt plugins and its resources without any environment tweaking.
# ---------------------------------------------------------------------------------------------------------------------
set(MUSICHIEN_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

foreach(configuration IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${configuration} ${MUSICHIEN_OUTPUT_DIRECTORY})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${configuration} ${MUSICHIEN_OUTPUT_DIRECTORY})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${configuration} ${MUSICHIEN_OUTPUT_DIRECTORY})
endforeach()

# Same idea for single-configuration generators (Ninja, Makefiles).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${MUSICHIEN_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${MUSICHIEN_OUTPUT_DIRECTORY})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${MUSICHIEN_OUTPUT_DIRECTORY})

# Make generated sources (moc, uic, rcc) reachable with #include "ui_*.h" and friends.
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# ---------------------------------------------------------------------------------------------------------------------
# Library kind
# ---------------------------------------------------------------------------------------------------------------------
set(BUILD_SHARED_LIBS ${MUSICHIEN_BUILD_SHARED_LIBS})

# ---------------------------------------------------------------------------------------------------------------------
# QML debugging: enables the QML inspector over a TCP port
# ---------------------------------------------------------------------------------------------------------------------
if(MUSICHIEN_ENABLE_QML_INSPECTION)
    add_compile_definitions(QT_QML_DEBUG)
endif()

# ---------------------------------------------------------------------------------------------------------------------
# clang-tidy: applied to every target through the musichienFunction_* helpers
# ---------------------------------------------------------------------------------------------------------------------
if(MUSICHIEN_ENABLE_CLANG_TIDY)
    find_program(MUSICHIEN_CLANG_TIDY_EXECUTABLE NAMES clang-tidy)
    if(MUSICHIEN_CLANG_TIDY_EXECUTABLE)
        message(STATUS "Musichien: clang-tidy found at ${MUSICHIEN_CLANG_TIDY_EXECUTABLE}")
    else()
        message(WARNING "Musichien: clang-tidy was requested but not found; the check will be skipped.")
        set(MUSICHIEN_ENABLE_CLANG_TIDY OFF)
    endif()
endif()

# ---------------------------------------------------------------------------------------------------------------------
# Sanitizers
# ---------------------------------------------------------------------------------------------------------------------
if(MUSICHIEN_ENABLE_ADDRESS_SANITIZER)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
    message(STATUS "Musichien: Address sanitizer enabled")
endif()

# =====================================================================================================================
# Helper functions shared by every target
# =====================================================================================================================
include(musichienFunctionConfigureTarget)
include(musichienFunctionAddLibrary)
include(musichienFunctionAddTest)


