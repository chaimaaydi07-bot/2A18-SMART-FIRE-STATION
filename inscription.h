#ifndef INSCRIPTION_H
#define INSCRIPTION_H
#include <QString>
#include <QDate>
#include <QList>
#include <QSqlQuery>

struct Retour { bool ok; QString message; };

struct Recommandation { int idAgent; QString nom; QDate echeance; };

// Inscriptions des agents aux formations et règles métier innovantes.
class Inscription {
public:
    // Contrôle intelligent : doublon, conflit de calendrier, capacité (liste d'attente).
    static Retour inscrire(int idFormation, int idAgent);
    // Désinscription + promotion automatique du premier agent en liste d'attente.
    static Retour desinscrire(int idFormation, int idAgent);
    // presence : -1 = inchangée, 0 = absent, 1 = présent ; resultat : "" = inchangé.
    static Retour marquer(int idFormation, int idAgent, int presence, const QString &resultat);
    // Intitulé de la formation en conflit avec celle-ci pour cet agent ("" si aucun).
    static QString conflit(int idAgent, int idFormation);
    // Agents dont la certification liée au type de la formation expire sous 90 jours.
    static QList<Recommandation> recommandations(int idFormation);
    // Requêtes de lecture pour les vues ; toute la logique SQL reste dans le modèle.
    static QSqlQuery participants(int idFormation);
    static QSqlQuery agentsDisponibles(int idFormation);
    static QSqlQuery attestations();

    static QString libelleResultat(const QString &code);
    static QString libelleEtat(const QString &code);
};
#endif
