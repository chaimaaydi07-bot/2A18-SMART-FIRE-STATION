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