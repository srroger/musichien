#include "domain/music/Chord.h"

#include <algorithm>
#include <array>

namespace musichien::domain
{

namespace
{

// Les intervalles de chaque qualite, en demi-tons depuis la tonique.
//
// Un tableau par qualite, et un span par-dessus : c'est ce qui permet a chordIntervals de ne rien allouer tout en
// restant une fonction. Ces nombres sont LA definition musicale de l'accord - tout le reste en decoule.
constexpr std::array<std::int32_t, 3> MAJOR_INTERVALS{ 0, 4, 7 };
constexpr std::array<std::int32_t, 3> MINOR_INTERVALS{ 0, 3, 7 };
constexpr std::array<std::int32_t, 3> SUS4_INTERVALS{ 0, 5, 7 };
constexpr std::array<std::int32_t, 3> SUS2_INTERVALS{ 0, 2, 7 };
constexpr std::array<std::int32_t, 3> DIMINISHED_INTERVALS{ 0, 3, 6 };
constexpr std::array<std::int32_t, 3> AUGMENTED_INTERVALS{ 0, 4, 8 };
constexpr std::array<std::int32_t, 4> DOMINANT_SEVENTH_INTERVALS{ 0, 4, 7, 10 };
constexpr std::array<std::int32_t, 4> MAJOR_SEVENTH_INTERVALS{ 0, 4, 7, 11 };
constexpr std::array<std::int32_t, 4> MINOR_SEVENTH_INTERVALS{ 0, 3, 7, 10 };
constexpr std::array<std::int32_t, 4> SIXTH_INTERVALS{ 0, 4, 7, 9 };
constexpr std::array<std::int32_t, 4> HALF_DIMINISHED_INTERVALS{ 0, 3, 6, 10 };
constexpr std::array<std::int32_t, 4> DIMINISHED_SEVENTH_INTERVALS{ 0, 3, 6, 9 };
constexpr std::array<std::int32_t, 4> MINOR_MAJOR_SEVENTH_INTERVALS{ 0, 3, 7, 11 };

// La neuvieme AJOUTEE garde ses trois notes de base et gagne une seconde une octave plus haut ; la neuvieme complete
// garde aussi la septieme mineure. C'est la difference entre "C add9" et "C9", et l'oreille l'entend.
constexpr std::array<std::int32_t, 4> ADD9_INTERVALS{ 0, 4, 7, 14 };
constexpr std::array<std::int32_t, 5> NINTH_INTERVALS{ 0, 4, 7, 10, 14 };

// L'ordre d'apprentissage, et il repete l'ordre de l'enumeration : la liste est ecrite pour etre LUE (et pour qu'un
// test puisse verifier que les deux ne divergent pas).
constexpr std::array<ChordQuality, CHORD_QUALITY_COUNT> LEARNING_ORDER{
  ChordQuality::Major,
  ChordQuality::Minor,
  ChordQuality::Sus4,
  ChordQuality::Sus2,
  ChordQuality::Diminished,
  ChordQuality::Augmented,
  ChordQuality::DominantSeventh,
  ChordQuality::MajorSeventh,
  ChordQuality::MinorSeventh,
  ChordQuality::Sixth,
  ChordQuality::HalfDiminished,
  ChordQuality::DiminishedSeventh,
  ChordQuality::MinorMajorSeventh,
  ChordQuality::Add9,
  ChordQuality::Ninth };

}    // namespace

std::span<const std::int32_t> chordIntervals( ChordQuality p_quality ) noexcept
{
    switch( p_quality )
    {
        case ChordQuality::Major:
            return MAJOR_INTERVALS;

        case ChordQuality::Minor:
            return MINOR_INTERVALS;

        case ChordQuality::Sus4:
            return SUS4_INTERVALS;

        case ChordQuality::Sus2:
            return SUS2_INTERVALS;

        case ChordQuality::Diminished:
            return DIMINISHED_INTERVALS;

        case ChordQuality::Augmented:
            return AUGMENTED_INTERVALS;

        case ChordQuality::DominantSeventh:
            return DOMINANT_SEVENTH_INTERVALS;

        case ChordQuality::MajorSeventh:
            return MAJOR_SEVENTH_INTERVALS;

        case ChordQuality::MinorSeventh:
            return MINOR_SEVENTH_INTERVALS;

        case ChordQuality::Sixth:
            return SIXTH_INTERVALS;

        case ChordQuality::HalfDiminished:
            return HALF_DIMINISHED_INTERVALS;

        case ChordQuality::DiminishedSeventh:
            return DIMINISHED_SEVENTH_INTERVALS;

        case ChordQuality::MinorMajorSeventh:
            return MINOR_MAJOR_SEVENTH_INTERVALS;

        case ChordQuality::Add9:
            return ADD9_INTERVALS;

        case ChordQuality::Ninth:
            return NINTH_INTERVALS;
    }

    // Aucune qualite ne tombe ici - l'enumeration est couverte en entier - mais une fonction qui rend un span ne peut
    // pas ne rien rendre, et l'accord majeur est la reponse la plus honnete qui soit.
    return MAJOR_INTERVALS;
}

std::size_t chordNoteCount( ChordQuality p_quality ) noexcept
{
    return chordIntervals( p_quality ).size();
}

std::string_view chordQualityName( ChordQuality p_quality ) noexcept
{
    switch( p_quality )
    {
        case ChordQuality::Major:
            return "Major";

        case ChordQuality::Minor:
            return "Minor";

        case ChordQuality::Sus4:
            return "Sus4";

        case ChordQuality::Sus2:
            return "Sus2";

        case ChordQuality::Diminished:
            return "Diminished";

        case ChordQuality::Augmented:
            return "Augmented";

        case ChordQuality::DominantSeventh:
            return "Dominant seventh";

        case ChordQuality::MajorSeventh:
            return "Major seventh";

        case ChordQuality::MinorSeventh:
            return "Minor seventh";

        case ChordQuality::Sixth:
            return "Sixth";

        case ChordQuality::HalfDiminished:
            return "Half-diminished";

        case ChordQuality::DiminishedSeventh:
            return "Diminished seventh";

        case ChordQuality::MinorMajorSeventh:
            return "Minor-major seventh";

        case ChordQuality::Add9:
            return "Added ninth";

        case ChordQuality::Ninth:
            return "Ninth";
    }

    return "Major";
}

std::string_view chordQualitySymbolSuffix( ChordQuality p_quality ) noexcept
{
    switch( p_quality )
    {
        case ChordQuality::Major:
            // Vide, et c'est juste : un accord majeur ne s'annonce pas. "C" EST un do majeur.
            return "";

        case ChordQuality::Minor:
            return "m";

        case ChordQuality::Sus4:
            return "sus4";

        case ChordQuality::Sus2:
            return "sus2";

        case ChordQuality::Diminished:
            return "dim";

        case ChordQuality::Augmented:
            return "aug";

        case ChordQuality::DominantSeventh:
            return "7";

        case ChordQuality::MajorSeventh:
            return "maj7";

        case ChordQuality::MinorSeventh:
            return "m7";

        case ChordQuality::Sixth:
            return "6";

        case ChordQuality::HalfDiminished:
            // Le symbole que les musiciens ecrivent vraiment : deux noms pour le meme accord, et c'est celui-la qui
            // tient sur une partition.
            return "m7b5";

        case ChordQuality::DiminishedSeventh:
            return "dim7";

        case ChordQuality::MinorMajorSeventh:
            return "mMaj7";

        case ChordQuality::Add9:
            return "add9";

        case ChordQuality::Ninth:
            return "9";
    }

    return "";
}

std::string_view chordQualityShortLabel( ChordQuality p_quality ) noexcept
{
    switch( p_quality )
    {
        case ChordQuality::Major:
            return "Maj";

        case ChordQuality::Minor:
            return "min";

        case ChordQuality::Sus4:
            return "sus4";

        case ChordQuality::Sus2:
            return "sus2";

        case ChordQuality::Diminished:
            return "dim";

        case ChordQuality::Augmented:
            return "aug";

        case ChordQuality::DominantSeventh:
            return "7";

        case ChordQuality::MajorSeventh:
            return "Maj7";

        case ChordQuality::MinorSeventh:
            return "min7";

        case ChordQuality::Sixth:
            return "6";

        case ChordQuality::HalfDiminished:
            return "m7b5";

        case ChordQuality::DiminishedSeventh:
            return "dim7";

        case ChordQuality::MinorMajorSeventh:
            return "mMaj7";

        case ChordQuality::Add9:
            return "add9";

        case ChordQuality::Ninth:
            return "9";
    }

    return "Maj";
}

std::span<const ChordQuality> chordLearningOrder() noexcept
{
    return LEARNING_ORDER;
}

std::vector<ChordQuality> beginnerChordPalette( std::size_t p_qualityCount )
{
    const std::span<const ChordQuality> order = chordLearningOrder();

    const std::size_t count = std::clamp( p_qualityCount, std::size_t{ 1 }, order.size() );

    // Une tranche du debut de l'ordre, par iterateurs : l'indexation d'un span est justement ce que le projet evite
    // (libc++ du NDK n'a pas de span::at(), donc l'idiome maison est la tranche, pas l'indice).
    const auto first = order.begin();

    return std::vector<ChordQuality>{ first, first + static_cast<std::ptrdiff_t>( count ) };
}

std::vector<Note> Chord::notes() const
{
    std::vector<Note> notes;

    const std::span<const std::int32_t> intervals = chordIntervals( quality );

    notes.reserve( intervals.size() );

    for( const std::int32_t semitones : intervals )
    {
        notes.emplace_back( rootMidiNumber + semitones );
    }

    return notes;
}

}    // namespace musichien::domain
