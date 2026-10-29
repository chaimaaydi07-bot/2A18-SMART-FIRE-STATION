#ifndef INCIDENT_H
#define INCIDENT_H

#include <QString>
#include <QDateTime>
#include <QVariant>
#include <vector>

class Incident
{
private:
    // Données encapsulées : accès par getters et setters.
    QString id;
    QString code;
    QString type;
    QDateTime date;
    QString address;
    QString gravity;
    int victims = 0;
    QString description;
    QString status;
    bool archived = false;

    // Exécuter une requête préparée et confirmer la transaction.
    static bool executer(const QString &sql,
                         const QVariantList &valeurs,
                         QString &erreur,
                         bool verifierExistence = false);

public:
    Incident();

    QString getId() const;
    void setId(const QString & valeur);
    QString getCode() const;
    void setCode(const QString & valeur);
    QString getType() const;
    void setType(const QString & valeur);
    QDateTime getDate() const;
    void setDate(const QDateTime & valeur);
    QString getAddress() const;
    void setAddress(const QString & valeur);
    QString getGravity() const;
    void setGravity(const QString & valeur);
    int getVictims() const;
    void setVictims(int valeur);
    QString getDescription() const;
    void setDescription(const QString & valeur);
    QString getStatus() const;
    void setStatus(const QString & valeur);
    bool getArchived() const;
    void setArchived(bool valeur);

    // Ajouter un incident dans Oracle.
    bool ajouter(QString &erreur) const;

    // Modifier un incident existant.
    bool modifier(QString &erreur) const;

    // Lire les incidents depuis Oracle.
    static bool afficher(std::vector<Incident> &liste,
                         QString &erreur);

    // Supprimer un incident.
    static bool supprimer(const QString &id,
                          QString &erreur);

    // Archiver un incident.
    static bool archiver(const QString &id,
                         QString &erreur);

};

#endif // INCIDENT_H
