#include "domain/audio/NotePlayerFake.h"
#include "ui/ExerciseSessionController.h"
#include "ui/IntervalPlaybackController.h"
#include "ui/MicrophoneController.h"
#include "ui/RhythmController.h"
#include "ui/StatisticsController.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QMetaObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariant>

#include <gtest/gtest.h>

#include <memory>

namespace musichien::ui
{

// ---------------------------------------------------------------------------------------------------------------------
// L'ecran PRINCIPAL, charge pour de vrai
//
// ExerciseScreen_test instancie l'ecran d'exercice depuis le debut, et il a deja servi : il a trouve une boucle de
// liaison, et il est le seul endroit qui aurait vu une propriete declaree deux fois. Main.qml, lui, n'etait charge par
// AUCUN test - alors que c'est la page sur laquelle l'application s'ouvre, et celle ou vivent tous les reglages.
//
// Un QML casse ne fait echouer aucun test de view model, ne casse aucun build, et fait demarrer l'application sur une
// page noire : c'est le joueur qui le decouvre. Ce fichier existe pour que ce soit le build.
//
// Les quatre singletons sont enregistres ICI, dans la meme forme que main.cpp : c'est le prix d'un ecran qui lit quatre
// view models, et il se paie une fois.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{

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

// Une application Qt pour tout le binaire : QML ne peut pas instancier un Item sans elle, et il n'en faut qu'une.
//
// Sur le TAS et jamais detruite, comme dans ExerciseScreen_test : detruire une application Qt pendant que des objets Qt
// vivent encore termine un processus de test par un segfault, APRES que tous les tests ont passe.
[[nodiscard]] QGuiApplication & application()
{
    static int argumentCount = 1;
    static char argumentName[]{ "test_ui" };
    static char * arguments[]{ argumentName, nullptr };

    static QGuiApplication * instance = new QGuiApplication{ argumentCount, arguments };

    return *instance;
}

// Les quatre view models de la page, exposes au QML comme main.cpp le fait.
//
// Ils vivent sur le TAS pour la meme raison que l'application : qmlRegisterSingletonInstance prend POSSESSION de
// l'instance, et un objet statique se ferait detruire une fois par Qt et une fois par le programme.
void registerViewModels()
{
    application();

    static auto * notePlayer = new domain::NotePlayerFake;

    static auto * exerciseController = new ExerciseSessionController{ *notePlayer };
    static auto * intervalController = new IntervalPlaybackController{ *notePlayer };
    static auto * rhythmController = new RhythmController{ *notePlayer };

    static auto * microphoneController = new MicrophoneController{ QStringList{}, {}, nullptr, notePlayer };

    // Le journal et la page de statistiques : la page de profil lit le premier a travers le second, et sans eux
    // Main.qml ne se charge pas du tout.
    static auto * questionLog = new domain::QuestionLogFake;
    static auto * statisticsController = new StatisticsController{ *questionLog };

    static const bool REGISTERED = [] {
        qmlRegisterSingletonInstance( "Musichien", 1, 0, "ExerciseController", exerciseController );
        qmlRegisterSingletonInstance( "Musichien", 1, 0, "IntervalController", intervalController );
        qmlRegisterSingletonInstance( "Musichien", 1, 0, "RhythmController", rhythmController );
        qmlRegisterSingletonInstance( "Musichien", 1, 0, "MicrophoneController", microphoneController );
        qmlRegisterSingletonInstance( "Musichien", 1, 0, "StatisticsController", statisticsController );

        return true;
    }();

    (void)REGISTERED;
}

// Tous les textes affiches a l'ecran, en profondeur : c'est ce qui permet de demander « le bouton Jouer est-il la ? »
// sans dependre de la moindre position.
void collectTexts( QObject * p_object, QStringList & p_texts )
{
    if( p_object == nullptr )
    {
        return;
    }

    const QVariant text = p_object->property( "text" );

    if( text.isValid() && !text.toString().isEmpty() )
    {
        p_texts.append( text.toString() );
    }

    for( QObject * child : p_object->children() )
    {
        collectTexts( child, p_texts );
    }
}

[[nodiscard]] std::unique_ptr<QObject> loadMainScreen( QQmlEngine & p_engine )
{
    registerViewModels();

    QQmlComponent component{ &p_engine, QUrl{ QStringLiteral( "qrc:/qml/Main.qml" ) } };

    std::unique_ptr<QObject> root{ component.create() };

    if( root == nullptr )
    {
        qmlMessages().append( component.errorString() );
    }

    return root;
}

}    // namespace

