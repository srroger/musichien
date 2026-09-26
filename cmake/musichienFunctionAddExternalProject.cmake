# =====================================================================================================================
# Declares an external dependency that can be built by the superbuild.
#
# Usage:
#   musichienFunction_AddExternalProject(<projectName>)
#
# A file superbuild/CMakeExternals/<projectName>.cmake must exist and contain the build rules.
# This function creates a MUSICHIEN_SUPERBUILD_COMPILE_<projectName> cache variable so that the
# developer can pick exactly which dependency to rebuild.
# =====================================================================================================================
function(musichienFunction_AddExternalProject p_projectName)

    # MUSICHIEN_SUPERBUILD_ONLY lets the developer rebuild a single dependency, for instance:
    #   cmake --preset "Superbuild Musichien" -DMUSICHIEN_SUPERBUILD_ONLY=GoogleTest
    #
    # The version suffix is stripped so that the developer never has to type a version number.
    if(DEFINED MUSICHIEN_SUPERBUILD_ONLY)

        string(FIND ${p_projectName} "-" dashIndex REVERSE)

        if(dashIndex GREATER -1)
            string(SUBSTRING ${p_projectName} 0 ${dashIndex} shortName)
        else()
            set(shortName ${p_projectName})
        endif()

        if(MUSICHIEN_SUPERBUILD_ONLY STREQUAL shortName)
            set(compileThisProject ON)
        else()
            set(compileThisProject OFF)
        endif()

        # CACHE INTERNAL is what makes the decision survive the return of this function: a plain
        # set() would be forgotten, and the superbuild would silently build nothing at all.
        set("COMPILE_${p_projectName}" ${compileThisProject} CACHE INTERNAL "Build ${p_projectName}")
        set("MUSICHIEN_SUPERBUILD_COMPILE_${p_projectName}" ${compileThisProject} CACHE BOOL "Build ${p_projectName}")

    else()

        # First configuration: every dependency is enabled, then the value is kept in the cache.
        set("MUSICHIEN_SUPERBUILD_COMPILE_${p_projectName}" ON CACHE BOOL "Build ${p_projectName}")

        # Same reasoning as above: CACHE INTERNAL, otherwise the loop below would skip everything.
        set("COMPILE_${p_projectName}" "${MUSICHIEN_SUPERBUILD_COMPILE_${p_projectName}}" CACHE INTERNAL "Build ${p_projectName}")

    endif()


    message(STATUS "Musichien superbuild: dependency '${p_projectName}' registered")

    set(MUSICHIEN_EXTERNAL_PROJECTS ${MUSICHIEN_EXTERNAL_PROJECTS} ${p_projectName} PARENT_SCOPE)

endfunction()
