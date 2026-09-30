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
    music/Temperament.h
    music/Temperament.cpp
    music/TunerGuide.h
    music/TunerGuide.cpp
    music/StaffPosition.h
    music/StaffPosition.cpp
    music/Interval.h
    music/Interval.cpp
    music/Chord.h
    music/Chord.cpp

    # Clavier musical : la gamme majeure, et les sept modes obtenus par rotation de celle-ci
    music/Mode.h
    music/Mode.cpp

    # Le cercle des quintes : les armures, les relatives mineures, et la qualite des degres
    music/KeySignature.h
    music/KeySignature.cpp

    # Une phrase modale, et le generateur qui la propose : des degres, jamais des notes
    music/Phrase.h
    music/Phrase.cpp

    # L'arbre des accords : comment chaque couleur s'obtient d'une autre, en un geste
    music/ChordTree.h
    music/ChordTree.cpp

    # Audio, expressed as a port: the domain says WHAT it needs, never HOW it is done
    audio/NotePlayer.h
    audio/NotePlayerFake.h
    audio/PitchDetector.h
    audio/SungIntervalDetector.h
    audio/SungIntervalDetector.cpp
    audio/ToneSynthesizer.h
    audio/ToneSynthesizer.cpp
    audio/SampledInstrument.h
    audio/SampledInstrument.cpp
    audio/PitchEstimator.h
    audio/PitchEstimator.cpp
    audio/FastFourierTransform.h
    audio/FastFourierTransform.cpp
    audio/DrumSynthesizer.h
    audio/DrumSynthesizer.cpp

    # Rhythm, as pure rules: the metronome's grid and the judgement of a tap, no clock and no sound
    rhythm/Rhythm.h
    rhythm/Rhythm.cpp
    rhythm/MetronomeGrid.h
    rhythm/MetronomeGrid.cpp
    rhythm/RhythmPattern.h
    rhythm/RhythmPattern.cpp

    # The exercise loop, as pure rules: no sound, no clock, no screen
    exercise/AnswerGrid.h
    exercise/AnswerGrid.cpp
    exercise/LearningOrder.h
    exercise/LearningOrder.cpp
    exercise/SessionScore.h
    exercise/SessionScore.cpp
    exercise/ExerciseSession.h
    exercise/ExerciseSession.cpp

    # Le journal des questions conclues, et ce qu'il permet de dire : la fondation des statistiques
    exercise/QuestionRecord.h
    exercise/QuestionRecord.cpp
    exercise/QuestionLog.h
    exercise/QuestionStatistics.h
    exercise/QuestionStatistics.cpp

    # Quand l'application a le droit de se rappeler au joueur : une decision de design, testable
    exercise/ReminderSchedule.h

    # Et quand elle met le Bilan en avant : le week-end, ce qui se discute et se teste aussi
    exercise/Weekend.h

    # The memory hooks, as content the domain holds and the infrastructure fills
    exercise/HintBook.h
    exercise/HintBook.cpp

    # The loading-screen anecdotes, content of the same kind, and the streak rank
    exercise/AnecdoteBook.h
    exercise/AnecdoteBook.cpp
    exercise/Rank.h

    # What the player already knows, and where the answer is remembered
    exercise/PlayerLevel.h
    exercise/PlayerLevel.cpp
    exercise/PlayerPreferences.h
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
        music/Temperament_test.cpp
        music/TunerGuide_test.cpp
        music/StaffPosition_test.cpp
        music/Chord_test.cpp
        music/Mode_test.cpp
        music/KeySignature_test.cpp
        music/Phrase_test.cpp
        music/ChordTree_test.cpp
        audio/ToneSynthesizer_test.cpp
        audio/PitchEstimator_test.cpp
        audio/FastFourierTransform_test.cpp
        audio/SungIntervalDetector_test.cpp
        audio/DrumSynthesizer_test.cpp
        rhythm/Rhythm_test.cpp
        rhythm/MetronomeGrid_test.cpp
        rhythm/RhythmPattern_test.cpp
        exercise/AnswerGrid_test.cpp
        exercise/AnecdoteBook_test.cpp
        exercise/ExerciseSession_test.cpp
        exercise/HintBook_test.cpp
        exercise/LearningOrder_test.cpp
        exercise/PlayerLevel_test.cpp
        exercise/SessionScore_test.cpp
        exercise/QuestionStatistics_test.cpp
        exercise/ReminderSchedule_test.cpp
        exercise/Weekend_test.cpp
    )

endif()

