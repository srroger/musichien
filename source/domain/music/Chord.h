#pragma once

// =====================================================================================================================
// Musichien - Chord
//
// Un accord, comme REGLE PURE : quelles notes le composent, dans quel ordre on les apprend, et quel nom on lui donne.
// Ni Qt, ni son, ni horloge - un accord est une liste de distances depuis une tonique, et rien d'autre.
//
// ---------------------------------------------------------------------------------------------------------------------
// Pourquoi l'ordre d'apprentissage est ecrit ici, et pas dans un ecran
//
// C'est une decision MUSICALE, comme l'ordre des intervalles (voir LearningOrder.h), et elle se defend :
//
//   * les deux premieres couleurs sont majeur et mineur, parce que tout le reste se compare a elles ; on ne connait ni
//     l'une ni l'autre sans l'autre ;
//   * les suspendues viennent ensuite : c'est la tierce REMPLACEE, donc une seule chose a entendre quand les deux
//     premieres sont en place ;
//   * les deux tendues (diminue, augmente) apres, la quinte y est serree ou elargie, et c'est ce qui s'entend ;
//   * les accords a QUATRE notes en dernier, et dans l'ordre ou l'oreille les rencontre : la septieme de dominante
//     d'abord, puis la majeur, puis la mineur - qui ne different de la premiere que par une seule note.
//
// La source de cet ordre est la progression des cours d'harmonie (musictheory.net : les quatre triades, puis les
// septiemes) ; elle est notee dans le Vault, note 24.
// =====================================================================================================================

#include "domain/music/Note.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace musichien::domain
{

// Les qualites d'accord que le jeu fait entendre.
//
// L'ordre de l'enumeration EST l'ordre d'apprentissage : une palette est un PREFIXE de cette liste, et un rang plus
// grand veut dire un accord plus difficile. Changer l'ordre, c'est changer la difficulte de tout le monde.
enum class ChordQuality : std::size_t
{
    Major = 0,
    Minor = 1,
    Sus4 = 2,
    Sus2 = 3,
    Diminished = 4,
    Augmented = 5,
    DominantSeventh = 6,
    MajorSeventh = 7,
    MinorSeventh = 8
};

// Combien de qualites le domaine connait, ce qu'un ecran a besoin de savoir pour proposer une liste complete.
inline constexpr std::size_t CHORD_QUALITY_COUNT = 9;

// La tonique est TOUJOURS la premiere note jouee, et c'est une convention du projet : la note de reference s'entend
// d'abord, comme la tonique d'un intervalle. Elle vaut zero demi-ton, par definition.
inline constexpr std::int32_t CHORD_ROOT_SEMITONES = 0;

// Les intervalles d'une qualite, en demi-tons depuis la tonique, du grave vers l'aigu. Le premier est toujours zero.
//
// C'est la donnee dont tout le reste decoule : les notes jouees, le nombre de notes, et (plus tard) l'exercice qui
// demande de retrouver ces intervalles un par un.
[[nodiscard]] std::span<const std::int32_t> chordIntervals( ChordQuality p_quality ) noexcept;

// Combien de notes compte un accord de cette qualite : trois pour une triade, quatre pour une septieme.
[[nodiscard]] std::size_t chordNoteCount( ChordQuality p_quality ) noexcept;

// Le nom d'une qualite, tel qu'un musicien le lit. Neutre et anglais, comme les noms d'intervalles du domaine : une
// traduction est un travail de traduction, pas une affaire de modele.
[[nodiscard]] std::string_view chordQualityName( ChordQuality p_quality ) noexcept;

// La fin du symbole d'un accord : vide pour majeur (un accord majeur ne s'annonce pas), "m", "sus4", "dim", "aug",
// "7", "maj7", "m7". Un symbole se COLLE a la tonique ("Cm", "C7"), il ne la remplace pas.
[[nodiscard]] std::string_view chordQualitySymbolSuffix( ChordQuality p_quality ) noexcept;

// Toutes les qualites, de la plus simple a la plus riche.
[[nodiscard]] std::span<const ChordQuality> chordLearningOrder() noexcept;

// Le palette d'un joueur qui connait les p_qualityCount premieres qualites de l'ordre.
//
// Toujours au moins une qualite : une question qui n'offrirait rien a choisir ne serait pas une question. Demander
// plus que l'ordre n'en contient rend l'ordre entier.
[[nodiscard]] std::vector<ChordQuality> beginnerChordPalette( std::size_t p_qualityCount );

// Un accord, en pratique : une qualite, posee sur une tonique.
struct Chord
{
    ChordQuality quality{ ChordQuality::Major };

    // La note grave de l'accord, en numero MIDI. Tout l'accord est construit au-dessus d'elle.
    std::int32_t rootMidiNumber{ 60 };

    // Les notes, du grave vers l'aigu, tonique comprise : ce que l'adaptateur audio a besoin de savoir, et rien de
    // plus. Une fonction plutot qu'un membre : un accord ne STOCKE pas ses notes, il les calcule depuis sa qualite.
    [[nodiscard]] std::vector<Note> notes() const;
};

}    // namespace musichien::domain
