// =====================================================================================================================
// Musichien - render_preview
//
// Un outil de DEVELOPPEMENT : il ecrit sur le disque ce que le domaine sait deja jouer, pour qu'une oreille puisse en
// juger sans lancer l'application.
//
// Il n'appartient a aucune des quatre couches, et c'est delibere : ce n'est ni un module, ni l'application, ni un
// test. C'est un executable en ligne de commande qui ne depend que du DOMAINE, donc qui se compile et se lance
// partout ou le domaine se compile - sans Qt, sans carte son, sans telephone et sans permission.
//
// Usage :
//
//     musichien_render_preview [dossier_de_sortie]
//
// Ce qu'il ecrit : un WAV par mode, la gamme montee et descendue sur un BOURDON TENU, plus l'index qui dit dans quel
// ordre les ecouter - l'ordre de couleur, du plus clair au plus sombre.
//
// C'est la forme la plus courte de la question qui deviendra l'exercice du degrade, et c'est aussi pourquoi cet outil
// existe : il y a des choses qu'aucun test ne juge. Une couleur, une balance, un timbre : ca s'ecoute.
// =====================================================================================================================

#include "domain/audio/SampledInstrument.h"
#include "domain/audio/ToneSynthesizer.h"
#include "domain/music/Mode.h"
#include "domain/music/Note.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

using musichien::domain::Mode;
using musichien::domain::MODE_COUNT;
using musichien::domain::Note;
using musichien::domain::ToneSynthesizer;

// Le taux d'echantillonnage des appareils Android, et celui de tous les tests du domaine.
constexpr std::int32_t PREVIEW_SAMPLE_RATE = 48000;

constexpr std::chrono::milliseconds NOTE_DURATION{ 420 };
constexpr std::chrono::milliseconds GAP{ 70 };

// La tonique ecoutee : re4. C'est la tonique de tous les tableaux de la note 27, donc celle qui permet de comparer ce
// qu'on entend a ce qui est ecrit.
constexpr std::int32_t TONIC_MIDI_NUMBER = 62;

// Le bourdon : la tonique deux octaves sous la melodie. Assez bas pour etre SENTI plutot qu'ecoute, assez haut pour
// qu'un haut-parleur de telephone le reproduise encore.
constexpr std::int32_t DRONE_MIDI_NUMBER = 38;

// La gamme, montee puis descendue.
//
// Une couleur s'entend mieux quand elle va et vient, et la DESCENTE est ce qui fait entendre ou se trouve le centre :
// c'est en revenant sur la tonique qu'un mode se dit. Monter seulement laisserait la phrase en l'air.
[[nodiscard]] std::vector<Note> scaleUpAndDown( Note p_tonic, Mode p_mode )
{
    const std::vector<Note> ascending = musichien::domain::notesOfMode( p_tonic, p_mode );

    std::vector<Note> melody = ascending;

    for( auto note = ascending.rbegin() + 1; note != ascending.rend(); ++note )
    {
        melody.push_back( *note );
    }

    return melody;
}

