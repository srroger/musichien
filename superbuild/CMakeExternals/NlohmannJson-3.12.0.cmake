# =====================================================================================================================
# nlohmann/json
#
# Header only JSON library. Used for:
#   * the save file (avatar, experience, chapter progress, spaced repetition)
#   * the content files (chapters, exercises, rewards) - see docs/ARCHITECTURE.md
#
# Header only means there is nothing to compile: the dependency is downloaded and its include
# directory is installed as is. Doing it through the superbuild keeps the version pinned.
#
# Installed into: ${MUSICHIEN_EXTERNAL_DIR}/NlohmannJson-<version>
# =====================================================================================================================

ExternalProject_Add(${p_externalProjectName}

    GIT_REPOSITORY  https://github.com/nlohmann/json.git
    GIT_TAG         ${MUSICHIEN_NLOHMANN_JSON_GIT_TAG}
    GIT_SHALLOW     TRUE
    CONFIGURE_COMMAND ""
    BUILD_COMMAND     ""

    INSTALL_COMMAND
        ${CMAKE_COMMAND} -E make_directory ${SUPERBUILD_DEPENDENCIES_INSTALL_PATH}/${p_externalProjectName}
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                <SOURCE_DIR>/include
                ${SUPERBUILD_DEPENDENCIES_INSTALL_PATH}/${p_externalProjectName}/include
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                <SOURCE_DIR>/LICENSE.MIT
                ${SUPERBUILD_DEPENDENCIES_INSTALL_PATH}/${p_externalProjectName}/LICENSE.MIT

    LOG_DOWNLOAD  1
    LOG_INSTALL   1
)

# Keep the installation directory in a clean state: the previous one is renamed now and deleted once
# the new installation has succeeded.
musichienFunction_DeleteDirectoryOnSuccess(${p_externalProjectName})
