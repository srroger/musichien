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
    audio/AudioMixer.h
    audio/AudioMixer.cpp
    audio/QAudioNotePlayer.h
    audio/QAudioNotePlayer.cpp
    audio/NullPitchDetector.h
    audio/QAudioPitchDetector.h
    audio/QAudioPitchDetector.cpp
    content/JsonHintBook.h
    content/JsonHintBook.cpp
    content/JsonAnecdoteBook.h
    content/JsonAnecdoteBook.cpp
    # Le glossaire : un mot par entree, une ligne par definition. Meme contrat que les autres lecteurs de contenu.
    content/JsonGlossary.h
    content/JsonGlossary.cpp
    content/JsonTunerGuide.h
    content/JsonTunerGuide.cpp

    # Le livre des phrases modales : le meme contrat que les indices - du TEXTE en entree, un livre du domaine en sortie.
    content/JsonPhraseBook.h
    content/JsonPhraseBook.cpp

    # LES COURS DE L'ECOLE DES CHIOTS. Meme contrat, autre format : du MARKDOWN, ou le contenu porte des
    # cartes (ce que le jeu joue, ce qu'on va ecouter ailleurs, ce qu'on essaie). Voir le contrat, note 29.
    content/MarkdownCourse.h
    content/MarkdownCourse.cpp
    haptics/DeviceHaptics.h
    haptics/DeviceHaptics.cpp
    notifications/NotificationScheduler.h
    notifications/NullNotificationScheduler.h
    preferences/QSettingsPlayerPreferences.h
    preferences/QSettingsPlayerPreferences.cpp

    # Le journal des questions conclues : une ligne JSON par question, en ajout seul (fondation des statistiques)
    statistics/JsonLinesQuestionLog.h
    statistics/JsonLinesQuestionLog.cpp
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
    target_sources(musichien_infrastructure PRIVATE android/AndroidSystemBars.cpp)
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
        audio/AudioMixer_test.cpp
        content/JsonHintBook_test.cpp
        content/JsonTunerGuide_test.cpp
        content/JsonPhraseBook_test.cpp

        # Le contrat des cours, verifie comme les autres : une faute dans un fichier ecrit a la main
        # coute une carte, jamais l'application.
        content/MarkdownCourse_test.cpp
        statistics/JsonLinesQuestionLog_test.cpp
    )

endif()
