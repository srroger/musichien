#pragma once

// =====================================================================================================================
// Musichien - QuestionLog
//
// OU va une question conclue. Un port, exactement comme NotePlayer et PlayerPreferences : le domaine et le controleeur
// disent « enregistre ceci » sans savoir si la ligne part dans un fichier, dans une base ou dans une variable.
//
// ---------------------------------------------------------------------------------------------------------------------
// Pourquoi un JOURNAL, et pas une base de donnees (decision du 2026-09-28)
//
// La question se posait serieusement, et voici la reponse retenue :
//
//   1. une ligne par question se RELIT, se sauvegarde et se repare - une base corrompue ne se repare pas a la main ;
//   2. le fichier est en AJOUT SEUL : un plantage en pleine partie ne coute que la derniere ligne, jamais tout ;
//   3. aucune dependance a embarquer, aucune migration de schema a ecrire quand un champ s'ajoute : une ligne ancienne
//      se lit avec la valeur par defaut du champ manquant ;
//   4. le volume est ridicule : dix questions par session, trois sessions par jour, dix ans = cent mille lignes, une
//      quinzaine de megaoctets. Une base ne gagne rien sur un million de lignes, et ce jeu n'y arrivera pas ;
//   5. la charte y gagne : un fichier lisible par son proprietaire, dans son propre dossier, sans serveur ;
//   6. et si une base devenait necessaire un jour, le journal serait la SOURCE de son import. C'est le bon sens de la
//      fleche : un journal produit une base, jamais l'inverse.
// =====================================================================================================================

#include "domain/exercise/QuestionRecord.h"

#include <chrono>
#include <cstddef>
#include <vector>

namespace musichien::domain
{

class QuestionLog
{
public:
    QuestionLog() = default;

    QuestionLog( const QuestionLog & ) = delete;
    QuestionLog & operator=( const QuestionLog & ) = delete;
    QuestionLog( QuestionLog && ) = delete;
    QuestionLog & operator=( QuestionLog && ) = delete;

    virtual ~QuestionLog() = default;

    // Enregistre une question conclue.
    //
    // Ne rend rien et n'echoue pas du point de vue de l'appelant : un journal qui refuse de s'ecrire ne doit JAMAIS
    // interrompre une partie. L'infrastructure se plaint dans son coin, et le jeu continue - c'est une statistique,
    // pas une regle du jeu.
    virtual void append( const QuestionRecord & p_record ) = 0;

    // Les questions conclues depuis p_since, de la plus ancienne a la plus recente.
    [[nodiscard]] virtual std::vector<QuestionRecord> since( std::chrono::system_clock::time_point p_since ) const = 0;

    // Efface TOUT l'historique.
    //
    // C'est la seule operation destructrice du journal, et elle n'existe que parce que le joueur a le droit de repartir
    // de zero : « Remise a zero » remet le score a zero, et un score sans ses statistiques serait un demi-mensonge. Le
    // fichier n'est pas garde de cote : une remise a zero qui laisserait l'ancien journal quelque part ne serait pas une
    // remise a zero.
    virtual void clear() = 0;
};

// Un journal en memoire : les tests, et une application qui n'a nulle part ou ecrire.
class QuestionLogFake final : public QuestionLog
{
public:
    void append( const QuestionRecord & p_record ) override { m_records.push_back( p_record ); }

    [[nodiscard]] std::vector<QuestionRecord> since( std::chrono::system_clock::time_point p_since ) const override
    {
        std::vector<QuestionRecord> recent;

        for( const QuestionRecord & record : m_records )
        {
            if( record.askedAt >= p_since )
            {
                recent.push_back( record );
            }
        }

        return recent;
    }

    [[nodiscard]] const std::vector<QuestionRecord> & records() const noexcept { return m_records; }

    void clear() override { m_records.clear(); }

    [[nodiscard]] std::size_t size() const noexcept { return m_records.size(); }

private:
    std::vector<QuestionRecord> m_records;
};

}    // namespace musichien::domain
