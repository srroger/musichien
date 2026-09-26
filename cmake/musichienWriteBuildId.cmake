# =====================================================================================================================
# Musichien - identifier of the build
#
# Writes a header holding the identifier of the build being compiled.
#
# ---------------------------------------------------------------------------------------------------------------------
# What the identifier says, and what it does not
#
# It says WHICH COMMIT the binary was compiled from, and that is all. It deliberately does NOT say whether the
# working tree had uncommitted changes: that cannot be kept exact without regenerating - and therefore
# recompiling - on every single edit, and a flag that is right only sometimes is worse than no flag at all.
# "Does this binary match a commit?" is a question for `git status`, which answers it perfectly.
#
# Nor is it 'git describe': that prefixes the identifier with the last TAG, and this repository still carries
# a tag from an older numbering ("v0.2.1-25-g12c9c48"). Displayed next to the application's own version, that
# reads as a contradiction - two numbers, one of them wrong.
#
# ---------------------------------------------------------------------------------------------------------------------
# Why it is generated at BUILD time and not at configure time
#
# A version computed when CMake configures the project is as old as the last configuration: commit three
# times, build, and the application would still claim to be the code of last Tuesday. The whole point of this
# identifier is to answer "what is on my phone right now", so it has to be read at the moment of the build.
#
# The command that calls this script depends on the files git rewrites when the repository moves - HEAD, and
# the branch it points at. The identifier is therefore regenerated exactly when it can have changed, and
# nothing is recompiled when it has not.
# =====================================================================================================================

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${MUSICHIEN_SOURCE_DIR}"
    OUTPUT_VARIABLE musichienBuildId
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE musichienGitResult
)

if(NOT musichienGitResult EQUAL 0)
    # Not a checkout: a source archive, or a machine without git. "unknown" is the honest answer, and a build
    # must never fail over an identifier.
    set(musichienBuildId "unknown")
endif()

set(musichienHeaderContent "// Written by cmake/musichienWriteBuildId.cmake - do not edit.
#pragma once

// Identifier of the build: the commit it was compiled from, plus '-dirty' when the working tree was not clean
// at that moment.
#define MUSICHIEN_BUILD_ID \"${musichienBuildId}\"
")

set(musichienPreviousContent "")

if(EXISTS "${MUSICHIEN_OUTPUT_FILE}")
    file(READ "${MUSICHIEN_OUTPUT_FILE}" musichienPreviousContent)
endif()

if(NOT musichienPreviousContent STREQUAL musichienHeaderContent)
    file(WRITE "${MUSICHIEN_OUTPUT_FILE}" "${musichienHeaderContent}")
    message(STATUS "Musichien: this build is ${musichienBuildId}")
endif()
