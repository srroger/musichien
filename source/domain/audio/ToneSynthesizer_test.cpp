#include "domain/audio/ToneSynthesizer.h"

#include "domain/audio/NotePlayerFake.h"
#include "domain/audio/SampledInstrument.h"
#include "domain/music/Note.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <ranges>
#include <span>
#include <vector>

namespace musichien::domain
{

namespace
{

// Sample rate used by every test: the one Android devices use natively.
constexpr std::int32_t TEST_SAMPLE_RATE = 48000;

// Peak level actually present in a buffer.
[[nodiscard]] float peakAmplitudeOf( std::span<const float> p_samples )
{
    if( p_samples.empty() )
    {
        return 0.0F;
    }

    const auto absoluteValues = p_samples | std::views::transform( []( float p_sample ) {
                                    return std::abs( p_sample );
                                } );

    return std::ranges::max( absoluteValues );
}

// Counts how many times the signal crosses zero.
//
// This is how a test verifies the PITCH without any audio hardware: a sine of frequency f played for
// one second crosses zero 2 * f times. It checks the whole chain note -> frequency -> waveform.
[[nodiscard]] std::size_t countZeroCrossings( std::span<const float> p_samples )
{
    if( p_samples.size() < 2 )
    {
        return 0;
    }

    std::size_t crossingCount = 0;

    for( const auto & [previousSample, currentSample] : p_samples | std::views::adjacent<2> )
    {
        if( ( previousSample >= 0.0F ) != ( currentSample >= 0.0F ) )
        {
            ++crossingCount;
        }
    }

    return crossingCount;
}

// Energy of a buffer: what the ear averages, and what the synthesiser now normalises on.
[[nodiscard]] float rmsOf( std::span<const float> p_samples )
{
    if( p_samples.empty() )
    {
        return 0.0F;
    }

    double sumOfSquares = 0.0;

    for( const float sample : p_samples )
    {
        sumOfSquares += static_cast<double>( sample ) * static_cast<double>( sample );
    }

    return static_cast<float>( std::sqrt( sumOfSquares / static_cast<double>( p_samples.size() ) ) );
}

// Frequency of the strongest periodicity of a buffer, found by AUTOCORRELATION.
//
// Counting zero crossings - what these tests did before - worked on a sine and stops working on a struck
// string: every harmonic crosses zero on its own account, so the count no longer says anything about the
// fundamental. Autocorrelation asks the question that actually matters here: "after how many samples does
// the waveform look like itself again?", and the answer IS the period.
//
// The search runs over a window around the expected period, wide enough (plus or minus ten per cent) that
// being right is not assumed, and narrow enough that it cannot lock onto a harmonic instead.
[[nodiscard]] double measuredFrequency( std::span<const float> p_samples, double p_expectedFrequency )
{
    const auto minimumLag = static_cast<std::size_t>( TEST_SAMPLE_RATE / ( p_expectedFrequency * 1.1 ) );
    const auto maximumLag = static_cast<std::size_t>( TEST_SAMPLE_RATE / ( p_expectedFrequency * 0.9 ) );

    // Only the beginning of the note is analysed: enough periods to be certain, and SHORT enough that the
    // decay does not bias the measurement. Over a quarter of a second a note loses several decibels, and a
    // correlation that weights its early samples more heavily drifts towards shorter lags - which reads as a
    // note sharper than it is. Sixty milliseconds is twenty seven periods of A4.
    const std::size_t analysisLength =
      std::min( p_samples.size(), static_cast<std::size_t>( TEST_SAMPLE_RATE ) / 16 );

    const std::span<const float> analysed = p_samples.first( analysisLength );

    // Iterators rather than indices: no arithmetic on positions, and nothing that can leave the buffer.
    const auto correlationAtLag = [analysed]( std::size_t p_lag ) {
        double correlation = 0.0;

        auto first = analysed.begin();
        auto second = analysed.begin() + static_cast<std::ptrdiff_t>( p_lag );

        for( ; second != analysed.end(); ++first, ++second )
        {
            correlation += static_cast<double>( *first ) * static_cast<double>( *second );
        }

        return correlation;
    };

    std::size_t bestLag = minimumLag;

    double bestCorrelation = -1.0;

    for( std::size_t lag = minimumLag; lag <= maximumLag; ++lag )
    {
        const double correlation = correlationAtLag( lag );

        if( correlation > bestCorrelation )
        {
            bestCorrelation = correlation;
            bestLag = lag;
        }
    }

    // Sub-sample refinement, and it is not a luxury: the correlation is only evaluated at whole sample
    // lags, and ONE SAMPLE at 440 Hz is 15 cents. Far too coarse to judge the tuning of an application
    // whose subject is the distance between two notes - the measurement would be less precise than the
    // synth. Fitting a parabola through the three correlations around the peak locates the true maximum to
    // a small fraction of a sample.
    const double correlationBefore = correlationAtLag( bestLag - 1 );
    const double correlationAtBest = correlationAtLag( bestLag );
    const double correlationAfter = correlationAtLag( bestLag + 1 );

    const double curvature = ( correlationBefore - ( 2.0 * correlationAtBest ) ) + correlationAfter;

    const double refinement = ( curvature != 0.0 )
                                ? ( 0.5 * ( correlationBefore - correlationAfter ) / curvature )
                                : 0.0;

    return static_cast<double>( TEST_SAMPLE_RATE ) / ( static_cast<double>( bestLag ) + refinement );
}

// How much energy a buffer holds at one exact frequency: the Goertzel algorithm, which is the discrete
// Fourier transform of a single bin.
//
// This is how a test proves that a note HAS HARMONICS without a sound card, and the whole reason for
// leaving the sine wave behind lives in that measurement. See the sonar note: "rendering a low note
// audible on a phone is a TIMBRE problem, and a sine has no harmonic to hang the ear on".
[[nodiscard]] double energyAtFrequency( std::span<const float> p_samples, double p_frequency )
{
    const double angularFrequency =
      ( 2.0 * std::numbers::pi * p_frequency ) / static_cast<double>( TEST_SAMPLE_RATE );

    const double coefficient = 2.0 * std::cos( angularFrequency );

    double previous = 0.0;
    double previousPrevious = 0.0;

    for( const float sample : p_samples )
    {
        const double current =
          static_cast<double>( sample ) + ( coefficient * previous ) - previousPrevious;

        previousPrevious = previous;
        previous = current;
    }

    return ( previous * previous ) + ( previousPrevious * previousPrevious ) - ( coefficient * previous * previousPrevious );
}

}    // namespace

// ---------------------------------------------------------------------------------------------------------------------
// Length of the rendered buffers
// ---------------------------------------------------------------------------------------------------------------------

TEST( ToneSynthesizerTest, sample_count_matches_the_requested_duration )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    EXPECT_EQ( 0, synthesizer.sampleCountFor( std::chrono::milliseconds{ 0 } ) );
    EXPECT_EQ( 24000, synthesizer.sampleCountFor( std::chrono::milliseconds{ 500 } ) );
    EXPECT_EQ( 48000, synthesizer.sampleCountFor( std::chrono::milliseconds{ 1000 } ) );
}

