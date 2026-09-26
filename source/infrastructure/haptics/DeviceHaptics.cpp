#include "infrastructure/haptics/DeviceHaptics.h"

#ifdef Q_OS_ANDROID
#    include <QCoreApplication>
#    include <QJniObject>
#endif

namespace musichien::infrastructure
{

void vibrateForMistake()
{
#ifdef Q_OS_ANDROID

    // How long the buzz lasts. Short on purpose: the vibration is a punctuation mark that doubles the
    // message of the shake, not a punishment of its own.
    constexpr jlong VIBRATION_DURATION_MILLISECONDS = 40;

    // VibrationEffect.DEFAULT_AMPLITUDE, which is -1: "use the intensity the system considers normal
    // for a notification". Imposing one would override the setting of the player.
    constexpr jint DEFAULT_AMPLITUDE = -1;

    const QJniObject context = QNativeInterface::QAndroidApplication::context();

    if( !context.isValid() )
    {
        return;
    }

    // "vibrator" is the name of the system service; the Context is what owns it.
    const QJniObject vibrator = context.callObjectMethod(
      "getSystemService",
      "(Ljava/lang/String;)Ljava/lang/Object;",
      QJniObject::fromString( QStringLiteral( "vibrator" ) ).object<jstring>() );

    if( !vibrator.isValid() )
    {
        return;
    }

    // VibrationEffect exists since API 26, and the project requires API 28: the older vibrate(long)
    // overload is deprecated and, since API 26, silently ignored unless an amplitude is given.
    const QJniObject effect = QJniObject::callStaticObjectMethod(
      "android/os/VibrationEffect",
      "createOneShot",
      "(JI)Landroid/os/VibrationEffect;",
      VIBRATION_DURATION_MILLISECONDS,
      DEFAULT_AMPLITUDE );

    if( !effect.isValid() )
    {
        return;
    }

    vibrator.callMethod<void>( "vibrate", "(Landroid/os/VibrationEffect;)V", effect.object() );

#endif

    // Everywhere else, nothing happens, and that is the correct behaviour rather than a degradation: a
    // machine without a motor has nothing to say about a mistake.
}

}    // namespace musichien::infrastructure
