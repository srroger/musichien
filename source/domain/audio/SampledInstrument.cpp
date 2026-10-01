#include "domain/audio/SampledInstrument.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <ranges>
#include <string_view>

namespace musichien::domain
{

namespace
{

// The smallest thing that can possibly be a wave file: the RIFF header, the WAVE tag, and one chunk header.
constexpr std::size_t WAVE_MINIMUM_SIZE = 12 + 8;

// Where the chunks START: right after "RIFF", the file size, and the tag "WAVE" - that is, twelve bytes in.
//
// It is NOT the same thing as the minimum size above, and confusing the two is a silent bug: reading the first
// chunk header eight bytes too far matches no tag at all, so the file parses as "not a wave file", and the
// sampler ends up with nothing to play while everything looks correct.
constexpr std::size_t WAVE_CHUNKS_START = 12;

// Little endian, and read through iterators rather than by index: indexing a span is an unchecked access, which
// the clang-tidy configuration of this project refuses. The caller guarantees there are four bytes.
[[nodiscard]] std::uint32_t readLittleEndianWord( std::span<const std::byte> p_bytes )
{
    std::uint32_t value = 0;
    std::uint32_t shift = 0;

    for( const std::byte currentByte : p_bytes.first( 4 ) )
    {
        value |= static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( currentByte ) ) << shift;
        shift += 8;
    }

    return value;
}

// Sixteen bit fields exist in a wave file - the format, the channel count, the sample size - and reading four
// bytes where the format has two walks right past the end of the chunk. The two readers are named apart on
// purpose: the mistake is silent on a lucky padding and fatal on the next file.
[[nodiscard]] std::uint16_t readLittleEndianHalfWord( std::span<const std::byte> p_bytes )
{
    std::uint16_t value = 0;
    std::uint32_t shift = 0;

    for( const std::byte currentByte : p_bytes.first( 2 ) )
    {
        value |= static_cast<std::uint16_t>(
          static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( currentByte ) ) << shift );
        shift += 8;
    }

    return value;
}

[[nodiscard]] bool startsWithTag( std::span<const std::byte> p_bytes, const char * p_tag )
{
    if( p_bytes.size() < 4 )
    {
        return false;
    }

    auto byteIterator = p_bytes.begin();

    for( const char expectedLetter : std::string_view{ p_tag, 4 } )
    {
        if( std::to_integer<char>( *byteIterator ) != expectedLetter )
        {
            return false;
        }

        ++byteIterator;
    }

    return true;
}

// What a 'fmt ' chunk said about the samples.
struct WaveFormat
{
    std::int32_t channelCount{ 0 };
    std::int32_t sampleRate{ 0 };
    std::int32_t bitsPerSample{ 0 };
    std::int32_t formatCode{ 0 };
};

// Turns the 16 bit samples of a 'data' chunk into mono values in [-1, 1].
[[nodiscard]] std::vector<float> toMonoSamples( std::span<const std::byte> p_data, std::int32_t p_channelCount )
{
    std::vector<float> monoSamples;

    if( p_channelCount <= 0 )
    {
        return monoSamples;
    }

    monoSamples.reserve( p_data.size() / ( static_cast<std::size_t>( 2 ) * static_cast<std::size_t>( p_channelCount ) ) );

    auto byteIterator = p_data.begin();

    // One frame at a time, all channels together: a stereo file is averaged rather than half discarded, which
    // would silently lose whatever the two microphones did not record identically.
    while( byteIterator + ( static_cast<std::ptrdiff_t>( 2 ) * static_cast<std::ptrdiff_t>( p_channelCount ) ) <= p_data.end() )
    {
        float sum = 0.0F;

        for( std::int32_t channel = 0; channel < p_channelCount; ++channel )
        {
            const auto lowByte = std::to_integer<std::uint8_t>( *byteIterator );
            ++byteIterator;

            const auto highByte = std::to_integer<std::uint8_t>( *byteIterator );
            ++byteIterator;

            const auto rawValue = static_cast<std::uint32_t>( lowByte ) | ( static_cast<std::uint32_t>( highByte ) << 8U );

            sum += static_cast<float>( static_cast<std::int16_t>( static_cast<std::uint16_t>( rawValue ) ) ) / 32768.0F;
        }

        monoSamples.push_back( sum / static_cast<float>( p_channelCount ) );
    }

    return monoSamples;
}

[[nodiscard]] float rootMeanSquareOf( std::span<const float> p_samples )
{
    if( p_samples.empty() )
    {
        return 0.0F;
    }

    const double sumOfSquares = std::accumulate( p_samples.begin(),
                                                 p_samples.end(),
                                                 0.0,
                                                 []( double p_sum, float p_sample ) {
                                                     const auto sample = static_cast<double>( p_sample );

                                                     return p_sum + ( sample * sample );
                                                 } );

    return static_cast<float>( std::sqrt( sumOfSquares / static_cast<double>( p_samples.size() ) ) );
}

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