TEST( ToneSynthesizerTest, rendered_note_has_the_requested_length )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    const std::vector<float> samples =
      synthesizer.renderNote( Note{ 69 }, std::chrono::milliseconds{ 500 } );

    EXPECT_EQ( 24000, samples.size() );
}

// ---------------------------------------------------------------------------------------------------------------------
// The envelope: what removes the click
// ---------------------------------------------------------------------------------------------------------------------

TEST( ToneSynthesizerTest, rendered_note_starts_and_ends_at_silence )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    const std::vector<float> samples =
      synthesizer.renderNote( Note{ 69 }, std::chrono::milliseconds{ 500 } );

    ASSERT_FALSE( samples.empty() );

    // A note starting or stopping on a non zero value is exactly what produces an audible click.
    EXPECT_FLOAT_EQ( 0.0F, samples.front() );
    EXPECT_FLOAT_EQ( 0.0F, samples.back() );
}

TEST( ToneSynthesizerTest, a_note_shorter_than_both_fades_still_has_an_envelope )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    // 20 ms is shorter than the attack (12 ms) plus the release (40 ms) added together.
    const std::vector<float> shortSamples =
      synthesizer.renderNote( Note{ 69 }, std::chrono::milliseconds{ 20 } );

    ASSERT_FALSE( shortSamples.empty() );

    EXPECT_FLOAT_EQ( 0.0F, shortSamples.front() );
    EXPECT_FLOAT_EQ( 0.0F, shortSamples.back() );
}

