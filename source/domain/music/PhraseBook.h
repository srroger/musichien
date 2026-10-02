#pragma once

// =====================================================================================================================
// Musichien - PhraseBook
//
// Le livre des phrases modales : ce qu'un fichier de contenu a appris au jeu, range par MODE.
//
// ---------------------------------------------------------------------------------------------------------------------
// Le pendant de HintBook, et la meme repartition des roles
//
// Le contenu est de la DONNEE. L'atelier en propose trois cents, l'oreille de Roger en garde une poignee, et le fichier
// embarque ne porte que celles-la. Ce type est ou le domaine tient le resultat - et il ne decide RIEN de plus : ni
// quand jouer une phrase, ni combien de fois. Cela appartient a l'ecran ou a la question, qui sont seuls a savoir ce
// qu'ils font entendre.
//
// ---------------------------------------------------------------------------------------------------------------------
// Range par MODE, et non dans une seule liste
//
// Une question de mode demande une phrase DE SON mode : chercher dans trois cents phrases pour en tirer une de dorien
// serait un tri que le livre fait une fois pour toutes, a la lecture du fichier.
//
// Et un mode SANS phrase n'est pas une erreur : c'est un mode ou l'ecran proposera autre chose, ou pas de phrase du
// tout. Le livre repond alors une liste vide, ce qui est une reponse.
// =====================================================================================================================

#include "domain/music/Phrase.h"

#include <cstddef>
#include <map>
#include <optional>
#include <random>
#include <span>
#include <vector>

namespace musichien::domain
{

class PhraseBook
{
public:
    void add( Phrase p_phrase );

    // Les phrases d'un mode, dans l'ordre ou le contenu les a donnees, ou une liste vide.
    [[nodiscard]] std::span<const Phrase> phrasesFor( Mode p_mode ) const noexcept;

    // Une phrase d'un mode, tiree UNIFORMEMENT, ou rien quand le mode n'en a aucune.
    //
    // Le moteur est un parametre, comme partout dans ce domaine : le livre n'a aucune source d'entropie a lui, donc ce
    // qu'il tire peut etre rejoue - et un tirage qu'on ne peut pas rejouer ne se verifie pas.
    [[nodiscard]] std::optional<Phrase> drawPhraseFor( Mode p_mode,
                                                       std::mt19937 & p_randomEngine ) const;

    [[nodiscard]] std::size_t phraseCount() const noexcept;
    [[nodiscard]] std::size_t phraseCountFor( Mode p_mode ) const noexcept;

private:
    std::map<Mode, std::vector<Phrase>> m_phrases;
};

}    // namespace musichien::domain
