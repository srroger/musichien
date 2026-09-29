#pragma once

// =====================================================================================================================
// Musichien - AudioMixer
//
// A tiny software mixer: a pull-mode QIODevice the audio sink reads from, holding the sounds that are playing.
//
// ---------------------------------------------------------------------------------------------------------------------
// Why it exists
//
// The first version PUSHED one buffer at a time and stopped everything before each one. That is right for an ear
// training interval - two overlapping notes would make the interval impossible to name - and completely wrong the
// moment two sounds must be heard TOGETHER: the metronome click and a drum hit, a drum roll, a backing pattern.
// Le symptome etait exact : lance le metronome, frappe la batterie au meme instant que le bip, et le bip ne joue pas.
//
// So the sounds are mixed instead. The sink PULLS from this device whenever it needs samples, and this device sums
// whatever is still playing.
//
// ---------------------------------------------------------------------------------------------------------------------
// Le METRONOME, et pourquoi il vit ici
//
// Un clic pousse par le thread d'interface - au moment ou un QTimer a bien voulu se declencher - n'est jamais juste :
// entre deux tics il y a le rendu de QML, les evenements du systeme, le ramasse-miettes, et le clic tombe avec
// plusieurs millisecondes de gigue. C'est cette gigue qui s'entend comme une instabilite, et aucune correction de
// derive ne peut la retirer.
//
// Le metronome bat donc ICI, dans le flux audio, en comptant des ECHANTILLONS : la grille (domain::MetronomeGrid) sait
// quelle position tombe sur quel temps, et cette classe insere le clic a cette position exacte. Le thread d'interface
// ne fait plus rien pour le son - il ne lui reste que l'affichage.
//
// Consequence heureuse, et ce n'est pas un detail : le temps du metronome est celui du FLUX. Si le flux s'arrete -
// l'application passe en arriere-plan, l'appareil audio est rendu - le metronome s'arrete avec lui, au lieu de
// continuer a battre dans le vide et d'arriver en retard sur tout ce qui suit.
//
// ---------------------------------------------------------------------------------------------------------------------
// Thread safety, and why it is not a detail
//
// The audio backend pulls from its own thread while the interface pushes from the main thread. Every access to the
// voices is therefore guarded, and the guard is the reason this class is not just a vector plus a loop.
// =====================================================================================================================

#include "domain/rhythm/MetronomeGrid.h"

