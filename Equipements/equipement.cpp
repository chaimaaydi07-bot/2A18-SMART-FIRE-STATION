#include "equipement.h"
#include <QSqlError>
#include <QObject>

Equipement::Equipement(QString id, QString nom, QString type, QString marque, QString modele,
                       int quantite, QString etat, QDate dateAchat, QDate dateEntretien,
                       QString idVehicule)
{
    this->id = id;
    this->nom = nom;
    this->type = type;
    this->marque = marque;
    this->modele = modele;
    this->quantite = quantite;
    this->etat = etat;
    this->dateAchat = dateAchat;
    this->dateEntretien = dateEntretien;
    this->idVehicule = idVehicule;
}

// Les dates sont envoyées sous forme de texte puis converties par Oracle (TO_DATE).
// Une date vide devient NULL dans la base.
static QString texteDate(const QDate &d)
{
    return d.isValid() ? d.toString("yyyy-MM-dd") : QString();
}

// ======================================================= Étape 1 : AFFICHER
static QVariant valeurVehicule(const QString &idVehicule)
{
    if (idVehicule.trimmed().isEmpty())
        return QVariant(QMetaType::fromType<int>());   // NULL de type entier
    return QVariant(idVehicule.trimmed().toInt());
}
QSqlQueryModel *Equipement::afficher()
{
    QSqlQueryModel *model = new QSqlQueryModel();

    // Les dates sont renvoyées en texte avec TO_CHAR pour être faciles à lire en C++.
    model->setQuery("SELECT id_equipement, nom, type, marque, modele, "
                    "quantite, etat, "
                    "TO_CHAR(date_achat, 'YYYY-MM-DD'), "
                    "TO_CHAR(date_entretien, 'YYYY-MM-DD'), "
                    "id_vehicule FROM EQUIPEMENT ORDER BY id_equipement");
    while (model->canFetchMore())       // charger toutes les lignes
        model->fetchMore();

    model->setHeaderData(0, Qt::Horizontal, QObject::tr("ID"));
    model->setHeaderData(1, Qt::Horizontal, QObject::tr("Nom"));
    model->setHeaderData(2, Qt::Horizontal, QObject::tr("Type"));
    model->setHeaderData(3, Qt::Horizontal, QObject::tr("Marque"));
    model->setHeaderData(4, Qt::Horizontal, QObject::tr("Modèle"));
    model->setHeaderData(5, Qt::Horizontal, QObject::tr("Quantité"));
    model->setHeaderData(6, Qt::Horizontal, QObject::tr("État"));
    model->setHeaderData(7, Qt::Horizontal, QObject::tr("Date d'achat"));
    model->setHeaderData(8, Qt::Horizontal, QObject::tr("Date d'entretien"));
    model->setHeaderData(9, Qt::Horizontal, QObject::tr("Véhicule"));

    erreur = model->lastError().isValid() ? model->lastError().text() : QString();
    return model;
}

// ======================================================= Étape 2 : AJOUTER
bool Equipement::ajouter()
{
    QSqlQuery query;

    // prepare() prend la requête en paramètre pour la préparer à l'exécution.
    query.prepare("INSERT INTO EQUIPEMENT (id_equipement, nom, type, marque, modele, "
                  "quantite, etat, date_achat, date_entretien, id_vehicule) "
                  "VALUES (:id, :nom, :type, :marque, :modele, :quantite, :etat, "
                  "TO_DATE(:achat, 'YYYY-MM-DD'), TO_DATE(:entretien, 'YYYY-MM-DD'), "
                  ":vehicule)");

    // Création des variables liées (protection contre l'injection SQL)
    query.bindValue(":id", id);
    query.bindValue(":nom", nom);
    query.bindValue(":type", type);
    query.bindValue(":marque", marque);
    query.bindValue(":modele", modele);
    query.bindValue(":quantite", quantite);
    query.bindValue(":etat", etat);
    query.bindValue(":achat", texteDate(dateAchat));
    query.bindValue(":entretien", texteDate(dateEntretien));
    // QVariant() envoie NULL : un équipement peut ne pas être dans un véhicule.
    query.bindValue(":vehicule", valeurVehicule(idVehicule));

    // exec() envoie la requête pour l'exécuter.
    const bool ok = query.exec();
    erreur = ok ? QString() : query.lastError().text();
    return ok;
}

// ======================================================= Étape 3 : MODIFIER
bool Equipement::modifier()
{
    QSqlQuery query;

    // Sans la condition WHERE, on modifierait toutes les lignes de la table !
    query.prepare("UPDATE EQUIPEMENT SET nom = :nom, type = :type, marque = :marque, "
                  "modele = :modele, quantite = :quantite, etat = :etat, "
                  "date_achat = TO_DATE(:achat, 'YYYY-MM-DD'), "
                  "date_entretien = TO_DATE(:entretien, 'YYYY-MM-DD'), "
                  "id_vehicule = :vehicule "
                  "WHERE id_equipement = :id");

    // mêmes bindValue que dans ajouter()
    query.bindValue(":nom", nom);
    query.bindValue(":type", type);
    query.bindValue(":marque", marque);
    query.bindValue(":modele", modele);
    query.bindValue(":quantite", quantite);
    query.bindValue(":etat", etat);
    query.bindValue(":achat", texteDate(dateAchat));
    query.bindValue(":entretien", texteDate(dateEntretien));
    query.bindValue(":vehicule", valeurVehicule(idVehicule));
    query.bindValue(":id", id);

    if (!query.exec()) {
        erreur = query.lastError().text();
        return false;
    }
    erreur = QString();
    return query.numRowsAffected() > 0;   // faux si l'id n'existe pas
}

// ======================================================= Étape 4 : SUPPRIMER
bool Equipement::supprimer(QString id)
{
    QSqlQuery query;
    query.prepare("DELETE FROM EQUIPEMENT WHERE id_equipement = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        erreur = query.lastError().text();
        return false;
    }
    erreur = QString();
    return query.numRowsAffected() > 0;
}