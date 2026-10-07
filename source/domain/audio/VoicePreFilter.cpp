#include "domain/audio/VoicePreFilter.h"

#include <numbers>

namespace musichien::domain
{

void VoicePreFilter::configure( double p_sampleRateHz ) noexcept
{
    m_highPassCoefficient = 0.0;

    if( p_sampleRateHz <= 0.0 )
    {
        return;
    }

    // y[n] = a * (y[n-1] + x[n] - x[n-1]), la forme du premier ordre d'un passe-haut, avec a = RC / (RC + dt). Le
    // coefficient ne depend donc que du taux d'echantillonnage et de la frequence de coupure - rien d'autre a regler.
    const double deltaSeconds = 1.0 / p_sampleRateHz;
    const double timeConstant = 1.0 / ( 2.0 * std::numbers::pi * HIGH_PASS_CUTOFF_HZ );

    m_highPassCoefficient = timeConstant / ( timeConstant + deltaSeconds );
}

void VoicePreFilter::reset() noexcept
{
    m_previousInput = 0.0;
    m_previousOutput = 0.0;
    m_noiseFloor = 0.0;
    m_hasNoiseFloor = false;
}

double VoicePreFilter::processSample( double p_sample ) noexcept
{
    // Sans taux d'echantillonnage, le filtre n'est pas regle : on rend l'echantillon tel quel plutot que d'inventer un
    // coefficient. Un adaptateur qui n'a pas appele configure() coupe donc moins, jamais faux.
    if( m_highPassCoefficient <= 0.0 )
    {
        return p_sample;
    }

    const double output = m_highPassCoefficient * ( m_previousOutput + p_sample - m_previousInput );

    m_previousInput = p_sample;
    m_previousOutput = output;

    return output;
}

bool VoicePreFilter::isVoiceLevel( double p_rms ) noexcept
{
    if( !m_hasNoiseFloor )
    {
        // La premiere fenetre n'est JAMAIS une voix : elle EST le point de depart du plancher. Sans cela, la premiere
        // note chantee serait jugee contre un plancher de zero - donc acceptee, et le bruit avec elle.
        m_noiseFloor = p_rms;
        m_hasNoiseFloor = true;

        return false;
    }

    const bool isVoice = p_rms > ( m_noiseFloor * VOICE_MARGIN );

    // LA VOIX NE TOUCHE PAS AU PLANCHER. C'est le point delicat : si une note tenue faisait monter le plancher vers son
    // propre niveau, elle finirait par passer SOUS le seuil qu'elle a fait monter - et s'exclurait elle-meme, au bout de
    // quelques fenetres. Le plancher ne suit donc que ce qui n'est PAS de la voix : le bruit de fond.
    if( !isVoice )
    {
        if( p_rms < m_noiseFloor )
        {
            // Vers le bas, tout de suite : un lieu qui se calme doit etre suivi immediatement, sinon le gate resterait
            // sourd a une voix douce qui suit un brouhaha.
            m_noiseFloor = p_rms;
        }
        else
        {
            // Vers le haut, lentement : le bruit de fond qui monte est suivi, sans jamais prendre une voix pour lui.
            m_noiseFloor += NOISE_FLOOR_RISE * ( p_rms - m_noiseFloor );
        }
    }

    return isVoice;
}

}    // namespace musichien::domain