#include <QIODevice>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace musichien::infrastructure
{

// Note: NOT final, unlike the rest of the project. A test needs to subclass it to reach readData directly, because
// QIODevice::read() fills a buffer of its own and would make the test a test about Qt rather than about the mix.
class AudioMixer : public QIODevice
{
public:
    AudioMixer( int p_sampleRate, int p_channelCount, QObject * p_parent = nullptr );

    // Adds a sound to the mix NOW. Replaces NOTHING: this is the whole point of the class.
    void play( std::vector<float> p_samples, float p_gain = 1.0F );

    // Adds a sound to the mix AT A GIVEN FRAME of the stream.
    //
    // C'est la seule facon de poser un son a un instant EXACT : l'instant est un numero d'echantillon, et non une
    // intention. Un son dont l'instant est deja passe est joue tout de suite, pour qu'un retard ne devienne jamais un
    // silence.
    void playAt( std::vector<float> p_samples, std::int64_t p_startFrame, float p_gain = 1.0F );

    // Drops everything that is playing, at once.
    void clear();

    // True while at least one sound is still playing: the caller stops the sink when it goes false.
    [[nodiscard]] bool isPlaying() const;

    // How many bytes are ready to be read.
    //
    // OVERRIDDEN, and this is not a detail - it is the whole reason the application was silent for a while. A
    // sequential QIODevice that announces nothing to read is read as EMPTY, and the audio device then stays idle
    // without ever asking for samples: the sound never comes out. The sink asks this method before pulling.
    //
    // Et pendant que le metronome bat, il y a TOUJOURS quelque chose a lire - ne serait-ce que le silence entre deux
    // clics - parce que c'est ce silence qui fait avancer le temps. Un flux qui s'endort entre deux clics perdrait
    // exactement la mesure qu'il est cense tenir.
    [[nodiscard]] qint64 bytesAvailable() const override;

    // How many sounds are being mixed right now. Exposed because it is what a test needs to see, and because it
    // makes the mixer's behaviour readable from the outside.
    [[nodiscard]] std::size_t voiceCount() const;

    // -----------------------------------------------------------------------------------------------------------------
    // Le metronome
    // -----------------------------------------------------------------------------------------------------------------

    // Les deux clics, donnes une fois : ils sont joues des milliers de fois, et les copier a chaque temps ferait une
    // allocation dans le chemin audio.
    void setMetronomeClicks( std::vector<float> p_accented, std::vector<float> p_plain );

    // Demarre le metronome, le temps 0 tombant a la position COURANTE du flux. Un appel pendant que la grille tourne la
    // replace : c'est ce qui fait qu'un changement de tempo s'entend tout de suite, et proprement.
    void startMetronome( double p_bpm, int p_beatsPerBar );

    void stopMetronome();

    [[nodiscard]] bool isMetronomeRunning() const;

    // Combien d'echantillons ont ete ecrits depuis l'ouverture du flux : la seule horloge du metronome.
    [[nodiscard]] std::int64_t framesWritten() const;

    // Ce que l'oreille ENTEND, en millisecondes depuis le premier temps de la grille.
    //
    // La position ECRITE est en avance sur la position entendue de tout le tampon de sortie : sans cette correction,
    // une frappe juste serait jugee en avance de quarante millisecondes, et le joueur apprendrait a jouer en retard.
    [[nodiscard]] double metronomeElapsedMs() const;

    // Le rang du temps en cours, et s'il tombe sur le premier temps d'une mesure.
    [[nodiscard]] std::int64_t metronomeBeatIndex() const;
    [[nodiscard]] bool isMetronomeBeatAccented() const;

    // Combien d'echantillons sont deja dans le tampon de sortie, et donc pas encore entendus. L'application le sait
    // quand elle fixe la taille de ce tampon ; le mixer, lui, ne peut pas le deviner.
    void setOutputLatencyFrames( std::int64_t p_frames );

protected:
    // Fills the buffer the backend asked for. Always returns complete frames: an empty answer would be read as
    // "nothing to play", and the stream would stall, so silence is written instead.
    qint64 readData( char * p_data, qint64 p_maximumByteCount ) override;

    // A mixer is a source: it is never written to.
    qint64 writeData( const char * p_data, qint64 p_byteCount ) override;

private:
    struct Voice
    {
        // La source est PARTAGEE, et c'est ce qui permet a un clic de metronome d'etre joue mille fois sans etre copie
        // mille fois : la voix ne fait que pointer sur lui.
        std::shared_ptr<const std::vector<float>> samples;
        std::size_t position{ 0 };
        float gain{ 1.0F };
        // La position du flux ou la voix commence a se faire entendre. Un son planifie pour un instant a venir attend
        // en silence, et c'est ce qui rend un clic EXACT plutot qu'immediat.
        std::int64_t startFrame{ 0 };
    };

    // Planifie les temps du metronome qui tombent dans la fenetre a venir. Appelee au debut de readData, donc dans le
    // thread audio et jamais depuis l'interface.
    void scheduleMetronomeBeats( std::int64_t p_firstFrame, std::int64_t p_frameCount );

    // La position ECRITE du flux, corrigee de la latence : ce que l'oreille entend a cet instant.
    [[nodiscard]] std::int64_t listenedFrame() const noexcept;

    mutable std::mutex m_mutex;
    std::vector<Voice> m_voices;
    int m_sampleRate{ 0 };
    int m_channelCount{ 1 };

    domain::MetronomeGrid m_grid;
    std::shared_ptr<const std::vector<float>> m_accentedClick;
    std::shared_ptr<const std::vector<float>> m_plainClick;

    // La position du flux se lit depuis les deux threads, et s'ecrit dans un seul : atomique plutot que verrouillee,
    // parce qu'un verrou dans un chemin audio est une facon de faire attendre le son.
    std::atomic<std::int64_t> m_framesWritten{ 0 };
    std::atomic<std::int64_t> m_outputLatencyFrames{ 0 };
};

}    // namespace musichien::infrastructure
