#pragma once

// =====================================================================================================================
// Musichien - VoicePreFilter
//
// LE PRE-TRAITEMENT DU CHANT, ET RIEN QUE DU CHANT.
//
// Roger : « il faut que l'accordeur reste fonctionnel et pas calibre pour la voix. Donc ce pre-process ne doit etre fait
// que pour les jeux. » C'est exactement pourquoi cette classe est SEPAREE de l'estimateur : l'accordeur garde la bande
// large et la finesse dont il a besoin pour nommer n'importe quelle note ; le chant, lui, recoit un signal de voix.
//
// Deux gestes, et ils sont complementaires :
//
//   1. UN PASSE-HAUT. Sous ~90 Hz il n'y a jamais de voix : il y a la circulation, la climatisation, le pas, et le bruit
//      de manipulation du telephone. Ce sont eux qui faisaient devier le creux de YIN - une periode de grondement est
//      une periode comme une autre pour un estimateur qui ne sait pas ce qu'il ecoute.
//   2. UN GATE ADAPTATIF. Le seuil de silence de la capture est FIXE : dans un lieu bruyant, le plancher monte
//      au-dessus de lui, et le bruit passe pour du signal. Ici, le plancher est MESURE et SUIVI, et une fenetre ne
//      compte comme voix que si elle le depasse nettement.
//
// Pourquoi dans le domaine : la regle dit « est-ce de la voix ? », ce qui est un jugement sur un signal musical, pas une
// affaire de materiel. Elle se teste donc avec des nombres, sans micro et sans Qt.
// =====================================================================================================================

namespace musichien::domain
{

class VoicePreFilter
{
public:
    // Le passe-haut. Quatre-vingt-dix hertz : sous le si1 (123 Hz) d'une voix d'homme, et largement au-dessus de tout ce
    // qui n'est pas chante. L'accordeur, lui, descend jusqu'a 30 Hz - d'ou la SEPARATION.
    static constexpr double HIGH_PASS_CUTOFF_HZ = 90.0;

    // Combien la voix doit depasser le plancher de bruit suivi pour etre crue. Trois fois : assez pour ignorer un
    // brouhaha qui monte, assez peu pour ne pas exiger une voix forte.
    static constexpr double VOICE_MARGIN = 3.0;

    // La vitesse a laquelle le plancher REMONTE, en fraction de l'ecart, a chaque fenetre. Lente : le bruit de fond qui
    // monte est suivi, mais une note tenue ne fait pas monter le plancher assez vite pour s'exclure elle-meme.
    static constexpr double NOISE_FLOOR_RISE = 0.02;

    // Le taux d'echantillonnage a change : le passe-haut se recale. A appeler a l'ouverture du peripherique.
    void configure( double p_sampleRateHz ) noexcept;

    // Une nouvelle prise commence : l'etat du filtre repart de zero, et le plancher de bruit est a nouveau inconnu.
    void reset() noexcept;

    // Le passe-haut, un echantillon a la fois. Un filtre du premier ordre suffit : il n'y a qu'une chose a couper.
    [[nodiscard]] double processSample( double p_sample ) noexcept;

    // La fenetre est-elle de la VOIX ? Le plancher de bruit s'adapte ici, une fois par fenetre - c'est le seul endroit
    // ou l'on connait le niveau de la fenetre entiere. Le plancher ne suit que les fenetres qui ne sont PAS de la voix,
    // sans quoi une note tenue finirait par s'exclure elle-meme (voir le .cpp).
    [[nodiscard]] bool isVoiceLevel( double p_rms ) noexcept;

private:
    // Le coefficient du passe-haut, derive du taux d'echantillonnage. Voir configure().
    double m_highPassCoefficient{ 0.0 };

    double m_previousInput{ 0.0 };
    double m_previousOutput{ 0.0 };

    // Le plancher de bruit suivi, et s'il a ete mesure une fois.
    double m_noiseFloor{ 0.0 };
    bool m_hasNoiseFloor{ false };
};

}    // namespace musichien::domain
