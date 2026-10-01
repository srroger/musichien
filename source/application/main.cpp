// =====================================================================================================================
// Musichien - application entry point
//
// This file only wires things together: it creates the QML engine, exposes the domain to QML and
// hands control over. No musical logic lives here, and no rule of the game either.
// =====================================================================================================================

#include "infrastructure/audio/QAudioNotePlayer.h"
#include "infrastructure/audio/QAudioPitchDetector.h"
#include "infrastructure/content/JsonAnecdoteBook.h"
#include "infrastructure/content/JsonHintBook.h"
#include "infrastructure/content/JsonPhraseBook.h"
#include "infrastructure/content/JsonTunerGuide.h"
#include "infrastructure/haptics/DeviceHaptics.h"
#ifdef Q_OS_ANDROID
#    include "infrastructure/android/AndroidSystemBars.h"
#    include "infrastructure/notifications/AndroidNotificationScheduler.h"
#else
#    include "infrastructure/notifications/NullNotificationScheduler.h"
#endif
#include "infrastructure/preferences/QSettingsPlayerPreferences.h"
#include "infrastructure/statistics/JsonLinesQuestionLog.h"
#include "musichienBuildId.h"
#include "ui/ExerciseSessionController.h"
#include "ui/IntervalPlaybackController.h"
#include "ui/KeyCircleController.h"
#include "ui/MicrophoneController.h"
#include "ui/ModePreviewController.h"
#include "ui/RhythmController.h"
#include "ui/StatisticsController.h"

#include <QAudioDevice>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QMediaDevices>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QString>
#include <QUrl>
#include <QtQml>

