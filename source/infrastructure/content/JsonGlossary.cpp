#include "infrastructure/content/JsonGlossary.h"

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace musichien::infrastructure
{

std::vector<domain::GlossaryEntry> readGlossary( std::string_view p_json )
{
    std::vector<domain::GlossaryEntry> entries;

    // `false` : ne pas lever. Un fichier en cours d'ecriture rend un document « discarded », et l'application continue
    // sans glossaire. C'est le meme contrat que partout ailleurs - un contenu casse coute le contenu, jamais le jeu.
    const nlohmann::json document = nlohmann::json::parse( p_json, nullptr, false );

    if( document.is_discarded() || !document.contains( "termes" ) || !document.at( "termes" ).is_array() )
    {
        return entries;
    }

    for( const nlohmann::json & entry : document.at( "termes" ) )
    {
        // Une entree qui n'est pas un OBJET est ignoree sans bruit : le fichier peut contenir un commentaire mal
        // ecrit, une virgule de trop, une ligne vide - et rien de tout cela ne doit priver le joueur du reste.
        if( !entry.is_object() )
        {
            continue;
        }

        const auto word = entry.find( "mot" );
        const auto definition = entry.find( "definition" );

        if( ( word == entry.end() ) || !word->is_string() || ( definition == entry.end() ) || !definition->is_string() )
        {
            continue;
        }

        const std::string wordText = word->get<std::string>();
        const std::string definitionText = definition->get<std::string>();

        // UN MOT SANS DEFINITION N'EST PAS UN MOT DE GLOSSAIRE. Une ligne vide dans la liste vaut moins qu'une ligne en
        // moins : elle ferait croire a un bug d'affichage plutot'a un contenu inacheve.
        if( wordText.empty() || definitionText.empty() )
        {
            continue;
        }

        entries.push_back( domain::GlossaryEntry{ wordText, definitionText } );
    }

    return entries;
}

}    // namespace musichien::infrastructure