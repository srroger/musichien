# =====================================================================================================================
# Module: infrastructure
#
# Everything that talks to the outside world: audio output, persistence, clock, notifications.
#
# This module IMPLEMENTS the ports declared by the domain. The dependency arrow therefore still points
# towards the domain: infrastructure knows the domain, and the domain has never heard of
# infrastructure.
#
# Consequence: replacing QAudioSink with Oboe on Android only touches this directory.
# =====================================================================================================================

musichienFunction_AddLibrary(infrastructure
    audio/QAudioNotePlayer.h
    audio/QAudioNotePlayer.cpp
    audio/PitchDetector.h
    audio/NullPitchDetector.h
    content/JsonHintBook.h
    content/JsonHintBook.cpp
    content/JsonAnecdoteBook.h
    content/JsonAnecdoteBook.cpp
    haptics/DeviceHaptics.h
    haptics/DeviceHaptics.cpp
    notifications/NotificationScheduler.h
    notifications/NullNotificationScheduler.h
    preferences/QSettingsPlayerPreferences.h
    preferences/QSettingsPlayerPreferences.cpp
)

# nlohmann/json is header only: linking it is only about the include path of the pinned version. It is
# what reads the content files, which live in assets/content and are embedded in the resources.
target_link_libraries(musichien_infrastructure PRIVATE nlohmann_json)

target_link_libraries(musichien_infrastructure PUBLIC
    musichien_domain

    Qt6::Core
    Qt6::Multimedia
)

set_target_properties(musichien_infrastructure PROPERTIES FOLDER "infrastructure")

# ---------------------------------------------------------------------------------------------------------------------
# La vraie planification des rappels, compilee seulement sur Android : elle parle a AlarmManager par JNI, et elle
# n'a rien a faire - ni a compiler - sur le bureau, ou le port "rien" est celui qui compte.
# ---------------------------------------------------------------------------------------------------------------------
if(ANDROID)
    target_sources(musichien_infrastructure PRIVATE notifications/AndroidNotificationScheduler.cpp)
endif()

# ---------------------------------------------------------------------------------------------------------------------
# Unit tests
#
# What is tested here is a CONTRACT with something outside the code: the shape of a content file a human
# edits. No Qt application object is needed, because reading JSON needs no running engine - only the
# pinned nlohmann/json, which comes with the module.
# ---------------------------------------------------------------------------------------------------------------------
if(MUSICHIEN_BUILD_TESTING)

    musichienFunction_AddTest(infrastructure
        musichien_infrastructure
        content/JsonHintBook_test.cpp
    )

endif()