TEST( MainScreenTest, the_main_screen_instantiates )
{
    // L'application AVANT le moteur : QQmlEngine refuse d'exister sans instance de QCoreApplication, et il le dit en
    // ABORTANT. C'est exactement le crash que ce fichier a attrape la premiere fois qu'il a tourne - et c'est la raison
    // pour laquelle Main.qml a maintenant un test.
    application();

    QQmlEngine engine;

    const std::unique_ptr<QObject> screen = loadMainScreen( engine );

    ASSERT_NE( nullptr, screen.get() )
      << "l'ecran principal n'a pas pu etre instancie : " << qmlMessages().join( '\n' ).toStdString();
}

TEST( MainScreenTest, the_main_screen_shows_its_main_actions )
{
    application();

    QQmlEngine engine;

    const std::unique_ptr<QObject> screen = loadMainScreen( engine );

    ASSERT_NE( nullptr, screen.get() );

    QStringList texts;

    collectTexts( screen.get(), texts );

    // La porte d'entree de l'application. Le test cherche un MOT, et non un libelle exact : une icone peut s'ajouter
    // devant, et ce fichier ne doit pas se casser a chaque retouche d'orthographe d'un bouton.
    const auto hasTextContaining = [&texts]( const QString & p_word ) {
        return std::ranges::any_of( texts, [&p_word]( const QString & p_text ) {
            return p_text.contains( p_word );
        } );
    };

    EXPECT_TRUE( hasTextContaining( QStringLiteral( "Jouer" ) ) ) << "le bouton Jouer est absent de l'ecran principal";

    // Et la page construit vraiment quelque chose : une vingtaine de textes, c'est le minimum d'une page de reglages
    // qui en compte des dizaines.
    EXPECT_GT( texts.size(), 20 ) << "l'ecran principal ne construit presque rien";
}

