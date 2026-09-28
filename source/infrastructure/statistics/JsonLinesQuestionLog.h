#pragma once

// =====================================================================================================================
// Musichien - JsonLinesQuestionLog
//
// Le journal, dans un fichier : une ligne JSON par question conclue. C'est l'implementation du port pour l'application,
// et elle tient en deux gestes - ouvrir en AJOUT, ecrire une ligne.
//
// ---------------------------------------------------------------------------------------------------------------------
// Pourquoi une ligne JSON, et pas un tableau JSON
//
// Un tableau oblige a relire tout le fichier, le modifier et le reecrire entierement : une coupure au mauvais moment,
// et c'est tout l'historique qui part. Une ligne par question s'ajoute sans jamais toucher a ce qui precede, et un
// fichier dont la derniere ligne est tronquee se lit quand meme - la ligne abimee est celle qu'on vient d'ecrire.
//
// Le format reste du JSON, donc lisible par n'importe quel outil, et il vieillit bien : un champ ajoute demain se lit
// sur les lignes anciennes avec sa valeur par defaut.
//
// Les enums sont ecrites en NOMBRES, volontairement : quelqu'un renommera une enumeration un jour, et un fichier ecrit
// il y a six mois doit rester lisible. C'est la meme raison qui fait qu'un identifiant stable ne s'ecrit jamais dans la
// langue de l'ecran.
//
// ---------------------------------------------------------------------------------------------------------------------
// Un journal qui echoue ne casse rien
//
// Aucune de ces fonctions ne rend d'erreur, et aucune ne lance : un disque plein, un dossier absent ou une ligne
// illisible coutent des statistiques, jamais une partie. L'application appelle, et continue.
// =====================================================================================================================

#include "domain/exercise/QuestionLog.h"

#include <QString>

#include <chrono>
#include <vector>

namespace musichien::infrastructure
{

class JsonLinesQuestionLog final : public domain::QuestionLog
{
public:
    explicit JsonLinesQuestionLog( QString p_filePath );

    void append( const domain::QuestionRecord & p_record ) override;

    [[nodiscard]] std::vector<domain::QuestionRecord> since(
      std::chrono::system_clock::time_point p_since ) const override;

    // Le chemin du fichier : l'application peut le montrer a un joueur curieux, et un test s'en sert pour ecrire dans
    // un dossier temporaire.
    [[nodiscard]] const QString & filePath() const noexcept { return m_filePath; }

private:
    QString m_filePath;
};

}    // namespace musichien::infrastructure
