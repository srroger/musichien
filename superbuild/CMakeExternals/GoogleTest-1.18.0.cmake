# =====================================================================================================================
# GoogleTest / GoogleMock
#
# Unit test framework of the project. Built from source so that the version never depends on the
# machine, and so that a contributor cannot "accidentally" test against another version.
#
# Installed into: ${MUSICHIEN_EXTERNAL_DIR}/GoogleTest-<version>
# =====================================================================================================================

ExternalProject_Add(${proj}

    GIT_REPOSITORY  https://github.com/google/googletest.git
    GIT_TAG         ${MUSICHIEN_GOOGLETEST_GIT_TAG}
    GIT_SHALLOW     TRUE

    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX:PATH=${SUPERBUILD_DEPENDENCIES_INSTALL_PATH}/${proj}
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_C_COMPILER:STRING=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER}
        -DCMAKE_CXX_STANDARD=17
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DBUILD_GMOCK:BOOL=ON
        -DINSTALL_GTEST:BOOL=ON
        -DGTEST_HAS_PTHREAD:BOOL=ON
        -Dgtest_build_samples:BOOL=OFF
        -Dgtest_build_tests:BOOL=OFF
        -Dgtest_disable_pthreads:BOOL=OFF

    BUILD_COMMAND
        ${CMAKE_COMMAND} --build .

    INSTALL_COMMAND
        ${CMAKE_COMMAND} --build . --target install

    LOG_DOWNLOAD  1
    LOG_CONFIGURE 1
    LOG_BUILD     1
    LOG_INSTALL   1
)

# Keep the installation directory in a clean state: the previous one is renamed now and deleted once
# the new installation has succeeded.
musichienFunction_DeleteDirectoryOnSuccess(${proj})
