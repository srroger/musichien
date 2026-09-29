#pragma once

// =====================================================================================================================
// Musichien - NotificationScheduler
//
// Les notifications quotidiennes : trois anecdotes dans la journee, pour s'instruire musique avec le sourire, et un
// rappel qui n'a rien a vendre.
//
// C'est un PORT, exactement comme NotePlayer : l'application demande des notifications, et elle ne sait pas si la
// reponse est un AlarmManager Android, un silence de bureau ou un service qui n'existe pas.
//
// ---------------------------------------------------------------------------------------------------------------------
// Pourquoi UNE liste, et pas une par une
//
// `scheduleDailyNotifications` remplace la journee ENTIERE. Une porte unique plutot que quatre appels independants, et
// c'est ce qui garantit que l'ensemble veut toujours dire quelque chose : programmer une anecdote et oublier le rappel
// devient impossible, alors que deux methodes separees finiraient par se contredire.
//
// L'implementation Android utilise un SLOT par notification (un requestCode distinct) : deux notifications qui
// partageraient le meme creneau seraient la meme alarme, et la seconde effacerait la premiere.
// =====================================================================================================================

#include <span>
#include <string>
#include <string_view>

namespace musichien::infrastructure
{

class NotificationScheduler
{
public:
    NotificationScheduler() = default;

    NotificationScheduler( const NotificationScheduler & ) = delete;
    NotificationScheduler & operator=( const NotificationScheduler & ) = delete;
    NotificationScheduler( NotificationScheduler && ) = delete;
    NotificationScheduler & operator=( NotificationScheduler && ) = delete;

    virtual ~NotificationScheduler() = default;

    // Une notification quotidienne : un instant dans la journee, et ce qu'elle raconte.
    struct DailyNotification
    {
        int hour{ 0 };
        int minute{ 0 };

        // Le texte de la notification. Pour une anecdote, c'est l'anecdote elle-meme : elle est FIGEE dans l'alarme,
        // parce que la notification doit pouvoir s'afficher sans que l'application se reveille - et une notification
        // qui a besoin de l'application pour dire quelque chose est une notification qui n'arrive pas.
        std::string content;
    };

    // Remplace TOUTES les notifications quotidiennes par celles-ci.
    virtual void scheduleDailyNotifications( std::span<const DailyNotification> p_notifications ) = 0;

    // Plus aucune notification quotidienne.
    virtual void cancelNotifications() = 0;

    // Affiche une notification MAINTENANT : le bouton de developpement, pour verifier que la plomberie fonctionne ET
    // que les anecdotes se lisent bien sur un vrai ecran. Tester un rappel est la premiere chose dont un rappel a
    // besoin, et il est honnete de l'exposer ici.
    virtual void showReminderNow( std::string_view p_content ) = 0;

    // Demande a l'UTILISATEUR l'autorisation d'afficher des notifications, si elle manque.
    //
    // Depuis Android 13, une notification ne s'affiche pas sans cette autorisation, et Android ne la donne pas a
    // l'installation : il faut la demander, une fois, devant l'utilisateur. La demander au demarrage plutot qu'a
    // l'instant ou la notification doit s'afficher est la seule facon qui marche : une alarme qui sonne n'a pas
    // d'ecran ou poser la question.
    //
    // Sur une machine sans notifications - un bureau, un test - c'est une methode qui ne fait rien, et c'est exact.
    virtual void requestNotificationPermission() = 0;
};

}    // namespace musichien::infrastructure
