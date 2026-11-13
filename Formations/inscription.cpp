#include "inscription.h"
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QDebug>
#include <utility>

static QString g_erreurInscription;

QString Inscription::derniereErreur()
{
    return g_erreurInscription;
}

static bool executerInscription(QSqlQuery &query)
{
    g_erreurInscription.clear();
    if (!QSqlDatabase::database().isOpen()) {
        g_erreurInscription = "Connexion Oracle non ouverte.";
        return false;
    }
    if (!query.exec()) {
        g_erreurInscription = query.lastError().text();
        qDebug().noquote() << "[INSCRIPTION SQL ERREUR]" << g_erreurInscription;
        return false;
    }
    return true;
}

Inscription::Inscription(int formation, int agent, QDate inscriptionDate,
                         QString inscriptionStatus, int present, QString outcome)
    : idFormation(formation), idAgent(agent), dateInscription(inscriptionDate),
      statut(std::move(inscriptionStatus)), presence(present), resultat(std::move(outcome))
{}

bool Inscription::ajouter()
{
    QSqlQuery query;
    query.prepare("INSERT INTO INSCRIRE (ID, ID_AGENT, DATE_INSCRIPTION, "
                  "STATUT_INSCRIPTION, PRESENCE, RESULTAT) "
                  "VALUES (:formation, :agent, :dateInscription, :statut, :presence, :resultat)");
    query.bindValue(":formation", idFormation);
    query.bindValue(":agent", idAgent);
    query.bindValue(":dateInscription", dateInscription.isValid() ? dateInscription : QDate::currentDate());
    query.bindValue(":statut", statut == "ATTENTE" ? "En attente"
                                 : statut == "ANNULE" ? "Annulé"
                                 : statut == "CONFIRME" ? "Confirmé" : "Inscrit");
    if (presence == 0 || presence == 1)
        query.bindValue(":presence", presence == 1 ? "Présent" : "Absent");
    else
        query.bindValue(":presence", QVariant(QMetaType::fromType<QString>()));
    query.bindValue(":resultat", resultat == "REUSSI" ? "Réussi"
                                   : resultat == "ECHEC" ? "Échoué" : "En cours");
    return executerInscription(query);
}

bool Inscription::modifier()
{
    QSqlQuery query;
    query.prepare("UPDATE INSCRIRE SET DATE_INSCRIPTION=:dateInscription, "
                  "STATUT_INSCRIPTION=:statut, PRESENCE=:presence, RESULTAT=:resultat "
                  "WHERE ID=:formation AND ID_AGENT=:agent");
    query.bindValue(":dateInscription", dateInscription.isValid() ? dateInscription : QDate::currentDate());
    query.bindValue(":statut", statut == "ATTENTE" ? "En attente"
                                 : statut == "ANNULE" ? "Annulé"
                                 : statut == "CONFIRME" ? "Confirmé" : "Inscrit");
    if (presence == 0 || presence == 1)
        query.bindValue(":presence", presence == 1 ? "Présent" : "Absent");
    else
        query.bindValue(":presence", QVariant(QMetaType::fromType<QString>()));
    query.bindValue(":resultat", resultat == "REUSSI" ? "Réussi"
                                   : resultat == "ECHEC" ? "Échoué" : "En cours");
    query.bindValue(":formation", idFormation);
    query.bindValue(":agent", idAgent);
    if (!executerInscription(query)) return false;
    if (query.numRowsAffected() == 0) {
        g_erreurInscription = "Aucune inscription ne correspond à ces identifiants.";
        return false;
    }
    return true;
}

bool Inscription::supprimer()
{
    QSqlQuery query;
    query.prepare("DELETE FROM INSCRIRE WHERE ID=:formation AND ID_AGENT=:agent");
    query.bindValue(":formation", idFormation);
    query.bindValue(":agent", idAgent);
    if (!executerInscription(query)) return false;
    if (query.numRowsAffected() == 0) {
        g_erreurInscription = "Aucune inscription ne correspond à ces identifiants.";
        return false;
    }
    return true;
}

QSqlQueryModel* Inscription::afficher()
{
    QSqlQuery query("SELECT ID, ID_AGENT, "
                    "TO_CHAR(DATE_INSCRIPTION, 'YYYY-MM-DD') AS DATE_INSCRIPTION, "
                    "CASE STATUT_INSCRIPTION WHEN 'En attente' THEN 'ATTENTE' "
                    "WHEN 'Annulé' THEN 'ANNULE' WHEN 'Confirmé' THEN 'CONFIRME' ELSE 'INSCRIT' END, "
                    "CASE PRESENCE WHEN 'Présent' THEN 1 WHEN 'Absent' THEN 0 END, "
                    "CASE RESULTAT WHEN 'Réussi' THEN 'REUSSI' WHEN 'Échoué' THEN 'ECHEC' ELSE 'EN_ATTENTE' END "
                    "FROM INSCRIRE ORDER BY ID, ID_AGENT");
    QSqlQueryModel *model = new QSqlQueryModel();
    if (!QSqlDatabase::database().isOpen()) {
        g_erreurInscription = "Connexion Oracle non ouverte.";
        return model;
    }
    // QSqlQuery(sql) can execute at construction; avoid re-executing the
    // active Oracle ODBC statement (which can raise SQLSTATE S1010).
    if (!query.isActive() && (query.lastError().isValid() || !query.exec())) {
        g_erreurInscription = query.lastError().text();
        qDebug().noquote() << "[INSCRIPTION SQL ERREUR]" << g_erreurInscription;
        return model;
    }
    g_erreurInscription.clear();
    model->setQuery(std::move(query));
    model->setHeaderData(0, Qt::Horizontal, QObject::tr("Formation"));
    model->setHeaderData(1, Qt::Horizontal, QObject::tr("Agent"));
    model->setHeaderData(2, Qt::Horizontal, QObject::tr("Date d'inscription"));
    model->setHeaderData(3, Qt::Horizontal, QObject::tr("Statut"));
    model->setHeaderData(4, Qt::Horizontal, QObject::tr("Présence (0/1)"));
    model->setHeaderData(5, Qt::Horizontal, QObject::tr("Résultat"));
    return model;
}
