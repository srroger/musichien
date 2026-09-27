#include "domain/audio/NotePlayerFake.h"
#include "ui/ExerciseSessionController.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QMap>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QtLogging>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

namespace musichien::ui
{

// ---------------------------------------------------------------------------------------------------------------------
// L'ecran d'exercice, charge pour de vrai
//
// Les tests du view model verifient ce que l'ecran AFFICHERAIT. Celui-ci verifie que l'ecran s'INSTANCIE, et c'est une
// autre question : un QML qui ne se charge pas n'echoue a aucun test, ne casse aucun build, et fait demarrer
// l'application sur une page noire. C'est le joueur qui le decouvre.
//
// Ce test a deja servi deux fois. Il a trouve une boucle de liaison qui empechait le texte des boutons de se peindre,
// et il est le seul endroit qui aurait vu une propriete declaree deux fois - QML ne se contente pas d'ignorer la
// seconde, il refuse la page entiere. Il avait ete retire sur un diagnostic faux, et cette erreur a coute une
// soiree : le remettre, et le garder, est la lecon de la journee.
//
// Les MESSAGES DE QML sont captures, et affiches quand une attente echoue. Sans cela, l'ecran se charge, ne
// construit rien, et ne dit pas pourquoi : il ne reste qu'a deviner. Avec, l'echec porte la phrase de QML - et
// "Cannot assign to non-existent property contentItem" fait gagner des heures.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

// Le greffon d'affichage est impose AVANT que la moindre application existe : QML a besoin d'un moteur graphique,
// ctest tourne sans ecran, et le choix du greffon se fait a la creation de l'application - se plaindre apres ne sert
// a rien.
[[maybe_unused]] const bool OFFSCREEN_IS_IMPOSED = qputenv( "QT_QPA_PLATFORM", "offscreen" );

// Les messages de QML, retenus plutot que perdus sur la sortie d'erreur du binaire de test.
[[nodiscard]] QStringList & qmlMessages()
{
    static QStringList messages;

    return messages;
}

[[maybe_unused]] const bool MESSAGES_ARE_CAPTURED = [] {
    qInstallMessageHandler( []( QtMsgType, const QMessageLogContext &, const QString & p_message ) {
        qmlMessages().append( p_message );
    } );

    return true;
}();

// Une application Qt, pour toute la duree du binaire de test : QML ne peut pas instancier un Item sans elle, et il
// n'en faut qu'une.
[[nodiscard]] QGuiApplication & application()
{
    // Sur le TAS, et jamais detruite : volontairement.
    //
    // Une application Qt et un moteur QML sont des statiques qui se detruisent dans un ordre que rien ne controle -
    // et ici l'ordre compte, parce que le moteur detruit les objets QML pendant que l'application se termine. La
    // sanction est un segfault APRES que tous les tests ont passe : le pire des echecs, celui qui ne dit rien
    // d'utile. Un processus de test qui fuit ce qu'il a cree et se termine proprement est un processus honnete.
    static int argumentCount = 1;
    static char argumentName[]{ "test_ui" };
    static char * arguments[]{ argumentName, nullptr };

    static QGuiApplication * instance = new QGuiApplication{ argumentCount, arguments };

    return *instance;
}

// Les reglages du controleur de test : la carte ENTIERE.
//
// Choisie parce que c'est le cas le plus dur a afficher - vingt-cinq intervalles, dont trois sur la meme place. Un
// ecran qui montre ceux-la montre forcement les autres.
[[nodiscard]] domain::SessionSettings wholeMapSettings()
{
    domain::SessionSettings settings;

    settings.startingPaletteSize = domain::SUPPORTED_INTERVAL_COUNT;
    settings.choiceCount = domain::SUPPORTED_INTERVAL_COUNT;

    return settings;
}

// Le controleur, expose au QML une seule fois pour tout le binaire.
//
// Il est cree sur le TAS et son adresse est donnee a Qt, parce que qmlRegisterSingletonInstance prend POSSESSION de
// l'instance : un objet statique se ferait detruire une fois par Qt et une fois par le programme, ce qui s'appelle
// un "double free or corruption" et se termine par un test qui plante au lieu d'echouer proprement.
//
// La session est DEMARREE ici, et pas au moment ou l'ecran se charge : une propriete lue avant sa premiere
// notification ne se rattrape pas toute seule, et un ecran sans un seul bouton ressemble alors a un QML casse.
[[nodiscard]] ExerciseSessionController & wholeMapController()
{
    application();

    static domain::NotePlayerFake notePlayer;

    static ExerciseSessionController * controller = [] {
        auto * created = new ExerciseSessionController{ notePlayer, wholeMapSettings() };

        created->startSession();

        return created;
    }();

    return *controller;
}

// Tous les items d'un ecran, en profondeur, par childItems().
//
// Et NON par findChildren<QQuickItem *>(), ce qui a coute des heures : un delegue cree par un Repeater a bien sa
// case pour parent VISUEL - childItems() le montre - mais pas pour parent QObject. L'arbre visuel et l'arbre des
// QObject ne sont pas le meme arbre, et findChildren ne voit que le second.
//
// Consequence : un test qui cherche les boutons du cercle avec findChildren trouve un ecran VIDE, en conclut que le
// QML ne construit rien, et envoie chercher pendant une soiree un bug qui n'existe pas.
[[nodiscard]] std::vector<QQuickItem *> everyItem( QQuickItem & p_root )
{
    std::vector<QQuickItem *> items;
    std::vector<QQuickItem *> pending{ &p_root };

    while( !pending.empty() )
    {
        QQuickItem * current = pending.back();

        pending.pop_back();

        for( QQuickItem * child : current->childItems() )
        {
            items.push_back( child );
            pending.push_back( child );
        }
    }

    return items;
}

// Ce que QML a dit depuis le debut : la phrase exacte, quand une attente echoue.
[[nodiscard]] std::string qmlMessagesAsText()
{
    return qmlMessages().join( QStringLiteral( "\n" ) ).toStdString();
}

// Tous les textes des boutons de l'arbre, et le compte des items par type.
//
// Un echec qui dit "le bouton P1 n'est pas la" laisse deux pistes ouvertes : l'ecran ne l'a pas construit, ou il l'a
// construit sans texte. Ce rapport les separe, et c'est la difference entre chercher une heure et lire une ligne.
[[nodiscard]] std::string screenReport( QQuickItem & p_screen )
{
    QStringList texts;
    QMap<QString, int> typeCounts;

    for( QQuickItem * child : everyItem( p_screen ) )
    {
        const QString type = QString::fromLatin1( child->metaObject()->className() );

        typeCounts[type] += 1;

        if( child->property( "down" ).isValid() )
        {
            texts.append( QStringLiteral( "'%1'" ).arg( child->property( "text" ).toString() ) );
        }
    }

    QStringList types;

    for( auto iterator = typeCounts.constBegin(); iterator != typeCounts.constEnd(); ++iterator )
    {
        types.append( QStringLiteral( "%1 x%2" ).arg( iterator.key() ).arg( iterator.value() ) );
    }

    QStringList repeaters;

    for( QQuickItem * child : p_screen.findChildren<QQuickItem *>() )
    {
        if( QString::fromLatin1( child->metaObject()->className() ).contains( QStringLiteral( "Repeater" ) ) )
        {
            auto * parent = qobject_cast<QQuickItem *>( child->parent() );

            QStringList childTypes;

            if( parent != nullptr )
            {
                for( QQuickItem * grandChild : parent->childItems() )
                {
                    childTypes.append( QString::fromLatin1( grandChild->metaObject()->className() ) );
                }
            }

            repeaters.append( QStringLiteral( "count=%1 parent=%2 enfants=%3 sousEnfants=[%4]" )
                                .arg( child->property( "count" ).toInt() )
                                .arg( QString::fromLatin1( child->parent()->metaObject()->className() ) )
                                .arg( child->childItems().size() )
                                .arg( childTypes.join( QStringLiteral( ", " ) ) ) );
        }
    }

    return QStringLiteral( "boutons : [%1] | repeaters : [%2] | items : %3" )
      .arg( texts.join( QStringLiteral( ", " ) ), repeaters.join( QStringLiteral( ", " ) ), types.join( QStringLiteral( ", " ) ) )
      .toStdString();
}

// L'ecran charge, avec de quoi le garder vivant.
//
// Le COMPOSANT doit vivre aussi longtemps que l'objet qu'il a produit : un objet cree par un composant est son
// enfant, et le composant detruit emporte l'ecran avec lui. Un unique_ptr sur l'ecran seul laisserait un pointeur
// dans le vide - invisible jusqu'au moment ou le test plante a la fin.
struct LoadedScreen
{
    // Pointeurs NUS, et aucune destruction : le moteur QML possede ses objets, et les detruire soi-meme produit
    // un "double free or corruption" APRES que tous les tests ont passe. Fuir volontairement dans un test est
    // honnete : le processus se termine juste apres.
    QQmlComponent * component = nullptr;
    QQuickItem * item = nullptr;
};

[[nodiscard]] LoadedScreen loadExerciseScreen()
{
    ExerciseSessionController & controller = wholeMapController();

    // L'enregistrement a lieu UNE SEULE FOIS, et c'est ce qui evite le double free raconte plus haut.
    static const bool IS_REGISTERED = [&controller] {
        qmlRegisterSingletonInstance( "Musichien", 1, 0, "ExerciseController", &controller );

        // Qt prend POSSESSION d'un singleton enregistre ainsi, et le detruira avec le moteur. Comme ce controleur
        // vit deja pour tout le binaire, la propriete est rendue au C++ - c'est l'autre face du meme piege que le
        // "double free" raconte plus haut, et elle se manifeste par un segfault APRES que tous les tests ont passe.
        QQmlEngine::setObjectOwnership( &controller, QQmlEngine::CppOwnership );

        return true;
    }();

    (void)IS_REGISTERED;

    // Le moteur aussi vit sur le tas, pour la raison dite dans application().
    static QQmlEngine * engine = new QQmlEngine;

    LoadedScreen screen;

    screen.component = new QQmlComponent{ engine, QUrl{ QStringLiteral( "qrc:/qml/ExerciseScreen.qml" ) } };

    screen.item = qobject_cast<QQuickItem *>( screen.component->create() );

    if( screen.component->isError() )
    {
        ADD_FAILURE() << "le QML n'a pas pu etre charge : " << screen.component->errorString().toStdString();
    }

    if( screen.item != nullptr )
    {
        // Une taille de telephone : sans elle les items mesurent zero, et un bouton de taille nulle est un bouton
        // dont on ne peut rien dire.
        screen.item->setWidth( 360 );
        screen.item->setHeight( 640 );

        // Et les evenements doivent etre TRAITES, sans quoi l'arbre est incomplet.
        //
        // C'est la decouverte de la journee, et elle m'a coute des heures : QML construit de facon DIFFEREE, sur
        // plusieurs tours de boucle. Juste apres create(), le Repeater connait ses vingt-cinq elements - count vaut
        // 25 - et pourtant il n'a encore construit aucun item ; les delegues, une fois construits, mettent un tour
        // de plus a construire les leurs.
        //
        // L'application ne s'en apercoit jamais : elle tourne sa boucle d'evenements, et tout arrive. Un test qui
        // lit l'arbre trop tot voit un ecran VIDE - pas un ecran casse - et conclut a tort que le QML ne construit
        // rien. Traiter les evenements une seule fois ne suffit pas : il faut laisser la boucle vivre un instant.
        QEventLoop eventLoop;

        QTimer::singleShot( 50, &eventLoop, &QEventLoop::quit );

        eventLoop.exec();
    }

    return screen;
}

// Le bouton qui porte ce texte, quand il existe : c'est ainsi qu'on retrouve les boutons du cercle sans avoir a
// deviner lequel est lequel.
[[nodiscard]] QQuickItem * buttonWithText( QQuickItem & p_screen, const QString & p_text )
{
    for( QQuickItem * child : everyItem( p_screen ) )
    {
        if( !child->property( "down" ).isValid() )
        {
            continue;
        }

        if( child->property( "text" ).toString() == p_text )
        {
            return child;
        }
    }

    return nullptr;
}

}    // namespace