// The tail of the sample is faded out, and the head is NOT: the attack of a piano is the whole point of having
// recorded one, and a fade in would erase the very thing the ear recognises.
void fadeOutTheTail( std::span<float> p_samples, std::int32_t p_sampleRate )
{
    constexpr std::chrono::milliseconds FADE_DURATION{ 40 };

    if( ( p_sampleRate <= 0 ) || p_samples.empty() )
    {
        return;
    }

    const auto exactFadeLength = ( static_cast<double>( FADE_DURATION.count() ) / 1000.0 ) * static_cast<double>( p_sampleRate );

    const auto fadeLength = std::min( p_samples.size(), static_cast<std::size_t>( exactFadeLength ) );

    if( fadeLength == 0 )
    {
        return;
    }

    std::size_t samplesFromTheEnd = fadeLength;

    for( float & sample : p_samples.last( fadeLength ) )
    {
        --samplesFromTheEnd;

        sample *= static_cast<float>( samplesFromTheEnd ) / static_cast<float>( fadeLength );
    }
}

// The same rule as the synthesiser, and for the same reason: the player is asked to COMPARE two sounds, so a
// note, a chord and a melody must be heard at the same level. Measured on the attack, because the energy of a
// whole decaying note is mostly a measure of how long it rings.
void normaliseOnsetEnergy( std::span<float> p_samples, std::int32_t p_sampleRate )
{
    if( ( p_sampleRate <= 0 ) || p_samples.empty() )
    {
        return;
    }

    const auto exactOnsetLength =
      ( static_cast<double>( ToneSynthesizer::NOTE_ONSET_DURATION.count() ) / 1000.0 ) * static_cast<double>( p_sampleRate );

    const auto onsetLength = std::min( p_samples.size(), static_cast<std::size_t>( exactOnsetLength ) );

    const float energy = rootMeanSquareOf( p_samples.first( onsetLength ) );

    if( energy <= 0.0F )
    {
        return;
    }

    float gain = ToneSynthesizer::TARGET_RMS_AMPLITUDE / energy;

    const float peak = peakAmplitudeOf( p_samples );

    if( ( peak > 0.0F ) && ( peak * gain > ToneSynthesizer::MAXIMUM_PEAK_AMPLITUDE ) )
    {
        gain = ToneSynthesizer::MAXIMUM_PEAK_AMPLITUDE / peak;
    }

    std::ranges::for_each( p_samples, [gain]( float & p_sample ) { p_sample *= gain; } );
}

}    // namespace

std::vector<bool> defaultEnabledInstruments()
{
    std::vector<bool> instruments( INSTRUMENT_COUNT, false );

    // Le piano et la guitare : les deux premiers de la liste, et les deux plus neutres a l'oreille. Par INDEX plutot que
    // par nom, parce qu'un nom se traduit et qu'un index est un contrat (voir INSTRUMENT_NAMES - l'ordre ne bouge
    // jamais, les nouveaux instruments vont a la fin).
    instruments.at( 0 ) = true;
    instruments.at( 1 ) = true;

    return instruments;
}