// Ecrit un WAV MONO 16 BITS.
//
// C'est le format que tout lecteur audio ouvre sans discuter, et c'est exactement ce dont une oreille a besoin : seize
// bits suffisent tres largement pour juger une couleur, et un fichier brut ne demande aucune bibliotheque.
void writeWavFile( const std::filesystem::path & p_path, std::span<const float> p_samples )
{
    constexpr std::uint16_t CHANNEL_COUNT = 1;
    constexpr std::uint16_t BITS_PER_SAMPLE = 16;
    constexpr std::uint16_t BYTES_PER_SAMPLE = BITS_PER_SAMPLE / 8U;

    const auto sampleRate = static_cast<std::uint32_t>( PREVIEW_SAMPLE_RATE );
    const auto dataByteCount = static_cast<std::uint32_t>( p_samples.size() ) * BYTES_PER_SAMPLE;

    std::ofstream file{ p_path, std::ios::binary };

    if( !file )
    {
        std::cerr << "Musichien: cannot write " << p_path.string() << '\n';

        return;
    }

    const auto writeTag = [&file]( const char * p_tag ) {
        file.write( p_tag, 4 );
    };

    const auto writeUint32 = [&file]( std::uint32_t p_value ) {
        const std::array<char, 4> bytes{ static_cast<char>( p_value & 0xFFU ),
                                         static_cast<char>( ( p_value >> 8U ) & 0xFFU ),
                                         static_cast<char>( ( p_value >> 16U ) & 0xFFU ),
                                         static_cast<char>( ( p_value >> 24U ) & 0xFFU ) };

        file.write( bytes.data(), 4 );
    };

    const auto writeUint16 = [&file]( std::uint16_t p_value ) {
        // Le passage en 32 bits n'est pas cosmetique : un uint16_t est PROMU en int avant le decalage, et une
        // operation binaire sur un entier SIGNE est exactement ce que clang-tidy signale (hicpp-signed-bitwise).
        const auto value = static_cast<std::uint32_t>( p_value );

        const std::array<char, 2> bytes{ static_cast<char>( value & 0xFFU ),
                                         static_cast<char>( ( value >> 8U ) & 0xFFU ) };

        file.write( bytes.data(), 2 );
    };

    writeTag( "RIFF" );
    writeUint32( 36U + dataByteCount );
    writeTag( "WAVE" );

    writeTag( "fmt " );
    writeUint32( 16U );
    writeUint16( 1U );    // PCM entier, sans compression
    writeUint16( CHANNEL_COUNT );
    writeUint32( sampleRate );
    writeUint32( sampleRate * static_cast<std::uint32_t>( CHANNEL_COUNT * BYTES_PER_SAMPLE ) );
    writeUint16( static_cast<std::uint16_t>( CHANNEL_COUNT * BYTES_PER_SAMPLE ) );
    writeUint16( BITS_PER_SAMPLE );

    writeTag( "data" );
    writeUint32( dataByteCount );

    for( const float sample : p_samples )
    {
        const auto clamped = static_cast<double>( std::clamp( sample, -1.0F, 1.0F ) );
        const auto value = static_cast<std::int16_t>( std::lround( clamped * 32767.0 ) );

        writeUint16( static_cast<std::uint16_t>( value ) );
    }
}

// Lit un fichier entier en octets.
//
// C'est un OUTIL de developpement, donc lire un fichier y est normal : le domaine, lui, ne lit jamais rien - il recoit
// des octets et les interprete. C'est ce qui garde le decodeur WAV testable sans systeme de fichiers.
[[nodiscard]] std::vector<std::byte> readFileBytes( const std::filesystem::path & p_path )
{
    std::ifstream file{ p_path, std::ios::binary };

    if( !file )
    {
        std::cerr << "Musichien: cannot read " << p_path.string() << '\n';

        return {};
    }

    const std::vector<char> rawBytes{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };

    std::vector<std::byte> bytes( rawBytes.size() );

    std::ranges::transform( rawBytes, bytes.begin(), []( char p_byte ) {
        return static_cast<std::byte>( p_byte );
    } );

    return bytes;
}

// Ecrit une SERIE : les sept modes, la gamme montee puis descendue, sur un bourdon TENU.
//
// Le bourdon arrive DEJA RENDU, et c'est le point : cette fonction ne sait pas s'il vient de la synthese ou d'un
// instrument enregistre, et elle n'a pas a le savoir. Elle applique la regle d'assemblage du domaine, une seule fois,
// pour tous les timbres - c'est exactement pour cela que mixMelodyOverDrone existe.
void renderSeries( const ToneSynthesizer & p_synthesizer,
                   std::span<const float> p_droneSamples,
                   Note p_tonic,
                   const std::filesystem::path & p_directory,
                   std::string_view p_description )
{
    std::error_code error;
    std::filesystem::create_directories( p_directory, error );

    std::ofstream listing{ p_directory / "index.txt" };

    listing << "# " << p_description << '\n';
    listing << "# Les sept modes, dans l'ordre de couleur, du plus clair au plus sombre\n\n";

    for( std::size_t modeIndex = 0; modeIndex < MODE_COUNT; ++modeIndex )
    {
        const auto mode = static_cast<Mode>( modeIndex );

        const std::vector<Note> melody = scaleUpAndDown( p_tonic, mode );

        const std::vector<float> samples =
          p_synthesizer.mixMelodyOverDrone( melody, p_droneSamples, NOTE_DURATION, GAP );

        const std::string fileName = std::string( musichien::domain::modeIdentifier( mode ) ) + ".wav";

        writeWavFile( p_directory / fileName, samples );

        listing << ( modeIndex + 1 ) << ". " << fileName << '\n';

        std::cout << "Musichien: " << p_directory.filename().string() << "/" << fileName << " - " << melody.size()
                  << " notes, " << samples.size() << " samples\n";
    }
}

}    // namespace

