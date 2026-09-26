# =====================================================================================================================
# Declares a unit test executable based on GoogleTest.
#
# Usage:
#   musichienFunction_AddTest(<testName> <librariesToLink...> <sources...>)
#
# The target is named test_<testName> and is registered with CTest, so that it can be run with:
#
#   ctest --preset "CTest Debug Musichien"
#
# A test file follows the naming convention <ClassName>_test.cpp and lives next to the class it
# tests. See docs/CODE_CONVENTIONS.md.
# =====================================================================================================================
function(musichienFunction_AddTest p_testName)

    if(ARGC LESS 3)
        message(FATAL_ERROR
            "musichienFunction_AddTest(${p_testName}) requires at least one library and one source file.")
    endif()

    set(targetName "test_${p_testName}")

    # The first argument is the library under test, the rest are the sources.
    list(GET ARGN 0 libraryUnderTest)
    list(REMOVE_AT ARGN 0)
    set(testSources ${ARGN})

    add_executable(${targetName} ${testSources})

    musichienFunction_ConfigureTarget(${targetName})

    target_link_libraries(${targetName} PRIVATE
        ${libraryUnderTest}
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
    )

    set_target_properties(${targetName} PROPERTIES FOLDER "tests")

    # clang-tidy is deliberately disabled on test targets.
    #
    # Reason: GoogleTest generates class names out of the TEST() macro arguments, and those names can
    # never satisfy the naming rules of the project. Running the checks on tests would only produce
    # noise, never a real finding. Tests are still compiled with every warning enabled.
    set_target_properties(${targetName} PROPERTIES CXX_CLANG_TIDY "")


    add_test(NAME ${targetName} COMMAND ${targetName})

    # A unit test that hangs is a bug: it must never block the whole build.
    set_tests_properties(${targetName} PROPERTIES TIMEOUT 120)

    message(STATUS "Musichien: test '${targetName}' declared from ${CMAKE_CURRENT_SOURCE_DIR}")

endfunction()