TEST( ExerciseScreenTest, the_screen_is_built_and_shows_every_choice )
{
    const LoadedScreen screen = loadExerciseScreen();

    ASSERT_NE( nullptr, screen.item ) << "l'ecran n'a pas pu etre instancie. QML : " << qmlMessagesAsText();

    // Le controleur doit REELLEMENT etre en train de poser une question : c'est de la que viennent les boutons, et
    // une session qui n'a pas demarre ressemble a s'y meprendre a un QML qui ne construit rien.
    ASSERT_TRUE( wholeMapController().running() );
    ASSERT_FALSE( wholeMapController().gridPositions().isEmpty() );

    for( const QVariant & choice : wholeMapController().choices() )
    {
        const QString identifier = choice.toMap().value( "identifier" ).toString();

        // Le vrai sujet : tout ce que la session propose doit se retrouver ECRIT sur l'ecran. Une place restee vide
        // par erreur, un intervalle ecrase par un autre de sa classe, un Repeater qui ne produit rien - chacun de
        // ces silences se voit ici.
        EXPECT_NE( nullptr, buttonWithText( *screen.item, identifier ) )
          << "l'intervalle " << identifier.toStdString() << " n'est pas affiche. QML : " << qmlMessagesAsText()
          << " | positions : " << wholeMapController().gridPositions().size()
          << " choix : " << wholeMapController().choices().size() << " | " << screenReport( *screen.item );
    }
}