int main( int p_argumentCount, char * p_arguments[] )
{
    const std::filesystem::path outputDirectory =
      ( p_argumentCount > 1 ) ? std::filesystem::path{ p_arguments[1] } : std::filesystem::path{ "preview" };

    std::error_code error;
    std::filesystem::create_directories( outputDirectory, error );

    const ToneSynthesizer synthesizer{ PREVIEW_SAMPLE_RATE };

    const Note tonic{ TONIC_MIDI_NUMBER };

    // La duree du bourdon : la MEME regle que celle du domaine, et c'est la seule chose qui doive coincider avec elle.
    // Treize notes : sept degres montes, six redescendus.
    constexpr std::size_t SERIES_NOTE_COUNT = 13;

    const musichien::domain::DroneFraming framing{};

    const std::chrono::milliseconds droneDuration =
      framing.leadIn + ( ( NOTE_DURATION + GAP ) * static_cast<std::int64_t>( SERIES_NOTE_COUNT ) ) + framing.tail;

    // PREMIER CAS : un bourdon ENREGISTRE est donne en deuxieme argument.
    //
    // On rend alors UNE SEULE serie, et c'est deliberé : quand un timbre est choisi, c'est CE timbre qu'on veut
    // entendre sur les sept modes, pas une comparaison avec autre chose.
    if( p_argumentCount > 2 )
    {
        const std::filesystem::path droneWavePath{ p_arguments[2] };

        // La note que l'echantillon EST. Un enregistrement ne la dit pas lui-meme, et c'est la seule chose que l'outil
        // ne peut pas deviner : sans elle, la transposition partirait d'une hauteur fausse.
        std::int32_t droneRootMidiNumber = DRONE_MIDI_NUMBER;

        if( p_argumentCount > 3 )
        {
            const std::string_view text{ p_arguments[3] };

            if( std::from_chars( text.data(), text.data() + text.size(), droneRootMidiNumber ).ec != std::errc{} )
            {
                std::cerr << "Musichien: " << text << " is not a MIDI number\n";

                return 1;
            }
        }

        const std::optional<musichien::domain::SampledNote> recordedNote =
          musichien::domain::sampledNoteFromWave( readFileBytes( droneWavePath ), droneRootMidiNumber );

        if( !recordedNote.has_value() )
        {
            std::cerr << "Musichien: " << droneWavePath.string() << " is not a readable 16 bit wave file\n";

            return 1;
        }

        musichien::domain::SampledInstrument droneInstrument;
        droneInstrument.addNote( *recordedNote );

        // Le bourdon est rendu par l'ECHANTILLONNEUR, exactement comme une note du jeu : meme transposition, meme
        // normalisation, meme fondu de queue. C'est ce qui garantit qu'il sonnera dans l'application comme il sonne ici.
        const std::vector<float> droneSamples =
          droneInstrument.renderNote( Note{ droneRootMidiNumber }, droneDuration, PREVIEW_SAMPLE_RATE );

        renderSeries( synthesizer,
                      droneSamples,
                      tonic,
                      outputDirectory / droneWavePath.stem().string(),
                      droneWavePath.filename().string() );

        std::cout << "Musichien: " << outputDirectory.string() << " written\n";

        return 0;
    }

    // SECOND CAS : aucun fichier. Les deux bourdons fabriques par la synthese - la note seule et la quinte - pour
    // entendre ce que le REPLI sait faire quand aucun echantillon n'est disponible.
    constexpr std::int32_t FIFTH_IN_SEMITONES = 7;

    const std::vector<std::pair<std::string_view, std::vector<Note>>> droneChoices{
        { "single", { Note{ DRONE_MIDI_NUMBER } } },
        { "fifth", { Note{ DRONE_MIDI_NUMBER }, Note{ DRONE_MIDI_NUMBER + FIFTH_IN_SEMITONES } } },
    };

    for( const auto & [name, notes] : droneChoices )
    {
        const std::vector<float> droneSamples = synthesizer.renderWaveChord( notes, musichien::domain::Waveform::Organ, droneDuration );

        renderSeries( synthesizer, droneSamples, tonic, outputDirectory / name, "bourdon fabrique par la synthese (repli)" );
    }


    std::cout << "Musichien: " << outputDirectory.string() << " written\n";

    return 0;
}
