#ifndef EQUIPEMENT_H
#define EQUIPEMENT_H
#include <QString>
#include <QDate>
#include <QSqlQuery>
#include <QSqlQueryModel>



class Equipement
{
    QString id, nom, type, marque, modele, etat, idVehicule;
    int quantite = 0;
    QDate dateAchat, dateEntretien;
    QString erreur;

public:
    // Constructeurs
    Equipement() {}
    Equipement(QString id, QString nom, QString type, QString marque, QString modele,
               int quantite, QString etat, QDate dateAchat, QDate
                   dateEntretien,
               QString idVehicule);
    // Getters
    QString getId() { return id; }
    QString getNom() { return nom; }
    QString getType() { return type; }
    QString getMarque() { return marque; }
    QString getModele() { return modele; }
    int getQuantite() { return quantite; }
    QString getEtat() { return etat; }
    QDate getDateAchat() { return dateAchat; }
    QDate getDateEntretien() { return dateEntretien; }
    QString getIdVehicule() { return idVehicule; }
    // Setters
    void setId(QString v) { id = v; }
    void setNom(QString v) { nom = v; }
    void setType(QString v) { type = v; }
    void setMarque(QString v) { marque = v; }
    void setModele(QString v) { modele = v; }
    void setQuantite(int v) { quantite = v; }
    void setEtat(QString v) { etat = v; }
    void setDateAchat(QDate v) { dateAchat = v; }
    void setDateEntretien(QDate v) { dateEntretien = v; }
    void setIdVehicule(QString v) { idVehicule = v; }
    QString derniereErreur() { return erreur; }
    // Fonctionnalités de base (CRUD)
    QSqlQueryModel *afficher();
    bool ajouter();
    bool modifier();
    bool supprimer(QString id);
};
#endif
