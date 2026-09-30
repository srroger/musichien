// =====================================================================================================================
// Musichien - render_phrases
//
// L'ATELIER : il genere des phrases modales, ecrit leurs WAV, et pose a cote une page pour les ECOUTER et les TRIER.
// C'est ce que Roger a demande : « genere plein de phrases, je fais le tri derriere ; mais donne-moi de quoi les
// ecouter, les voir et les editer ».
//
// ---------------------------------------------------------------------------------------------------------------------
// Deux mondes, et c'est ce qui rend le tri possible
//
//   * le TRAVAIL : le JSON et les WAV que cet outil ecrit, dans un dossier de l'atelier ;
//   * le CONTENU : ce qui a survecu au tri, copie dans assets/content/ et embarque par resources.qrc.
//
// L'outil ne connait que le premier. C'est l'oreille qui fait passer de l'un a l'autre, et c'est exactement ce qu'on
// veut : un generateur ne decide pas de ce qui est beau.
//
// ---------------------------------------------------------------------------------------------------------------------
// Pourquoi la page HTML est ECRITE ici, et pas fournie a cote
//
// Parce qu'une page qu'on oublie de copier est une page qui manque le jour ou on en a besoin. L'outil ecrit donc son
// index.html lui-meme, avec la liste des phrases qu'il vient de produire : le dossier de sortie s'ouvre tel quel.
//
// Usage :
//   musichien_render_phrases [dossier] [--count N] [--seed N] [--mode identifiant]
// =====================================================================================================================

#include "domain/audio/ToneSynthesizer.h"
#include "domain/music/Mode.h"
#include "domain/music/Note.h"
#include "domain/music/Phrase.h"
#include "tools/WavFile.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{

using musichien::domain::Mode;
using musichien::domain::MODE_COUNT;
using musichien::domain::Note;
using musichien::domain::Phrase;
using musichien::domain::ToneSynthesizer;

constexpr std::int32_t PHRASE_SAMPLE_RATE = 48000;

// La tonique des phrases ecoutees : re 3, donc un bourdon en re 2 sous la melodie. C'est la tonique de tous les
// tableaux de la note 27, et celle pour laquelle les echantillons de bourdon sont enregistres.
constexpr std::int32_t PHRASE_TONIC_MIDI_NUMBER = 50;
constexpr std::int32_t DRONE_ROOT_MIDI_NUMBER = 38;

// Le bourdon : la tonique tenue DEUX OCTAVES sous la melodie, et sa quinte. C'est le centre, et sans lui une phrase
// modale n'est qu'une suite de notes justes.
constexpr std::int32_t FIFTH_IN_SEMITONES = 7;
constexpr std::int32_t DRONE_OCTAVE_OFFSET = -24;

// La duree d'un temps, en millisecondes. Le tempo de la phrase est en battements par minute, et un temps vaut 60000/bpm.
constexpr double MILLISECONDS_PER_MINUTE = 60000.0;

// Les notes ne sont jamais collees : un silence court entre elles est ce qui fait entendre deux notes plutot qu'une
// glissade, exactement comme pour un intervalle melodique.
constexpr std::chrono::milliseconds PHRASE_NOTE_GAP{ 60 };

// Le bourdon sonne seul avant et apres la phrase : c'est ce qui installe le centre avant la couleur.
constexpr std::chrono::milliseconds PHRASE_LEAD_IN{ 900 };
constexpr std::chrono::milliseconds PHRASE_TAIL{ 700 };

// Ce qu'une phrase devient sur le disque : son nom de fichier, et le WAV ecrit.
struct RenderedPhrase
{
    std::string fileName;
    std::string modeIdentifier;
    std::string tonicName;
    std::int32_t bpm{ 72 };
    std::size_t stepCount{ 0 };
    std::string degrees;    // « 1 4 5 1 », tel qu'un humain le lit
};

