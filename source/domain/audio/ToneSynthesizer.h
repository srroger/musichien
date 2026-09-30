#pragma once

// =====================================================================================================================
// Musichien - ToneSynthesizer
//
// Generates PCM audio for notes: plain mono samples in [-1, 1].
//
// This class is PURE. No Qt, no file, no sound card, no clock. It only does arithmetic, therefore the
// entire sound generation is unit tested on any machine. That is what makes it safe to change when
// the sound has to be tuned.
//
// ---------------------------------------------------------------------------------------------------------------------
// A STRUCK STRING, not a sine wave
//
// The signal is a delay line excited by a short burst of noise - the hammer - with a low pass filter
// inside the loop and a little loss on every round trip. Three things fall out of that model, and all
// three were the reason to adopt it:
//
//   * a sine has NO harmonic at all, so the brain has nothing to hang a low note on. On a phone
//     speaker, which cannot produce a low fundamental at a useful level, a sine simply disappears,
//     while a struck string stays perfectly identifiable. See the sonar note: "rendering a low note
//     audible on a phone is a TIMBRE problem, not a volume one";
//   * the low pass in the loop makes the HIGH harmonics die first, which is what a real string does.
//     The result sounds struck rather than held;
//   * the excitation is NOISE, different for every note. Two notes of an interval therefore start on
//     unrelated waveforms instead of on two sines that begin in phase, add up on the attack and then
//     hollow each other out - the "the chords cancel themselves" the ear picks up immediately.
//
// Every buffer is normalised on its ENERGY, not on its peak, with a ceiling that prevents clipping.
// Peak normalisation gives equal volts; it does not give equal loudness, and this application asks the
// player to compare two sounds. See the sonar note, "normaliser - three things the word mixes up".
// =====================================================================================================================

#include "domain/music/Note.h"
#include "domain/music/Temperament.h"

#include <chrono>
#include <cstdint>
#include <span>
#include <vector>

namespace musichien::domain
{

// The pure waveforms the synthesiser can render as instruments. Unlike the struck string, they carry a CONTROLLED
// spectrum: the sine has no harmonic, the sawtooth has every harmonic (falling as 1/n), the square only the odd
// ones. That is what makes them the honest tools for hearing a temperament - the beating of two simple spectra
// leaves nothing else to listen to, and the ear can actually count it.
enum class Waveform
{
    Sine,        // the fundamental only
    Sawtooth,    // every harmonic, falling as 1/n
    Square,      // the odd harmonics only, falling as 1/n

    // Un son TENU et DOUX : la fondamentale et quelques harmoniques basses, comme les tirettes d'un orgue.
    //
    // Pourquoi cette forme existe, et c'est un compte rendu, pas une intuition. Le premier bourdon de Musichien
    // utilisait une dents de scie, et Roger l'a dit tout de suite : « en mode sustain, il rend tres moche, on dirait
    // un vieux son NES qui gresille ». La cause est technique et exacte : une dents de scie contient des harmoniques
    // jusqu'a Nyquist, et celles qui DEPASSENT la frequence d'echantillonnage se replient vers le bas du spectre -
    // c'est le repliement, et il s'entend comme un gresillement metallique.
    //
    // La bonne reponse a un bourdon n'est pas de filtrer la dents de scie : c'est de ne pas en jouer une. Un bourdon
    // n'a pas besoin d'un timbre riche, il a besoin d'un timbre TENU et NON AGRESSIF - donc d'harmoniques basses,
    // peu nombreuses, et faibles.
    Organ
};

// Comment un bourdon ENCADRE la melodie : un temps ou il sonne SEUL avant, et un temps ou il traine apres.
//
// Declare ICI, hors de la classe, parce que le PORT de lecture s'en sert aussi (NotePlayer::playMelodyOverDrone) : un
// type partage par un port et par son implementation appartient au domaine, pas a l'une de ses classes.
//
// Pourquoi cet encadrement n'est pas un ornement, et pourquoi ses valeurs par defaut ne sont pas nulles : une couleur
// modale ne s'entend que si le CENTRE est deja installe quand la melodie arrive. Un bourdon qui demarrerait en meme
// temps que la premiere note obligerait l'oreille a deviner ou est le centre pendant qu'elle ecoute deja la couleur ;
// et un bourdon qui s'arreterait avec la derniere note laisserait la fin de la phrase en l'air.
//
// C'est le verdict d'ecoute du 30/09/2026 qui a tranche ce point, mot pour mot : « le bourdon est au bon volume, mais
// on ne l'entend pas assez longtemps ».
struct DroneFraming
{
    // Le bourdon sonne seul avant que la melodie n'arrive.
    std::chrono::milliseconds leadIn{ 1500 };

