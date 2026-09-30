#pragma once

// =====================================================================================================================
// Musichien - readPhraseBook
//
// Remplit un livre de phrases a partir du TEXTE JSON d'un fichier de contenu.
//
// ---------------------------------------------------------------------------------------------------------------------
// Pourquoi le TEXTE, et non un nom de fichier
//
// Meme raison que pour les indices : le contenu est embarque dans les ressources Qt, pour que le telephone et le bureau
// lisent les MEMES octets par le meme chemin. Un lecteur qui ouvrirait un chemin lui-meme marcherait sur la machine de
// developpement et ne trouverait rien sur l'appareil - une classe de bug que ce projet a deja payee. L'appelant lit le
// texte ; cette fonction ne fait que le comprendre.
//
// ---------------------------------------------------------------------------------------------------------------------
// Le format, qui est un contrat avec un fichier qu'un humain edite
//
//     {
//       "phrases": [
//         { "mode": "dorian", "tonique_midi": 50, "bpm": 72,
//           "degres": [ { "degre": 1, "duree": 2 }, { "degre": 3, "duree": 1 } ] }
//       ]
//     }
//
// 'tonique_midi' est un NUMERO MIDI, jamais un nom de note, et c'est la meme regle que pour les indices : le domaine
// cherche par une valeur qu'il connait. Un « D3 » demanderait au lecteur de savoir nommer une note, ce qui est une
// affaire d'affichage - et le fichier de l'atelier porte d'ailleurs les deux, l'un pour l'oreille, l'autre pour le code.
//
// Une phrase ILLISIBLE est ignoree et signalee, jamais fatale : une faute de frappe doit couter une phrase, pas
// l'application. La phrase entiere est refusee des qu'un de ses pas est invalide, et non le pas fautif : une phrase a
// qui l'on retire un pas n'est plus la phrase que l'oreille avait gardee.
// =====================================================================================================================

#include "domain/music/PhraseBook.h"

#include <string_view>

namespace musichien::infrastructure
{

[[nodiscard]] domain::PhraseBook readPhraseBook( std::string_view p_jsonText );

}    // namespace musichien::infrastructure