#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace
{

// Version of the application, injected by the build system so that there is only one place to
// maintain it: the project() call of the top level CMakeLists.txt.
constexpr const char * APPLICATION_NAME = "Musichien";

// QML module under which the view models are exposed to the interface.
//
// Registering a singleton rather than a context property keeps the QML honest: it imports a module and
// calls a named object, instead of relying on a global name injected from the outside.
constexpr const char * QML_MODULE_NAME = "Musichien";
constexpr int QML_MODULE_MAJOR_VERSION = 1;
constexpr int QML_MODULE_MINOR_VERSION = 0;

// Where the content files live once embedded. The path is the one they have on disk, thanks to the alias
// declared in resources.qrc: one path to remember, identical on the desktop and on the phone.
constexpr const char * INTERVAL_HINTS_RESOURCE = ":/assets/content/interval-hints.json";

constexpr const char * ANECDOTES_RESOURCE = ":/assets/content/anecdotes.json";

constexpr const char * TUNER_GUIDE_RESOURCE = ":/assets/content/tuner.json";

// Les phrases modales : ce que l'oreille de Roger a garde a l'atelier, et rien de plus. Le fichier porte des DEGRES,
// une tonique et un tempo - jamais d'audio - et c'est le moteur du jeu qui les rejoue.
constexpr const char * MODAL_PHRASES_RESOURCE = ":/assets/content/modal-phrases.json";

// Reads the memory hints from the resources.
//
// The file is opened HERE and not inside the reader: understanding JSON is the infrastructure's job,
// reading a Qt resource is an application concern, and this is the layer allowed to know both.
//
// Missing or broken content is deliberately not fatal. A game that refuses to start because a hint is
// malformed would trade a small loss for a total one.
[[nodiscard]] musichien::domain::HintBook loadHintBook()
{
    QFile contentFile{ QString::fromUtf8( INTERVAL_HINTS_RESOURCE ) };

    if( !contentFile.open( QIODevice::ReadOnly ) )
    {
        std::cerr << "Musichien: the interval hints are missing from the resources.\n";

        return {};
    }

    const QByteArray content = contentFile.readAll();

    musichien::domain::HintBook hintBook = musichien::infrastructure::readHintBook(
      std::string_view{ content.constData(), static_cast<std::size_t>( content.size() ) } );

    std::cerr << "Musichien: " << hintBook.hintCount() << " interval hints read\n";

    return hintBook;
}

// Reads the loading-screen anecdotes, the Morrowind-style little texts. Same contract as the hints: a missing or
// broken file costs the anecdotes, never the application.
[[nodiscard]] musichien::domain::AnecdoteBook loadAnecdoteBook()
{
    QFile contentFile{ QString::fromUtf8( ANECDOTES_RESOURCE ) };

    if( !contentFile.open( QIODevice::ReadOnly ) )
    {
        std::cerr << "Musichien: the anecdotes are missing from the resources.\n";

        return {};
    }

    const QByteArray content = contentFile.readAll();

    musichien::domain::AnecdoteBook book = musichien::infrastructure::readAnecdoteBook(
      std::string_view{ content.constData(), static_cast<std::size_t>( content.size() ) } );

    std::cerr << "Musichien: " << book.count() << " anecdotes read\n";

    return book;
}

// Les phrases modales, du meme contrat que tout le reste : un fichier manquant ou casse coute les phrases, jamais
// l'application. Le banc d'essai s'en passe alors, simplement.
[[nodiscard]] musichien::domain::PhraseBook loadPhraseBook()
{
    QFile contentFile{ QString::fromUtf8( MODAL_PHRASES_RESOURCE ) };

    if( !contentFile.open( QIODevice::ReadOnly ) )
    {
        std::cerr << "Musichien: the modal phrases are missing from the resources.\n";

        return {};
    }

    const QByteArray content = contentFile.readAll();

    musichien::domain::PhraseBook phraseBook = musichien::infrastructure::readPhraseBook(
      std::string_view{ content.constData(), static_cast<std::size_t>( content.size() ) } );

    // La ligne qui prouve le plus court chemin entre le fichier et le binaire : elle distingue « le fichier est la »
    // de « le fichier a ete compris », et c'est la seule verification de l'embarquement qui ne se discute pas.
    std::cerr << "Musichien: " << phraseBook.phraseCount() << " modal phrases read\n";

    return phraseBook;
}

// Les textes de la page Accordeur : ce qu'un temperament est, d'ou il vient, ce que sont le diapason et la note de
// reference, et comment se servir d'un accordeur. Meme contrat que les indices et les anecdotes : un fichier manquant
// ou casse coute les explications, jamais l'application.
[[nodiscard]] musichien::domain::TunerGuide loadTunerGuide()
{
    QFile contentFile{ QString::fromUtf8( TUNER_GUIDE_RESOURCE ) };

    if( !contentFile.open( QIODevice::ReadOnly ) )
    {
        std::cerr << "Musichien: the tuner texts are missing from the resources.\n";

        return {};
    }

    const QByteArray content = contentFile.readAll();

    musichien::domain::TunerGuide guide = musichien::infrastructure::readTunerGuide(
      std::string_view{ content.constData(), static_cast<std::size_t>( content.size() ) } );

    std::cerr << "Musichien: " << guide.temperamentCount() << " temperament texts read\n";

    return guide;
}

// UNE anecdote au hasard, prete a devenir le texte d'une notification.
//
// UNE seule, et c'est tout le point : une notification qui contiendrait les vingt anecdotes du fichier ne serait pas une
// notification, ce serait un mur de texte - et personne ne lit un mur de texte sur un ecran verrouille. La litterature
// du fichier sert l'ecran d'accueil et l'ecran de fin de session ; ici, c'est une phrase qui doit tenir dans une bulle.
[[nodiscard]] std::string randomAnecdoteText( const musichien::domain::AnecdoteBook & p_anecdotes,
                                              std::mt19937 & p_randomEngine )
{
    const std::optional<musichien::domain::Anecdote> anecdote = p_anecdotes.random( p_randomEngine );

    return anecdote.has_value() ? anecdote->text : std::string{};
}

// Reads ONE sampled instrument from the embedded wave files.
//
// Five recorded notes an octave apart, and the sampler picks the closest one: that is enough for the whole
// range, because the transposition never goes past three semitones. The list of notes is written here rather
// than guessed from the file names, so that renaming a file cannot silently move a note.
//
// A missing note costs that note and nothing else: the instrument works with what it has, and three notes are
// still an instrument. Nothing about a missing sample is worth refusing to start over.
[[nodiscard]] musichien::domain::SampledInstrument loadInstrument( const QString & p_instrumentName )
{
    constexpr std::array<std::int32_t, 5> ROOT_MIDI_NUMBERS{ 36, 48, 60, 72, 84 };
    constexpr std::array<const char *, 5> NOTE_NAMES{ "c2", "c3", "c4", "c5", "c6" };

    musichien::domain::SampledInstrument instrument;

    for( std::size_t noteIndex = 0; noteIndex < ROOT_MIDI_NUMBERS.size(); ++noteIndex )
    {
        QFile sampleFile{ QStringLiteral( ":/assets/soundfonts/%1_%2.wav" )
                            .arg( p_instrumentName, QString::fromLatin1( NOTE_NAMES.at( noteIndex ) ) ) };

        if( !sampleFile.open( QIODevice::ReadOnly ) )
        {
            continue;
        }

        const QByteArray content = sampleFile.readAll();

        // The bytes of a Qt array read as bytes: the sampler understands a FORMAT, not a file.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        const std::span<const std::byte> bytes{ reinterpret_cast<const std::byte *>( content.constData() ),
                                                static_cast<std::size_t>( content.size() ) };

        const std::optional<musichien::domain::SampledNote> note =
          musichien::domain::sampledNoteFromWave( bytes, ROOT_MIDI_NUMBERS.at( noteIndex ) );

        if( note.has_value() )
        {
            instrument.addNote( *note );
        }
    }

    return instrument;
}

