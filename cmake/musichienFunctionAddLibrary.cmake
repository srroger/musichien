# =====================================================================================================================
# Declares a library module of Musichien.
#
# Usage:
#   musichienFunction_AddLibrary(<moduleName> <sources...>)
#
# The target is named musichien_<moduleName>. The source directory of the project is added to the
# public include path so that headers are always included with an explicit module prefix, e.g.:
#
#   #include <domain/music/Interval.h>
#
# This makes the dependency between modules readable in every single source file.
# =====================================================================================================================
function(musichienFunction_AddLibrary p_moduleName)

    if(ARGC LESS 2)
        message(FATAL_ERROR "musichienFunction_AddLibrary(${p_moduleName}) requires at least one source file.")
    endif()

    set(targetName "musichien_${p_moduleName}")

    add_library(${targetName} ${ARGN})

    musichienFunction_ConfigureTarget(${targetName})

    target_include_directories(${targetName} PUBLIC ${CMAKE_SOURCE_DIR}/source)

    set_target_properties(${targetName} PROPERTIES FOLDER "modules")

    message(STATUS "Musichien: module '${targetName}' declared from ${CMAKE_CURRENT_SOURCE_DIR}")

    # Expose the real target name to the calling scope.
    set(${p_moduleName}_TARGET ${targetName} PARENT_SCOPE)

endfunction()
