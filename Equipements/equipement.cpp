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

QSqlQueryModel *Equipement::afficher()
{
    QSqlQueryModel *model = new QSqlQueryModel();
    model->setQuery("SELECT id_equipement, nom, type, marque, modele, quantite, etat, "
                    "TO_CHAR(date_achat, 'YYYY-MM-DD'), TO_CHAR(date_entretien, 'YYYY-MM-DD'), "
                    "id_vehicule FROM EQUIPEMENT ORDER BY id_equipement");

    model->setHeaderData(0, Qt::Horizontal, QObject::tr("ID"));
    model->setHeaderData(1, Qt::Horizontal, QObject::tr("Nom"));
    model->setHeaderData(2, Qt::Horizontal, QObject::tr("Type"));
    model->setHeaderData(3, Qt::Horizontal, QObject::tr("Marque"));
    model->setHeaderData(4, Qt::Horizontal, QObject::tr("Modèle"));
    model->setHeaderData(5, Qt::Horizontal, QObject::tr("Quantité"));
    model->setHeaderData(6, Qt::Horizontal, QObject::tr("État"));
    model->setHeaderData(7, Qt::Horizontal, QObject::tr("Date d'achat"));
    model->setHeaderData(8, Qt::Horizontal, QObject::tr("Date d'entretien"));
    model->setHeaderData(9, Qt::Horizontal, QObject::tr("Vehicule"));
    return model ;
}
bool Equipement::ajouter()
{
    QSqlQuery query;
    query.prepare("INSERT INTO EQUIPEMENT (id_equipement, nom, type, marque,
                  modele, "
                  "quantite, etat, date_achat, date_entretien, id_vehicule) "
                  "VALUES (:id, :nom, :type, :marque, :modele, :quantite,
                  :etat, "
                  "TO_DATE(:achat, 'YYYY-MM-DD'), TO_DATE(:entretien, 'YYYYMM-DD'), :vehicule)");
    query.bindValue(":id", id);
    query.bindValue(":nom", nom);
    query.bindValue(":type", type);
    query.bindValue(":marque", marque);
    query.bindValue(":modele", modele);
    query.bindValue(":quantite", quantite);
    query.bindValue(":etat", etat);
    query.bindValue(":achat", dateAchat); // texte "2026-10-22"
    query.bindValue(":entretien", dateEntretien);
    query.bindValue(":vehicule", idVehicule.isEmpty() ? QVariant() :
                                     idVehicule);
    return query.exec();
}

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
    query.bindValue(":vehicule", idVehicule.isEmpty() ? QVariant() : QVariant(idVehicule));
    query.bindValue(":id", id);

    if (!query.exec()) {
        erreur = query.lastError().text();
        return false;
    }
    erreur = QString();
    return query.numRowsAffected() > 0;   // faux si l'id n'existe pas
}


bool Equipement::supprimer(QString id)
{
    QSqlQuery query;
    query.prepare("DELETE FROM EQUIPEMENT WHERE id_equipement = :id");
    query.bindValue(":id", id);
    if (!query.exec()) return false;
    return query.numRowsAffected() > 0;
}
