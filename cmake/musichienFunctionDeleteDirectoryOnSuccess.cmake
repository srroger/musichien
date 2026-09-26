# =====================================================================================================================
# Deleting the previous installation of a dependency, safely.
#
# Usage:
#   musichienFunction_DeleteDirectoryOnSuccess(<externalProjectName>)
#
# Two steps, on purpose:
#   1. the installation directory is renamed right now, during configuration. A failed build can
#      therefore never leave a half updated dependency in place, silently used by the next build.
#   2. the renamed directory is deleted only once the installation step has succeeded.
# =====================================================================================================================
function(musichienFunction_DeleteDirectoryOnSuccess p_projectName)

    get_filename_component(projectInstallDirectory
        ${SUPERBUILD_DEPENDENCIES_INSTALL_PATH}/${p_projectName} ABSOLUTE)

    if(EXISTS "${projectInstallDirectory}")

        message(STATUS "Musichien superbuild: moving the previous '${p_projectName}' installation aside")

        # Remove a leftover from a previous interrupted run before renaming.
        file(REMOVE_RECURSE "${projectInstallDirectory}_old")
        file(RENAME "${projectInstallDirectory}" "${projectInstallDirectory}_old")

    endif()

    ExternalProject_Add_Step(${p_projectName} Delete-previous-installation
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${projectInstallDirectory}_old"
        COMMENT "Deleting the previous installation of ${p_projectName}"
        DEPENDEES install
    )

endfunction()
