# =====================================================================================================================
# Module: ui
#
# Owns everything that is displayed: the QML files, and later the view models exposed to them.
#
# HARD RULE: no rule of the game is implemented here. The QML displays and delegates; the domain
# decides. See docs/CODE_CONVENTIONS.md, section "QML".
# =====================================================================================================================

# The QML files are embedded into the executable through the Qt resource system. Consequence: the
# loading path is strictly identical on the desktop and on the Android device, and nothing has to be
# copied next to the binary.
set(MUSICHIEN_QML_RESOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc PARENT_SCOPE)

# Listed explicitly rather than globbed: a new QML file must be added here on purpose, which keeps
# the build honest and reproducible.
set(MUSICHIEN_QML_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/Main.qml
    PARENT_SCOPE
)

# The directory a QML IMPORT SCANNER must walk to discover the QML modules this application uses
# (QtQuick, QtQuick.Controls, QtQuick.Controls.Material, QtQuick.Layouts...).
#
# It is exported from HERE, and not guessed in the application module, because this module is what
# owns the QML files: if a QML file moves, only this line has to follow it.
#
# This value is only used by the Android build, where missing it costs a package that installs, starts
# and dies immediately. See source/application/CMakeLists.txt.
set(MUSICHIEN_QML_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)

# ---------------------------------------------------------------------------------------------------------------------
# The view models exposed to QML
#
# They are the ONLY bridge between the interface and the domain. The QML displays and calls them; the
# rules of the game stay in the domain, where they are testable without a graphical interface.
# ---------------------------------------------------------------------------------------------------------------------
musichienFunction_AddLibrary(ui
    IntervalPlaybackController.h
    IntervalPlaybackController.cpp
)

target_link_libraries(musichien_ui PUBLIC
    musichien_domain

    Qt6::Core
    Qt6::Qml
)

# ---------------------------------------------------------------------------------------------------------------------
# Unit tests
#
# The view model is tested with the fake note player of the domain, so these tests need neither a
# sound card nor a phone. They check the two things a view model can get wrong: what would be heard,
# and what would be displayed.
# ---------------------------------------------------------------------------------------------------------------------
if(MUSICHIEN_BUILD_TESTING)

    musichienFunction_AddTest(ui
        musichien_ui
        IntervalPlaybackController_test.cpp
    )

endif()

