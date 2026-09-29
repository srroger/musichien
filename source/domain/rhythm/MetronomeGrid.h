#pragma once

// =====================================================================================================================
// Musichien - MetronomeGrid
//
// LA GRILLE D'UN METRONOME, EXPRIMEE EN ECHANTILLONS : ou tombe le temps numero n, et lequel tombe dans les
// echantillons qu'on s'apprete a ecrire. C'est le coeur de la fiabilite du metronome, et c'est pour cela qu'il vit ici,
// dans le domaine, ou il est pur et testable.
//
// ---------------------------------------------------------------------------------------------------------------------
// Pourquoi des echantillons, et pas des millisecondes
//
// Parce qu'un metronome juste est un metronome qui COMPTE DES ECHANTILLONS. Le flux audio sort a 48000 par seconde,
// chaque echantillon a une position exacte, et une position de temps calculee en entiers ne peut ni deriver ni
// trembler.
//
// Un QTimer, lui, mesure le temps du THREAD D'INTERFACE : entre deux tics il y a le rendu de QML, les evenements du
// systeme, le ramasse-miettes. Le clic tombe donc la ou le thread a bien voulu, avec plusieurs millisecondes de
// gigue - et c'est cette gigue qui s'entend comme une instabilite. Aucune correction de derive ne peut la retirer :
// un retard qui se rattrape n'est pas un retard qui n'existe pas.
//
// ---------------------------------------------------------------------------------------------------------------------
// L'arrondi, et pourquoi il ne s'accumule jamais
//
// Un temps a rarement un nombre ENTIER d'echantillons : a 130 bpm et 48000 Hz, il en fait 22153,846... La position du
// temps n est donc arrondie UNE SEULE FOIS, depuis n, et jamais obtenue en ajoutant la duree d'un temps a celle du
// precedent : quinze cents arrondis mis bout a bout feraient un metronome qui derape d'un dixieme de seconde au bout
// d'un quart d'heure. Calculee depuis n, l'erreur reste bornee a un demi-echantillon, pour toujours.
// =====================================================================================================================

#include <cstdint>

namespace musichien::domain
{

class MetronomeGrid
{
public:
    // Le taux d'echantillonnage du flux audio : c'est lui qui traduit un tempo en un nombre d'echantillons, et il ne
    // change pas pendant la vie d'une grille. Un appareil qui change de taux en cours de route fait renaitre une
    // grille, ce qui est le seul comportement honnete.
    explicit MetronomeGrid( int p_sampleRate ) noexcept;

    // Demarre la grille : le temps 0 tombe a p_startFrame, et les suivants s'espacent d'un temps au tempo donne. Une
    // grille arretee est l'etat par defaut : rien ne bat tant que personne ne l'a demande.
    void start( double p_bpm, int p_beatsPerBar, std::int64_t p_startFrame ) noexcept;

    void stop() noexcept;

    [[nodiscard]] bool isRunning() const noexcept { return m_isRunning; }

    [[nodiscard]] int beatsPerBar() const noexcept { return m_beatsPerBar; }

    // Vrai pour le premier temps d'une mesure, qui est le temps ACCENTUE.
    [[nodiscard]] bool isAccented( std::int64_t p_beatIndex ) const noexcept;

    // La position, en echantillons, du temps de rang p_beatIndex depuis le demarrage de la grille.
    [[nodiscard]] std::int64_t frameOfBeat( std::int64_t p_beatIndex ) const noexcept;

    // Un temps : son rang, et la position ou il tombe.
    struct Beat
    {
        std::int64_t index{ 0 };
        std::int64_t frame{ 0 };
    };

    // Le premier temps qui tombe a p_frame ou apres. C'est la question que pose un moteur audio a chaque tampon :
    // « quels temps tombent dans les echantillons que je m'apprete a ecrire ? »
    [[nodiscard]] Beat firstBeatAtOrAfter( std::int64_t p_frame ) const noexcept;

    // Le rang du temps qui EST en cours a cette position : le dernier temps tombe. C'est ce que l'interface affiche.
    [[nodiscard]] std::int64_t beatIndexAt( std::int64_t p_frame ) const noexcept;

    // Combien de millisecondes se sont ecoulees depuis le demarrage de la grille. C'est contre cette duree qu'une
    // frappe est jugee, et non contre une horloge d'interface.
    [[nodiscard]] double elapsedMsAt( std::int64_t p_frame, int p_sampleRate ) const noexcept;

private:
    // Le rang du temps a une position, avant les ajustements d'arrondi. Peut etre negatif avant le demarrage.
    [[nodiscard]] std::int64_t approximateBeatIndexAt( std::int64_t p_frame ) const noexcept;

    bool m_isRunning{ false };
    double m_framesPerBeat{ 0.0 };
    std::int64_t m_startFrame{ 0 };
    int m_beatsPerBar{ 4 };
    int m_sampleRate{ 0 };
};

}    // namespace musichien::domain
