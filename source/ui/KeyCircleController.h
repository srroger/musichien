#pragma once

// =====================================================================================================================
// Musichien - KeyCircleController
//
// L'ecran du cercle des quintes : une page de REFERENCE, et non un exercice. On n'y repond a rien, on y lit.
//
// Elle porte les trois choses qui vont ensemble, et que la theorie separe trop souvent :
//   * la roue des douze tonalites, avec leur armure et leur relative mineure ;
//   * la tonalite choisie, ouverte : ses sept degres, leur chiffre romain et la QUALITE de l'accord qu'ils portent.
//
// Un controleur fin, comme les autres : il demande au domaine ce qui se calcule, et il n'invente rien. La seule chose
// qu'il decide est ce qui est AFFICHE (les mots) - ce qui est la definition d'une couche d'interface.
// =====================================================================================================================

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace musichien::ui
{

class KeyCircleController final : public QObject
{
    Q_OBJECT

    // Les douze cases du cercle, dans l'ordre de la roue : cinq bemols, do majeur au milieu, six dieses.
    Q_PROPERTY( QVariantList keys READ keys CONSTANT )

    // La case ouverte, decrite comme une entree de la liste ci-dessus plus sa liste de degres. Vide au demarrage :
    // l'ecran demande d'abord, il n'invente pas une tonalite par defaut.
    Q_PROPERTY( QVariantMap selectedKey READ selectedKey NOTIFY selectionChanged )

    // Les sept degres de la case ouverte : le rang, le chiffre romain, la qualite, et le nom de l'accord.
    Q_PROPERTY( QVariantList degrees READ degrees NOTIFY selectionChanged )

public:
    explicit KeyCircleController( QObject * p_parent = nullptr );

    [[nodiscard]] QVariantList keys() const;
    [[nodiscard]] QVariantMap selectedKey() const;
    [[nodiscard]] QVariantList degrees() const;

    // Ouvre une case du cercle. Un index hors bornes ne fait rien : il peut venir d'un ecran, donc d'une donnee.
    Q_INVOKABLE void selectKey( int p_index );

signals:
    void selectionChanged();

private:
    QVariantList m_keys;
    int m_selectedIndex{ -1 };
};

}    // namespace musichien::ui
