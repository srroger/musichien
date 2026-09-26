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
