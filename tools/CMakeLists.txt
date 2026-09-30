# =====================================================================================================================
# Musichien - les outils de developpement
#
# Ce dossier n'appartient a AUCUNE des quatre couches : ce n'est ni un module, ni l'application, ni un test. Ce sont
# des executables en ligne de commande, qui depend du DOMAINE SEUL - donc sans Qt, sans carte son, sans telephone.
#
# Leur raison d'etre est l'OREILLE : il y a des choses qu'aucun test ne juge (une couleur, un timbre, une balance), et
# un WAV ecoute une fois par un humain vaut mieux qu'une capture d'ecran interpretee.
# =====================================================================================================================

add_executable(musichien_render_preview render_preview.cpp)

musichienFunction_ConfigureTarget(musichien_render_preview)

target_link_libraries(musichien_render_preview PRIVATE musichien_domain)

set_target_properties(musichien_render_preview PROPERTIES FOLDER "tools")
