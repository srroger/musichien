#include "infrastructure/android/AndroidSystemBars.h"

#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>

namespace musichien::infrastructure
{

void applyAndroidNightSystemBars()
{
    // The CONTEXT is the Activity here: Qt's own activity extends android.app.Activity, which is what the Java side
    // needs to reach the window.
    const QJniObject activity = QNativeInterface::QAndroidApplication::context();

    if( !activity.isValid() )
    {
        return;
    }

    QJniObject::callStaticMethod<void>( "io/github/srroger/musichien/SystemBars",
                                        "applyNightStyle",
                                        "(Landroid/app/Activity;)V",
                                        activity.object() );
}

}    // namespace musichien::infrastructure