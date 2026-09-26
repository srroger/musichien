# =====================================================================================================================
# Module: domain
#
# The musical heart of Musichien: notes, intervals, scales, chords, exercise generation, scoring,
# spaced repetition.
#
# HARD RULE: this module must never depend on Qt, on the file system, on the network or on the
# audio hardware. Everything enters and leaves through plain C++ types.
#
# Consequence: it is testable on the development machine, in milliseconds, without a phone.
# =====================================================================================================================

musichienFunction_AddLibrary(domain
    # Music theory primitives
    music/Note.h
    music/Note.cpp
    music/Interval.h
    music/Interval.cpp

    # Audio, expressed as a port: the domain says WHAT it needs, never HOW it is done
    audio/NotePlayer.h
    audio/NotePlayerFake.h
    audio/ToneSynthesizer.h
    audio/ToneSynthesizer.cpp
)

# Expose the module to the rest of the project under a stable name.
target_include_directories(musichien_domain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# ---------------------------------------------------------------------------------------------------------------------
# Unit tests
#
# Note that these tests link NOTHING but the domain and GoogleTest: no Qt, no sound card, no device.
# That is the concrete payoff of keeping the domain free of any external dependency.
# ---------------------------------------------------------------------------------------------------------------------
if(MUSICHIEN_BUILD_TESTING)

    musichienFunction_AddTest(domain
        musichien_domain
        music/Interval_test.cpp
        audio/ToneSynthesizer_test.cpp
    )

endif()

