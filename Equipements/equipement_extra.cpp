#include "equipement_extra.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

QMap<QString, EquipementExtra::Infos> EquipementExtra::toutes(QString *erreur)
{
    QMap<QString, Infos> resultat;

    QSqlQuery query;
    if (!query.exec("SELECT id_equipement, seuil, "
                    "TO_CHAR(date_prochaine, 'YYYY-MM-DD'), remarques, localisation "
                    "FROM EQUIPEMENT")) {
        if (erreur) *erreur = query.lastError().text();
        return resultat;
    }

    while (query.next()) {
        Infos i;
        i.seuil        = query.value(1).isNull() ? 2 : query.value(1).toInt();
        i.prochaine    = QDate::fromString(query.value(2).toString(), "yyyy-MM-dd");
        i.remarques    = query.value(3).toString();
        i.localisation = query.value(4).toString();
        resultat.insert(query.value(0).toString(), i);
    }

    if (erreur) erreur->clear();
    return resultat;
}

bool EquipementExtra::enregistrer(const QString &id, const Infos &infos, QString *erreur)
{
    QSqlQuery query;
    query.prepare("UPDATE EQUIPEMENT "
                  "SET seuil = :seuil, "
                  "    date_prochaine = TO_DATE(:proch, 'YYYY-MM-DD'), "
                  "    remarques = :rem, "
                  "    localisation = :loc "
                  "WHERE id_equipement = :id");

    query.bindValue(":seuil", infos.seuil);
    query.bindValue(":proch", infos.prochaine.isValid()
                                  ? infos.prochaine.toString("yyyy-MM-dd")
                                  : QString());
    query.bindValue(":rem", infos.remarques);
    query.bindValue(":loc", infos.localisation);
    query.bindValue(":id", id);

    if (!query.exec()) {
        if (erreur) *erreur = query.lastError().text();
        return false;
    }
    if (erreur) erreur->clear();
    return true;
}