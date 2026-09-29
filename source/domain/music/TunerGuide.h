#pragma once

// =====================================================================================================================
// Musichien - TunerGuide
//
// Les TEXTES de la page Accordeur : ce qu'un temperament est, d'ou il vient, comment il fonctionne, ce que sont le
// diapason et la note de reference, et comment se servir d'un accordeur.
//
// Pourquoi ces textes ne sont pas ecrits dans le QML : c'est du CONTENU, et le contenu est de la donnee. Le fichier qui
// les porte (assets/content/tuner.json) se corrige et s'enrichit sans toucher a une ligne de code - exactement comme
// les anecdotes et les indices.
//
// Pourquoi dans le domaine, et pas dans l'infrastructure : ranger un texte par TEMPERAMENT demande de savoir ce qu'est
// un temperament. C'est une connaissance du domaine ; l'infrastructure, elle, ne connait que le JSON.
//
// Un guide vide est un cas NORMAL : un fichier manquant ou casse coute les explications, jamais l'application. L'ecran
// garde alors ses propres phrases, plus courtes.
// =====================================================================================================================

#include "domain/music/Temperament.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace musichien::domain
{

class TunerGuide
{
public:
    // Le texte d'un temperament. Vide si rien n'a ete lu pour celui-la.
    void setTemperamentText( Temperament p_temperament, std::string p_text );

    [[nodiscard]] std::string_view temperamentText( Temperament p_temperament ) const noexcept;

    void setDiapasonText( std::string p_text );

    [[nodiscard]] std::string_view diapasonText() const noexcept { return m_diapasonText; }

    void setReferenceNoteText( std::string p_text );

    [[nodiscard]] std::string_view referenceNoteText() const noexcept { return m_referenceNoteText; }

    // Le mode d'emploi, une phrase par etape, dans l'ordre ou on les lit.
    void addHowToStep( std::string p_step );

    [[nodiscard]] const std::vector<std::string> & howToSteps() const noexcept { return m_howToSteps; }

    // Combien de textes de temperament ont ete lus : c'est ce que l'application annonce au demarrage, et c'est la
    // preuve la plus courte que le fichier de contenu a bien suivi jusqu'au binaire.
    [[nodiscard]] std::size_t temperamentCount() const noexcept;

    // Vrai quand rien n'a pu etre lu du tout.
    [[nodiscard]] bool isEmpty() const noexcept;

private:
    std::array<std::string, TEMPERAMENT_NAMES.size()> m_temperamentTexts;
    std::string m_diapasonText;
    std::string m_referenceNoteText;
    std::vector<std::string> m_howToSteps;
};

}    // namespace musichien::domain
