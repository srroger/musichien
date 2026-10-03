#pragma once

// =====================================================================================================================
// Musichien - SungIntervalDetector
//
// What the player just SANG, read as an interval: two held notes, and the distance between them.
//
// The hard part is not measuring a pitch - PitchDetector does that. It is deciding WHICH pitches were MEANT. A voice
// glides, wobbles and cracks, so a naive reading would report ten different notes in two seconds. A note therefore
// counts only when it is HELD: steady within a tolerance, and long enough to be a note rather than a slide.
//
// This is a pure rule, deliberately kept out of the screen that displays it: readings and time go in, an answer comes
// out, and the tests below run it on synthetic readings with no microphone in sight.
//
// The voice is also what makes this a MUSICAL rule rather than a signal-processing one: judging an interval means
// comparing a note to another note, exactly like the rest of the domain, and never comparing a frequency to a
// reference frequency.
// =====================================================================================================================

#include <algorithm>
#include <cstdint>

namespace musichien::domain
{

class SungIntervalDetector
{
public:
    // What the detector has understood so far.
    struct Reading
    {
        // The note the singer started on, once one has been held long enough. 0 means "not yet heard".
        std::int32_t firstMidiNumber{ 0 };

        // The note the singer arrived on, once IT has been held long enough. 0 means "not yet".
        std::int32_t secondMidiNumber{ 0 };

        // True once both notes have been heard: the answer is complete.
        [[nodiscard]] bool hasInterval() const noexcept { return ( firstMidiNumber != 0 ) && ( secondMidiNumber != 0 ); }

        // The distance between the two, in semitones. Negative when the singer went DOWN, which matters: a falling
        // fifth and a rising fifth are not the same interval.
        [[nodiscard]] std::int32_t semitones() const noexcept { return secondMidiNumber - firstMidiNumber; }

        // De combien l'intervalle a ete chante, en CENTS - la mesure CONTINUE, et non l'arrondi au demi-ton.
        //
        // C'est ce qui permet de dire « juste, et a douze cents pres » au lieu d'un « juste » sec : 1200 cents font
        // une octave, 100 un demi-ton, et une quinte du tempere egal en vaut 700. Le chant n'etant jamais exactement
        // juste, savoir de combien il ne l'est pas est toute la difference entre un verdict et une mesure.
        double cents{ 0.0 };
    };

    // A reading arrives every few milliseconds. The caller says how much time passed since the previous one: the
    // detector has no clock of its own, and a rule of the game must not own one.
    void update( double p_frequencyHz, double p_referencePitchHz, std::int32_t p_elapsedMilliseconds ) noexcept;

    void reset() noexcept;

    [[nodiscard]] const Reading & reading() const noexcept { return m_reading; }

    // The note being held right now (0 when nothing is), and how far its hold has gone towards validation. The
    // controller turns this into the little STABILITY BAR: the player must see whether his note is holding or
    // slipping, otherwise "nothing happens" is the only feedback a voice gets.
    [[nodiscard]] std::int32_t heldMidiNumber() const noexcept { return m_heldMidiNumber; }

    [[nodiscard]] double stabilityFraction() const noexcept
    {
        return std::min( 1.0,
                         static_cast<double>( m_heldMilliseconds ) / static_cast<double>( MINIMUM_HOLD_MILLISECONDS ) );
    }

private:
    // How long a note must be held before it counts as the note the singer MEANT. Long on purpose: a wobble, a breath
    // or a slide never reads as a note - the voice gets the time it needs, and the result feels smooth rather than
    // twitchy.
    //
    // SEVEN HUNDRED AND FIFTY, and not a whole second. Roger relayed what the singers told him: "c'etait dur d'arriver
    // au bout de la progress bar". A second was chosen for smoothness, but the bar is what the singer actually watches,
    // and it RESTARTS at zero the moment the note moves - so the second did not buy smoothness, it bought despair.
    // Three quarters of a second still outlasts a wobble, and the bar can now be filled by an honest voice.
    static constexpr std::int32_t MINIMUM_HOLD_MILLISECONDS = 750;

    // Combien de temps de silence fait une REPRISE.
    //
    // La regle demandait une deuxieme note DIFFERENTE de la premiere, ce qui rendait l'unisson - un intervalle de zero
    // demi-ton, et un exercice parfaitement juste - impossible a reussir.
    //
    // Une note REPRISE apres un vrai silence est une nouvelle note, meme si c'est la meme hauteur. Et le silence doit
    // DURER : une detection qui vacille un instant, un vibrato, une syllabe, ne doit pas transformer une note tenue en
    // deux notes.
    static constexpr std::int32_t MINIMUM_SILENCE_MILLISECONDS = 150;

    // How fast the tracked pitch follows a reading. 0.15 keeps 85% of the previous estimate at every reading: slow
    // enough to absorb a singing voice's vibrato and wobble, fast enough to follow a real change of note. The tracked
    // pitch is then ROUNDED to the nearest note, which is what gives the tolerance: a reading must move the average
    // half a semitone before the note changes.
    static constexpr double TRACKING_ALPHA = 0.15;

    Reading m_reading;

    // The note being held right now, and for how long. 0 means no note is being held.
    std::int32_t m_heldMidiNumber{ 0 };

    std::int32_t m_heldMilliseconds{ 0 };

    // The tracked pitch, smoothed across readings. This is what absorbs the noise: a single reading never decides a
    // note, the average of the last few does. 0.0 means "not tracking yet".
    double m_trackedMidi{ 0.0 };

    // La hauteur continue de la PREMIERE note, au moment ou elle a ete acceptee. C'est elle qui sert de point de
    // depart au calcul des cents : garder la hauteur lissee plutot que l'entier arrondi est ce qui donne a la mesure
    // sa finesse, et le demi-ton n'est qu'un nom.
    double m_firstTrackedMidi{ 0.0 };

    // Le silence en cours, et s'il a ete assez long pour valoir une reprise. Voir MINIMUM_SILENCE_MILLISECONDS : c'est
    // ce couple qui permet de chanter l'unisson sans qu'un vibrato passe pour deux notes.
    std::int32_t m_silenceMilliseconds{ 0 };

    bool m_voiceRestarted{ false };
};

}    // namespace musichien::domain
