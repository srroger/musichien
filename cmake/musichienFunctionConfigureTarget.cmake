# =====================================================================================================================
# Common target configuration
#
# Every target of the project goes through musichienFunction_ConfigureTarget so that the C++ standard,
# the warnings, the static analysis and the IDE folder are never forgotten.
# =====================================================================================================================

option(MUSICHIEN_STRICT_WARNINGS "Add -Wconversion and -Wsign-conversion (noisy, very strict)" OFF)

# ---------------------------------------------------------------------------------------------------------------------
# Applies the warnings of the project to a target.
#
# Qt and third party headers are linked through imported targets, whose include directories are
# treated as SYSTEM by CMake: they never produce warnings of their own.
# ---------------------------------------------------------------------------------------------------------------------
function(musichienFunction_SetWarningFlags p_target)

    set(warningFlags
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
        -Wnull-dereference
        -Wcast-qual
        -Wundef
        -Wredundant-decls
    )

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # -Wlogical-op only exists in GCC; asking clang for it produces its own warning.
        list(APPEND warningFlags -Wlogical-op -fconcepts-diagnostics-depth=4)
    endif()


    if(MUSICHIEN_STRICT_WARNINGS)
        list(APPEND warningFlags -Wconversion -Wsign-conversion)
    endif()

    if(MUSICHIEN_WARNINGS_AS_ERRORS)
        list(APPEND warningFlags -Werror)
    endif()

    target_compile_options(${p_target} PRIVATE ${warningFlags})

endfunction()


# ---------------------------------------------------------------------------------------------------------------------
# Configures a target with every convention of the project.
#
# Usage:
#   musichienFunction_ConfigureTarget(my_target)
# ---------------------------------------------------------------------------------------------------------------------
function(musichienFunction_ConfigureTarget p_target)

    # C++26 is required, not merely requested: the standard is a hard requirement of the project.
    target_compile_features(${p_target} PUBLIC cxx_std_26)

    musichienFunction_SetWarningFlags(${p_target})

    # Position independent code keeps the door open for shared libraries and Android builds.
    set_target_properties(${p_target} PROPERTIES POSITION_INDEPENDENT_CODE ON)

    # FOLDER only affects the way IDEs group the targets.
    set_target_properties(${p_target} PROPERTIES FOLDER ${PROJECT_NAME})

    if(MUSICHIEN_ENABLE_CLANG_TIDY AND MUSICHIEN_CLANG_TIDY_EXECUTABLE)
        set_target_properties(${p_target} PROPERTIES
            CXX_CLANG_TIDY "${MUSICHIEN_CLANG_TIDY_EXECUTABLE};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy")
    endif()

    # Sources are compiled from the project root so that double-clicking an error works in Qt Creator.
    target_include_directories(${p_target} PRIVATE ${CMAKE_SOURCE_DIR})

endfunction()
