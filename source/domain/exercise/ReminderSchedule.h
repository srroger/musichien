#pragma once

// =====================================================================================================================
// Musichien - ReminderSchedule
//
// QUAND l'application a le droit de se rappeler au joueur. C'est une decision de game design, pas une constante
// technique : elle vit donc dans le domaine, ou elle se discute, se teste et se retouche.
//
// ---------------------------------------------------------------------------------------------------------------------
// Quatre notifications par jour, et c'est demande
//
// Plusieurs notifications par jour : une le matin avant 9 h, une le midi, et une le soir apres 18 h. L'idee est
// d'occuper le terrain comme le ferait un fil d'actualite - mais pour la bonne cause : s'instruire en musique avec le
// sourire, grace aux anecdotes, qui peuvent donner envie de lancer une petite partie.
//
// Plus son rappel d'entrainement, cela fait QUATRE messages dans une journee.
//
// Le contenu n'est pas ici : c'est du CONTENU, et il vit avec les anecdotes (assets/content/anecdotes.json).
// =====================================================================================================================

#include <algorithm>
#include <array>
#include <cstddef>

namespace musichien::domain
{

// Un instant de la journee. Des heures et des minutes, jamais un instant absolu : une notification quotidienne se repete
// tous les jours a la meme heure, et c'est ce qui la distingue d'un rendez-vous.
struct ReminderMoment
{
    int hour{ 0 };
    int minute{ 0 };
};

// Les trois moments des ANECDOTES : le matin, le midi, le soir.
//
// 8 h 00, 12 h 30 et 20 h 30, et ces heures ne sont pas un hasard :
//
//   * 8 h tombe AVANT 9 h : c'est le premier moment ou l'on regarde son telephone ;
//   * 12 h 30 tombe a table, la ou une anecdote se lit bien ;
//   * 20 h 30 tombe APRES 18 h, apres le diner, et laisse une heure et demie libre apres le rappel d'entrainement.
//
// Trois moments FIXES plutot que tires au hasard : un rendez-vous regulier devient une habitude, et une heure qui bouge
// chaque jour n'en devient jamais une. Un tirage partiel sur le NOMBRE de notifications par jour reste possible plus
// tard, mais trois par defaut est ce qui doit s'installer d'abord : c'est ce nombre qui fait une habitude.
inline constexpr std::array<ReminderMoment, 3> ANECDOTE_REMINDER_MOMENTS{
  ReminderMoment{ .hour = 8, .minute = 0 },
  ReminderMoment{ .hour = 12, .minute = 30 },
  ReminderMoment{ .hour = 20, .minute = 30 },
};

// Combien de creneaux une journee peut occuper, anecdotes et rappel compris.
//
// HUIT, et c'est la meme valeur que cote Android : quatre notifications ont la place, et une cinquieme - un jour,
// peut-etre - n'obligera pas a toucher au Java.
inline constexpr std::size_t REMINDER_SLOT_COUNT = 8;

// Un moment ramene de force dans une journee : 25 h n'existe pas, et -1 non plus.
//
// Un reglage vient d'un fichier qu'un joueur peut ouvrir, ou d'un ecran qui peut se tromper. Plutot que de refuser - ce
// qui le laisserait sans aucun rappel, sans rien lui dire - on RAMENE la valeur dans les bornes : une heure fausse
// coute une heure, jamais le rappel.
[[nodiscard]] constexpr ReminderMoment clampedReminderMoment( ReminderMoment p_moment ) noexcept
{
    return ReminderMoment{ .hour = std::clamp( p_moment.hour, 0, 23 ),
                           .minute = std::clamp( p_moment.minute, 0, 59 ) };
}

}    // namespace musichien::domain
