#ifndef EQUIPEMENT_EXTRA_H
#define EQUIPEMENT_EXTRA_H

#include <QString>
#include <QDate>
#include <QMap>

// Couche Modèle : colonnes ajoutées à la table EQUIPEMENT
// (SEUIL, DATE_PROCHAINE, REMARQUES, LOCALISATION).
class EquipementExtra
{
public:
    struct Infos {
        int     seuil = 2;
        QDate   prochaine;
        QString remarques;
        QString localisation;
    };

    // Lit les 4 colonnes pour tous les équipements (clé = id_equipement).
    static QMap<QString, Infos> toutes(QString *erreur = nullptr);

    // Enregistre les 4 colonnes d'un équipement.
    static bool enregistrer(const QString &id, const Infos &infos, QString *erreur = nullptr);
};

#endif // EQUIPEMENT_EXTRA_H
