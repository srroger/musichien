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

# Liste des QML en chemins absolus : les dependances reelles de la ressource, pour la cible de build declaree plus
# bas. Voir le long commentaire qui l'accompagne.
set(MUSICHIEN_QML_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/ExerciseScreen.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/Main.qml
)

# Listed explicitly rather than globbed: a new QML file must be added here on purpose, which keeps
# the build honest and reproducible.
set(MUSICHIEN_QML_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/ExerciseScreen.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/Main.qml
    PARENT_SCOPE
)

# ---------------------------------------------------------------------------------------------------------------------
# The QML files are DEPENDENCIES of the resource file, and this is not a refinement
#
# AUTORCC reads resources.qrc, sees a list of relative paths, and generates a C++ file that carries the CONTENT of
# each file - at that moment, and never again. Change a QML file and the generated C++ is not regenerated: the
# resource it holds is yesterday's screen. The build succeeds, the tests pass, the application starts, and the
# screen is the one from before the change.
#
# It happened. An afternoon went into looking for a QML bug in a QML file that was never loaded: the screen on test
# was three versions old, and nothing anywhere said so. Declaring the files as explicit dependencies of the
# resource is what makes "modified" mean "rebuilt".
#
# The list is repeated here as a plain variable because MUSICHIEN_QML_FILES is exported to the PARENT scope, which
# means it is EMPTY in this very file.
set(MUSICHIEN_QML_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/ExerciseScreen.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/Main.qml
)

set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc PROPERTY OBJECT_DEPENDS "${MUSICHIEN_QML_SOURCES}")

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
    ExerciseSessionController.h
    ExerciseSessionController.cpp
    IntervalDescription.h
    IntervalDescription.cpp
    IntervalPlaybackController.h
    IntervalPlaybackController.cpp
)

target_link_libraries(musichien_ui PUBLIC
    musichien_domain

    Qt6::Core
    Qt6::Qml
)

# ---------------------------------------------------------------------------------------------------------------------
# A changed QML file must rebuild the RESOURCE, and AUTORCC will not do it on its own
#
# AUTORCC reads resources.qrc, sees a list of relative paths, and generates a C++ file carrying the CONTENT of each
# one - at that moment, and never again. It regenerates that file when the .qrc changes, and it does NOT look at
# the files the .qrc cites.
#
# So: change a QML file, and nothing is regenerated. The build succeeds, the tests pass, the application starts,
# and the screen is yesterday's. That is exactly what happened, and an afternoon went into hunting a bug in a QML
# file that was never loaded - the screen under test was three versions old.
#
# The target below is what makes the dependency real: it touches the .qrc whenever a QML is newer, which is the
# only thing AUTORCC listens to. Touching changes the CONTENT of nothing, so the file stays clean in git.
add_custom_target(musichien-touch-qml-resources ALL
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc
    DEPENDS ${MUSICHIEN_QML_SOURCES}
    COMMENT "Musichien: QML resources up to date"
    VERBATIM
)

# ---------------------------------------------------------------------------------------------------------------------
# Unit tests
#
# The view model is tested with the fake note player of the domain, so these tests need neither a
# sound card nor a phone. They check the two things a view model can get wrong: what would be heard,
# and what would be displayed.
# ---------------------------------------------------------------------------------------------------------------------
if(MUSICHIEN_BUILD_TESTING)

    # L'ecran est charge pour de vrai dans ExerciseScreen_test.cpp : la ressource QML doit donc etre embarquee dans
    # le binaire de test, exactement comme l'application l'embarque, et Qt Quick doit etre lie pour qu'un composant
    # devienne un item. Ces deux lignes restent valables meme quand ce test est desactive : elles disent ce qu'il
    # faut pour charger un ecran QML depuis un binaire de test.
    musichienFunction_AddTest(ui
        musichien_ui
        ExerciseSessionController_test.cpp
        IntervalPlaybackController_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc
    )

    target_link_libraries(test_ui PRIVATE Qt6::Quick)

    # ExerciseScreen_test.cpp n'est PAS compile encore, et c'est une dette assumee plutot qu'un oubli.
    #
    # Le test est ecrit, il pose la bonne question - l'ecran s'instancie-t-il vraiment - et il a meme deja servi :
    # c'est lui qui a trouve une boucle de liaison qui cassait l'affichage des pastilles. Mais le binaire de test
    # ne recoit pas les ressources QML : elles sont compilees, puis jetees par l'editeur de liens, parce que rien
    # dans le test ne les reference. Le composant se charge alors d'un fichier qui n'existe pas.
    #
    # Le remettre demande de declarer les ressources pour la cible de test (qt_add_resources, avec un alias qui
    # garde "qrc:/qml/..."), ce qui touche la facon dont le projet embarque son interface. C'est un chantier a part,
    # et il vaut mieux le faire proprement que le bricoler au milieu d'une livraison.
    #
    # ExerciseScreen_test.cpp

endif()

