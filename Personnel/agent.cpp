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

// Les dates sont envoyées sous forme de texte puis converties par Oracle (TO_DATE).
// Une date vide devient NULL dans la base.
static QString texteDate(const QDate &d)
{
    return d.isValid() ? d.toString("yyyy-MM-dd") : QString();
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

// ======================================================= Étape 2 : AJOUTER
bool Agent::ajouter()
{
    QSqlQuery query;

    // prepare() prend la requête en paramètre pour la préparer à l'exécution.
    query.prepare("INSERT INTO AGENT (id_agent, nom, prenom, poste, grade, specialite, "
                  "telephone, disponibilite, certification, date_obtention, date_echeance, "
                  "mot_de_passe) "
                  "VALUES (:id, :nom, :prenom, :poste, :grade, :specialite, :telephone, "
                  ":disponibilite, :certification, TO_DATE(:obtention, 'YYYY-MM-DD'), "
                  "TO_DATE(:echeance, 'YYYY-MM-DD'), :motDePasse)");

    // Création des variables liées (protection contre l'injection SQL)
    query.bindValue(":id", id);
    query.bindValue(":nom", nom);
    query.bindValue(":prenom", prenom);
    query.bindValue(":poste", poste);
    query.bindValue(":grade", grade);
    query.bindValue(":specialite", specialite);
    query.bindValue(":telephone", telephone);
    query.bindValue(":disponibilite", disponibilite);
    query.bindValue(":certification", certification);
    query.bindValue(":obtention", texteDate(dateObtention));
    query.bindValue(":echeance", texteDate(dateEcheance));
    query.bindValue(":motDePasse", motDePasse);

    // exec() envoie la requête pour l'exécuter.
    const bool ok = query.exec();
    erreur = ok ? QString() : query.lastError().text();
    return ok;
}

// Contrôle de saisie : vrai si un autre agent a déjà ce numéro
// (saufId = l'agent en cours de modification, vide pour un ajout).
bool Agent::telephoneExiste(QString telephone, QString saufId)
{
    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM AGENT WHERE telephone = :telephone "
                  "AND id_agent <> :id");
    query.bindValue(":telephone", telephone);
    query.bindValue(":id", saufId.isEmpty() ? QString("-") : saufId);
    return query.exec() && query.next() && query.value(0).toInt() > 0;
}