// ---------------------------------------------------------------------------------------------------------------------
// Loudness: a note and a chord must be comparable by ear
// ---------------------------------------------------------------------------------------------------------------------

TEST( ToneSynthesizerTest, notes_of_the_whole_range_are_heard_at_the_same_level )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    constexpr auto QUARTER_SECOND = std::chrono::milliseconds{ 250 };

    // Three notes, two octaves apart from each other. The synthesiser was rewritten over exactly this
    // defect: with sine waves, the phone speaker made the low one disappear and the high one pierce.
    const auto onsetEnergyOf = [&]( std::int32_t p_midiNumber ) {
        const std::vector<float> samples = synthesizer.renderNote( Note{ p_midiNumber }, QUARTER_SECOND );

        const std::size_t onsetSampleCount = synthesizer.sampleCountFor( ToneSynthesizer::NOTE_ONSET_DURATION );

        return static_cast<double>( rmsOf( std::span<const float>( samples ).first( onsetSampleCount ) ) );
    };

    const double lowEnergy = onsetEnergyOf( 36 );
    const double middleEnergy = onsetEnergyOf( 60 );
    const double highEnergy = onsetEnergyOf( 84 );

    constexpr auto REACHED_TARGET = static_cast<double>( ToneSynthesizer::TARGET_RMS_AMPLITUDE );

    // Within three decibels of each other, and of the target. That is the whole point: the player compares
    // two sounds, so a level difference between notes would be a clue that has nothing to do with the
    // interval being taught.
    EXPECT_NEAR( REACHED_TARGET, lowEnergy, REACHED_TARGET * 0.3 );
    EXPECT_NEAR( REACHED_TARGET, middleEnergy, REACHED_TARGET * 0.3 );
    EXPECT_NEAR( REACHED_TARGET, highEnergy, REACHED_TARGET * 0.3 );
}

TEST( ToneSynthesizerTest, no_note_ever_clips )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    constexpr auto QUARTER_SECOND = std::chrono::milliseconds{ 250 };

    for( const std::int32_t midiNumber : std::views::iota( 21, 109 ) )
    {
        const std::vector<float> samples = synthesizer.renderNote( Note{ midiNumber }, QUARTER_SECOND );

        // Clipping is not a subtlety on a phone speaker, it is distortion - and distortion on a note whose
        // pitch is being judged is worse than no sound at all.
        EXPECT_LE( peakAmplitudeOf( samples ),
                   ToneSynthesizer::MAXIMUM_PEAK_AMPLITUDE + 1.0e-5F )
          << "MIDI " << midiNumber;
    }
}

TEST( ToneSynthesizerTest, a_chord_is_heard_at_the_level_of_a_single_note )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    const std::vector<Note> chord{ Note{ 60 }, Note{ 64 }, Note{ 67 } };

    const std::vector<float> noteSamples =
      synthesizer.renderNote( Note{ 60 }, std::chrono::milliseconds{ 500 } );

    const std::vector<float> chordSamples =
      synthesizer.renderChord( chord, std::chrono::milliseconds{ 500 } );

    // Without normalising the MIX, three voices would be three times louder than one - and the player would
    // hear the loudness instead of the interval.
    //
    // The tolerance is a couple of decibels, and it is honest: three struck strings have taller peaks than
    // one, so the ceiling bites a little earlier on a chord. Being within 2 dB by ear is what "comparable"
    // means here, and clipping in order to be exactly equal would be a great deal worse.
    const auto noteEnergy = static_cast<double>( rmsOf( noteSamples ) );
    const auto chordEnergy = static_cast<double>( rmsOf( chordSamples ) );

    EXPECT_NEAR( noteEnergy, chordEnergy, noteEnergy * 0.35 );

    EXPECT_LE( peakAmplitudeOf( chordSamples ), ToneSynthesizer::MAXIMUM_PEAK_AMPLITUDE + 1.0e-5F );
}

