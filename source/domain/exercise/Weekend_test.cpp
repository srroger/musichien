#include "domain/exercise/Weekend.h"

#include <gtest/gtest.h>

namespace musichien::domain
{

TEST( WeekendTest, the_week_end_is_saturday_and_sunday )
{
    // 1 = lundi ... 7 = dimanche, la convention de Qt. C'est elle qui est verifiee : d'autres langages commencent la
    // semaine le dimanche, et une confusion ferait apparaitre le bilan un jour trop tot - ou jamais.
    EXPECT_FALSE( isWeekEnd( 1 ) );    // lundi
    EXPECT_FALSE( isWeekEnd( 2 ) );
    EXPECT_FALSE( isWeekEnd( 3 ) );
    EXPECT_FALSE( isWeekEnd( 4 ) );
    EXPECT_FALSE( isWeekEnd( 5 ) );    // vendredi

    EXPECT_TRUE( isWeekEnd( 6 ) );    // samedi
    EXPECT_TRUE( isWeekEnd( 7 ) );    // dimanche

    // Et un jour qui n'existe pas n'est pas le week-end : une date fausse ne doit pas declencher une invitation.
    EXPECT_FALSE( isWeekEnd( 0 ) );
    EXPECT_FALSE( isWeekEnd( 8 ) );
}

}    // namespace musichien::domain