std::optional<SampledNote> sampledNoteFromWave( std::span<const std::byte> p_bytes,
                                                std::int32_t p_rootMidiNumber )
{
    if( p_bytes.size() < WAVE_MINIMUM_SIZE )
    {
        return std::nullopt;
    }

    if( !startsWithTag( p_bytes, "RIFF" ) || !startsWithTag( p_bytes.subspan( 8 ), "WAVE" ) )
    {
        return std::nullopt;
    }

    std::optional<WaveFormat> format;
    std::vector<float> monoSamples;

    // The chunks are WALKED rather than read at fixed offsets. A wave file may carry a 'LIST' chunk, a 'fact'
    // chunk, or padding between chunks: a reader that assumes 44 bytes works on exactly one file out of many, and
    // fails on the next one without saying why.
    std::size_t offset = WAVE_CHUNKS_START;

    while( ( offset + 8 ) <= p_bytes.size() )
    {
        const std::span<const std::byte> chunkHeader = p_bytes.subspan( offset, 8 );

        const auto chunkSize = static_cast<std::size_t>( readLittleEndianWord( chunkHeader.subspan( 4 ) ) );

        const std::size_t availableBytes = p_bytes.size() - offset - 8;

        const std::span<const std::byte> chunkPayload =
          p_bytes.subspan( offset + 8, std::min( chunkSize, availableBytes ) );

        if( startsWithTag( chunkHeader, "fmt " ) && ( chunkPayload.size() >= 16 ) )
        {
            WaveFormat parsedFormat;
            parsedFormat.formatCode = static_cast<std::int32_t>( readLittleEndianHalfWord( chunkPayload ) );
            parsedFormat.channelCount = static_cast<std::int32_t>(
              readLittleEndianHalfWord( chunkPayload.subspan( 2 ) ) );
            parsedFormat.sampleRate = static_cast<std::int32_t>(
              readLittleEndianWord( chunkPayload.subspan( 4 ) ) );
            parsedFormat.bitsPerSample = static_cast<std::int32_t>(
              readLittleEndianHalfWord( chunkPayload.subspan( 14 ) ) );

            format = parsedFormat;
        }
        else if( startsWithTag( chunkHeader, "data" ) && format.has_value() )
        {
            monoSamples = toMonoSamples( chunkPayload, format->channelCount );
        }

        // Chunk sizes are padded to an even number of bytes.
        offset += 8 + chunkSize + ( chunkSize % 2 );
    }

    // Only uncompressed 16 bit PCM. Anything else - a compressed wave, 8 or 24 bits, floating point - is refused
    // rather than guessed at: a wrong sample is worse than no sample, because it sounds ALMOST right.
    constexpr std::int32_t PCM_FORMAT_CODE = 1;
    constexpr std::int32_t SUPPORTED_BITS_PER_SAMPLE = 16;

    if( !format.has_value() || ( format->formatCode != PCM_FORMAT_CODE ) || ( format->bitsPerSample != SUPPORTED_BITS_PER_SAMPLE ) || ( format->sampleRate <= 0 ) || monoSamples.empty() )
    {
        return std::nullopt;
    }

    SampledNote note;
    note.rootMidiNumber = p_rootMidiNumber;
    note.sampleRate = format->sampleRate;
    note.samples = std::move( monoSamples );

    return note;
}

void SampledInstrument::addNote( SampledNote p_note )
{
    if( p_note.samples.empty() || ( p_note.sampleRate <= 0 ) )
    {
        return;
    }

    m_notes.push_back( std::move( p_note ) );

    // Kept sorted, which is what lets the closest recording be found by a single pass.
    std::ranges::sort( m_notes, {}, &SampledNote::rootMidiNumber );
}

const SampledNote & SampledInstrument::closestNoteTo( const Note & p_note ) const
{
    const auto closest = std::ranges::min_element( m_notes, {}, [&p_note]( const SampledNote & p_recorded ) {
        return std::abs( p_recorded.rootMidiNumber - p_note.midiNumber() );
    } );

    return *closest;
}

std::vector<float> SampledInstrument::renderNote( const Note & p_note,
                                                  std::chrono::milliseconds p_duration,
                                                  std::int32_t p_sampleRate,
                                                  TuningContext p_tuning ) const
{
    // A note on its own is heard FROM itself: its root is the note.
    return renderNoteAt( p_note,
                         frequencyFor( p_note,
                                       p_note,
                                       p_tuning.temperament,
                                       p_tuning.referencePitchHz ),
                         p_duration,
                         p_sampleRate );
}

std::vector<float> SampledInstrument::renderNoteAt( const Note & p_note,
                                                    double p_frequencyHz,
                                                    std::chrono::milliseconds p_duration,
                                                    std::int32_t p_sampleRate ) const
{
    const auto exactSampleCount = ( static_cast<double>( p_duration.count() ) / 1000.0 ) * static_cast<double>( p_sampleRate );

    const auto sampleCount = static_cast<std::size_t>( std::llround( exactSampleCount ) );

    std::vector<float> samples( sampleCount, 0.0F );

    if( ( sampleCount == 0 ) || ( p_sampleRate <= 0 ) || m_notes.empty() || !p_note.isValid() )
    {
        return samples;
    }

    const SampledNote & recordedNote = closestNoteTo( p_note );

    // How fast the recording is played back, and there are two reasons in that ratio:
    //
    //   * the DISTANCE between the target frequency and the recorded note's own frequency - an octave up is twice
    //     the speed. The closest recording is never more than three semitones away, which is what keeps the
    //     transposition inaudible. The recorded note's frequency is its EQUAL-temperament frequency: a real
    //     recording is always made at the standard diapason, and the temperament only decides how far FROM that
    //     frequency the target is;
    //   * the RATE of the recording against the rate of the OUTPUT, so that a 44.1 kHz sample still plays in tune
    //     on a 48 kHz output. Forgetting it would detune the instrument by a semitone and a half.
    const double playbackRatio =
      ( p_frequencyHz / Note{ recordedNote.rootMidiNumber }.frequencyHz() ) * ( static_cast<double>( recordedNote.sampleRate ) / static_cast<double>( p_sampleRate ) );

    for( std::size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex )
    {
        const double exactPosition = static_cast<double>( sampleIndex ) * playbackRatio;

        const auto wholePosition = static_cast<std::size_t>( exactPosition );

        // The recording simply ends before the note does: the rest stays SILENT rather than repeating, because a
        // looped piano sounds like an organ.
        if( ( wholePosition + 1 ) >= recordedNote.samples.size() )
        {
            break;
        }

        const auto fraction = static_cast<float>( exactPosition - static_cast<double>( wholePosition ) );

        samples.at( sampleIndex ) = ( recordedNote.samples.at( wholePosition ) * ( 1.0F - fraction ) ) + ( recordedNote.samples.at( wholePosition + 1 ) * fraction );
    }

    fadeOutTheTail( samples, p_sampleRate );

    normaliseOnsetEnergy( samples, p_sampleRate );

    return samples;
}