TEST( MainScreenTest, nothing_needs_to_scroll_sideways_on_a_phone )
{
    application();

    QQmlEngine engine;

    const std::unique_ptr<QObject> screen = loadMainScreen( engine );

    ASSERT_NE( nullptr, screen.get() );

    // La taille d'un telephone : un Pixel 7a, en points logiques. C'est la LARGEUR qui compte, et c'est pour elle que
    // toutes les pages doivent tenir.
    screen->setProperty( "width", 412.0 );
    screen->setProperty( "height", 915.0 );

    QCoreApplication::processEvents();

    // Les dialogues sont FERMES au chargement : il faut les ouvrir pour mesurer ce qu'ils contiennent, sinon le test
    // regarde des pages de largeur nulle et ne prouve rien.
    //
    // ET IL FAUT LEUR LAISSER LE TEMPS de se mettre en page : un dialogue qui vient de s'ouvrir n'a pas encore reparti
    // ses enfants, et mesurer tout de suite donnerait leur largeur IMPLICITE - donc un faux positif sur chaque texte un
    // peu long. C'est la lecon de ce test, apprise en le faisant mentir.
    for( int pass = 0; pass < 6; ++pass )
    {
        if( pass == 0 )
        {
            const QList<QObject *> dialogs = screen->findChildren<QObject *>( QString{}, Qt::FindChildrenRecursively );

            for( QObject * dialog : dialogs )
            {
                if( dialog->inherits( "QQuickDialog" ) )
                {
                    QMetaObject::invokeMethod( dialog, "open" );
                }
            }
        }

        QCoreApplication::processEvents();
    }

    // CE QUE ROGER A VU : « la page est trop large, du coup ca scroll aussi a l'horizontal, ce qui n'est pas agreable ».
    // Un contenu plus large que sa vue est exactement ce symptome, et c'est mesurable sans connaitre le type des objets :
    // un Flickable est le seul a porter un « contentWidth », donc le chercher par sa PROPRIETE marche pour tous les
    // ScrollView du fichier, presents et futurs.
    const QList<QObject *> items = screen->findChildren<QObject *>( QString{}, Qt::FindChildrenRecursively );

    int flickableCount = 0;

    for( QObject * item : items )
    {
        const QVariant contentWidth = item->property( "contentWidth" );

        if( !contentWidth.isValid() )
        {
            continue;
        }

        // Un champ de saisie a le droit d'avoir un curseur un peu plus large que sa vue : c'est le seul depassement
        // normal, et il ne se voit pas.
        if( item->inherits( "QQuickTextInput" ) )
        {
            continue;
        }

        // Et on ne mesure que ce qui est AFFICHE. Un dialogue ferme - ou qu'un environnement sans ecran n'arrive pas a
        // ouvrir - n'a pas encore reparti ses enfants : sa mesure serait leur largeur implicite, donc un faux positif sur
        // chaque texte un peu long. Le test dit ce qu'il peut prouver, et rien de plus.
        if( !item->property( "visible" ).toBool() )
        {
            continue;
        }

        ++flickableCount;

        const double viewWidth = item->property( "width" ).toDouble();

        // Une vue de largeur nulle n'a pas encore ete mise en page : sa mesure ne veut rien dire.
        if( viewWidth <= 0.0 )
        {
            continue;
        }

        // La TOLERANCE est l'epaisseur d'une barre de defilement verticale - sept points - et elle est inevitable : une
        // page qui defile verticalement reserve cette largeur, et la colonne de contenu est calculee avant que la barre
        // apparaisse. Ces sept points sont caches sous la barre, et la barre HORIZONTALE est desactivee : rien ne bouge a
        // l'ecran. Ce qui compte, c'est d'attraper les cent-cinquante points de trop, ceux qui se voyaient vraiment.
        constexpr double SCROLLBAR_TOLERANCE = 10.0;

        if( contentWidth.toDouble() > viewWidth + SCROLLBAR_TOLERANCE )
        {
            // Le TEXTE de l'objet est dans le message : c'est lui qui dit quel enfant est trop large, et sans lui il
            // faudrait chercher a l'oeil dans un fichier de deux mille lignes.
            const QString label = item->property( "text" ).toString().left( 60 );

            // Et quand ce n'est pas un texte, on liste les ENFANTS trop larges : c'est l'un d'eux qui elargit la page, et
            // un test qui dit « ca deborde » sans dire qui est un test qu'on n'ecoute pas.
            QStringList culprits;

            for( QObject * child : item->findChildren<QObject *>( QString{}, Qt::FindChildrenRecursively ) )
            {
                const QVariant childWidth = child->property( "width" );

                if( childWidth.isValid() && ( childWidth.toDouble() > viewWidth + 1.0 ) && ( culprits.size() < 4 ) )
                {
                    const QString childText = child->property( "text" ).toString().left( 50 );

                    culprits << QStringLiteral( "%1:%2(%3)" )
                                  .arg( QString::fromUtf8( child->metaObject()->className() ) )
                                  .arg( childWidth.toDouble() )
                                  .arg( childText );
                }
            }

            ADD_FAILURE() << item->metaObject()->className() << " fait " << contentWidth.toDouble() << " pour une vue de "
                          << viewWidth << " — « " << label.toStdString() << " » — trop larges : "
                          << culprits.join( QStringLiteral( ", " ) ).toStdString();
        }
    }

    ASSERT_GT( flickableCount, 0 ) << "aucune page defilante trouvee : le test ne mesure rien";
}

}    // namespace musichien::ui