    // Et il continue seul apres la derniere note.
    std::chrono::milliseconds tail{ 1500 };
};

// La duree d'un bourdon qui encadre une phrase de p_noteCount notes.
//
// EXPOSEE parce que deux endroits en ont besoin, et qu'ils doivent trouver le meme nombre : le domaine, qui fabrique
// son propre bourdon quand aucun echantillon n'est disponible, et l'ADAPTATEUR, qui rend un bourdon ENREGISTRE quand il
// en a un. Deux calculs qui doivent coincider finissent toujours par diverger en silence - et ici, la consequence serait
// un bourdon qui s'arrete avant la fin de la phrase, ou qui traine apres.
[[nodiscard]] std::chrono::milliseconds droneDurationFor( std::size_t p_noteCount,
                                                          std::chrono::milliseconds p_noteDuration,
                                                          std::chrono::milliseconds p_gap,
                                                          DroneFraming p_framing = {} ) noexcept;

class ToneSynthesizer
{
public:
    // Fade in of the cue that marks a mistake, which removes the click at its beginning.
    static constexpr std::chrono::milliseconds CUE_ATTACK_DURATION{ 12 };

    // Fade out of everything, which removes the click at the end of a buffer.
    static constexpr std::chrono::milliseconds RELEASE_DURATION{ 40 };

    // Fade in of a NOTE, and it is ten times shorter than the cue's on purpose.
    //
    // A struck string starts with the hammer: a fade of 12 ms would swallow the very attack that makes
    // the sound what it is. Two milliseconds is enough to remove the click of the first sample and short
    // enough to leave the strike intact.
    static constexpr std::chrono::milliseconds STRIKE_ATTACK_DURATION{ 2 };

    // Energy level every buffer of NOTES aims for, before the ceiling below is applied.
    //
    // Energy - the root mean square - and not the peak, because that is what the ear averages. Two sounds
    // with the same peak but different waveforms are heard at different loudnesses, and the player is asked
    // to compare two sounds.
    //
    // The value is what a struck string can actually reach while staying under the ceiling, measured rather
    // than guessed: across the whole range, the onset energy of a note lands between 0.20 and 0.26. A higher
    // target would simply never be reached, and the constant would be a lie.
    static constexpr float TARGET_RMS_AMPLITUDE = 0.24F;

    // The beginning of the note, and it is what the loudness is measured on.
    //
    // Measuring the energy of the WHOLE buffer would be measuring a decay: a note that rings twice as long
    // would come out half as loud, and the treble would sound weaker than the bass for no musical reason at
    // all. What must be equal from one note to the next is the level at the moment the note SOUNDS.
    //
    // Short, too, and for the same reason: the longer this window is, the more of the note's DECAY it
    // contains - and a window full of decay has a tall peak compared to its average, which is what the
    // ceiling below reacts to. Thirty milliseconds is the attack of a piano hammer.
    static constexpr std::chrono::milliseconds NOTE_ONSET_DURATION{ 30 };

    // Hard ceiling on the amplitude of any buffer. A rich waveform can reach its energy target only by
    // exceeding the maximum, so the ceiling always wins: it is what keeps the sound out of the clipping
    // that a phone speaker turns into distortion.
    static constexpr float MAXIMUM_PEAK_AMPLITUDE = 0.92F;

    // Energy level of the cue that marks a mistake, deliberately BELOW the notes.
    //
    // Noise sounds much louder than a pitched sound of the same energy, and a cue is there to be noticed,
    // not to make the player jump. Two sounds meant to carry different weights must not be normalised to
    // the same number.
    static constexpr float MISTAKE_CUE_RMS_AMPLITUDE = 0.10F;

    // The cue that marks a mistake, as a SHORT NOISE BURST.
    //
    // Noise, and not a note: this is a musical decision before it is a technical one. A cue with a pitch
    // would teach the ear to associate a note with failure, and what a note means is exactly what this
    // application exists to train. A noise burst has no pitch to learn, which is the whole point.
    //
    // The sequence comes from a FIXED seed: the domain owns no entropy source of its own, and two runs
    // must produce the same cue. It is also what makes the burst testable.
    [[nodiscard]] std::vector<float> renderMistakeCue( std::chrono::milliseconds p_duration ) const;

    // How long the cue lasts: short enough to be a punctuation mark rather than a sound of its own.
    static constexpr std::chrono::milliseconds MISTAKE_CUE_DURATION{ 90 };

