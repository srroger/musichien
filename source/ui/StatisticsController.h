#pragma once

// =====================================================================================================================
// Musichien - StatisticsController
//
// La page de statistiques, vue depuis QML : des grands chiffres, un histogramme, une repartition par genre, les points
// faibles, et le temps de jeu.
//
// ---------------------------------------------------------------------------------------------------------------------
// Ce qui se calcule ICI, et ce qui ne s'y calcule pas
//
// L'AGREGATION vient du domaine (QuestionStatistics) : il compte, il trie, et il ne sait rien d'autre. Le DECOUPAGE vient
// d'ici, parce que grouper des questions par JOUR demande un calendrier - donc un fuseau horaire - et que le domaine n'en
// connait aucun. C'est precisement ce qui le garde pur.
//
// Le controleur SAIT quelle heure il est ; le domaine sait compter. Chacun son metier.
//
// ---------------------------------------------------------------------------------------------------------------------
// Rien n'est calcule tant que personne ne regarde
//
// refresh() est appelee a l'OUVERTURE de la page. Une page de statistiques qui couterait au demarrage de l'application
// serait une page qu'on regrette, et Roger ouvre rarement son profil en lancant le jeu.
// =====================================================================================================================

#include "domain/exercise/QuestionLog.h"
#include "domain/exercise/QuestionStatistics.h"

#include <QObject>
#include <QString>
#include <QVariantList>

#include <chrono>
#include <cstdint>
#include <vector>

namespace musichien::ui
{

class StatisticsController final : public QObject
{
    Q_OBJECT

    // -----------------------------------------------------------------------------------------------------------------
    // Tout ce que l'ecran lit est declare ICI.
    //
    // Une methode C++ sans Q_PROPERTY n'existe PAS pour QML, et QML ne s'en plaint pas : il ecrit « undefined » et
    // continue. C'est exactement ce qui est arrive la premiere fois que cette page a ete vue sur un telephone - des
    // « undefined % » partout, et aucun histogramme - et c'est la raison pour laquelle ces lignes sont maintenant
    // ecrites AVANT les methodes qu'elles exposent.
    // -----------------------------------------------------------------------------------------------------------------
    Q_PROPERTY( bool hasHistory READ hasHistory NOTIFY statisticsChanged )
    Q_PROPERTY( int questionCount READ questionCount NOTIFY statisticsChanged )
    Q_PROPERTY( int successPercent READ successPercent NOTIFY statisticsChanged )
    Q_PROPERTY( int firstTryPercent READ firstTryPercent NOTIFY statisticsChanged )
    Q_PROPERTY( int playingDayStreak READ playingDayStreak NOTIFY statisticsChanged )
    Q_PROPERTY( QString playTimeText READ playTimeText NOTIFY statisticsChanged )
    Q_PROPERTY( QString recentPlayTimeText READ recentPlayTimeText NOTIFY statisticsChanged )
    Q_PROPERTY( QVariantList lastDays READ lastDays NOTIFY statisticsChanged )
    Q_PROPERTY( QVariantList kinds READ kinds NOTIFY statisticsChanged )
    Q_PROPERTY( QVariantList weakestTargets READ weakestTargets NOTIFY statisticsChanged )

public:
    // Le journal est lu, jamais ecrit : cette page ne peut pas abimer ce qu'elle raconte.
    explicit StatisticsController( const domain::QuestionLog & p_log, QObject * p_parent = nullptr );

    // Relit le journal et refait tous les calculs.
    Q_INVOKABLE void refresh();

    // Y a-t-il quelque chose a raconter ? Sans journal, la page le dit plutot que d'afficher des zeros - un ecran rempli
    // de « 0 % » n'apprend rien et decourage.
    [[nodiscard]] bool hasHistory() const noexcept { return !m_records.empty(); }

    [[nodiscard]] int questionCount() const noexcept;
    [[nodiscard]] int successPercent() const noexcept;
    [[nodiscard]] int firstTryPercent() const noexcept;

    // Le nombre de JOURS d'affilee ou le joueur a joue, aujourd'hui compris.
    [[nodiscard]] int playingDayStreak() const noexcept { return m_playingDayStreak; }

    // Le temps de jeu total, et celui de la semaine, en clair : « 3 h 25 », « 12 min ».
    [[nodiscard]] QString playTimeText() const { return m_playTimeText; }
    [[nodiscard]] QString recentPlayTimeText() const { return m_recentPlayTimeText; }

    // L'histogramme des quatorze derniers jours : un objet par jour, avec son libelle, son nombre de questions, son
    // nombre de reussites et sa hauteur relative (0-1).
    [[nodiscard]] QVariantList lastDays() const { return m_lastDays; }

    // La repartition par genre : nom, nombre, pourcentage, et les angles du camembert (debut et balayage, en degres).
    [[nodiscard]] QVariantList kinds() const { return m_kinds; }

    // Les points faibles, du plus faible au moins faible : nom lisible, pourcentage de reussite, nombre de questions.
    [[nodiscard]] QVariantList weakestTargets() const { return m_weakestTargets; }

signals:
    void statisticsChanged();

private:
    const domain::QuestionLog & m_log;

    // Ce que le dernier refresh a calcule.
    std::vector<domain::QuestionRecord> m_records;
    domain::QuestionStatistics m_overall;
    std::chrono::seconds m_playTime{ 0 };
    int m_playingDayStreak{ 0 };

    QString m_playTimeText;
    QString m_recentPlayTimeText;

    QVariantList m_lastDays;
    QVariantList m_kinds;
    QVariantList m_weakestTargets;
};

}    // namespace musichien::ui
