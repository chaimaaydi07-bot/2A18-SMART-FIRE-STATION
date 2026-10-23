#ifndef FORMATION_H
#define FORMATION_H
#include <QString>
#include <QDate>
#include <QSqlQuery>

// Entité Formation + règles métier (validation, statut automatique, requêtes).
class Formation {
public:
    int id = 0;
    QString intitule, type, formateur, lieu;
    QDate dateDebut, dateFin;
    int duree = 0, capacite = 0;
    bool certification = false;
    QString statut;               // planifiee | en_cours | terminee | annulee

    QString valider() const;      // "" si valide, sinon message d'erreur
    bool ajouter(QString *erreur);
    bool modifier(QString *erreur);
    static bool supprimer(int id);
    static bool charger(int id, Formation &f);

    static QString statutAuto(const QDate &debut, const QDate &fin);
    static void majStatuts();     // planifiee -> en_cours -> terminee
    static int inscrits(int idFormation);
    static int placesRestantes(int idFormation);

    static QSqlQuery liste(const QString &recherche, const QString &colonneTri, bool croissant);
    static QStringList types();
    static QString libelleStatut(const QString &code);
};
#endif
