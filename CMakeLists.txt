# =====================================================================================================================
# Musichien - top level CMake entry point
#
# Two configuration modes:
#   * normal build      -> builds the application against the pinned external dependencies
#   * superbuild build  -> builds ONLY the external dependencies we compile from source
#
# The toolchain is never taken from the host by accident: it is pinned by
# scripts/setup_env.sh, through MUSICHIEN_EXTERNAL_DIR and MUSICHIEN_QT_DIR.
# =====================================================================================================================
cmake_minimum_required(VERSION 3.26)

cmake_policy(VERSION 3.26)

project(Musichien VERSION 1.6.3 LANGUAGES C CXX)


# Organise the targets into folders in Qt Creator, Visual Studio and Xcode.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Rename the folder holding CMake predefined targets so that it always comes first in IDEs.
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "_CMakePredefinedTargets")

# =====================================================================================================================
# CMake modules
# =====================================================================================================================
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

include(musichienCheckCompiler)
include(musichienVersions)


# =====================================================================================================================
# Superbuild
#
# When MUSICHIEN_SUPERBUILD is ON, this configuration only builds the external dependencies declared in
# superbuild/CMakeExternals. The application itself is not configured: this configuration is a tool.
# =====================================================================================================================
if(MUSICHIEN_SUPERBUILD)
    include(musichienSuperbuild)
    return()
endif()

# =====================================================================================================================
# Project configuration and dependencies
# =====================================================================================================================
include(musichienConfig)
include(musichienFindDependencies)

if(MUSICHIEN_BUILD_TESTING)
    enable_testing()
endif()

add_subdirectory(source)

# Les outils de developpement : ils ne dependent que du domaine, donc ils n'ont rien a faire dans un paquet Android.
# La meme regle que pour les tests : ce qui ne sert qu'a la machine de developpement n'entre pas dans le telephone.
if(NOT ANDROID)
    add_subdirectory(tools)
endif()