std::vector<float> SampledInstrument::renderChord( std::span<const Note> p_notes,
                                                   std::chrono::milliseconds p_duration,
                                                   std::int32_t p_sampleRate,
                                                   TuningContext p_tuning ) const
{
    const auto exactSampleCount = ( static_cast<double>( p_duration.count() ) / 1000.0 ) * static_cast<double>( p_sampleRate );

    std::vector<float> mixedSamples( static_cast<std::size_t>( std::llround( exactSampleCount ) ), 0.0F );

    if( mixedSamples.empty() || p_notes.empty() )
    {
        return mixedSamples;
    }

    const Note root = p_notes.front();

    for( const Note & note : p_notes )
    {
        const std::vector<float> noteSamples =
          renderNoteAt( note, frequencyFor( note, root, p_tuning.temperament, p_tuning.referencePitchHz ), p_duration, p_sampleRate );

        std::ranges::transform( noteSamples, mixedSamples, mixedSamples.begin(), std::plus<>{} );
    }

    // Normalising the MIX, not each voice: that is what keeps an interval at the level of a single note.
    normaliseOnsetEnergy( mixedSamples, p_sampleRate );

    return mixedSamples;
}

std::vector<float> SampledInstrument::renderMelody( std::span<const Note> p_notes,
                                                    std::chrono::milliseconds p_noteDuration,
                                                    std::chrono::milliseconds p_gap,
                                                    std::int32_t p_sampleRate,
                                                    TuningContext p_tuning ) const
{
    // La version uniforme est le cas PARTICULIER d'une duree repetee, exactement comme dans le domaine et pour la meme
    // raison : deux boucles qui font la meme chose finissent toujours par diverger, et c'est la phrase qu'on entendrait
    // differemment selon celle qui a servi.
    const std::vector<std::chrono::milliseconds> durations( p_notes.size(), p_noteDuration );

    return renderMelody( p_notes, durations, p_gap, p_sampleRate, p_tuning );
}

std::vector<float> SampledInstrument::renderMelody( std::span<const Note> p_notes,
                                                    std::span<const std::chrono::milliseconds> p_durations,
                                                    std::chrono::milliseconds p_gap,
                                                    std::int32_t p_sampleRate,
                                                    TuningContext p_tuning ) const
{
    std::vector<float> melodySamples;

    if( ( p_sampleRate <= 0 ) || p_notes.empty() )
    {
        return melodySamples;
    }

    const auto exactGapSampleCount =
      ( static_cast<double>( p_gap.count() ) / 1000.0 ) * static_cast<double>( p_sampleRate );

    const auto gapSampleCount = static_cast<std::size_t>( std::llround( exactGapSampleCount ) );

    const Note root = p_notes.front();

    for( std::size_t index = 0; index < p_notes.size(); ++index )
    {
        // Une duree MANQUANTE retombe sur la premiere : une phrase dont le contenu serait plus court que la melodie doit
        // sonner de travers, pas s'arreter au milieu. L'index est verifie avant d'etre lu, parce que std::span::at()
        // n'existe pas sur le NDK Android.
        const std::chrono::milliseconds noteDuration = ( index < p_durations.size() ) ? p_durations[index]
                                                                                      : p_durations.front();

        const Note note = p_notes[index];

        const std::vector<float> noteSamples = renderNoteAt(
          note, frequencyFor( note, root, p_tuning.temperament, p_tuning.referencePitchHz ), noteDuration, p_sampleRate );

        melodySamples.insert( melodySamples.end(), noteSamples.begin(), noteSamples.end() );

        // The gap is silence, and it is not optional: two notes played back to back with no silence sound like one
        // continuous glide, which makes the interval impossible to hear.
        melodySamples.insert( melodySamples.end(), gapSampleCount, 0.0F );
    }

    return melodySamples;
}

}    // namespace musichien::domain