// ---------------------------------------------------------------------------------------------------------------------
// The timbre, and why it is not a sine any more
//
// These two tests are the reason the whole class was rewritten. They measure the very defects the ear
// reported: a low note that disappears on a phone speaker because it carries no harmonic, and a chord that
// hollows itself out because its members are sine waves starting in phase.
// ---------------------------------------------------------------------------------------------------------------------

TEST( ToneSynthesizerTest, a_note_carries_harmonics_above_its_fundamental )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    // MIDI 45 is A2, 110 Hz: low enough that a phone speaker cannot reproduce its fundamental, which is
    // exactly the case the harmonics have to carry.
    const Note lowNote{ 45 };

    const std::vector<float> samples =
      synthesizer.renderNote( lowNote, std::chrono::milliseconds{ 500 } );

    const double fundamentalEnergy = energyAtFrequency( samples, lowNote.frequencyHz() );
    const double secondHarmonicEnergy = energyAtFrequency( samples, lowNote.frequencyHz() * 2.0 );
    const double thirdHarmonicEnergy = energyAtFrequency( samples, lowNote.frequencyHz() * 3.0 );

    // A sine would leave these at the numerical noise floor. The ear reconstructs the missing fundamental
    // FROM them, and that is what makes a low note audible on a small speaker at all.
    EXPECT_GT( secondHarmonicEnergy, fundamentalEnergy * 0.05 );
    EXPECT_GT( thirdHarmonicEnergy, fundamentalEnergy * 0.02 );
}

// The pitch
// ---------------------------------------------------------------------------------------------------------------------

TEST( ToneSynthesizerTest, a_note_sounds_at_the_frequency_it_asks_for )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    // MIDI 69 is A4, the reference note at 440 Hz.
    const Note referenceNote{ 69 };

    const std::vector<float> samples =
      synthesizer.renderNote( referenceNote, std::chrono::milliseconds{ 1000 } );

    // Within a quarter of a per cent, that is under 5 cents. This is an application about the distance
    // between two notes, so a note out of tune is a wrong answer taught to the player.
    EXPECT_NEAR( referenceNote.frequencyHz(),
                 measuredFrequency( samples, referenceNote.frequencyHz() ),
                 referenceNote.frequencyHz() * 0.0025 );
}

TEST( ToneSynthesizerTest, an_octave_higher_sounds_exactly_twice_as_high )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    const Note lowerNote{ 69 };
    const Note upperNote{ 81 };

    const std::vector<float> lowerSamples =
      synthesizer.renderNote( lowerNote, std::chrono::milliseconds{ 500 } );

    const std::vector<float> upperSamples =
      synthesizer.renderNote( upperNote, std::chrono::milliseconds{ 500 } );

    const double lowerFrequency = measuredFrequency( lowerSamples, lowerNote.frequencyHz() );
    const double upperFrequency = measuredFrequency( upperSamples, upperNote.frequencyHz() );

    ASSERT_GT( lowerFrequency, 0.0 );

    // The ratio is what an octave IS. The delay line is read at a fractional position for exactly this
    // reason: rounding the delay to a whole number of samples would detune the top of the range by several
    // cents, and the interval would stop being the interval.
    EXPECT_NEAR( 2.0, upperFrequency / lowerFrequency, 0.01 );
}

TEST( ToneSynthesizerTest, a_note_follows_the_chosen_diapason )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    const Note referenceNote{ 69 };    // A4

    // A diapason of 442 scales every frequency by 442/440, and the synthesiser must follow it: a tuner is only
    // useful if the sound it plays is at the diapason it claims.
    const TuningContext sharpDiapason{ Temperament::Equal, 442.0 };

    const std::vector<float> samples =
      synthesizer.renderNote( referenceNote, std::chrono::milliseconds{ 1000 }, sharpDiapason );

    const double expectedFrequency = referenceNote.frequencyHz() * ( 442.0 / 440.0 );

    EXPECT_NEAR( expectedFrequency,
                 measuredFrequency( samples, expectedFrequency ),
                 expectedFrequency * 0.0025 );
}

