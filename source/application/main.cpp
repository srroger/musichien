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
#include "infrastructure/haptics/DeviceHaptics.h"
#ifdef Q_OS_ANDROID
#    include "infrastructure/android/AndroidSystemBars.h"
#    include "infrastructure/notifications/AndroidNotificationScheduler.h"
#else
#    include "infrastructure/notifications/NullNotificationScheduler.h"
#endif
#include "infrastructure/preferences/QSettingsPlayerPreferences.h"
#include "musichienBuildId.h"
#include "ui/ExerciseSessionController.h"
#include "ui/IntervalPlaybackController.h"
#include "ui/MicrophoneController.h"

#include <QAudioDevice>
#include <QFile>
#include <QGuiApplication>
#include <QMediaDevices>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QUrl>
#include <QtQml>

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

// Builds the reminder's content pool: every anecdote, one per line, so that the Android receiver can draw a
// DIFFERENT one on each daily firing without the application running. An empty book falls back to the plain nudge.
[[nodiscard]] std::string reminderContentFor( const musichien::domain::AnecdoteBook & p_anecdotes )
{
    std::string content;

    for( const std::string & text : p_anecdotes.texts() )
    {
        if( !content.empty() )
        {
            content += '\n';
        }

        content += text;
    }

    if( content.empty() )
    {
        return "Une oreille, une minute : l'intervalle du jour t'attend.";
    }

    return content;
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
constexpr std::array<const char *, 3> INSTRUMENT_NAMES{ "piano", "guitare", "saxo" };

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

    // What the application remembers about its player. A small settings file, on the device: the package
    // cannot reach the network, so nothing about him ever leaves the phone.
    musichien::infrastructure::QSettingsPlayerPreferences playerLevelStore;

    // The sampled instruments, embedded in the resources. From here on they are the sound of the EXERCISES; the
    // synthesiser keeps the mistake cue, which must not be beautiful, and stays the fallback if a sample is
    // missing.
    std::vector<musichien::domain::SampledInstrument> instruments;

    for( const char * instrumentName : INSTRUMENT_NAMES )
    {
        musichien::domain::SampledInstrument instrument = loadInstrument( QString::fromLatin1( instrumentName ) );

        if( !instrument.isEmpty() )
        {
            instruments.push_back( std::move( instrument ) );
        }
    }

    std::cerr << "Musichien: " << instruments.size() << " sampled instrument(s)\n";

    notePlayer.useInstruments( instruments, {} );

    // Opening the output now, rather than at the first note, means a machine without a sound card is
    // reported at start up instead of silently refusing to play in the middle of an exercise.
    notePlayer.prepareAudioOutput();

    std::cerr << "Musichien: audio output is " << notePlayer.audioOutputDescription() << "\n";

    // The view model only receives the PORT, never the adapter: it could be handed the fake player of
    // the unit tests without a single line of it changing.
    musichien::ui::IntervalPlaybackController intervalController{ notePlayer };

    qmlRegisterSingletonInstance( QML_MODULE_NAME,
                                  QML_MODULE_MAJOR_VERSION,
                                  QML_MODULE_MINOR_VERSION,
                                  "IntervalController",
                                  &intervalController );

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

    musichien::ui::ExerciseSessionController exerciseController{ notePlayer,
                                                                 {},
                                                                 loadHintBook(),
                                                                 anecdoteBook,
                                                                 musichien::infrastructure::vibrateForMistake,
                                                                 &playerLevelStore };

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

    // What the player WANTS to hear. The filtering happens HERE, in the wiring layer, which is what keeps the
    // audio adapter from having to know anything about preferences - and it happens again on every change, so
    // that unticking the saxophone is heard on the very next question rather than at the next launch.
    const auto playWantedInstruments = [&exerciseController, &notePlayer, &instruments]() {
        const std::vector<bool> enabled = exerciseController.enabledInstruments();

        std::vector<musichien::domain::SampledInstrument> wantedInstruments;

        for( std::size_t index = 0; index < instruments.size(); ++index )
        {
            if( ( index >= enabled.size() ) || enabled.at( index ) )
            {
                wantedInstruments.push_back( instruments.at( index ) );
            }
        }

        // The waveforms are the instruments that are NOT samples: the flags after the sampled ones ask the adapter
        // to render a pure spectrum (sine, sawtooth, square) instead of a recording.
        std::vector<musichien::domain::Waveform> wantedWaveforms;

        for( std::size_t waveformIndex = 0; waveformIndex < musichien::domain::WAVEFORM_INSTRUMENTS.size(); ++waveformIndex )
        {
            const std::size_t flagIndex = instruments.size() + waveformIndex;

            if( ( flagIndex >= enabled.size() ) || enabled.at( flagIndex ) )
            {
                wantedWaveforms.push_back( musichien::domain::WAVEFORM_INSTRUMENTS.at( waveformIndex ) );
            }
        }

        notePlayer.useInstruments( std::move( wantedInstruments ), std::move( wantedWaveforms ) );
    };

    QObject::connect( &exerciseController,
                      &musichien::ui::ExerciseSessionController::instrumentsChanged,
                      playWantedInstruments );

    playWantedInstruments();

    // Le réglage descend jusqu'à la couche audio, et il descend à CHAQUE changement : basculer le tempérament ou le
    // diapason s'entend à la note suivante, pas au prochain lancement. La TONIQUE n'en fait pas partie - la couche
    // audio connaît toujours la sienne (la première note qu'elle joue) ; seul l'accordeur a besoin d'une tonique à lui.
    const auto applyTuning = [&playerLevelStore, &notePlayer]() {
        notePlayer.setTuning( { playerLevelStore.storedTemperament(), playerLevelStore.storedReferencePitch() } );
    };

    QObject::connect( &exerciseController,
                      &musichien::ui::ExerciseSessionController::temperamentChanged,
                      applyTuning );

    QObject::connect( &exerciseController,
                      &musichien::ui::ExerciseSessionController::referencePitchChanged,
                      applyTuning );

    applyTuning();

    // Le rappel quotidien. Le port cache la plateforme : sur le bureau, rien ne se planifie ; sur Android, une
    // vraie notification sera posee. Ce que l'application sait, c'est qu'une case a ete cochee, et elle demande au
    // port de s'en occuper.
#ifdef Q_OS_ANDROID
    musichien::infrastructure::AndroidNotificationScheduler notificationScheduler;
#else
    musichien::infrastructure::NullNotificationScheduler notificationScheduler;
#endif

    const auto applyReminder = [&exerciseController, &notificationScheduler, &anecdoteBook]() {
        if( exerciseController.dailyReminderEnabled() )
        {
            notificationScheduler.scheduleDailyReminder( exerciseController.reminderHour(),
                                                         exerciseController.reminderMinute(),
                                                         reminderContentFor( anecdoteBook ) );
        }
        else
        {
            notificationScheduler.cancelReminder();
        }
    };

    QObject::connect( &exerciseController,
                      &musichien::ui::ExerciseSessionController::dailyReminderChanged,
                      applyReminder );

    QObject::connect( &exerciseController,
                      &musichien::ui::ExerciseSessionController::testReminderRequested,
                      [&notificationScheduler, &anecdoteBook]() {
                          notificationScheduler.showReminderNow( reminderContentFor( anecdoteBook ) );
                      } );

    applyReminder();

    // Bonjour. Un arpège montant de do, sol, do : une quinte et une octave, aucune tierce, donc rien
    // à comprendre - seulement quelque chose qui monte et qui flotte. Au piano, et très discret : c'est
    // la moitié du reproche qui était juste.
    notePlayer.playGreeting();

    qmlRegisterSingletonInstance( QML_MODULE_NAME,
                                  QML_MODULE_MAJOR_VERSION,
                                  QML_MODULE_MINOR_VERSION,
                                  "ExerciseController",
                                  &exerciseController );

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