    // Strings struck per note, and how far apart they are tuned.
    //
    // A real piano has three strings for the middle of its range, tuned a hair apart. The beating between
    // them is what gives the note its body - and, again, what stops the members of a chord from lining up
    // into a single waveform that cancels itself out.
    static constexpr std::size_t STRING_COUNT = 3;
    static constexpr double STRING_DETUNE_CENTS = 0.9;

    explicit ToneSynthesizer( std::int32_t p_sampleRate );

    [[nodiscard]] std::int32_t sampleRate() const noexcept { return m_sampleRate; }

    // Number of samples a duration represents at this sample rate.
    [[nodiscard]] std::size_t sampleCountFor( std::chrono::milliseconds p_duration ) const noexcept;

    // A single note.
    [[nodiscard]] std::vector<float> renderNote( const Note & p_note,
                                                 std::chrono::milliseconds p_duration,
                                                 TuningContext p_tuning = {} ) const;

    // Several notes sounded at the same time: a harmonic interval, a chord. The tuning's root is the FIRST note,
    // which is the note the interval is heard from.
    [[nodiscard]] std::vector<float> renderChord( std::span<const Note> p_notes,
                                                  std::chrono::milliseconds p_duration,
                                                  TuningContext p_tuning = {} ) const;

    // Several notes sounded one after another, separated by silence: a melodic interval, a scale. Root as above.
    [[nodiscard]] std::vector<float> renderMelody( std::span<const Note> p_notes,
                                                   std::chrono::milliseconds p_noteDuration,
                                                   std::chrono::milliseconds p_gap,
                                                   TuningContext p_tuning = {} ) const;

    // A melody heard OVER a drone that is held from its first note to its last: the two are heard TOGETHER.
    //
    // -------------------------------------------------------------------------------------------------------------
    // Pourquoi cette regle vit dans le domaine, et non dans l'adaptateur audio
    //
    // Parce qu'un mode n'est pas un jeu de notes, c'est un jeu de notes PLUS UN CENTRE : les sept memes notes sur
    // re sont du re dorien, sur si bemol elles sont du si bemol majeur. Sans centre, l'oreille entend une gamme et
    // aucun mode - ce qui veut dire que « ecoute cette gamme et nomme le mode » n'est pas une question difficile,
    // c'est une question SANS REPONSE.
    //
    // Le bourdon EST ce centre. Le rendre ici plutot que dans l'adaptateur, c'est la meme raison que partout
    // ailleurs : une balance de deux voix est une regle, donc elle se calcule et elle se teste - et un adaptateur
    // qui doit inventer le niveau du bourdon inventerait aussi le gout de l'exercice.
    //
    // -------------------------------------------------------------------------------------------------------------
    // La duree du bourdon, et pourquoi elle est calculee
    //
    // Le bourdon dure EXACTEMENT ce que dure la melodie, silence final compris. Un bourdon plus court laisserait
    // la derniere note seule, et la fin de la phrase redeviendrait une note isolee - c'est-a-dire, de nouveau, une
    // question sans reponse.
    // Comment un bourdon ENCADRE la melodie : un temps ou il sonne SEUL avant, et un temps ou il traine apres.
    //
    // Pourquoi cet encadrement n'est pas un ornement, et pourquoi ses valeurs par defaut ne sont pas nulles : une
    // couleur modale ne s'entend que si le CENTRE est deja installe quand la melodie arrive. Un bourdon qui
    // demarrerait en meme temps que la premiere note obligerait l'oreille a deviner ou est le centre pendant qu'elle
    // ecoute deja la couleur ; et un bourdon qui s'arreterait avec la derniere note laisserait la fin de la phrase
    // en l'air.
    //
    // C'est le verdict d'ecoute du 30/09/2026 qui a tranche ce point, mot pour mot : « le bourdon est au bon volume,
    // mais on ne l'entend pas assez longtemps ».
    [[nodiscard]] std::vector<float> renderMelodyOverDrone( std::span<const Note> p_melody,
                                                            std::span<const Note> p_drone,
                                                            std::chrono::milliseconds p_noteDuration,
                                                            std::chrono::milliseconds p_gap,
                                                            TuningContext p_tuning = {},
                                                            DroneFraming p_framing = {} ) const;

