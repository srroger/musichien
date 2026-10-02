# =====================================================================================================================
# Musichien - identifier of the build
#
# Writes a header holding the DATE AND TIME at which this binary is being built.
#
# ---------------------------------------------------------------------------------------------------------------------
# What the identifier says, and what it does not
#
# It says WHEN the binary was made, and that is all. Roger l'a demande explicitement : « au lieu de mettre l'id du
# commit, me mettre la date et heure plutot ».
#
# The reason is the one a player actually has. An application installed on a phone can be weeks old, and comparing
# two APKs means asking « which one is the newer? ». A date answers that in one look; a commit identifier answers
# « which code is this? », which is a question for the repository, not for the phone.
#
# The version number is NOT repeated here: it already has a single home - the project() line - and a number written
# twice is a number that lies once.
#
# ---------------------------------------------------------------------------------------------------------------------
# Why it is regenerated on EVERY build
#
# Because the date changes with the build, not with the commit. A value computed only when git moves would claim
# the time of last Tuesday for a binary built today, after three hours of work.
#
# The file is rewritten only when its CONTENT changes - so the minute, not the second. Two builds in the same
# minute therefore recompile nothing, and an unchanged file costs nothing at all.
# =====================================================================================================================

string(TIMESTAMP musichienBuildId "%Y-%m-%d %H:%M")

set(musichienHeaderContent "// Written by cmake/musichienWriteBuildId.cmake - do not edit.
#pragma once

// When this build was made: local date and time, to the minute. It is what a player compares between two APKs.
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