TEST( ToneSynthesizerTest, a_pythagorean_fifth_is_pure_in_a_melody )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    const Note root{ 60 };     // C4
    const Note fifth{ 67 };    // G4

    constexpr std::chrono::milliseconds noteDuration{ 500 };
    constexpr std::chrono::milliseconds gap{ 100 };

    const TuningContext pythagorean{ Temperament::Pythagorean, 440.0 };

    const std::vector<Note> melody{ root, fifth };

    const std::vector<float> melodySamples = synthesizer.renderMelody( melody, noteDuration, gap, pythagorean );

    // The second note starts after the first note and its gap.
    const std::size_t secondNoteStart = synthesizer.sampleCountFor( noteDuration ) + synthesizer.sampleCountFor( gap );

    const std::span<const float> allSamples = melodySamples;

    const std::span<const float> secondNote = allSamples.subspan( secondNoteStart );

    const double rootFrequency = frequencyFor( root, root, Temperament::Pythagorean, 440.0 );
    const double fifthFrequency = frequencyFor( fifth, root, Temperament::Pythagorean, 440.0 );

    // A Pythagorean fifth is a chain of pure fifths: 3/2, not the tempered 2^(7/12). The synthesiser must play
    // what the temperament says, otherwise the setting is a lie the ear can hear.
    EXPECT_NEAR( 3.0 / 2.0, fifthFrequency / rootFrequency, 1e-9 );

    EXPECT_NEAR( fifthFrequency,
                 measuredFrequency( secondNote, fifthFrequency ),
                 fifthFrequency * 0.0025 );
}

TEST( ToneSynthesizerTest, the_sine_is_the_pure_fundamental )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    const Note note{ 69 };    // A4

    const std::vector<float> sine =
      synthesizer.renderWaveNote( note, Waveform::Sine, std::chrono::milliseconds{ 1000 } );

    EXPECT_NEAR( note.frequencyHz(),
                 measuredFrequency( sine, note.frequencyHz() ),
                 note.frequencyHz() * 0.0025 );

    // A pure sine has its energy at the fundamental and NONE at the second harmonic: that is the whole point of the
    // instrument, and what makes it the honest tool for hearing a temperament.
    const double fundamentalEnergy = energyAtFrequency( sine, note.frequencyHz() );
    const double secondHarmonicEnergy = energyAtFrequency( sine, note.frequencyHz() * 2.0 );

    EXPECT_GT( fundamentalEnergy, secondHarmonicEnergy * 1000.0 );
}

TEST( ToneSynthesizerTest, the_sawtooth_and_square_carry_the_harmonics_they_should )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    const Note note{ 69 };    // A4

    const std::vector<float> sawtooth =
      synthesizer.renderWaveNote( note, Waveform::Sawtooth, std::chrono::milliseconds{ 1000 } );

    const std::vector<float> square =
      synthesizer.renderWaveNote( note, Waveform::Square, std::chrono::milliseconds{ 1000 } );

    // The sawtooth has EVERY harmonic, including the second.
    const double sawFundamental = energyAtFrequency( sawtooth, note.frequencyHz() );
    const double sawSecond = energyAtFrequency( sawtooth, note.frequencyHz() * 2.0 );

    EXPECT_GT( sawSecond, sawFundamental * 0.01 );

    // The square has only the ODD harmonics: the second is missing, the third is strong.
    const double squareFundamental = energyAtFrequency( square, note.frequencyHz() );
    const double squareSecond = energyAtFrequency( square, note.frequencyHz() * 2.0 );
    const double squareThird = energyAtFrequency( square, note.frequencyHz() * 3.0 );

    EXPECT_LT( squareSecond, squareFundamental * 0.01 );
    EXPECT_GT( squareThird, squareFundamental * 0.01 );
}

// ---------------------------------------------------------------------------------------------------------------------
// Degenerate cases must not crash and must not produce noise
// ---------------------------------------------------------------------------------------------------------------------

TEST( ToneSynthesizerTest, a_note_outside_the_playable_range_produces_silence )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    // MIDI 200 does not exist: the renderer must return silence, not an arbitrary frequency.
    const std::vector<float> samples =
      synthesizer.renderNote( Note{ 200 }, std::chrono::milliseconds{ 200 } );

    ASSERT_FALSE( samples.empty() );

    EXPECT_FLOAT_EQ( 0.0F, peakAmplitudeOf( samples ) );
}