// The instruments the game plays with, in the order they are offered.
//
// CE SONT DES NOMS, ET RIEN D'AUTRE : l'application charge <nom>_c2.wav a <nom>_c6.wav. Ajouter un timbre, c'est donc une
// ligne ici, une ligne dans INSTRUMENT_NAMES (le domaine, qui en connait aussi les trois formes d'onde) et cinq fichiers
// declares dans resources.qrc. En retirer un, c'est les memes lignes et ses fichiers. Voir
// scripts/render_instrument_samples.py, qui les fabrique.
//
// Les cinq derniers sont DOUX, et c'est une demande de Roger : « j'utilise que la guitare et le piano, les autres sont
// trop agressifs. N'hesite pas a rajouter des timbres doux ». Une seconde mineure deja dissonante devient boueuse avec un
// timbre riche, et un timbre qui grince fait fermer l'application.
constexpr std::array<const char *, 8> SAMPLED_INSTRUMENT_NAMES{
  "piano", "guitare", "saxo", "flute", "cordes", "clarinette", "marimba", "harpe" };

// Les trois bourdons enregistres, et l'ordre dans lequel ils sont offerts.
//
// Ce ne sont PAS des instruments de melodie : ce sont les sons qui tiennent SOUS une gamme, et c'est une autre
// fonction. Ils sont joues en QUINTE (la tonique et sa quinte), ce que Roger a demande deux fois de suite - une note
// seule dit « ceci est la tonique », une quinte dit « ceci est le CENTRE », et ce n'est pas la meme information.
//
// Ils viennent de scripts/render_drone_samples.py, donc de la meme banque libre que le piano, la guitare et la batterie.
[[nodiscard]] musichien::domain::SampledInstrument loadDroneInstrument( const QString & p_droneName )
{
    // DEUX notes, et deux seulement : re 2 et la 2, soit une quinte juste. Le bourdon ne change de tonique qu'a
    // quelques demi-tons, donc cette paire couvre tout ce dont un exercice modal a besoin.
    constexpr std::array<std::int32_t, 2> ROOT_MIDI_NUMBERS{ 38, 45 };
    constexpr std::array<const char *, 2> NOTE_NAMES{ "d2", "a2" };

    musichien::domain::SampledInstrument drone;

    for( std::size_t noteIndex = 0; noteIndex < ROOT_MIDI_NUMBERS.size(); ++noteIndex )
    {
        QFile sampleFile{ QStringLiteral( ":/assets/soundfonts/drone_%1_%2.wav" )
                            .arg( p_droneName, QString::fromLatin1( NOTE_NAMES.at( noteIndex ) ) ) };

        if( !sampleFile.open( QIODevice::ReadOnly ) )
        {
            continue;
        }

        const QByteArray content = sampleFile.readAll();

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        const std::span<const std::byte> bytes{ reinterpret_cast<const std::byte *>( content.constData() ),
                                                static_cast<std::size_t>( content.size() ) };

        const std::optional<musichien::domain::SampledNote> note =
          musichien::domain::sampledNoteFromWave( bytes, ROOT_MIDI_NUMBERS.at( noteIndex ) );

        if( note.has_value() )
        {
            drone.addNote( *note );
        }
    }

    return drone;
}

// Reads one embedded wave file as mono samples.
//
// Empty when the file is missing or unreadable, which costs a sound and never the application: the synthesiser takes
// over, exactly like it does for a missing instrument.
[[nodiscard]] std::vector<float> loadSample( const char * p_fileName )
{
    QFile sampleFile{ QStringLiteral( ":/assets/soundfonts/%1.wav" ).arg( QString::fromLatin1( p_fileName ) ) };

    if( !sampleFile.open( QIODevice::ReadOnly ) )
    {
        return {};
    }

    const QByteArray content = sampleFile.readAll();

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const std::span<const std::byte> bytes{ reinterpret_cast<const std::byte *>( content.constData() ),
                                            static_cast<std::size_t>( content.size() ) };

    // La hauteur n'a aucune importance pour une percussion : seul l'echantillon compte.
    const std::optional<musichien::domain::SampledNote> note = musichien::domain::sampledNoteFromWave( bytes, 0 );

    return note.has_value() ? note->samples : std::vector<float>{};
}

// Les quatre sons de batterie, dans l'ordre de domain::Drum.
//
// Une percussion n'est PAS une note : elle ne se transpose pas, elle se joue telle quelle. Le fichier est donc lu pour
// ses echantillons seulement.
[[nodiscard]] std::array<std::vector<float>, musichien::domain::DRUM_COUNT> loadDrumSamples()
{
    constexpr std::array<const char *, musichien::domain::DRUM_COUNT> FILE_NAMES{
      "drum_kick", "drum_snare", "drum_hihat", "drum_tom" };

    std::array<std::vector<float>, musichien::domain::DRUM_COUNT> samples;

    for( std::size_t index = 0; index < FILE_NAMES.size(); ++index )
    {
        samples.at( index ) = loadSample( FILE_NAMES.at( index ) );
    }

    return samples;
}

}    // namespace

