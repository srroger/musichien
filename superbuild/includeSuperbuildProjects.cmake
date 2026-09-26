# =====================================================================================================================
# Superbuild driver
#
# Iterates over the dependencies registered by musichienFunction_AddExternalProject and includes the
# matching build rules from superbuild/CMakeExternals/.
# =====================================================================================================================

include(ExternalProject)

# Every external project is unarchived, configured, built and installed under this base directory.
set_property(DIRECTORY PROPERTY EP_BASE ext)

# Force the stamps to be regenerated at every configuration.
#
# Consequence: reconfiguring the project always means "rebuild the dependencies", which is exactly
# what we want here: the superbuild is never part of the normal development loop.
execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/ext/Stamp)
message(STATUS "Musichien superbuild: stamp directory cleared, dependencies will be rebuilt")

# ---------------------------------------------------------------------------------------------------------------------
# Build the dependencies registered with the superbuild system
# ---------------------------------------------------------------------------------------------------------------------
foreach(dependency IN LISTS MUSICHIEN_EXTERNAL_PROJECTS)

    if(COMPILE_${dependency})

        message(STATUS "Musichien superbuild: building '${dependency}'")

        # The build rules of superbuild/CMakeExternals use p_externalProjectName as the name of the
        # dependency currently being built. Passing it explicitly avoids relying on a shared loop
        # variable name, which is exactly the kind of implicit coupling that breaks silently.
        set(p_externalProjectName "${dependency}")

        include(${CMAKE_SOURCE_DIR}/superbuild/CMakeExternals/${dependency}.cmake)

        unset(p_externalProjectName)

    else()

        message(STATUS "Musichien superbuild: skipping '${dependency}'")

    endif()

endforeach()