TEST( ToneSynthesizerTest, a_melody_contains_the_silence_between_its_notes )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    const std::vector<Note> melody{ Note{ 60 }, Note{ 67 } };

    constexpr std::chrono::milliseconds noteDuration{ 300 };
    constexpr std::chrono::milliseconds gap{ 100 };

    const std::vector<float> melodySamples = synthesizer.renderMelody( melody, noteDuration, gap );

    const std::size_t expectedLength = melody.size() * ( synthesizer.sampleCountFor( noteDuration ) + synthesizer.sampleCountFor( gap ) );

    EXPECT_EQ( expectedLength, melodySamples.size() );

    // The melody must be longer than the two notes alone, otherwise the gap was not inserted and the
    // two notes would sound like one continuous glide.
    EXPECT_GT( melodySamples.size(), 2 * synthesizer.sampleCountFor( noteDuration ) );
}

// ---------------------------------------------------------------------------------------------------------------------
// The test double itself
// ---------------------------------------------------------------------------------------------------------------------

TEST( NotePlayerFakeTest, records_what_it_is_asked_to_play )
{
    NotePlayerFake player;

    const std::vector<Note> interval{ Note{ 60 }, Note{ 67 } };

    player.playNote( Note{ 60 } );
    player.playMelody( interval, std::chrono::milliseconds{ 120 } );
    player.playChord( interval );
    player.stopAll();

    EXPECT_EQ( 1, player.playedNotes().size() );

    ASSERT_EQ( 1, player.playedMelodies().size() );
    EXPECT_EQ( 2, player.playedMelodies().front().notes.size() );
    EXPECT_EQ( 120, player.playedMelodies().front().gap.count() );

    ASSERT_EQ( 1, player.playedChords().size() );

    EXPECT_EQ( 1, player.stopCount() );

    // The cue is not an interval: it is counted on its own, precisely so that a test can tell "the player
    // heard the answer again" from "the player heard that they were wrong".
    player.playMistakeCue();

    EXPECT_EQ( 1, player.mistakeCueCount() );
}

// ---------------------------------------------------------------------------------------------------------------------
// The cue that marks a mistake
//
// What it SOUNDS like is the adapter's business. What the domain has to guarantee is that it carries no
// pitch - the whole reason a mistake is signalled by noise rather than by a note - and that it stays out
// of the way of the sounds being taught.
// ---------------------------------------------------------------------------------------------------------------------

TEST( ToneSynthesizerTest, the_mistake_cue_is_noise_and_not_a_note )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    const std::vector<float> cue = synthesizer.renderMistakeCue( ToneSynthesizer::MISTAKE_CUE_DURATION );

    ASSERT_FALSE( cue.empty() );

    // A tone crosses zero twice per period. The comparison uses the HIGHEST note the game ever plays, C6
    // at 1046 Hz, so that the margin does not quietly depend on a pitch being low: over the length of
    // this cue that tone would cross zero around 190 times, while noise crosses it on about half of its
    // samples. Three times the worst case leaves no doubt about what this is.
    constexpr double HIGHEST_PLAYED_FREQUENCY = 1046.5;

    const double durationInSeconds =
      std::chrono::duration<double>( ToneSynthesizer::MISTAKE_CUE_DURATION ).count();

    const double highestToneCrossings = 2.0 * HIGHEST_PLAYED_FREQUENCY * durationInSeconds;

    EXPECT_GT( static_cast<double>( countZeroCrossings( cue ) ), 3.0 * highestToneCrossings );
}

TEST( ToneSynthesizerTest, the_mistake_cue_is_quieter_than_a_note_and_dies_out )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    const std::vector<float> cue = synthesizer.renderMistakeCue( ToneSynthesizer::MISTAKE_CUE_DURATION );

    const std::vector<float> note = synthesizer.renderNote( Note{ 69 }, std::chrono::milliseconds{ 500 } );

    const std::size_t onsetSampleCount = synthesizer.sampleCountFor( ToneSynthesizer::NOTE_ONSET_DURATION );

    // Compared on the same thing: the cue in full - it lasts 90 ms, it has no other part - against the
    // attack of a note. Noise at the same energy as a pitched sound is heard much louder, so the cue has to
    // be well below it.
    EXPECT_LT( rmsOf( cue ), rmsOf( std::span<const float>( note ).first( onsetSampleCount ) ) );

    // It stays a punctuation mark all the same: loud enough to be noticed without being looked for.
    EXPECT_GT( peakAmplitudeOf( cue ), 0.15F );

    // And it ends in silence, which is what keeps the end of the burst from being a click of its own.
    ASSERT_GE( cue.size(), 2 );

    EXPECT_LT( std::abs( cue.back() ), 0.001F );
}