int main( int p_argumentCount, char * p_arguments[] )
{
    QGuiApplication application{ p_argumentCount, p_arguments };

    // These members are static in Qt: calling them on the class makes that explicit, and avoids a
    // "static member accessed through instance" finding.
    QGuiApplication::setApplicationName( APPLICATION_NAME );

    // The version says what the code is SUPPOSED to be; the build identifier says which commit was actually
    // compiled. Both are displayed together, and until the first release that is deliberate: what runs on a
    // phone can be weeks old, and "0.5.0" alone cannot tell two builds of it apart.
    QGuiApplication::setApplicationVersion( QStringLiteral( MUSICHIEN_VERSION " · " MUSICHIEN_BUILD_ID ) );

    QGuiApplication::setOrganizationName( "Musichien" );

    // LA POLICE DE L'INTERFACE. Une police ne se devine pas d'apres le systeme : elle voyage avec l'application, comme
    // la police musicale voyage deja avec elle. Le style Material peint ses controles avec la police de
    // l'application, donc ce seul appel habille TOUTE l'interface, y compris les boutons que le style dessine
    // lui-meme - c'est ce qui evite d'ecrire un font.family dans chacun des quelque trois cents Text du projet.
    //
    // C'est une RESSOURCE : si elle manque, on garde la police du systeme et l'application s'ouvre quand meme. Une
    // jolie police ne vaut pas un demarrage rate.
    const int uiFontIdentifier = QFontDatabase::addApplicationFont( ":/assets/fonts/Quicksand-Variable.ttf" );
    const QStringList uiFontFamilies = QFontDatabase::applicationFontFamilies( uiFontIdentifier );

    if( !uiFontFamilies.isEmpty() )
    {
        QFont uiFont{ uiFontFamilies.constFirst() };

        // Medium, et pas Regular : Quicksand est une police fine, et un texte fin sur la nuit violette se lit mal de
        // loin. Elle reste ronde - c'est sa forme, pas son epaisseur, qui fait sa douceur.
        uiFont.setWeight( QFont::Medium );
        QGuiApplication::setFont( uiFont );

        std::cerr << "Musichien: interface font \"" << uiFontFamilies.constFirst().toStdString() << "\"\n";
    }

    // Material is the style Qt Quick Controls maps onto the Android look and feel. Using it from the
    // first line guarantees that what is developed on the desktop looks like what runs on the phone.
    QQuickStyle::setStyle( "Material" );

    // Diagnostics go to std::cerr on purpose: stderr is not buffered, so these lines always appear
    // immediately, even when the output is redirected to a file or to a pipe.
    std::cerr << APPLICATION_NAME << " " << MUSICHIEN_VERSION << " (" << MUSICHIEN_BUILD_ID << ")\n";

    // -------------------------------------------------------------------------------------------------------------
    // Wiring
    //
    // The domain declares a port (NotePlayer), the infrastructure provides an adapter
    // (QAudioNotePlayer), and this function connects the two. It is the only place in the project that
    // knows about the domain, the interface and the concrete implementations at the same time.
    // -------------------------------------------------------------------------------------------------------------
    musichien::infrastructure::QAudioNotePlayer notePlayer;

    // Le metronome et le jeu de rythme. Un seul controleur pour les deux : le metronome qui bat est la meme boucle
    // que le jeu, et un outil de musicien ne demande pas deux classes.
    musichien::ui::RhythmController rhythmController{ notePlayer };

    // What the application remembers about its player. A small settings file, on the device: the package
    // cannot reach the network, so nothing about him ever leaves the phone.
    musichien::infrastructure::QSettingsPlayerPreferences playerLevelStore;

    // The sampled instruments, embedded in the resources. From here on they are the sound of the EXERCISES; the
    // synthesiser keeps the mistake cue, which must not be beautiful, and stays the fallback if a sample is
    // missing.
    std::vector<musichien::domain::SampledInstrument> instruments;
    instruments.reserve( SAMPLED_INSTRUMENT_NAMES.size() );

    for( const char * instrumentName : SAMPLED_INSTRUMENT_NAMES )
    {
        // AUCUN INSTRUMENT N'EST RETIRE DE LA LISTE, meme quand sa ressource n'a pas pu etre lue.
        //
        // Le RANG d'un instrument est son identite : le domaine dit « joue le 4e », et le cable ne traduit rien. Un
        // instrument saute ferait glisser tous les suivants d'un cran, et l'apercu comme les cases a cocher joueraient
        // alors d'autres timbres que les leurs. Une ressource manquante laisse donc un TROU a sa place, et l'adaptateur
        // retombe sur la synthese pour ce rang.
        instruments.push_back( loadInstrument( QString::fromLatin1( instrumentName ) ) );
    }

    const auto loadedInstrumentCount = static_cast<std::size_t>( std::ranges::count_if(
      instruments, []( const musichien::domain::SampledInstrument & p_instrument ) { return !p_instrument.isEmpty(); } ) );

    std::cerr << "Musichien: " << loadedInstrumentCount << " sampled instrument(s) read, " << instruments.size()
              << " timbre(s) offered\n";

    // La batterie, rendue depuis la meme banque libre que les instruments : une vraie peau vaut mieux qu'une chute de
    // sinus.
    std::array<std::vector<float>, musichien::domain::DRUM_COUNT> drumSamples = loadDrumSamples();

    const auto loadedDrumCount = static_cast<std::size_t>( std::ranges::count_if(
      drumSamples, []( const std::vector<float> & p_samples ) { return !p_samples.empty(); } ) );

    std::cerr << "Musichien: " << loadedDrumCount << " drum sample(s) read\n";

    notePlayer.useDrumSamples( std::move( drumSamples ) );

    // Les deux clics du metronome : deux blocs de bois, dans la meme banque libre.
    notePlayer.useMetronomeClicks( loadSample( "drum_click_high" ), loadSample( "drum_click_low" ) );

    // Les quatre woufs du chien, synthetises une fois pour toutes par scripts/render_dog_bark.py et charges comme le
    // reste. Ils TOURNENT a chaque aboiement : c'est ce qui fait que le chien ne repete pas la meme phrase.
    // Une ressource manquante coute une variante, jamais le demarrage : le clic de menu prend sa place.
    constexpr std::array<const char *, 4> DOG_BARK_NAMES{ "dog_bark", "dog_bark_2", "dog_bark_3", "dog_bark_4" };

    std::vector<std::vector<float>> dogBarks;
    dogBarks.reserve( DOG_BARK_NAMES.size() );

    for( const char * dogBarkName : DOG_BARK_NAMES )
    {
        dogBarks.push_back( loadSample( dogBarkName ) );
    }

    notePlayer.useDogBarks( std::move( dogBarks ) );

    // Les trois bourdons enregistres, charges ICI comme les instruments et pour la meme raison : une ressource
    // manquante coute un timbre, jamais le demarrage.
    constexpr std::array<const char *, 3> DRONE_NAMES{ "strings", "choir", "pad" };

    std::vector<musichien::domain::SampledInstrument> drones;

    for( const char * droneName : DRONE_NAMES )
    {
        musichien::domain::SampledInstrument drone = loadDroneInstrument( QString::fromLatin1( droneName ) );

        if( !drone.isEmpty() )
        {
            drones.push_back( std::move( drone ) );
        }
    }

    // Le journal de demarrage dit ce qui a ete LU, comme pour les intervalles et la batterie : c'est la preuve la plus
    // courte que les fichiers ont suivi jusqu'au binaire.
    std::cerr << "Musichien: " << drones.size() << " drone timbre(s) read\n";

    notePlayer.useDroneInstruments( std::move( drones ) );

    // Les trois formes d'onde pures FERMENT la liste des timbres, et leur rang suit celui des echantillons - exactement
    // comme domain::INSTRUMENT_NAMES les nomme. Elles ne coutent rien (un drapeau dans les reglages), donc elles gardent
    // TOUJOURS leur place, meme decochees : c'est ce qui permet plus bas de parler de « l'instrument 10 ».
    const std::vector<musichien::domain::Waveform> waveforms{ musichien::domain::WAVEFORM_INSTRUMENTS.begin(),
                                                              musichien::domain::WAVEFORM_INSTRUMENTS.end() };

    notePlayer.useInstruments( std::move( instruments ), waveforms );

    // Opening the output now, rather than at the first note, means a machine without a sound card is
    // reported at start up instead of silently refusing to play in the middle of an exercise.
    notePlayer.prepareAudioOutput();

    std::cerr << "Musichien: audio output is " << notePlayer.audioOutputDescription() << "\n";

    // Un casque Bluetooth branche ou debranche change la liste des sorties : on rouvre la sortie sur le nouvel
    // appareil par defaut, sinon le son resterait sur un peripherique mort. Les ENTREES, elles, ne peuvent pas etre
    // rebranchees toutes seules : on avertit, et les reglages permettent de rechoisir le micro.
    QMediaDevices mediaDevices;

    QObject::connect( &mediaDevices, &QMediaDevices::audioOutputsChanged, &mediaDevices, [&notePlayer]() {
        std::cerr << "Musichien: audio outputs changed, reopening the output.\n";
        notePlayer.reopenAudioOutput();
    } );

    QObject::connect( &mediaDevices, &QMediaDevices::audioInputsChanged, &mediaDevices, []() {
        std::cerr << "Musichien: audio inputs changed - reopen the settings to pick the new microphone.\n";
    } );

    // The view model only receives the PORT, never the adapter: it could be handed the fake player of
    // the unit tests without a single line of it changing.
    musichien::ui::IntervalPlaybackController intervalController{ notePlayer };

    qmlRegisterSingletonInstance( QML_MODULE_NAME,
                                  QML_MODULE_MAJOR_VERSION,
                                  QML_MODULE_MINOR_VERSION,
                                  "IntervalController",
                                  &intervalController );

    // Les phrases modales, lues AVANT le banc d'essai qui va les jouer : le livre doit vivre plus longtemps que le
    // controleur, comme le journal des questions. Un objet detruit se voit tres mal, et se voit toujours trop tard.
    const musichien::domain::PhraseBook modalPhraseBook = loadPhraseBook();

    // Le banc d'essai des modes : le MEME port, et rien de plus. Sept boutons qui font entendre une couleur sur un
    // bourdon tenu - c'est exactement ce que l'exercice du degrade demandera, jusqu'au timbre du bourdon, qui est tire
    // par l'adaptateur. Un banc d'essai qui sonnerait autrement que le jeu serait pire qu'inutile.
    musichien::ui::ModePreviewController modePreviewController{ notePlayer };

    modePreviewController.setPhraseBook( modalPhraseBook );

    // Le tempo des phrases est un REGLAGE du joueur : le banc d'essai le lit donc la ou il vit, au moment de jouer.
    modePreviewController.setPreferences( playerLevelStore );

    qmlRegisterSingletonInstance( QML_MODULE_NAME,
                                  QML_MODULE_MAJOR_VERSION,
                                  QML_MODULE_MINOR_VERSION,
                                  "ModeController",
                                  &modePreviewController );

    // L'ecran du cercle des quintes : une page de REFERENCE, qui ne joue rien. Elle n'a donc meme pas besoin du
    // lecteur de notes - seulement du domaine, qui sait tout ce qu'elle affiche.
    musichien::ui::KeyCircleController keyCircleController;

    qmlRegisterSingletonInstance( QML_MODULE_NAME,
                                  QML_MODULE_MAJOR_VERSION,
                                  QML_MODULE_MINOR_VERSION,
                                  "KeyCircleController",
                                  &keyCircleController );

    // The exercise screen has its own view model. It receives the SAME port, and neither controller
    // knows the other exists: the bench and the loop are two independent uses of the same domain.
    //
    // The vibration is injected as a function rather than called from the view model, and the hint book
    // is handed over to be owned: one keeps Android out of the interface, the other keeps a reference to
    // somebody else's object out of it.
    // Les anecdotes servent a deux endroits : l'accueil (une par ouverture) et le rappel quotidien (le livre entier,
    // pour qu'une anecdote DIFFERENTE puisse tomber chaque jour). Le livre est donc charge ici, passe au controleur
    // par copie, et garde pour le rappel.
    musichien::domain::AnecdoteBook anecdoteBook = loadAnecdoteBook();

    // Le journal des questions conclues : c'est la FONDATION des statistiques, et il est cree AVANT
    // le controleur qui va l'utiliser - un pointeur vers un objet deja detruit ne se voit pas tout de suite, et se voit
    // tres mal.
    //
    // Le dossier vient de Qt, qui sait ou une application a le droit d'ecrire sur chaque plateforme. Il peut ne pas
    // exister au premier lancement : QDir le cree, et si cela echoue le journal se taira sans que rien d'autre ne
    // s'arrete.
    const QString applicationDataDirectory = QStandardPaths::writableLocation( QStandardPaths::AppDataLocation );

    QDir{}.mkpath( applicationDataDirectory );

    // Le journal ECRIT, donc il n'est pas const : c'est un objet a part entiere, pas une constante de configuration.
    musichien::infrastructure::JsonLinesQuestionLog questionLog{
      applicationDataDirectory + QStringLiteral( "/questions.jsonl" ) };

    musichien::ui::ExerciseSessionController exerciseController{ notePlayer,
                                                                 {},
                                                                 loadHintBook(),
                                                                 anecdoteBook,
                                                                 musichien::infrastructure::vibrateForMistake,
                                                                 &playerLevelStore };

    // Une question conclue est ecrite ici, une fois pour toutes les genres de question.
    exerciseController.setQuestionLog( &questionLog );

    // Et le MEME livre de phrases que le banc d'essai, pour la meme raison : c'est lui qui fait entendre un mode en
    // MELODIE quand la question demande de le nommer. Le jeu et le banc d'essai sonnent donc pareil - ce qui a demande de
    // sortir le calcul du controleeur pour le mettre dans le domaine.
    exerciseController.setPhraseBook( modalPhraseBook );

    // Les explications de la page Accordeur, lues dans leur fichier de contenu comme les indices et les anecdotes.
    exerciseController.setTunerGuide( loadTunerGuide() );

    // La page de statistiques : elle LIT le meme journal, et ne l'ecrit jamais. Un seul journal, une seule verite - deux
    // objets qui ecriraient le meme fichier finiraient par se contredire.
    musichien::ui::StatisticsController statisticsController{ questionLog };

    qmlRegisterSingletonInstance( QML_MODULE_NAME,
                                  QML_MODULE_MAJOR_VERSION,
                                  QML_MODULE_MINOR_VERSION,
                                  "StatisticsController",
                                  &statisticsController );

    // Une remise a zero efface le journal : la page de statistiques doit alors oublier ce qu'elle avait calcule, sinon
    // elle continuerait d'afficher l'histoire que le joueur vient d'effacer.
    QObject::connect( &exerciseController,
                      &musichien::ui::ExerciseSessionController::statisticsChanged,
                      &statisticsController,
                      &musichien::ui::StatisticsController::refresh );

    // Le micro. Le view model ne connait que le port PitchDetector : la vraie implementation (QAudioSource + YIN)
    // est construite ICI, dans la couche de câblage, et livrée par la factory à chaque changement de périphérique.
    // C'est ce qui permet au réglage de lister et de choisir le micro sans que le view model voie Qt Multimedia.
    const QList<QAudioDevice> inputDevices = QMediaDevices::audioInputs();

    QStringList inputDeviceNames;

    for( const QAudioDevice & device : inputDevices )
    {
        inputDeviceNames << device.description();
    }

    musichien::ui::MicrophoneController microphoneController{
      inputDeviceNames,
      [inputDevices]( int p_deviceIndex ) -> std::unique_ptr<musichien::domain::PitchDetector> {
          if( p_deviceIndex < 0 || p_deviceIndex >= inputDevices.size() )
          {
              return {};
          }

          return std::make_unique<musichien::infrastructure::QAudioPitchDetector>( inputDevices.at( p_deviceIndex ) );
      },
      &playerLevelStore,
      &notePlayer };

    qmlRegisterSingletonInstance( QML_MODULE_NAME,
                                  QML_MODULE_MAJOR_VERSION,
                                  QML_MODULE_MINOR_VERSION,
                                  "MicrophoneController",
                                  &microphoneController );

    // La session peut poser des questions CHANTEES : elle a besoin du micro pour les juger.
    exerciseController.setMicrophoneController( &microphoneController );

    // Ce que le joueur VEUT entendre. Le filtrage se fait ICI, dans la couche de cablage, ce qui evite a l'adaptateur
    // audio de connaitre les preferences - et il se refait a chaque changement, donc decocher le saxo s'entend des la
    // question suivante.
    //
    // IL N'ENVOIE PLUS UNE LISTE D'INSTRUMENTS, mais un drapeau par timbre. C'est le fond du bug que Roger a entendu -
    // « il ne joue pas forcement l'instrument en face » : en compactant la liste, chaque case decochee faisait GLISSER
    // les rangs, et l'index d'un instrument cessait de designer le meme son. Le filtre porte desormais sur le TIRAGE,
    // et les instruments gardent leur place - y compris ceux qu'on vient de decocher, qu'il faut encore pouvoir ecouter.
    const auto playWantedInstruments = [&exerciseController, &notePlayer]() {
        notePlayer.useEnabledInstruments( exerciseController.enabledInstruments() );
    };

    QObject::connect( &exerciseController,
                      &musichien::ui::ExerciseSessionController::instrumentsChanged,
                      playWantedInstruments );

    playWantedInstruments();

    // Le réglage descend jusqu'à la couche audio, et il descend à CHAQUE changement : basculer le tempérament ou le
    // diapason s'entend à la note suivante, pas au prochain lancement. La TONIQUE n'en fait pas partie - la couche
    // audio connaît toujours la sienne (la première note qu'elle joue) ; seul l'accordeur a besoin d'une tonique à lui.
    const auto applyTuning = [&playerLevelStore, &notePlayer, &intervalController, &modePreviewController]() {
        const musichien::domain::TuningContext tuning{ playerLevelStore.storedTemperament(),
                                                       playerLevelStore.storedReferencePitch() };

        notePlayer.setTuning( tuning );

        intervalController.setTuning( tuning );
        modePreviewController.setTuning( tuning );
    };

    QObject::connect( &exerciseController,
                      &musichien::ui::ExerciseSessionController::temperamentChanged,
                      applyTuning );

    QObject::connect( &exerciseController,
                      &musichien::ui::ExerciseSessionController::referencePitchChanged,
                      applyTuning );

    applyTuning();

    // Le rappel quotidien. Le port cache la plateforme : sur le bureau, rien ne se planifie ; sur Android, de
    // VRAIES notifications sont posees.
    //
    // QUATRE par jour : trois anecdotes - le matin, le midi, le soir - et le rappel d'entrainement a l'heure choisie.
    // Le contenu du rappel est le livre entier, pour qu'une anecdote DIFFERENTE
    // puisse tomber chaque jour ; celui des trois autres est tire ici, a chaque lancement, ce qui les fait changer d'une
    // session a l'autre sans qu'aucune alarme n'ait besoin de reveiller l'application.
#ifdef Q_OS_ANDROID
    musichien::infrastructure::AndroidNotificationScheduler notificationScheduler;
#else
    musichien::infrastructure::NullNotificationScheduler notificationScheduler;
#endif

    // Le moteur qui tire les anecdotes des notifications. Il vit le temps de l'application : deux lancements successifs
    // n'ont aucune raison de raconter la meme chose.
    std::mt19937 notificationRandomEngine{ std::random_device{}() };

    const auto applyNotifications = [&exerciseController, &notificationScheduler, &anecdoteBook, &notificationRandomEngine]() {
        if( !exerciseController.dailyReminderEnabled() )
        {
            notificationScheduler.cancelNotifications();

            return;
        }

        std::vector<musichien::infrastructure::NotificationScheduler::DailyNotification> notifications;

        // Les trois anecdotes : leur moment vient du DOMAINE, leur texte du livre. Si le livre est vide, il n'y a rien a
        // raconter - et une notification vide serait pire que pas de notification du tout.
        for( const musichien::domain::ReminderMoment & moment : musichien::domain::ANECDOTE_REMINDER_MOMENTS )
        {
            const std::string text = randomAnecdoteText( anecdoteBook, notificationRandomEngine );

            if( text.empty() )
            {
                continue;
            }

            notifications.push_back( { moment.hour, moment.minute, text } );
        }

        // Et le rappel d'entrainement, a l'heure que le joueur a choisie : il porte une anecdote lui aussi, parce que
        // c'est ce qui donne envie d'ouvrir l'application. Un rappel qui dit « viens t'entrainer » se fait ignorer ;
        // une chose drole a lire, non.
        notifications.push_back( { exerciseController.reminderHour(),
                                   exerciseController.reminderMinute(),
                                   randomAnecdoteText( anecdoteBook, notificationRandomEngine ) } );

        notificationScheduler.scheduleDailyNotifications( notifications );
    };

    QObject::connect( &exerciseController,
                      &musichien::ui::ExerciseSessionController::dailyReminderChanged,
                      applyNotifications );

    QObject::connect( &exerciseController,
                      &musichien::ui::ExerciseSessionController::testReminderRequested,
                      [&notificationScheduler, &anecdoteBook, &notificationRandomEngine]() {
                          notificationScheduler.showReminderNow( randomAnecdoteText( anecdoteBook, notificationRandomEngine ) );
                      } );

    // LA DEMANDE D'AUTORISATION, branchee comme le reste : le controleur dit qu'il faut demander, et c'est ici qu'on
    // sait a qui. Sans cette ligne, les notifications etaient declarees actives, les alarmes se declenchaient, et rien
    // n'apparaissait jamais - l'application avait simplement oublie de demander la permission.
    QObject::connect( &exerciseController,
                      &musichien::ui::ExerciseSessionController::notificationPermissionRequested,
                      [&notificationScheduler]() { notificationScheduler.requestNotificationPermission(); } );

    applyNotifications();

    // Bonjour. Un arpège montant de do, sol, do : une quinte et une octave, aucune tierce, donc rien
    // à comprendre - seulement quelque chose qui monte et qui flotte. Au piano, et très discret : c'est
    // la moitié du reproche qui était juste.
    notePlayer.playGreeting();

    qmlRegisterSingletonInstance( QML_MODULE_NAME,
                                  QML_MODULE_MAJOR_VERSION,
                                  QML_MODULE_MINOR_VERSION,
                                  "ExerciseController",
                                  &exerciseController );

    qmlRegisterSingletonInstance( QML_MODULE_NAME,
                                  QML_MODULE_MAJOR_VERSION,
                                  QML_MODULE_MINOR_VERSION,
                                  "RhythmController",
                                  &rhythmController );

    QQmlApplicationEngine qmlEngine;

    // The QML is embedded into the executable through resources.qrc, so the path is identical on the
    // desktop and on the device, and nothing has to be copied next to the binary.
    qmlEngine.load( QUrl{ QStringLiteral( "qrc:/qml/Main.qml" ) } );

    if( qmlEngine.rootObjects().isEmpty() )
    {
        std::cerr << "Musichien: the QML scene could not be loaded\n";
        return -1;
    }

#ifdef Q_OS_ANDROID
    // La barre systeme. Depuis Android 15, le theme ne decide plus de sa couleur ni de celle de ses icones : il faut
    // le redire a la fenetre, une fois qu'elle existe. Voir infrastructure/android/AndroidSystemBars.h.
    musichien::infrastructure::applyAndroidNightSystemBars();
#endif

    const int exitCode = QGuiApplication::exec();

    // Whatever happens, the audio device is released before leaving. Leaving an output stream open on
    // a phone is a battery drain, and a bug.
    notePlayer.stopAll();

    return exitCode;
}
