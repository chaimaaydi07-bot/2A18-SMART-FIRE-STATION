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