// Rend une phrase : sa melodie, avec ses DUREES, sur le bourdon qui tient dessous.
//
// Les durees sont la raison pour laquelle ce n'est pas un simple appel a renderMelodyOverDrone : une note de deux temps
// et une note d'un temps ne sont pas la meme phrase, et un generateur qui les produirait pour les aplatir aussitot
// n'aurait rien produit du tout.
[[nodiscard]] std::vector<float> renderPhraseSamples( const ToneSynthesizer & p_synthesizer,
                                                      const Phrase & p_phrase,
                                                      Note p_melodyTonic )
{
    const std::vector<Note> notes = p_phrase.notes( p_melodyTonic );

    const double millisecondsPerBeat = MILLISECONDS_PER_MINUTE / static_cast<double>( p_phrase.bpm );

    const std::size_t gapSampleCount = p_synthesizer.sampleCountFor( PHRASE_NOTE_GAP );

    std::vector<float> melody;

    for( std::size_t index = 0; index < notes.size(); ++index )
    {
        const auto duration = std::chrono::milliseconds{
          static_cast<std::int64_t>( millisecondsPerBeat * static_cast<double>( p_phrase.steps.at( index ).beats ) )
        };

        const std::vector<float> note = p_synthesizer.renderNote( notes.at( index ), duration );

        melody.insert( melody.end(), note.begin(), note.end() );
        melody.insert( melody.end(), gapSampleCount, 0.0F );
    }

    // Le bourdon : la tonique deux octaves sous la melodie, et sa quinte, tenues du debut a la fin - encadrement compris.
    const Note droneRoot{ DRONE_ROOT_MIDI_NUMBER };

    const std::array<Note, 2> drone{ droneRoot, droneRoot.transposedBy( FIFTH_IN_SEMITONES ) };

    const auto melodyDuration = std::chrono::milliseconds{
      static_cast<std::int64_t>( millisecondsPerBeat ) * static_cast<std::int64_t>( p_phrase.steps.size() )
    };

    const auto droneDuration = PHRASE_LEAD_IN + melodyDuration + PHRASE_TAIL;

    const std::vector<float> droneSamples =
      p_synthesizer.renderWaveChord( drone, musichien::domain::Waveform::Organ, droneDuration );

    const std::size_t leadInSampleCount = p_synthesizer.sampleCountFor( PHRASE_LEAD_IN );

    const std::size_t totalSampleCount = std::max( droneSamples.size(), leadInSampleCount + melody.size() );

    std::vector<float> mixed( totalSampleCount, 0.0F );

    for( std::size_t index = 0; index < totalSampleCount; ++index )
    {
        const float droneSample = ( index < droneSamples.size() ) ? droneSamples[index] : 0.0F;

        float melodySample = 0.0F;

        if( ( index >= leadInSampleCount ) && ( ( index - leadInSampleCount ) < melody.size() ) )
        {
            melodySample = melody[index - leadInSampleCount];
        }

        // Le meme rapport que dans le jeu : le bourdon est SENTI, la melodie s'entend.
        mixed.at( index ) = melodySample + ( droneSample * ToneSynthesizer::DRONE_GAIN );
    }

    return mixed;
}

// Le fichier JSON des phrases : ce que l'atelier garde, et ce qu'un humain peut editer.
void writePhraseJson( const std::filesystem::path & p_path,
                      const std::vector<Phrase> & p_phrases,
                      const std::vector<RenderedPhrase> & p_rendered )
{
    std::ofstream json{ p_path };

    json << "{\n  \"phrases\": [\n";

    for( std::size_t index = 0; index < p_phrases.size(); ++index )
    {
        const Phrase & phrase = p_phrases.at( index );
        const RenderedPhrase & rendered = p_rendered.at( index );

        json << "    { \"fichier\": \"" << rendered.fileName << "\", \"mode\": \"" << rendered.modeIdentifier
             << "\", \"tonique\": \"" << rendered.tonicName << "\", \"bpm\": " << rendered.bpm << ", \"degres\": [";

        for( std::size_t stepIndex = 0; stepIndex < phrase.steps.size(); ++stepIndex )
        {
            if( stepIndex > 0 )
            {
                json << ", ";
            }

            json << "{ \"degre\": " << phrase.steps.at( stepIndex ).degree << ", \"duree\": "
                 << phrase.steps.at( stepIndex ).beats << " }";
        }

        json << "] }" << ( ( index + 1 < p_phrases.size() ) ? "," : "" ) << "\n";
    }

    json << "  ]\n}\n";
}

// La page qui permet d'ECOUTER et de TRIER : ecrite par l'outil, pour qu'il n'y ait pas de page a copier.
//
// Elle ne fait que deux choses, et c'est assez : jouer un fichier, et montrer les degres de la phrase. Le tri lui-meme
// reste ce qu'il doit etre - un geste de l'oreille, une fois le casque sur la tete.
void writeIndexHtml( const std::filesystem::path & p_path, const std::vector<RenderedPhrase> & p_rendered )
{
    std::ofstream html{ p_path };

    html << "<!DOCTYPE html>\n<html lang='fr'>\n<head>\n<meta charset='utf-8'>\n";
    html << "<title>Atelier des phrases</title>\n<style>\n";
    html << "body{background:#1d1033;color:#e8dcff;font-family:sans-serif;padding:16px}\n";
    html << "li{margin:8px 0;padding:8px;border:1px solid #4a3170;border-radius:8px;list-style:none}\n";
    html << ".mode{color:#8ef2b0}.degres{color:#cbb8e8;font-size:13px}\n";
    html << "audio{vertical-align:middle;height:32px}\n</style>\n</head>\n<body>\n";
    html << "<h1>Atelier des phrases</h1>\n";
    html << "<p>Ecoute, puis garde ce qui te parle. Le tri se fait en deplacant les WAV retenus vers "
            "assets/content.</p>\n<ul>\n";

    for( const RenderedPhrase & rendered : p_rendered )
    {
        html << "<li><audio controls src='" << rendered.fileName << "'></audio> ";
        html << "<span class='mode'>" << rendered.modeIdentifier << "</span> ";
        html << "<span class='degres'>" << rendered.degrees << "</span></li>\n";
    }

    html << "</ul>\n</body>\n</html>\n";
}

}    // namespace

