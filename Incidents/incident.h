#ifndef INCIDENT_H
#define INCIDENT_H

#include <QString>
#include <QDateTime>
#include <QVariant>
#include <vector>

class Incident
{
public:
    Incident();

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

private:
    // Exécuter une requête préparée et confirmer la transaction.
    static bool executer(const QString &sql,
                         const QVariantList &valeurs,
                         QString &erreur,
                         bool verifierExistence = false);
};

#endif // INCIDENT_H
