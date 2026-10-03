#include "domain/music/Scale.h"

namespace musichien::domain
{

std::string_view scaleIdentifier( Scale p_scale ) noexcept
{
    switch( p_scale )
    {
        case Scale::PentatonicMinor:
            return "penta_minor";

        case Scale::PentatonicMajor:
            return "penta_major";

        case Scale::Blues:
            return "blues";

        case Scale::HarmonicMinor:
            return "harmonic_minor";

        case Scale::PhrygianDominant:
            return "phrygian_dominant";

        case Scale::MelodicMinor:
            return "melodic_minor";
    }

    return "penta_minor";
}

Scale scaleFromIndex( std::size_t p_index ) noexcept
{
    // Le rang vient d'un fichier ou d'un ecran, donc il peut etre absurde : on rend la PREMIERE gamme plutot que de
    // fabriquer une valeur qui n'existe pas. Une table indexee par un rang faux serait un plantage, et un reglage abime
    // ne doit jamais couter plus cher qu'un reglage.
    if( p_index >= SCALE_COUNT )
    {
        return Scale::PentatonicMinor;
    }

    return static_cast<Scale>( p_index );
}

}    // namespace musichien::domain
