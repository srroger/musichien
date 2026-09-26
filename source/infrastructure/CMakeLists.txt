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
)

target_link_libraries(musichien_infrastructure PUBLIC
    musichien_domain

    Qt6::Core
    Qt6::Multimedia
)

set_target_properties(musichien_infrastructure PROPERTIES FOLDER "infrastructure")
