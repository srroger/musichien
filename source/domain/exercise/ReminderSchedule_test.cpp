#include "domain/exercise/ReminderSchedule.h"

#include <gtest/gtest.h>

#include <cstddef>

namespace musichien::domain
{

TEST( ReminderScheduleTest, the_three_anecdote_moments_respect_what_roger_asked )
{
    ASSERT_EQ( 3U, ANECDOTE_REMINDER_MOMENTS.size() );

    // « Une le matin AVANT 9 h, une le midi, une le soir APRES 18 h. » Les fenetres sont verifiees, pas les heures :
    // celles-ci peuvent changer, la promesse faite au joueur non.
    EXPECT_LT( ANECDOTE_REMINDER_MOMENTS.at( 0 ).hour, 9 );

    EXPECT_GE( ANECDOTE_REMINDER_MOMENTS.at( 1 ).hour, 11 );
    EXPECT_LE( ANECDOTE_REMINDER_MOMENTS.at( 1 ).hour, 14 );

    EXPECT_GT( ANECDOTE_REMINDER_MOMENTS.at( 2 ).hour, 18 );

    // Et les moments sont ORDONNES, du matin au soir : c'est ce qui permet aux creneaux Android de rester previsibles,
    // et a un ecran de les lire dans l'ordre ou ils tomberont.
    for( std::size_t index = 1; index < ANECDOTE_REMINDER_MOMENTS.size(); ++index )
    {
        const ReminderMoment & previous = ANECDOTE_REMINDER_MOMENTS.at( index - 1 );
        const ReminderMoment & current = ANECDOTE_REMINDER_MOMENTS.at( index );

        EXPECT_LT( ( previous.hour * 60 ) + previous.minute, ( current.hour * 60 ) + current.minute );
    }

    // Et chaque moment est un moment VALIDE de la journee : le bornage ne le change pas.
    for( const ReminderMoment & moment : ANECDOTE_REMINDER_MOMENTS )
    {
        EXPECT_EQ( moment.hour, clampedReminderMoment( moment ).hour );
        EXPECT_EQ( moment.minute, clampedReminderMoment( moment ).minute );
    }
}

TEST( ReminderScheduleTest, an_impossible_moment_is_brought_back_into_the_day )
{
    // 25 h n'existe pas, et -1 non plus : un reglage vient d'un fichier qu'un joueur peut ouvrir, ou d'un ecran qui peut
    // se tromper. On RAMENE la valeur plutot que de refuser - refuser priverait le joueur de son rappel sans rien lui
    // dire, ce qui est pire qu'une heure approchante.
    EXPECT_EQ( 23, clampedReminderMoment( ReminderMoment{ 25, 0 } ).hour );
    EXPECT_EQ( 0, clampedReminderMoment( ReminderMoment{ -4, 0 } ).hour );
    EXPECT_EQ( 59, clampedReminderMoment( ReminderMoment{ 12, 90 } ).minute );
    EXPECT_EQ( 0, clampedReminderMoment( ReminderMoment{ 12, -3 } ).minute );

    // Un moment valide ne bouge pas : le bornage n'est pas une transformation.
    EXPECT_EQ( 19, clampedReminderMoment( ReminderMoment{ 19, 30 } ).hour );
    EXPECT_EQ( 30, clampedReminderMoment( ReminderMoment{ 19, 30 } ).minute );
}

TEST( ReminderScheduleTest, there_is_room_for_every_notification )
{
    // Trois anecdotes plus le rappel : quatre notifications par jour, et il doit rester de la place pour une cinquieme
    // sans toucher au Java cote Android.
    EXPECT_GE( REMINDER_SLOT_COUNT, ANECDOTE_REMINDER_MOMENTS.size() + 1 );
}

}    // namespace musichien::domain
