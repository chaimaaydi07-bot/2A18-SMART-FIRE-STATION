#ifndef INSCRIPTION_H
#define INSCRIPTION_H

#include <QDate>
#include <QSqlQueryModel>
#include <QString>

class Inscription
{
public:
    Inscription(int idFormation = 0, int idAgent = 0,
                QDate dateInscription = QDate(),
                QString statut = QStringLiteral("INSCRIT"),
                int presence = -1,
                QString resultat = QStringLiteral("EN_ATTENTE"));

    int getIdFormation() const { return idFormation; }
    int getIdAgent() const { return idAgent; }
    QDate getDateInscription() const { return dateInscription; }

    bool ajouter();
    bool modifier();
    bool supprimer();
    QSqlQueryModel* afficher();
    static QString derniereErreur();

private:
    int idFormation;
    int idAgent;
    QDate dateInscription;
    QString statut;
    int presence;
    QString resultat;
};

#endif // INSCRIPTION_H