int main( int p_argumentCount, char * p_arguments[] )
{
    std::filesystem::path outputDirectory{ "phrases" };
    int phraseCount = 24;
    std::uint32_t seed = 20261001U;
    std::optional<Mode> onlyMode;

    for( int argumentIndex = 1; argumentIndex < p_argumentCount; ++argumentIndex )
    {
        const std::string_view argument{ p_arguments[argumentIndex] };

        const bool hasValue = ( argumentIndex + 1 ) < p_argumentCount;

        if( ( argument == "--count" ) && hasValue )
        {
            const std::string_view value{ p_arguments[argumentIndex + 1] };

            if( std::from_chars( value.data(), value.data() + value.size(), phraseCount ).ec != std::errc{} )
            {
                std::cerr << "Musichien: " << value << " is not a count\n";

                return 1;
            }

            ++argumentIndex;
        }
        else if( ( argument == "--seed" ) && hasValue )
        {
            const std::string_view value{ p_arguments[argumentIndex + 1] };

            if( std::from_chars( value.data(), value.data() + value.size(), seed ).ec != std::errc{} )
            {
                std::cerr << "Musichien: " << value << " is not a seed\n";

                return 1;
            }

            ++argumentIndex;
        }
        else if( ( argument == "--mode" ) && hasValue )
        {
            onlyMode = musichien::domain::modeFromIdentifier( p_arguments[argumentIndex + 1] );

            if( !onlyMode.has_value() )
            {
                std::cerr << "Musichien: " << p_arguments[argumentIndex + 1] << " is not a mode\n";

                return 1;
            }

            ++argumentIndex;
        }
        else
        {
            outputDirectory = argument;
        }
    }

    std::error_code error;
    std::filesystem::create_directories( outputDirectory, error );

    const ToneSynthesizer synthesizer{ PHRASE_SAMPLE_RATE };

    const Note tonic{ PHRASE_TONIC_MIDI_NUMBER };

    std::mt19937 randomEngine{ seed };

    std::vector<Phrase> phrases;
    std::vector<RenderedPhrase> rendered;

    const auto modeCount = static_cast<std::int32_t>( MODE_COUNT );

    for( int phraseIndex = 0; phraseIndex < phraseCount; ++phraseIndex )
    {
        // Un mode par phrase, en tournant : une serie de vingt-quatre donne donc trois ou quatre phrases de chaque mode,
        // ce qui suffit a entendre si le generateur dit quelque chose d'un mode ou s'il n'en dit rien.
        const auto modeIndex = onlyMode.has_value() ? static_cast<std::size_t>( *onlyMode )
                                                    : static_cast<std::size_t>( phraseIndex % modeCount );

        const auto mode = static_cast<Mode>( modeIndex );

        Phrase phrase = musichien::domain::generatePhrase( mode, tonic, randomEngine );

        const std::vector<float> samples = renderPhraseSamples( synthesizer, phrase, tonic );

        const std::string fileName = "phrase_" + std::to_string( phraseIndex + 1 ) + "_"
                                     + std::string( musichien::domain::modeIdentifier( mode ) ) + ".wav";

        musichien::tools::writeWavFile( outputDirectory / fileName, samples, PHRASE_SAMPLE_RATE );

        // Les degres, ecrits comme un musicien les lit : un chiffre par pas, et la duree quand elle depasse un temps.
        std::string degrees;

        for( const musichien::domain::PhraseStep & step : phrase.steps )
        {
            if( !degrees.empty() )
            {
                degrees += " ";
            }

            degrees += std::to_string( step.degree );

            if( step.beats > 1 )
            {
                degrees += "(" + std::to_string( step.beats ) + ")";
            }
        }

        rendered.push_back( RenderedPhrase{ .fileName = fileName,
                                            .modeIdentifier = std::string( musichien::domain::modeIdentifier( mode ) ),
                                            .tonicName = tonic.name(),
                                            .bpm = phrase.bpm,
                                            .stepCount = phrase.steps.size(),
                                            .degrees = degrees } );

        phrases.push_back( std::move( phrase ) );
    }

    writePhraseJson( outputDirectory / "phrases.json", phrases, rendered );
    writeIndexHtml( outputDirectory / "index.html", rendered );

    std::cout << "Musichien: " << rendered.size() << " phrase(s) written in " << outputDirectory.string() << '\n';
    std::cout << "Musichien: open " << ( outputDirectory / "index.html" ).string() << " to listen and tri\n";

    return 0;
}