TEST( ToneSynthesizerTest, the_mistake_cue_is_the_same_every_time )
{
    const ToneSynthesizer synthesizer{ TEST_SAMPLE_RATE };

    // The seed is fixed. A cue that changed at every mistake would be impossible to recognise, and
    // recognition is the only thing a cue is for - besides which, an unpredictable buffer cannot be
    // asserted.
    EXPECT_EQ( synthesizer.renderMistakeCue( ToneSynthesizer::MISTAKE_CUE_DURATION ),
               synthesizer.renderMistakeCue( ToneSynthesizer::MISTAKE_CUE_DURATION ) );
}

TEST( SampledInstrumentTest, the_sampler_follows_the_temperament )
{
    // A synthetic C4: a pure sine at the equal-temperament frequency of C4. The sampler will transpose it by playing
    // it faster or slower, and the temperament decides how far.
    SampledNote recorded;
    recorded.rootMidiNumber = 60;
    recorded.sampleRate = TEST_SAMPLE_RATE;

    const double c4Hz = Note{ 60 }.frequencyHz();

    recorded.samples.reserve( static_cast<std::size_t>( TEST_SAMPLE_RATE ) );

    for( const std::size_t sampleIndex : std::views::iota( std::size_t{ 0 }, static_cast<std::size_t>( TEST_SAMPLE_RATE ) ) )
    {
        recorded.samples.push_back( static_cast<float>(
          std::sin( 2.0 * std::numbers::pi * c4Hz * static_cast<double>( sampleIndex ) / static_cast<double>( TEST_SAMPLE_RATE ) ) ) );
    }

    SampledInstrument instrument;
    instrument.addNote( std::move( recorded ) );

    // A fifth, played as a melody, in PYTHAGOREAN tuning: the second note (G4) must be 3/2 of C4, not the tempered
    // 2^(7/12). That is the whole question the sampler must answer, and it answers it by reading the sample at the
    // speed that reaches the temperament's frequency.
    const std::vector<Note> melody{ Note{ 60 }, Note{ 67 } };

    constexpr std::chrono::milliseconds noteDuration{ 500 };
    constexpr std::chrono::milliseconds gap{ 100 };

    const std::vector<float> pythagorean = instrument.renderMelody( melody,
                                                                    noteDuration,
                                                                    gap,
                                                                    TEST_SAMPLE_RATE,
                                                                    TuningContext{ Temperament::Pythagorean, 440.0 } );

    const std::size_t noteSampleCount = static_cast<std::size_t>( TEST_SAMPLE_RATE ) / 2;    // 500 ms
    const std::size_t gapSampleCount = static_cast<std::size_t>( TEST_SAMPLE_RATE ) / 10;    // 100 ms

    const std::span<const float> allSamples = pythagorean;

    const std::span<const float> secondNote = allSamples.subspan( noteSampleCount + gapSampleCount );

    const double rootFrequency = frequencyFor( Note{ 60 }, Note{ 60 }, Temperament::Pythagorean, 440.0 );
    const double fifthFrequency = frequencyFor( Note{ 67 }, Note{ 60 }, Temperament::Pythagorean, 440.0 );

    // The rule the temperament states: a Pythagorean fifth is 3/2.
    EXPECT_NEAR( 3.0 / 2.0, fifthFrequency / rootFrequency, 1e-9 );

    // And the SAMPLER renders it at that frequency - not at the tempered one.
    EXPECT_NEAR( fifthFrequency,
                 measuredFrequency( secondNote, fifthFrequency ),
                 fifthFrequency * 0.0025 );
}

}    // namespace musichien::domain
