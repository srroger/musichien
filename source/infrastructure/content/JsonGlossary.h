#pragma once

#include "domain/lesson/Glossary.h"

#include <string_view>
#include <vector>

namespace musichien::infrastructure
{

// Lit le glossaire depuis le TEXTE d'un fichier de contenu, et non depuis un chemin.
//
// Meme contrat que les autres lecteurs du projet : c'est l'appelant qui ouvre la ressource, parce que lire une ressource
// est une affaire Qt et que ce fichier-la ne connait que du JSON. Un appelant qui ouvrirait un chemin lui-meme marcherait
// sur la machine de developpement et ne trouverait rien sur le telephone.
//
// Les mots sont rendus DANS L'ORDRE DU FICHIER : le tri en ordre de dictionnaire appartient a l'affichage, et il est fait
// plus haut. Un fichier mal ecrit - un mot sans definition, une entree qui n'est pas un objet - coute CES MOTS-LA, et
// jamais le glossaire entier.
[[nodiscard]] std::vector<domain::GlossaryEntry> readGlossary( std::string_view p_json );

}    // namespace musichien::infrastructure