    // Mixe une melodie et un bourdon DEJA RENDUS : c'est la moitie « assemblage » de renderMelodyOverDrone.
    //
    // Pourquoi cette moitie existe separement : le TIMBRE du bourdon n'est pas une regle de musique, c'est une donnee de
    // restitution. Le domaine sait en fabriquer un (Waveform::Organ, le repli), et l'adaptateur peut en apporter un
    // autre - un ensemble a cordes enregistre, par exemple. Les deux ont besoin de la MEME regle d'assemblage : le
    // decalage du leadIn, la ponderation du bourdon, et le garde-fou anti-saturation. La dupliquer serait laisser deux
    // equilibres diverger en silence.
    //
    // p_droneSamples peut etre plus long ou plus court que la melodie encadree : ce qui existe s'entend, le reste est du
    // silence. Un bourdon plus court qu'une phrase est donc accepte, et c'est deliberé - c'est a l'appelant de savoir ce
    // qu'il donne.
    [[nodiscard]] std::vector<float> mixMelodyOverDrone( std::span<const Note> p_melody,
                                                         std::span<const float> p_droneSamples,
                                                         std::chrono::milliseconds p_noteDuration,
                                                         std::chrono::milliseconds p_gap,
                                                         TuningContext p_tuning = {},
                                                         DroneFraming p_framing = {} ) const;

    // Le niveau du bourdon, avant que la somme ne soit normalisee.
    //
    // Inferieur a un, et c'est une decision d'ecoute, pas une precaution technique : le bourdon doit etre SENTI
    // plutot qu'ecoute. Il est ce contre quoi la melodie se dit, pas quelque chose a reconnaitre - et un bourdon au
    // niveau de la melodie donnerait deux choses a suivre au lieu d'une couleur.
    //
    // Monte a 0.45, puis redescendu a 0.30 : une fois le bourdon devenu une onde ENTRETENUE (voir renderMelodyOverDrone),
    // il ne s'eteint plus, donc il est present tout du long au lieu de n'etre fort qu'a son attaque. Roger, 30/09/2026 :
    // « maintenant qu'il tient, il est un peu fort ». La baisse est le prix exact de la correction precedente.
    static constexpr float DRONE_GAIN = 0.30F;

    // The SAME three calls, but for a PURE WAVEFORM: a controlled spectrum instead of the struck string. See Waveform.
    // No hammer either: the attack is a short fade rather than a strike.
    [[nodiscard]] std::vector<float> renderWaveNote( const Note & p_note,
                                                     Waveform p_waveform,
                                                     std::chrono::milliseconds p_duration,
                                                     TuningContext p_tuning = {} ) const;

    [[nodiscard]] std::vector<float> renderWaveChord( std::span<const Note> p_notes,
                                                      Waveform p_waveform,
                                                      std::chrono::milliseconds p_duration,
                                                      TuningContext p_tuning = {} ) const;

    [[nodiscard]] std::vector<float> renderWaveMelody( std::span<const Note> p_notes,
                                                       Waveform p_waveform,
                                                       std::chrono::milliseconds p_noteDuration,
                                                       std::chrono::milliseconds p_gap,
                                                       TuningContext p_tuning = {} ) const;

private:
    // One note at an EXPLICIT frequency. The frequency is computed by the caller - which alone knows the root - and
    // this method only does the arithmetic of a struck string at that frequency.
    [[nodiscard]] std::vector<float> renderNoteAt( const Note & p_note,
                                                   double p_frequencyHz,
                                                   std::chrono::milliseconds p_duration ) const;

    // The pure-waveform counterpart of renderNoteAt.
    [[nodiscard]] std::vector<float> renderWaveNoteAt( const Note & p_note,
                                                       double p_frequencyHz,
                                                       Waveform p_waveform,
                                                       std::chrono::milliseconds p_duration ) const;

    // Adds ONE struck string to a buffer, without touching its level: mixing, enveloping and normalising
    // belong to the caller, which is the only one that knows how many strings are playing.
    //
    // p_seed decides the noise of the hammer, so two different notes - or two strings of the same note -
    // never start on the same waveform. It comes from the note itself, which keeps the whole thing
    // reproducible.
    void mixStruckStringInto( std::span<float> p_samples,
                              double p_frequency,
                              std::uint32_t p_seed ) const;

    // Applies a fade in and a fade out over a buffer already filled with the raw waveform.
    void applyEnvelope( std::span<float> p_samples,
                        std::chrono::milliseconds p_attack,
                        std::chrono::milliseconds p_release ) const;

    [[nodiscard]] static float rootMeanSquare( std::span<const float> p_samples );

    [[nodiscard]] static float peakAmplitude( std::span<const float> p_samples );

    // Scales a buffer on the ENERGY OF ITS BEGINNING, then brings it back down if that made it clip.
    //
    // Not static, unlike the two helpers above: it needs the sample rate to know how long the beginning is.
    void normaliseOnsetEnergyTo( std::span<float> p_samples,
                                 float p_targetRms,
                                 std::chrono::milliseconds p_onsetDuration ) const;

    std::int32_t m_sampleRate{ 0 };
};

}    // namespace musichien::domain
