#pragma once

// =====================================================================================================================
// Musichien - WeekEnd
//
// Le week-end, tel que l'application l'entend : le samedi et le dimanche.
//
// C'est peu de chose, et c'est une REGLE : « le week-end, l'application met le Bilan en avant » doit se lire quelque part
// ou on puisse la discuter et la tester, plutot que d'etre une condition cachee au fond d'un ecran. C'est la meme raison
// qui a mis les moments des anecdotes dans le domaine.
// =====================================================================================================================

namespace musichien::domain
{

// Un jour de la semaine, compte comme Qt le compte : 1 = lundi, ..., 7 = dimanche.
//
// La convention est ECRITE ici parce qu'elle ne va pas de soi : d'autres langages commencent la semaine le dimanche, et
// les confondre ferait apparaitre le bilan un jour trop tot - ou jamais.
[[nodiscard]] constexpr bool isWeekEnd( int p_dayOfWeek ) noexcept
{
    return ( p_dayOfWeek == 6 ) || ( p_dayOfWeek == 7 );
}

}    // namespace musichien::domain
