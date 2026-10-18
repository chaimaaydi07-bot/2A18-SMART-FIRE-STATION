#include "agent.h"
#include <QSqlError>
#include <QObject>

Agent::Agent(QString id, QString nom, QString prenom, QString poste, QString grade,
             QString specialite, QString telephone, QString disponibilite,
             QString certification, QDate dateObtention, QDate dateEcheance,
             QString motDePasse)
{
    this->id = id;
    this->nom = nom;
    this->prenom = prenom;
    this->poste = poste;
    this->grade = grade;
    this->specialite = specialite;
    this->telephone = telephone;
    this->disponibilite = disponibilite;
    this->certification = certification;
    this->dateObtention = dateObtention;
    this->dateEcheance = dateEcheance;
    this->motDePasse = motDePasse;
}

// ======================================================= Étape 1 : AFFICHER
QSqlQueryModel *Agent::afficher()
{
    QSqlQueryModel *model = new QSqlQueryModel();

    // Le nombre d'interventions de chaque agent vient de la table INTERVENTION
    // (association avec le module Incidents) : il sert aux statistiques.
    model->setQuery("SELECT a.id_agent, a.nom, a.prenom, a.poste, a.grade, a.specialite, "
                    "a.telephone, a.disponibilite, a.certification, "
                    "TO_CHAR(a.date_obtention, 'YYYY-MM-DD'), "
                    "TO_CHAR(a.date_echeance, 'YYYY-MM-DD'), a.mot_de_passe, "
                    "(SELECT COUNT(*) FROM INTERVENTION i WHERE i.id_agent = a.id_agent) "
                    "FROM AGENT a ORDER BY a.id_agent");
    while (model->canFetchMore())       // charger toutes les lignes
        model->fetchMore();

    model->setHeaderData(0, Qt::Horizontal, QObject::tr("ID"));
    model->setHeaderData(1, Qt::Horizontal, QObject::tr("Nom"));
    model->setHeaderData(2, Qt::Horizontal, QObject::tr("Prénom"));
    model->setHeaderData(3, Qt::Horizontal, QObject::tr("Poste"));
    model->setHeaderData(4, Qt::Horizontal, QObject::tr("Grade"));
    model->setHeaderData(5, Qt::Horizontal, QObject::tr("Spécialité"));
    model->setHeaderData(6, Qt::Horizontal, QObject::tr("Téléphone"));
    model->setHeaderData(7, Qt::Horizontal, QObject::tr("Disponibilité"));
    model->setHeaderData(8, Qt::Horizontal, QObject::tr("Certification"));
    model->setHeaderData(9, Qt::Horizontal, QObject::tr("Date d'obtention"));
    model->setHeaderData(10, Qt::Horizontal, QObject::tr("Date d'échéance"));
    model->setHeaderData(11, Qt::Horizontal, QObject::tr("Mot de passe"));
    model->setHeaderData(12, Qt::Horizontal, QObject::tr("Interventions"));

    erreur = model->lastError().isValid() ? model->lastError().text() : QString();
    return model;
}
