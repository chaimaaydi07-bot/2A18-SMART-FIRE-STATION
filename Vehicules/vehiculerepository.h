#ifndef VEHICULEREPOSITORY_H
#define VEHICULEREPOSITORY_H
#include <QDate>
#include <QString>
#include <QVector>

struct VehiculeRecord {
    QString id, matricule, type, marque, modele, etat, localisation, remarques;
    QString disponibilite = "Disponible";
    int carburant = 0;
    int seuil = 25;
    qint64 kilometrage = 0;
    QDate achat, entretien, prochainEntretien;
};

class VehiculeRepository {
public:
    static bool charger(QVector<VehiculeRecord> &records, QString &erreur);
    static bool enregistrer(VehiculeRecord &record, bool modification, QString &erreur);
    static bool supprimer(const QString &id, QString &erreur);
    static bool planifier(const QVector<QPair<QString, QDate>> &dates, QString &erreur);
};
#endif
