#include "incident.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <utility>

Incident::Incident()
{
}

// Exécuter une écriture avec une requête préparée.
bool Incident::executer(const QString &sql,
                        const QVariantList &valeurs,
                        QString &erreur,
                        bool verifierExistence)
{
    erreur.clear();

    // Réutiliser la connexion ouverte par le Singleton.
    QSqlDatabase db = QSqlDatabase::database(
        QSqlDatabase::defaultConnection, false);

    if (!db.isValid() || !db.isOpen()) {
        erreur = "La connexion Oracle est fermée.";
        return false;
    }

    if (!db.transaction()) {
        erreur = db.lastError().text();
        return false;
    }

    QSqlQuery query(db);

    if (!query.prepare(sql)) {
        erreur = query.lastError().text();
        query.finish();
        db.rollback();
        return false;
    }

    for (const QVariant &valeur : valeurs) {
        query.addBindValue(valeur);
    }

    if (!query.exec()) {
        erreur = query.lastError().text();
        query.finish();
        db.rollback();
        return false;
    }

    const qint64 lignes = query.numRowsAffected();
    query.finish();

    if (verifierExistence && lignes == 0) {
        erreur = "Incident introuvable ou déjà archivé. "
                 "Actualisez la liste.";
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        erreur = db.lastError().text();
        db.rollback();
        return false;
    }

    return true;
}

// CREATE : ajouter un incident.
bool Incident::ajouter(QString &erreur) const
{
    const QString sql =
        "INSERT INTO INCIDENT "
        "(id_incident, type, date_heure, adresse, "
        "niveau_gravite, nb_victimes, description, statut) "
        "VALUES (?, ?, "
        "TO_DATE(?, 'YYYY-MM-DD HH24:MI:SS'), "
        "?, ?, ?, ?, ?)";

    const QVariantList valeurs {
        id,
        type,
        date.toString("yyyy-MM-dd HH:mm:ss"),
        address,
        gravity,
        victims,
        description,
        status
    };

    return executer(sql, valeurs, erreur);
}

// READ : lire les incidents depuis Oracle.
bool Incident::afficher(std::vector<Incident> &liste,
                        QString &erreur)
{
    erreur.clear();

    QSqlDatabase db = QSqlDatabase::database(
        QSqlDatabase::defaultConnection, false);

    if (!db.isValid() || !db.isOpen()) {
        erreur = "La connexion Oracle est fermée.";
        return false;
    }

    QSqlQuery query(db);
    query.setForwardOnly(true);

    const QString sql =
        "SELECT id_incident, type, "
        "TO_CHAR(date_heure, 'YYYY-MM-DD HH24:MI:SS'), "
        "adresse, niveau_gravite, nb_victimes, "
        "description, statut "
        "FROM INCIDENT "
        "ORDER BY date_heure DESC, id_incident";

    if (!query.prepare(sql)) {
        erreur = query.lastError().text();
        return false;
    }

    if (!query.exec()) {
        erreur = query.lastError().text();
        return false;
    }

    std::vector<Incident> resultat;

    while (query.next()) {
        Incident incident;

        incident.id = query.value(0).toString();
        incident.type = query.value(1).toString();

        incident.date = QDateTime::fromString(
            query.value(2).toString(),
            "yyyy-MM-dd HH:mm:ss");

        incident.address = query.value(3).toString();
        incident.gravity = query.value(4).toString();
        incident.victims = query.value(5).toInt();
        incident.description = query.value(6).toString();
        incident.status = query.value(7).toString();
        incident.archived = incident.status == "Archivé";

        resultat.push_back(incident);
    }

    if (query.lastError().isValid()) {
        erreur = query.lastError().text();
        return false;
    }

    liste = std::move(resultat);
    return true;
}

// UPDATE : modifier un incident.
bool Incident::modifier(QString &erreur) const
{
    const QString sql =
        "UPDATE INCIDENT SET "
        "type = ?, "
        "date_heure = TO_DATE(?, 'YYYY-MM-DD HH24:MI:SS'), "
        "adresse = ?, "
        "niveau_gravite = ?, "
        "nb_victimes = ?, "
        "description = ?, "
        "statut = ? "
        "WHERE id_incident = ? "
        "AND statut <> 'Archivé'";

    const QVariantList valeurs {
        type,
        date.toString("yyyy-MM-dd HH:mm:ss"),
        address,
        gravity,
        victims,
        description,
        status,
        id
    };

    return executer(sql, valeurs, erreur, true);
}

// DELETE : supprimer un incident.
bool Incident::supprimer(const QString &id,
                         QString &erreur)
{
    const QString sql =
        "DELETE FROM INCIDENT WHERE id_incident = ?";

    return executer(sql, QVariantList {id}, erreur, true);
}

// Archiver sans supprimer la ligne.
bool Incident::archiver(const QString &id,
                        QString &erreur)
{
    const QString sql =
        "UPDATE INCIDENT SET statut = 'Archivé' "
        "WHERE id_incident = ? "
        "AND statut <> 'Archivé'";

    return executer(sql, QVariantList {id}, erreur, true);
}