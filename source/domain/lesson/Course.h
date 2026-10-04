#pragma once

// =====================================================================================================================
// Musichien - Course
//
// A lesson: what the School teaches, held as DATA.
//
// ---------------------------------------------------------------------------------------------------------------------
// Why a SEQUENCE of blocks, and not one big string of Markdown
//
// A lesson is not a page of text with a few links in it. It is a text that carries CARDS: something to
// play, something to listen to, something to try. Keeping the whole file as one string would mean
// finding those cards again later by scanning that string - in the interface, where nothing can be
// tested. Here, the order of the vector IS the order of the file, and it is data.
//
// ---------------------------------------------------------------------------------------------------------------------
// What the domain knows, and what it leaves alone
//
// No path, no Markdown renderer, no Qt. It says WHICH interval to play (a distance in semitones - the
// only thing the domain recognises) and WHERE a card leads (a URL, treated as an opaque string).
// Rendering it is the interface's business, and so is opening it.
//
// See the contract: notes 29 of the Vault, and MarkdownCourse.h in the infrastructure.
// =====================================================================================================================

#include "domain/music/Interval.h"

#include <cstdint>
#include <string>
#include <vector>

namespace musichien::domain
{

// One element of a lesson, in the order its writer put it.
struct CourseBlock
{
    enum class Kind
    {
        // A paragraph of Markdown, displayed as it is.
        Text,

        // ":: jeu" - the game plays this interval: nothing to open, nothing to download, no network.
        PlayInterval,

        // ":: écoute" - a card that LEAVES the application: the real music, elsewhere.
        Listen,

        // ":: essai" - the button that starts the matching exercise.
        TryExercise,

        // ":: chante" - the button that OPENS THE SINGING TOOL on this interval.
        //
        // Roger, sur le cours de la quinte juste : « on propose au joueur de chanter la quinte. Autant lui fournir l'outil
        // pour qu'il verifie lui-meme s'il chante juste. » Un cours qui demande une chose et ne donne pas le moyen de la
        // verifier est un cours qui laisse le joueur deviner - et c'est le seul endroit du jeu ou la voix sert.
        SingInterval,

        // ":: annexe" - the way to the long annexe.
        Annexe
    };

    Kind kind{ Kind::Text };

    // Kind::Text - the Markdown source of the paragraph, whole.
    std::string markdown;

    // Kind::PlayInterval and Kind::TryExercise - WHICH interval, as a distance in semitones.
    //
    // Never a name. A name is a label that gets renamed; a distance is what the domain understands,
    // and it is already how the memory hints are keyed. Two spellings of the same interval can never
    // become two entries here.
    std::int32_t semitones{ 0 };

    // Kind::PlayInterval - how it is sounded.
    IntervalDirection direction{ IntervalDirection::Ascending };

    // Kind::PlayInterval - what the card says under its button, as written by the lesson.
    std::string caption;

    // Kind::Listen - the card, whole.
    std::string source;    // "youtube" or "spotify", kept exactly as the file wrote it
    std::string url;
    std::string title;
    std::string listenFor;    // what to hear in it: required by the contract (Mayer's signalling)

    // Kind::Annexe - the name of the annexe, as that file's own front matter gives it.
    std::string annexeName;
};

// UNE SECTION, c'est-a-dire un « ## » du fichier et ce qu'il introduit.
//
// Roger : « je trouve que le cours en forme de grosse note qui descend, c'est bien mais un peu lourd. Je verrais plus ca
// comme plusieurs pages par chapitre : "Deux notes et rien entre elles", "L'ecoute"... Et a la fin, on verrait la note
// complete pour s'y referer. »
//
// Il a raison, et la structure est DEJA dans les fichiers : les titres de niveau 2 sont exactement les pages qu'il
// decrit - mon propre specimen en a sept. Les aplatir pour tout afficher d'un bloc, c'etait jeter une decoupe que
// l'auteur avait deja faite.
struct CourseSection
{
    // Le titre du « ## », sans les diese. VIDE pour ce qui precede le premier titre : un chapeau, s'il y en a un.
    std::string title;

    std::vector<CourseBlock> blocks;
};

// A lesson, ready to be shown.
struct Course
{
    std::string title;
    std::string subtitle;

    int chapter{ 0 };
    int order{ 0 };

    // What this lesson teaches, as distances in semitones.
    //
    // This is THE link with the game. The same value the hints are keyed on, so a lesson can name the
    // exercise it belongs to - and feed the review plan - without knowing anything about either.
    std::vector<std::int32_t> concepts;

    // La lecon, dans l'ordre, decoupee en pages.
    std::vector<CourseSection> sections;

    // LES MEMES BLOCS, A PLAT. Les deux existent, et c'est un choix assume : la page lit les SECTIONS, et le « voir la
    // note complete » lit la liste plate. Les deux sortent du MEME passage de lecture, donc elles ne peuvent pas
    // diverger - mais les recalculer l'une depuis l'autre a chaque affichage couterait une copie pour rien.
    std::vector<CourseBlock> blocks;

    [[nodiscard]] bool isEmpty() const noexcept { return blocks.empty(); }
};

}    // namespace musichien::domain
