#ifndef VEHICULEREPOSITORY_H
#define VEHICULEREPOSITORY_H
#include <QDate>
#include <QString>
#include <QVector>

struct VehiculeRecord {
    // Identite du vehicule.
    QString id;
    QString matricule;
    QString type;
    QString marque;
    QString modele;

    // Etat et informations pratiques.
    QString etat;
    QString localisation;
    QString remarques;
    QString disponibilite = "Disponible";
    int carburant = 0;
    int seuil = 25;
    qint64 kilometrage = 0;
    // Dates : une QDate vide represente une valeur NULL dans Oracle.
    QDate achat;
    QDate entretien;
    QDate prochainEntretien;
};

// Cette classe ne dessine rien : elle lit et ecrit uniquement dans Oracle.
// Chaque fonction retourne true en cas de reussite, sinon elle remplit erreur.
class VehiculeRepository {
public:
    static bool charger(QVector<VehiculeRecord> &records, QString &erreur);
    static bool enregistrer(VehiculeRecord &record, bool modification, QString &erreur);
    static bool supprimer(const QString &id, QString &erreur);
    static bool planifier(const QVector<QPair<QString, QDate>> &dates, QString &erreur);
};
#endif