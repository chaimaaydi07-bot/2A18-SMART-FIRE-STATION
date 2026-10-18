#ifndef AGENT_H
#define AGENT_H

#include <QString>
#include <QDate>
#include <QSqlQuery>
#include <QSqlQueryModel>

// ============================================================================
//  Classe Agent : entité gérée par le module Gestion du personnel
//  (couche Modèle de l'architecture Modèle-Vue)
//  Toutes les requêtes SQL sur la table AGENT sont ici, et nulle part ailleurs.
// ============================================================================
class Agent
{
    QString id, nom, prenom, poste, grade, specialite, telephone, disponibilite;
    QString certification;
    QDate   dateObtention, dateEcheance;
    QString motDePasse;            // mot de passe chiffré (SHA-256), vide = pas d'accès
    QString erreur;                // message d'Oracle si une requête échoue

public:
    // Constructeurs
    Agent() {}
    Agent(QString id, QString nom, QString prenom, QString poste, QString grade,
          QString specialite, QString telephone, QString disponibilite,
          QString certification, QDate dateObtention, QDate dateEcheance,
          QString motDePasse);

    // Getters
    QString getId() { return id; }
    QString getNom() { return nom; }
    QString getPrenom() { return prenom; }
    QString getPoste() { return poste; }
    QString getGrade() { return grade; }
    QString getSpecialite() { return specialite; }
    QString getTelephone() { return telephone; }
    QString getDisponibilite() { return disponibilite; }
    QString getCertification() { return certification; }
    QDate   getDateObtention() { return dateObtention; }
    QDate   getDateEcheance() { return dateEcheance; }
    QString getMotDePasse() { return motDePasse; }

    // Setters
    void setId(QString v) { id = v; }
    void setNom(QString v) { nom = v; }
    void setPrenom(QString v) { prenom = v; }
    void setPoste(QString v) { poste = v; }
    void setGrade(QString v) { grade = v; }
    void setSpecialite(QString v) { specialite = v; }
    void setTelephone(QString v) { telephone = v; }
    void setDisponibilite(QString v) { disponibilite = v; }
    void setCertification(QString v) { certification = v; }
    void setDateObtention(QDate v) { dateObtention = v; }
    void setDateEcheance(QDate v) { dateEcheance = v; }
    void setMotDePasse(QString v) { motDePasse = v; }

    QString derniereErreur() { return erreur; }

    // Fonctionnalités de base (CRUD) relatives à l'entité Agent
    QSqlQueryModel *afficher();                  // Étape 1 : AFFICHER
    bool ajouter();                              // Étape 2 : AJOUTER
    bool telephoneExiste(QString telephone, QString saufId);   // contrôle de saisie
};

#endif // AGENT_H