TEST( ExerciseScreenTest, the_three_octaves_of_a_note_are_all_on_the_screen )
{
    // Le cas exact rapporte par Roger : a la carte entiere, l'unisson, l'octave et la quinzieme visent la MEME place
    // du cercle. C'est le seul endroit ou trois intervalles partagent une case, et donc le seul ou l'ecran peut
    // cacher une reponse.
    const LoadedScreen screen = loadExerciseScreen();

    ASSERT_NE( nullptr, screen.item );

    for( const std::int32_t semitones : { 0, 12, 24 } )
    {
        const QString identifier = QString::fromStdString( domain::Interval{ semitones }.identifier() );

        EXPECT_NE( nullptr, buttonWithText( *screen.item, identifier ) )
          << "la place de l'unisson a perdu " << identifier.toStdString() << ". QML : " << qmlMessagesAsText();
    }

    // Et les trois sont bien la meme place du cercle : c'est ce qui fait qu'un joueur peut les comparer.
    const std::size_t unisonSlot = domain::circleOfFifthsSlot( domain::Interval{ 0 } );

    EXPECT_EQ( unisonSlot, domain::circleOfFifthsSlot( domain::Interval{ 12 } ) );
    EXPECT_EQ( unisonSlot, domain::circleOfFifthsSlot( domain::Interval{ 24 } ) );
}

}    // namespace musichien::ui
