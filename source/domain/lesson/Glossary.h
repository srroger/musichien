#pragma once

// =====================================================================================================================
// Musichien - Glossary
//
// LE GLOSSAIRE : un mot par entree, et une definition d'UNE PHRASE.
//
// C'est de la DONNEE, comme le livre d'anecdotes et les cours : le domaine la porte, l'infrastructure la remplit depuis
// un fichier, et ajouter un mot ne demande aucun rebuild du code.
//
// CE QUE C'EST, ET CE QUE CE N'EST PAS. Roger, 04/10/2026 : « je veux que ca fasse vraiment liste de dico, pas des
// chapitres ou des icones ou de mise en page particuliere. Des definitions courtes, c'est plus un aide-memoire qu'un vrai
// dictionnaire. » Une definition qui a besoin d'un paragraphe n'est pas un mot de glossaire : c'est un COURS.
//
// L'ORDRE, LUI, NE VIT PAS ICI. « Comme un dictionnaire » est une regle d'AFFICHAGE - elle depend de la casse, des
// accents et de la langue - et c'est donc le modele de vue qui la tient. Le domaine ne sait pas ce qu'est une lettre de
// section, et il n'a pas a le savoir.
// =====================================================================================================================

#include <string>

namespace musichien::domain
{

// Un mot et ce qu'il veut dire, en une phrase.
struct GlossaryEntry
{
    std::string word;
    std::string definition;
};

}    // namespace musichien::domain