#ifndef FORMATION_H
#define FORMATION_H

#include <QString>
#include <QDate>
#include <QSqlQueryModel>
#include <QMap>

class Formation
{
public:
    Formation();
    Formation(int id, QString titre, QString type, QDate debut, QDate fin,
              int duree, QString formateur, QString lieu, int places,
              QString niveau, QString statut);

    // Getters
    int     getId() const        { return id; }
    QString getTitre() const     { return titre; }
    QString getType() const      { return type; }
    QDate   getDateDebut() const { return dateDebut; }
    QDate   getDateFin() const   { return dateFin; }
    int     getDuree() const     { return duree; }
    QString getFormateur() const { return formateur; }
    QString getLieu() const      { return lieu; }
    int     getNbPlaces() const  { return nbPlaces; }
    QString getNiveau() const    { return niveau; }
    QString getStatut() const    { return statut; }

    // Setters
    void setId(int v)            { id = v; }
    void setTitre(QString v)     { titre = v; }
    void setType(QString v)      { type = v; }
    void setDateDebut(QDate v)   { dateDebut = v; }
    void setDateFin(QDate v)     { dateFin = v; }
    void setDuree(int v)         { duree = v; }
    void setFormateur(QString v) { formateur = v; }
    void setLieu(QString v)      { lieu = v; }
    void setNbPlaces(int v)      { nbPlaces = v; }
    void setNiveau(QString v)    { niveau = v; }
    void setStatut(QString v)    { statut = v; }

    // CRUD
    bool ajouter();
    bool modifier();
    bool supprimer(int id);
    QSqlQueryModel* afficher();

    // Metiers
    QSqlQueryModel* lister(const QString &critere, const QString &colonne, bool croissant);
    QSqlQueryModel* rechercher(const QString &critere);
    QSqlQueryModel* trier(const QString &colonne, bool croissant = true);
    QMap<QString,int> statistiquesParType();
    bool existe(int id);
    bool enregistrerPresence(int idFormation, int idPompier);
    int  nbInscrits(int idFormation);

private:
    int id;
    QString titre, type, formateur, lieu, niveau, statut;
    QDate dateDebut, dateFin;
    int duree, nbPlaces;
};

#endif // FORMATION_H
