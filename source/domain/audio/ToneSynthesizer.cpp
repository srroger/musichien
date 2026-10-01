#include "domain/audio/ToneSynthesizer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <numbers>
#include <numeric>
#include <random>
#include <ranges>
#include <span>

namespace musichien::domain
{

namespace
{

// Seed of the noise of the mistake cue.
//
// Fixed, and deliberately so: the domain owns no entropy source of its own, and the same cue must come
// out of every run and every machine. A cue that changed each time would also be impossible to test.
constexpr std::uint32_t MISTAKE_CUE_SEED = 20260926U;

// How fast the noise burst dies out, as an exponent applied over its whole length.
//
// Chosen so that the end of the burst is around a thousandth of its start: silence, to the ear, without
// needing a separate fade. The envelope is applied on top of it all the same - see renderMistakeCue.
constexpr double MISTAKE_CUE_DECAY = 6.0;

// How much the hammer lets go over the length of the string it strikes.
//
// It hits at full strength and eases off, which is what makes a strike rather than a flat burst of noise.
constexpr double HAMMER_RELEASE = 1.2;

// How many times the averaging filter is applied on every round trip.
//
// One stage is a slightly dark string; two make a much softer one, because each further stage takes the top
// off the harmonics that are still alive. The brightness of a struck string lives entirely in this number
// and in how the hammer is shaped: one stage is a thin, metallic string, four make a soft and round one.
constexpr std::size_t LOOP_FILTER_STAGES = 4;

// Latency, in samples, of the averaging filter inside the loop.
//
// The filter delays the wave by HALF A SAMPLE per stage, and that delay is part of the period the loop
// produces. Forgetting it detunes every note, and detunes the HIGH ones more than the low ones - which makes
// an octave slightly WIDER than an octave, a fifth slightly wider than a fifth. On an application whose
// subject is the distance between two notes, that is not a rounding error, it is a wrong answer taught to
// the player. Subtracting it is what keeps the tuning exact.
//
// DERIVED from the number of stages above, and never written by hand: adding a stage without moving this
// would silently break the tuning of the whole instrument.
constexpr double LOOP_FILTER_DELAY_SAMPLES = 0.5 * static_cast<double>( LOOP_FILTER_STAGES );

// How the HAMMER is shaped before it strikes the string: a one pole low pass, in hertz.
//
// This is the single most important number for how the instrument sounds, and the reason is worth writing
// down. The loop filter in the string only takes the top off the harmonics AS THE NOTE RINGS; the first
// milliseconds keep whatever spectrum the hammer brought in. And a two point average - the obvious way to
// soften noise - barely attenuates anything below a quarter of the sample rate, so it leaves the 1 to 8 kHz
// band exactly where it was. That band is what "aigu et numérique" describes.
//
// A real hammer settles this by itself: felt compresses and lets go over a millisecond or so, which caps how
// fast the string can be pushed. 1200 Hz is that behaviour, and a soft mallet besides.
//
// It sits OUTSIDE the loop, which matters: shaping the excitation cannot move the pitch by even a cent.
constexpr double HAMMER_CUTOFF_HZ = 500.0;

// Ratio between two frequencies a given number of cents apart.
[[nodiscard]] double centsToRatio( double p_cents )
{
    return std::pow( 2.0, p_cents / 1200.0 );
}

// How long a string of a given frequency takes to fall by 60 dB.
//
// Nearly the same everywhere in the range, and that is deliberate. A real string does decay faster in the
// treble, but the loop filter below already takes care of that - it attenuates the high notes on every
// round trip, and a high note makes many more round trips per second. Making the loss depend on the
// frequency on TOP of that would leave the top of the range ringing for a tenth of a second.
[[nodiscard]] double decayTimeFor( double p_frequency )
{
    constexpr double DECAY_AT_REFERENCE_SECONDS = 3.0;
    constexpr double REFERENCE_FREQUENCY_HZ = 220.0;
    constexpr double DECAY_EXPONENT = 0.15;

    const double decaySeconds =
      DECAY_AT_REFERENCE_SECONDS * std::pow( REFERENCE_FREQUENCY_HZ / p_frequency, DECAY_EXPONENT );

    return std::clamp( decaySeconds, 1.5, 4.5 );
}

// Seed of the hammer of one string of one note.
//
// Spread across the whole range, so that two different notes never share a hammer. Two notes of an
// interval that started on the SAME noise would add up on the attack and then cancel each other out -
// exactly what this model exists to avoid. The stride is Knuth's constant: an odd number that spreads
// consecutive MIDI numbers far apart in the seed space.
[[nodiscard]] std::uint32_t stringSeedFor( const Note & p_note, std::size_t p_stringIndex )
{
    constexpr std::uint32_t SEED_STRIDE_PER_NOTE = 2654435761U;

    return ( static_cast<std::uint32_t>( p_note.midiNumber() ) * SEED_STRIDE_PER_NOTE ) + static_cast<std::uint32_t>( p_stringIndex ) + 1U;
}

}    // namespace

ToneSynthesizer::ToneSynthesizer( std::int32_t p_sampleRate )
  : m_sampleRate{ p_sampleRate }
{
}

std::size_t ToneSynthesizer::sampleCountFor( std::chrono::milliseconds p_duration ) const noexcept
{
    if( ( m_sampleRate <= 0 ) || ( p_duration.count() <= 0 ) )
    {
        return 0;
    }

    const double durationInSeconds =
      std::chrono::duration_cast<std::chrono::duration<double>>( p_duration ).count();

    const double exactSampleCount = durationInSeconds * static_cast<double>( m_sampleRate );

    return static_cast<std::size_t>( std::llround( exactSampleCount ) );
}

std::vector<float> ToneSynthesizer::renderNote( const Note & p_note,
                                                std::chrono::milliseconds p_duration,
                                                TuningContext p_tuning ) const
{
    // A note on its own is heard FROM itself: its root is the note. Equal temperament ignores the root anyway.
    return renderNoteAt( p_note,
                         frequencyFor( p_note,
                                       p_note,
                                       p_tuning.temperament,
                                       p_tuning.referencePitchHz ),
                         p_duration );
}

std::vector<float> ToneSynthesizer::renderNoteAt( const Note & p_note,
                                                  double p_frequencyHz,
                                                  std::chrono::milliseconds p_duration ) const
{
    const std::size_t sampleCount = sampleCountFor( p_duration );

    std::vector<float> samples( sampleCount, 0.0F );

    if( ( sampleCount == 0 ) || !p_note.isValid() )
    {
        return samples;
    }

    const double frequency = p_frequencyHz;

    // The strings of the note, tuned a hair apart around the true frequency.
    //
    // The MIDDLE one is exactly right, and that detail matters more here than anywhere else: the player
    // is asked to hear an INTERVAL, so a note that drifted would move the interval with it. The outer
    // strings only add body, and their beat is far too slow to be heard as a wrong pitch.
    for( std::size_t stringIndex = 0; stringIndex < STRING_COUNT; ++stringIndex )
    {
        const double offsetInStrings = static_cast<double>( stringIndex ) - ( static_cast<double>( STRING_COUNT - 1 ) / 2.0 );

        mixStruckStringInto( samples,
                             frequency * centsToRatio( offsetInStrings * STRING_DETUNE_CENTS ),
                             stringSeedFor( p_note, stringIndex ) );
    }

    // A strike, not a fade: see STRIKE_ATTACK_DURATION. The release, on the other hand, is the same as
    // everywhere else, because the note is cut by the clock rather than by the string dying.
    applyEnvelope( samples, STRIKE_ATTACK_DURATION, RELEASE_DURATION );

    normaliseOnsetEnergyTo( samples, TARGET_RMS_AMPLITUDE, NOTE_ONSET_DURATION );

    return samples;
}

void ToneSynthesizer::mixStruckStringInto( std::span<float> p_samples,
                                           double p_frequency,
                                           std::uint32_t p_seed ) const
{
    // Length of the delay line, in samples: one round trip of the wave is one period.
    //
    // ROUNDED UP, and this is the subtlety that costs a quarter tone if it is got wrong. Reading the line
    // with linear interpolation at (1 - f) * line[i] + f * line[i + 1] does not ADD f samples of delay: the
    // second tap is the YOUNGER sample, so the interpolated read is f samples EARLIER than the line. The
    // delay a loop can produce is therefore in [N - 1, N], and the only way to land between two samples is
    // to take N = ceil(L) and f = N - L. Done the other way round, every note is sharp - and the sharpness
    // grows with the pitch, which quietly stretches every interval in the treble.
    const double exactDelayLength =
      ( static_cast<double>( m_sampleRate ) / p_frequency ) - LOOP_FILTER_DELAY_SAMPLES;

    if( exactDelayLength < 4.0 )
    {
        return;
    }

    const auto delayLength = static_cast<std::size_t>( std::ceil( exactDelayLength ) );

    const auto delayFraction =
      static_cast<float>( static_cast<double>( delayLength ) - exactDelayLength );

    std::vector<float> delayLine( delayLength, 0.0F );

    // The hammer: a burst of noise, shaped so that it strikes and lets go.
    std::mt19937 noiseEngine{ p_seed };

    std::uniform_real_distribution<float> amplitudeDistribution{ -1.0F, 1.0F };

    // The hammer strikes the WHOLE string, not one point of it: the excitation covers the delay line from
    // end to end.
    //
    // Filling the line matters far more than it looks. Exciting only its very beginning - three
    // milliseconds of noise followed by silence - produces a huge transient followed by a much quieter
    // sustain, and the ceiling that prevents clipping then has to crush the whole note to keep that first
    // moment inside the range. The note comes out several times too quiet.
    //
    // A long bass string genuinely does take several milliseconds to be struck and to let go, so covering
    // the whole line is also the more honest model: about a millisecond for the top of the range, about
    // ten for the bottom, which is exactly what a piano hammer does.
    for( const std::size_t sampleIndex : std::views::iota( std::size_t{ 0 }, delayLength ) )
    {
        const double progress = static_cast<double>( sampleIndex ) / static_cast<double>( delayLength );

        const auto shape = static_cast<float>( std::exp( -HAMMER_RELEASE * progress ) );

        delayLine.at( sampleIndex ) = amplitudeDistribution( noiseEngine ) * shape;
    }

    // The excitation must carry NO continuous component.
    //
    // Noise drawn over a few hundred samples is not centred by chance, and the loop passes a continuous
    // component almost without loss: a fraction of a per cent of offset would stay there for the whole
    // note. It eats the headroom the ceiling needs - the note then comes out several times too quiet - and
    // a phone speaker answers a continuous offset with a thump. One pass over the line removes it.
    const auto meanExcitation =
      std::accumulate( delayLine.begin(), delayLine.end(), 0.0 ) / static_cast<double>( delayLength );

    std::ranges::for_each( delayLine, [meanExcitation]( float & p_sample ) {
        p_sample -= static_cast<float>( meanExcitation );
    } );

    // And it is low passed, once, with a real filter.
    //
    // Three reasons that all point the same way: it softens the hammer (see HAMMER_CUTOFF_HZ, which is where
    // "doux" is decided); it lowers the crest factor, which is what limits how loud the note can be, since the
    // ceiling below is set by the tallest peak; and a spiky attack therefore forces different notes to be
    // quieter by different amounts, which is exactly the loudness difference this model exists to remove.
    //
    // A one pole filter and not a two point average: the average only bites near half the sample rate, and the
    // brightness that has to go is one octave lower than that.
    {
        const auto filterCoefficient = static_cast<float>(
          std::exp( -2.0 * std::numbers::pi * HAMMER_CUTOFF_HZ / static_cast<double>( m_sampleRate ) ) );

        float previousExcitation = 0.0F;

        std::ranges::for_each( delayLine, [&previousExcitation, filterCoefficient]( float & p_sample ) {
            previousExcitation = ( ( 1.0F - filterCoefficient ) * p_sample ) + ( filterCoefficient * previousExcitation );

            p_sample = previousExcitation;
        } );
    }

    // The loss on every round trip, derived from the time the string takes to fall by 60 dB.
    const auto loopLoss = static_cast<float>(
      std::pow( 10.0, -3.0 / ( decayTimeFor( p_frequency ) * p_frequency ) ) );

    std::size_t readIndex = 0;

    // What each stage of the loop filter produced on the PREVIOUS sample, which is what makes the average a
    // low pass rather than a plain sum.
    std::array<float, LOOP_FILTER_STAGES> previousStageOutputs{};

    for( float & sample : p_samples )
    {
        const std::size_t nextIndex = ( readIndex + 1 == delayLength ) ? 0 : readIndex + 1;

        const float currentSample = delayLine.at( readIndex );

        // The fractional read: the delay is a real number, the buffer is an array.
        const float delayedSample =
          currentSample + ( delayFraction * ( delayLine.at( nextIndex ) - currentSample ) );

        // The loop filter: LOOP_FILTER_STAGES passes of a two point average, each of which is a low pass. It
        // is what makes the HIGH harmonics die first, and therefore the whole reason the result sounds struck
        // rather than held - and how many passes there are is what decides how soft it sounds.
        float stageInput = delayedSample;

        for( float & previousStageOutput : previousStageOutputs )
        {
            const float stageOutput = 0.5F * ( stageInput + previousStageOutput );

            previousStageOutput = stageInput;
            stageInput = stageOutput;
        }

        delayLine.at( readIndex ) = loopLoss * stageInput;

        sample += delayedSample;

        readIndex = nextIndex;
    }
}

std::vector<float> ToneSynthesizer::renderChord( std::span<const Note> p_notes,
                                                 std::chrono::milliseconds p_duration,
                                                 TuningContext p_tuning ) const
{
    const std::size_t sampleCount = sampleCountFor( p_duration );

    std::vector<float> mixedSamples( sampleCount, 0.0F );

    if( ( sampleCount == 0 ) || p_notes.empty() )
    {
        return mixedSamples;
    }

    // The chord is heard FROM its first note: in a non-equal temperament, that first note is what gives every other
    // note of the chord its meaning.
    const Note root = p_notes.front();

    // Every note is rendered on its own and then added. Mixing this way keeps the code readable, and
    // a defect in one voice cannot stay invisible because another voice masked it.
    for( const Note & note : p_notes )
    {
        const std::vector<float> noteSamples =
          renderNoteAt( note, frequencyFor( note, root, p_tuning.temperament, p_tuning.referencePitchHz ), p_duration );

        std::ranges::transform( noteSamples, mixedSamples, mixedSamples.begin(), std::plus<>{} );
    }

    // Summing voices can exceed the maximum amplitude, and normalising the SUM - not each voice - is what
    // keeps an interval at the level of a single note. That is the whole point: the player compares two
    // sounds, and a chord that sounded louder or quieter than a note would be a clue that has nothing to
    // do with the interval.
    normaliseOnsetEnergyTo( mixedSamples, TARGET_RMS_AMPLITUDE, NOTE_ONSET_DURATION );

    return mixedSamples;
}

std::vector<float> ToneSynthesizer::renderMelody( std::span<const Note> p_notes,
                                                  std::chrono::milliseconds p_noteDuration,
                                                  std::chrono::milliseconds p_gap,
                                                  TuningContext p_tuning ) const
{
    // Une note, une duree, repetee : la version uniforme n'est qu'un CAS PARTICULIER de celle qui suit. Il n'y a donc
    // qu'une seule facon de rendre une melodie - deux boucles paralleles finiraient par diverger sur un detail, et c'est
    // la divergence qui coute, jamais la ligne economisee.
    const std::vector<std::chrono::milliseconds> durations( p_notes.size(), p_noteDuration );

    return renderMelody( p_notes, durations, p_gap, p_tuning );
}

std::vector<float> ToneSynthesizer::renderMelody( std::span<const Note> p_notes,
                                                  std::span<const std::chrono::milliseconds> p_durations,
                                                  std::chrono::milliseconds p_gap,
                                                  TuningContext p_tuning ) const
{
    std::vector<float> melodySamples;

    // Sans note, il n'y a rien a jouer ; sans DUREE, il n'y a rien a tenir. Les deux sont des reponses, et la seconde
    // ne se devine pas : une duree par defaut inventee ici ferait sonner une phrase que personne n'a ecrite.
    if( p_notes.empty() || p_durations.empty() )
    {
        return melodySamples;
    }

    const std::size_t gapSampleCount = sampleCountFor( p_gap );

    // Une duree par note, et la derniere connue pour celles qui n'en ont pas : c'est la somme qui dimensionne le
    // tampon, et la calculer evite de le faire grandir au fil des insertions.
    std::size_t sampleCount = 0;

    for( const std::size_t index : std::views::iota( std::size_t{ 0 }, p_notes.size() ) )
    {
        const std::chrono::milliseconds duration =
          ( index < p_durations.size() ) ? p_durations[index] : p_durations.back();

        sampleCount += sampleCountFor( duration ) + gapSampleCount;
    }

    melodySamples.reserve( sampleCount );

    // A melody is heard FROM its first note, exactly like a chord: the interval is built on the root it starts on.
    const Note root = p_notes.front();

    for( const std::size_t index : std::views::iota( std::size_t{ 0 }, p_notes.size() ) )
    {
        const std::chrono::milliseconds duration =
          ( index < p_durations.size() ) ? p_durations[index] : p_durations.back();

        const Note & note = p_notes[index];

        const std::vector<float> noteSamples =
          renderNoteAt( note, frequencyFor( note, root, p_tuning.temperament, p_tuning.referencePitchHz ), duration );

        melodySamples.insert( melodySamples.end(), noteSamples.begin(), noteSamples.end() );

        // The gap is silence, and it is not optional: two notes played back to back with no silence
        // sound like one continuous glide, which makes the interval impossible to hear.
        melodySamples.insert( melodySamples.end(), gapSampleCount, 0.0F );
    }

    return melodySamples;
}

std::chrono::milliseconds droneDurationFor( std::size_t p_noteCount,
                                            std::chrono::milliseconds p_noteDuration,
                                            std::chrono::milliseconds p_gap,
                                            DroneFraming p_framing ) noexcept
{
    return p_framing.leadIn + ( ( p_noteDuration + p_gap ) * static_cast<std::int64_t>( p_noteCount ) ) + p_framing.tail;
}

std::chrono::milliseconds droneDurationFor( std::span<const std::chrono::milliseconds> p_durations,
                                            std::chrono::milliseconds p_gap,
                                            DroneFraming p_framing ) noexcept
{
    // Le silence est compte UNE fois par note, comme dans la version uniforme : c'est la meme phrase musicale, lue pas a
    // pas au lieu d'etre multipliee. Aucune allocation, pour la meme raison que l'autre : cette fonction est appelee a
    // chaque lecture, et elle est `noexcept`.
    std::chrono::milliseconds total = p_framing.leadIn + p_framing.tail;

    for( const std::chrono::milliseconds duration : p_durations )
    {
        total += duration + p_gap;
    }

    return total;
}

std::vector<float> ToneSynthesizer::renderMelodyOverDrone( std::span<const Note> p_melody,
                                                           std::span<const Note> p_drone,
                                                           std::chrono::milliseconds p_noteDuration,
                                                           std::chrono::milliseconds p_gap,
                                                           TuningContext p_tuning,
                                                           DroneFraming p_framing ) const
{
    // Meme raison que pour renderMelody : une note, une duree, repetee, puis la vraie fonction.
    const std::vector<std::chrono::milliseconds> durations( p_melody.size(), p_noteDuration );

    return renderMelodyOverDrone( p_melody, p_drone, durations, p_gap, p_tuning, p_framing );
}

std::vector<float> ToneSynthesizer::renderMelodyOverDrone( std::span<const Note> p_melody,
                                                           std::span<const Note> p_drone,
                                                           std::span<const std::chrono::milliseconds> p_durations,
                                                           std::chrono::milliseconds p_gap,
                                                           TuningContext p_tuning,
                                                           DroneFraming p_framing ) const
{
    if( p_melody.empty() || p_drone.empty() )
    {
        return renderMelody( p_melody, p_durations, p_gap, p_tuning );
    }

    // Le bourdon tient PLUS LONGTEMPS que la melodie, des deux cotes : c'est ce qui installe le centre avant que la
    // couleur n'arrive, et ce qui le laisse sonner apres la derniere note. Le verdict d'ecoute du 30/09/2026 l'a
    // demande exactement ainsi : « le bourdon est au bon volume, mais on ne l'entend pas assez longtemps ».
    const std::chrono::milliseconds droneDuration = droneDurationFor( p_durations, p_gap, p_framing );

    // LE BOURDON EST UNE ONDE ENTRETENUE, et non une corde frappee : un test l'a montre apres que l'oreille l'avait
    // dit. Une corde frappee DECROIT - mesure faite, 1,85 s apres son depart, elle etait trente decibels sous son
    // attaque - et un bourdon qui s'eteint n'est plus un bourdon.
    //
    // Ici, et SEULEMENT ici, le bourdon est fabrique par la synthese : c'est le REPLI. Un ensemble a cordes enregistre
    // le remplace des que l'adaptateur en fournit un, et il passe alors par mixMelodyOverDrone, qui applique la meme
    // regle d'assemblage. Waveform::Organ garde ses harmoniques sous la huitieme, donc rien ne se replie et le son ne
    // gresille pas - ce que la dents de scie de la version precedente ne savait pas faire.
    const std::vector<float> droneSamples = renderWaveChord( p_drone, Waveform::Organ, droneDuration, p_tuning );

    return mixMelodyOverDrone( p_melody, droneSamples, p_durations, p_gap, p_tuning, p_framing );
}

std::vector<float> ToneSynthesizer::mixMelodyOverDrone( std::span<const Note> p_melody,
                                                        std::span<const float> p_droneSamples,
                                                        std::chrono::milliseconds p_noteDuration,
                                                        std::chrono::milliseconds p_gap,
                                                        TuningContext p_tuning,
                                                        DroneFraming p_framing ) const
{
    // Meme raison que pour renderMelody : la version uniforme est le cas particulier d'une note, une duree, repetee.
    const std::vector<std::chrono::milliseconds> durations( p_melody.size(), p_noteDuration );

    return mixMelodyOverDrone( p_melody, p_droneSamples, durations, p_gap, p_tuning, p_framing );
}

std::vector<float> ToneSynthesizer::mixMelodyOverDrone( std::span<const Note> p_melody,
                                                        std::span<const float> p_droneSamples,
                                                        std::span<const std::chrono::milliseconds> p_durations,
                                                        std::chrono::milliseconds p_gap,
                                                        TuningContext p_tuning,
                                                        DroneFraming p_framing ) const
{
    // PAS de const ici : un tampon const ne peut plus etre DEPLACE au retour, donc il serait copie - une copie de
    // 200 000 echantillons pour rien, dans la fonction la plus appelee de l'exercice.
    std::vector<float> melodySamples = renderMelody( p_melody, p_durations, p_gap, p_tuning );

    if( melodySamples.empty() )
    {
        return melodySamples;
    }

    // Le mixage lui-meme vit dans mixRenderedMelodyOverDrone : la phrase et la gamme d'un mode passent par la MEME regle,
    // qu'elles viennent de la synthese ou d'un instrument.
    return mixRenderedMelodyOverDrone( melodySamples, p_droneSamples, p_framing );
}

std::vector<float> ToneSynthesizer::mixRenderedMelodyOverDrone( std::span<const float> p_melodySamples,
                                                                std::span<const float> p_droneSamples,
                                                                DroneFraming p_framing ) const
{
    if( p_melodySamples.empty() )
    {
        // Rien a poser sur le bourdon : on rend une melodie vide, qui se joue comme un silence. Le bourdon seul serait une
        // autre question, et ce n'est pas a cette fonction d'en decider.
        return {};
    }

    const std::size_t leadInSampleCount = sampleCountFor( p_framing.leadIn );

    const std::size_t sampleCount = std::max( p_droneSamples.size(), leadInSampleCount + p_melodySamples.size() );

    std::vector<float> mixedSamples( sampleCount, 0.0F );

    for( const std::size_t index : std::views::iota( std::size_t{ 0 }, mixedSamples.size() ) )
    {
        // L'index est verifie AVANT d'etre utilise, et l'acces se fait donc par l'operateur : std::span::at() n'existe
        // que dans un C++26 tres recent, et le compilateur du NDK Android ne l'a pas encore. Ce qui compile sur le
        // bureau ne compile donc pas forcement sur le telephone - une lecon apprise par un build Android en echec.
        const float droneSample = ( index < p_droneSamples.size() ) ? p_droneSamples[index] : 0.0F;

        float melodySample = 0.0F;

        // La melodie est DECALEE du temps ou le bourdon sonne seul. C'est ce decalage qui fait entendre le centre
        // avant la couleur, et c'est tout l'interet de l'encadrement.
        if( ( index >= leadInSampleCount ) && ( ( index - leadInSampleCount ) < p_melodySamples.size() ) )
        {
            melodySample = p_melodySamples.at( index - leadInSampleCount );
        }

        mixedSamples.at( index ) = melodySample + ( droneSample * DRONE_GAIN );
    }

    // PAS de normalisation d'ensemble : chaque voix arrive deja normalisee - la melodie comme une note, le bourdon
    // comme ce qu'il est. Renormaliser la SOMME prendrait son energie au DEBUT du tampon, c'est-a-dire au bourdon SEUL,
    // qui deviendrait alors aussi fort qu'une note et noierait la melodie.
    //
    // Il ne reste donc qu'un garde-fou : si les deux voix depassent ensemble ce que le materiel accepte, on baisse TOUT
    // d'un meme facteur. Le rapport entre les voix est conserve, donc l'equilibre entendu aussi.
    constexpr float MAXIMUM_MIX_AMPLITUDE = 0.99F;

    const float peak = peakAmplitude( mixedSamples );

    if( peak > MAXIMUM_MIX_AMPLITUDE )
    {
        const float factor = MAXIMUM_MIX_AMPLITUDE / peak;

        for( float & sample : mixedSamples )
        {
            sample *= factor;
        }
    }

    return mixedSamples;
}

// One sample of a pure waveform at a given phase (a fraction of a cycle, 0 to 1).
//
// The three spectra are the POINT: the sine has no harmonic, the sawtooth has every harmonic falling as 1/n, the
// square only the odd ones. They are the honest tools for hearing a temperament, because nothing else is in the way.
// La forme d'onde d'un "Waveform", calculée à une phase donnée. STATIQUE, et non libre : clang-tidy le demande, et il a
// raison - elle n'est appelée que par ce fichier, donc tout autre nom qu'un lien interne invite un autre fichier à
// croire qu'elle existe, et à en dépendre un jour.
[[nodiscard]] static float waveformSample( Waveform p_waveform, double p_phase ) noexcept
{
    const double cycle = p_phase - std::floor( p_phase );

    switch( p_waveform )
    {
        case Waveform::Sine:
            return static_cast<float>( std::sin( 2.0 * std::numbers::pi * cycle ) );

        case Waveform::Sawtooth:
            return static_cast<float>( ( 2.0 * cycle ) - 1.0 );

        case Waveform::Square:
            return ( cycle < 0.5 ) ? 1.0F : -1.0F;

        case Waveform::Organ: {
            // Les tirettes basses d'un orgue : la fondamentale et six harmoniques, chacune plus faible. Rien au-dessus
            // de la huitieme, donc RIEN qui puisse se replier - et c'est ce qui rend le son tenu ET doux, la ou une
            // dents de scie gresille.
            constexpr std::array<double, 7> ORGAN_DRAW_BARS{ 1.0, 0.5, 1.0 / 3.0, 0.25, 0.2, 1.0 / 6.0, 0.125 };

            // La somme des tirettes est CALCULEE, et jamais ecrite a la main : ajouter ou retirer une tirette ne peut
            // donc pas desynchroniser la normalisation, qui ramenerait le son a un niveau faux sans que rien ne casse.
            constexpr double ORGAN_DRAW_BAR_SUM =
              std::accumulate( ORGAN_DRAW_BARS.begin(), ORGAN_DRAW_BARS.end(), 0.0 );

            double harmonicSum = 0.0;

            for( const std::size_t harmonicIndex : std::views::iota( std::size_t{ 0 }, ORGAN_DRAW_BARS.size() ) )
            {
                const double harmonicNumber = static_cast<double>( harmonicIndex ) + 1.0;

                harmonicSum +=
                  ORGAN_DRAW_BARS.at( harmonicIndex ) * std::sin( 2.0 * std::numbers::pi * cycle * harmonicNumber );
            }

            return static_cast<float>( harmonicSum / ORGAN_DRAW_BAR_SUM );
        }
    }

    return 0.0F;
}

std::vector<float> ToneSynthesizer::renderWaveNote( const Note & p_note,
                                                    Waveform p_waveform,
                                                    std::chrono::milliseconds p_duration,
                                                    TuningContext p_tuning ) const
{
    return renderWaveNoteAt( p_note,
                             frequencyFor( p_note,
                                           p_note,
                                           p_tuning.temperament,
                                           p_tuning.referencePitchHz ),
                             p_waveform,
                             p_duration );
}

std::vector<float> ToneSynthesizer::renderWaveNoteAt( const Note & p_note,
                                                      double p_frequencyHz,
                                                      Waveform p_waveform,
                                                      std::chrono::milliseconds p_duration ) const
{
    const std::size_t sampleCount = sampleCountFor( p_duration );

    std::vector<float> samples( sampleCount, 0.0F );

    if( ( sampleCount == 0 ) || !p_note.isValid() )
    {
        return samples;
    }

    const double cyclesPerSample = p_frequencyHz / static_cast<double>( m_sampleRate );

    for( const std::size_t sampleIndex : std::views::iota( std::size_t{ 0 }, sampleCount ) )
    {
        samples.at( sampleIndex ) =
          waveformSample( p_waveform, cyclesPerSample * static_cast<double>( sampleIndex ) );
    }

    // No hammer: the waveform starts with a short fade to remove the click, and ends with the same release as every
    // note. The sawtooth and the square are discontinuous, so the fade is what keeps the attack from clicking.
    applyEnvelope( samples, STRIKE_ATTACK_DURATION, RELEASE_DURATION );

    normaliseOnsetEnergyTo( samples, TARGET_RMS_AMPLITUDE, NOTE_ONSET_DURATION );

    return samples;
}

std::vector<float> ToneSynthesizer::renderWaveChord( std::span<const Note> p_notes,
                                                     Waveform p_waveform,
                                                     std::chrono::milliseconds p_duration,
                                                     TuningContext p_tuning ) const
{
    const std::size_t sampleCount = sampleCountFor( p_duration );

    std::vector<float> mixedSamples( sampleCount, 0.0F );

    if( ( sampleCount == 0 ) || p_notes.empty() )
    {
        return mixedSamples;
    }

    const Note root = p_notes.front();

    for( const Note & note : p_notes )
    {
        const std::vector<float> noteSamples = renderWaveNoteAt(
          note, frequencyFor( note, root, p_tuning.temperament, p_tuning.referencePitchHz ), p_waveform, p_duration );

        std::ranges::transform( noteSamples, mixedSamples, mixedSamples.begin(), std::plus<>{} );
    }

    normaliseOnsetEnergyTo( mixedSamples, TARGET_RMS_AMPLITUDE, NOTE_ONSET_DURATION );

    return mixedSamples;
}

std::vector<float> ToneSynthesizer::renderWaveMelody( std::span<const Note> p_notes,
                                                      Waveform p_waveform,
                                                      std::chrono::milliseconds p_noteDuration,
                                                      std::chrono::milliseconds p_gap,
                                                      TuningContext p_tuning ) const
{
    std::vector<float> melodySamples;

    if( p_notes.empty() )
    {
        return melodySamples;
    }

    const std::size_t gapSampleCount = sampleCountFor( p_gap );

    melodySamples.reserve( p_notes.size() * ( sampleCountFor( p_noteDuration ) + gapSampleCount ) );

    const Note root = p_notes.front();

    for( const Note & note : p_notes )
    {
        const std::vector<float> noteSamples = renderWaveNoteAt(
          note, frequencyFor( note, root, p_tuning.temperament, p_tuning.referencePitchHz ), p_waveform, p_noteDuration );

        melodySamples.insert( melodySamples.end(), noteSamples.begin(), noteSamples.end() );

        melodySamples.insert( melodySamples.end(), gapSampleCount, 0.0F );
    }

    return melodySamples;
}

void ToneSynthesizer::applyEnvelope( std::span<float> p_samples,
                                     std::chrono::milliseconds p_attack,
                                     std::chrono::milliseconds p_release ) const
{
    const std::size_t sampleCount = p_samples.size();

    if( sampleCount == 0 )
    {
        return;
    }

    // Each fade may take at most half of the buffer, otherwise the two fades would overlap and the
    // note would never reach its full level: a short note would sound like a click by itself.
    const std::size_t maximumFadeSampleCount = sampleCount / 2;

    const std::size_t attackSampleCount =
      std::min( sampleCountFor( p_attack ), maximumFadeSampleCount );

    const std::size_t releaseSampleCount =
      std::min( sampleCountFor( p_release ), maximumFadeSampleCount );

    if( ( attackSampleCount == 0 ) || ( releaseSampleCount == 0 ) )
    {
        return;
    }

    // Fade in: the gain starts at 0, so the very first sample of the buffer is silence.
    //
    // The two loops below walk SUBVIEWS of the buffer rather than addressing samples by index, for two
    // independent reasons:
    //
    //   * indexing a span is an unchecked access, which the clang-tidy configuration of this project
    //     refuses (cppcoreguidelines-pro-bounds-avoid-unchecked-container-access);
    //   * std::span::at(), the checked accessor used here before, is C++26 and is absent from the
    //     libc++ shipped with the Android NDK's Clang 18.
    //
    // 'first' and 'last' cannot leave the buffer - the callers clamp both counts to half of it - and
    // the gain is counted alongside the samples instead of being derived from an index.
    std::size_t attackIndex = 0;

    for( float & sample : p_samples.first( attackSampleCount ) )
    {
        sample *= static_cast<float>( attackIndex ) / static_cast<float>( attackSampleCount );

        ++attackIndex;
    }

    // Fade out: counted from the end, so the very last sample is exactly silence.
    std::size_t samplesFromTheEnd = releaseSampleCount;

    for( float & sample : p_samples.last( releaseSampleCount ) )
    {
        --samplesFromTheEnd;

        sample *= static_cast<float>( samplesFromTheEnd ) / static_cast<float>( releaseSampleCount );
    }
}

float ToneSynthesizer::rootMeanSquare( std::span<const float> p_samples )
{
    if( p_samples.empty() )
    {
        return 0.0F;
    }

    const double sumOfSquares = std::accumulate(
      p_samples.begin(),
      p_samples.end(),
      0.0,
      []( double p_sum, float p_sample ) {
          const auto sample = static_cast<double>( p_sample );

          return p_sum + ( sample * sample );
      } );

    return static_cast<float>( std::sqrt( sumOfSquares / static_cast<double>( p_samples.size() ) ) );
}

float ToneSynthesizer::peakAmplitude( std::span<const float> p_samples )
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

void ToneSynthesizer::normaliseOnsetEnergyTo( std::span<float> p_samples,
                                              float p_targetRms,
                                              std::chrono::milliseconds p_onsetDuration ) const
{
    // The onset, or the whole buffer when it is shorter than the onset: a very short sound has no beginning,
    // it IS one.
    const std::size_t onsetSampleCount = std::min( p_samples.size(), sampleCountFor( p_onsetDuration ) );

    const float energy = rootMeanSquare( p_samples.first( onsetSampleCount ) );

    if( energy <= 0.0F )
    {
        return;
    }

    float gain = p_targetRms / energy;

    // The ceiling, and it is not a detail: a rich waveform reaches a given energy only by showing peaks
    // well above it. Without this, the energy target would send the signal straight into clipping - and
    // on a phone speaker, clipping is not a subtlety, it is distortion.
    const float peak = peakAmplitude( p_samples );

    if( ( peak > 0.0F ) && ( peak * gain > MAXIMUM_PEAK_AMPLITUDE ) )
    {
        gain = MAXIMUM_PEAK_AMPLITUDE / peak;
    }

    std::ranges::for_each( p_samples, [gain]( float & p_sample ) { p_sample *= gain; } );
}

std::vector<float> ToneSynthesizer::renderMistakeCue( std::chrono::milliseconds p_duration ) const
{
    const std::size_t sampleCount = sampleCountFor( p_duration );

    std::vector<float> samples( sampleCount, 0.0F );

    if( sampleCount == 0 )
    {
        return samples;
    }

    // The seed is fixed: see the header. The draw itself is uniform over the whole range of a sample,
    // which is the simplest way to get something with no pitch and no memory.
    // The constant seed is the POINT here, and the check cannot know that: a cue has to be the same
    // every time, and a cue that changed would be impossible to recognise.
    // NOLINTNEXTLINE(bugprone-random-generator-seed, cert-msc32-c, cert-msc51-cpp)
    std::mt19937 noiseEngine{ MISTAKE_CUE_SEED };

    std::uniform_real_distribution<float> amplitudeDistribution{ -1.0F, 1.0F };

    for( const std::size_t sampleIndex : std::views::iota( std::size_t{ 0 }, sampleCount ) )
    {
        // A decay over the whole burst, so that it reads as a brief "thud" rather than a hiss.
        const float progress = static_cast<float>( sampleIndex ) / static_cast<float>( sampleCount );

        const auto decay =
          static_cast<float>( std::exp( -MISTAKE_CUE_DECAY * static_cast<double>( progress ) ) );

        samples.at( sampleIndex ) = amplitudeDistribution( noiseEngine ) * decay;
    }

    // The same fade in and fade out as a note, and for the same reason: a burst that starts or stops
    // abruptly adds a CLICK of its own, which is the artefact the envelope exists to remove.
    applyEnvelope( samples, CUE_ATTACK_DURATION, RELEASE_DURATION );

    // And a LOWER energy than a note, because noise at the same energy sounds much louder than a pitched
    // sound: it spreads the same energy over every frequency at once.
    normaliseOnsetEnergyTo( samples, MISTAKE_CUE_RMS_AMPLITUDE, MISTAKE_CUE_DURATION );

    return samples;
}

}    // namespace musichien::domain
