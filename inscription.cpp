#include "inscription.h"
#include "formation.h"
#include <QSqlDatabase>
#include <QSqlError>
#include <QVariant>

QString Inscription::libelleResultat(const QString &code)
{
    if (code == "reussi") return "Réussi";
    if (code == "echoue") return "Échoué";
    return "En attente";
}

QString Inscription::libelleEtat(const QString &code)
{
    return code == "attente" ? "Liste d'attente" : "Inscrit";
}

static QString nomAgent(int idAgent)
{
    QSqlQuery q;
    q.prepare("SELECT NOM || ' ' || PRENOM FROM AGENT WHERE ID_AGENT=?");
    q.addBindValue(idAgent);
    return q.exec() && q.next() ? q.value(0).toString() : QString();
}

QString Inscription::conflit(int idAgent, int idFormation)
{
    QSqlQuery q;
    q.prepare("SELECT f2.TITRE FROM INSCRIPTION i "
              "JOIN FORMATION f2 ON f2.ID=i.ID_FORMATION "
              "JOIN FORMATION f1 ON f1.ID=:formation "
              "WHERE i.ID_AGENT=:agent AND i.ETAT='inscrit' AND f2.ID<>f1.ID "
              "AND f2.STATUT<>'annulee' "
              "AND f2.DATE_DEBUT<=f1.DATE_FIN AND f2.DATE_FIN>=f1.DATE_DEBUT "
              "FETCH FIRST 1 ROWS ONLY");
    q.bindValue(":formation", idFormation);
    q.bindValue(":agent", idAgent);
    return q.exec() && q.next() ? q.value(0).toString() : QString();
}

Retour Inscription::inscrire(int idFormation, int idAgent)
{
    if (idFormation <= 0 || idAgent <= 0) return {false, "Sélectionnez une formation et un agent valides."};
    if (!QSqlDatabase::database().isOpen()) return {false, "La connexion Oracle n'est pas ouverte."};
    Formation formation;
    if (!Formation::charger(idFormation, formation)) return {false, "Formation introuvable."};
    if (formation.statut == "annulee") return {false, "Cette formation est annulée."};
    if (formation.statut == "terminee") return {false, "Cette formation est terminée."};

    QSqlQuery agent;
    agent.prepare("SELECT COUNT(*) FROM AGENT WHERE ID_AGENT=?");
    agent.addBindValue(idAgent);
    if (!agent.exec()) return {false, agent.lastError().text()};
    if (!agent.next() || agent.value(0).toInt() == 0) return {false, "Agent introuvable."};

    QSqlQuery doublon;
    doublon.prepare("SELECT COUNT(*) FROM INSCRIPTION WHERE ID_FORMATION=? AND ID_AGENT=?");
    doublon.addBindValue(idFormation); doublon.addBindValue(idAgent);
    if (!doublon.exec()) return {false, doublon.lastError().text()};
    if (doublon.next() && doublon.value(0).toInt() > 0)
        return {false, "Cet agent est déjà inscrit ou en liste d'attente pour cette formation."};

    const QString chevauchement = conflit(idAgent, idFormation);
    if (!chevauchement.isEmpty())
        return {false, QString("Conflit de calendrier : l'agent est déjà inscrit à « %1 » sur cette période.").arg(chevauchement)};

    const bool complet = Formation::placesRestantes(idFormation) <= 0;
    QSqlQuery q;
    q.prepare("INSERT INTO INSCRIPTION(ID_FORMATION,ID_AGENT,ETAT) VALUES(?,?,?)");
    q.addBindValue(idFormation); q.addBindValue(idAgent);
    q.addBindValue(complet ? "attente" : "inscrit");
    if (!q.exec()) return {false, q.lastError().text()};
    const QString nom = nomAgent(idAgent);
    return {true, complet ? QString("Formation complète : %1 est ajouté à la liste d'attente.").arg(nom)
                          : QString("%1 est inscrit.").arg(nom)};
}

Retour Inscription::desinscrire(int idFormation, int idAgent)
{
    if (idFormation <= 0 || idAgent <= 0) return {false, "Sélectionnez une inscription valide."};
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) return {false, "La connexion Oracle n'est pas ouverte."};
    if (!db.transaction()) return {false, db.lastError().text()};

    QSqlQuery selection;
    selection.prepare("SELECT ETAT FROM INSCRIPTION WHERE ID_FORMATION=? AND ID_AGENT=?");
    selection.addBindValue(idFormation); selection.addBindValue(idAgent);
    if (!selection.exec() || !selection.next()) { db.rollback(); return {false, "Inscription introuvable."}; }
    const bool etaitInscrit = selection.value(0).toString() == "inscrit";
    Formation formation;
    if (!Formation::charger(idFormation, formation)) { db.rollback(); return {false, "Formation introuvable."}; }
    if (formation.statut == "terminee" || formation.statut == "annulee") {
        db.rollback();
        return {false, "Impossible de modifier les inscriptions d'une formation terminée ou annulée."};
    }

    QSqlQuery suppression;
    suppression.prepare("DELETE FROM INSCRIPTION WHERE ID_FORMATION=? AND ID_AGENT=?");
    suppression.addBindValue(idFormation); suppression.addBindValue(idAgent);
    if (!suppression.exec()) { const QString err=suppression.lastError().text(); db.rollback(); return {false, err}; }

    QString message = "Désinscription effectuée.";
    if (etaitInscrit) {
        QSqlQuery attente;
        attente.prepare("SELECT ID_AGENT FROM INSCRIPTION WHERE ID_FORMATION=? AND ETAT='attente' "
                        "ORDER BY DATE_INSCRIPTION, ID_AGENT");
        attente.addBindValue(idFormation);
        if (!attente.exec()) { const QString err=attente.lastError().text(); db.rollback(); return {false, err}; }
        while (attente.next()) {
            const int candidat = attente.value(0).toInt();
            if (!conflit(candidat, idFormation).isEmpty()) continue;
            QSqlQuery promotion;
            promotion.prepare("UPDATE INSCRIPTION SET ETAT='inscrit' WHERE ID_FORMATION=? AND ID_AGENT=?");
            promotion.addBindValue(idFormation); promotion.addBindValue(candidat);
            if (!promotion.exec()) { const QString err=promotion.lastError().text(); db.rollback(); return {false, err}; }
            message += QString(" %1 est promu depuis la liste d'attente.").arg(nomAgent(candidat));
            break;
        }
    }
    if (!db.commit()) { const QString err=db.lastError().text(); db.rollback(); return {false, err}; }
    return {true, message};
}

Retour Inscription::marquer(int idFormation, int idAgent, int presence, const QString &resultat)
{
    if (idFormation <= 0 || idAgent <= 0) return {false, "Sélectionnez une inscription valide."};
    if (presence < -1 || presence > 1) return {false, "La valeur de présence est invalide."};
    if (!resultat.isEmpty() && resultat != "en_attente" && resultat != "reussi" && resultat != "echoue")
        return {false, "Le résultat de formation est invalide."};

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) return {false, "La connexion Oracle n'est pas ouverte."};
    if (!db.transaction()) return {false, db.lastError().text()};

    QSqlQuery q;
    q.prepare("SELECT ETAT, PRESENCE FROM INSCRIPTION WHERE ID_FORMATION=? AND ID_AGENT=?");
    q.addBindValue(idFormation); q.addBindValue(idAgent);
    if (!q.exec() || !q.next()) { db.rollback(); return {false, "Inscription introuvable."}; }
    if (q.value(0).toString() != "inscrit") { db.rollback(); return {false, "Cet agent est en liste d'attente."}; }
    const int presenceActuelle = q.value(1).isNull() ? -1 : q.value(1).toInt();

    Formation formation;
    if (!Formation::charger(idFormation, formation)) { db.rollback(); return {false, "Formation introuvable."}; }
    if (formation.statut == "planifiee") { db.rollback(); return {false, "La formation n'a pas encore commencé."}; }
    if (formation.statut == "annulee") { db.rollback(); return {false, "Cette formation est annulée."}; }
    const int presenceFinale = presence >= 0 ? presence : presenceActuelle;
    if (!resultat.isEmpty() && resultat != "en_attente" && presenceFinale != 1) {
        db.rollback();
        return {false, "Le résultat ne peut être saisi que pour un agent présent."};
    }

    QSqlQuery update;
    if (presence >= 0) {
        update.prepare("UPDATE INSCRIPTION SET PRESENCE=?, RESULTAT=CASE WHEN ?=0 THEN 'en_attente' ELSE RESULTAT END "
                       "WHERE ID_FORMATION=? AND ID_AGENT=?");
        update.addBindValue(presence); update.addBindValue(presence);
        update.addBindValue(idFormation); update.addBindValue(idAgent);
        if (!update.exec()) { const QString err=update.lastError().text(); db.rollback(); return {false, err}; }
    }
    if (!resultat.isEmpty()) {
        update.prepare("UPDATE INSCRIPTION SET RESULTAT=? WHERE ID_FORMATION=? AND ID_AGENT=?");
        update.addBindValue(resultat); update.addBindValue(idFormation); update.addBindValue(idAgent);
        if (!update.exec()) { const QString err=update.lastError().text(); db.rollback(); return {false, err}; }
    }

    QString message = "Présence et résultat enregistrés.";
    if (resultat == "reussi" && formation.certification) {
        QSqlQuery certification;
        certification.prepare("SELECT ID_CERTIFICATION FROM CERTIFICATION WHERE ID_AGENT=? AND NOM=?");
        certification.addBindValue(idAgent); certification.addBindValue(formation.type);
        if (!certification.exec()) { const QString err=certification.lastError().text(); db.rollback(); return {false, err}; }
        const QDate obtention = formation.dateFin;
        const QDate echeance = formation.dateFin.addYears(2);
        if (certification.next()) {
            update.prepare("UPDATE CERTIFICATION SET DATE_OBTENTION=?, DATE_ECHEANCE=? WHERE ID_CERTIFICATION=?");
            update.addBindValue(obtention); update.addBindValue(echeance); update.addBindValue(certification.value(0));
        } else {
            update.prepare("INSERT INTO CERTIFICATION(ID_AGENT,NOM,DATE_OBTENTION,DATE_ECHEANCE) VALUES(?,?,?,?)");
            update.addBindValue(idAgent); update.addBindValue(formation.type);
            update.addBindValue(obtention); update.addBindValue(echeance);
        }
        if (!update.exec()) { const QString err=update.lastError().text(); db.rollback(); return {false, err}; }
        message = "Résultat enregistré ; certification mise à jour (échéance " + echeance.toString("dd/MM/yyyy") + ").";
    }
    if (!db.commit()) { const QString err=db.lastError().text(); db.rollback(); return {false, err}; }
    return {true, message};
}

QList<Recommandation> Inscription::recommandations(int idFormation)
{
    QList<Recommandation> recommandations;
    Formation formation;
    if (!Formation::charger(idFormation, formation)) return recommandations;
    QSqlQuery q;
    q.prepare("SELECT a.ID_AGENT, a.NOM || ' ' || a.PRENOM, TO_CHAR(c.DATE_ECHEANCE,'YYYY-MM-DD') "
              "FROM CERTIFICATION c JOIN AGENT a ON a.ID_AGENT=c.ID_AGENT "
              "WHERE c.NOM=? AND c.DATE_ECHEANCE<=? "
              "AND a.ID_AGENT NOT IN (SELECT ID_AGENT FROM INSCRIPTION WHERE ID_FORMATION=?) "
              "ORDER BY c.DATE_ECHEANCE");
    q.addBindValue(formation.type);
    q.addBindValue(QDate::currentDate().addDays(90));
    q.addBindValue(idFormation);
    if (!q.exec()) return recommandations;
    while (q.next())
        recommandations.append({q.value(0).toInt(), q.value(1).toString(),
                                QDate::fromString(q.value(2).toString(), Qt::ISODate)});
    return recommandations;
}

QSqlQuery Inscription::participants(int idFormation)
{
    QSqlQuery q;
    q.prepare("SELECT i.ID_FORMATION, i.ID_AGENT, a.NOM || ' ' || a.PRENOM, a.GRADE, "
              "CASE i.ETAT WHEN 'inscrit' THEN 'Inscrit' ELSE 'Liste d''attente' END, "
              "CASE WHEN i.PRESENCE=1 THEN 'Présent' WHEN i.PRESENCE=0 THEN 'Absent' ELSE '—' END, "
              "CASE i.RESULTAT WHEN 'reussi' THEN 'Réussi' WHEN 'echoue' THEN 'Échoué' ELSE 'En attente' END "
              "FROM INSCRIPTION i JOIN AGENT a ON a.ID_AGENT=i.ID_AGENT "
              "WHERE i.ID_FORMATION=? ORDER BY CASE i.ETAT WHEN 'inscrit' THEN 0 ELSE 1 END, i.DATE_INSCRIPTION, i.ID_AGENT");
    q.addBindValue(idFormation);
    q.exec();
    return q;
}

QSqlQuery Inscription::agentsDisponibles(int idFormation)
{
    QSqlQuery q;
    q.prepare("SELECT a.ID_AGENT, a.NOM || ' ' || a.PRENOM || ' (' || NVL(a.GRADE,'') || ')' "
              "FROM AGENT a WHERE a.ID_AGENT NOT IN "
              "(SELECT ID_AGENT FROM INSCRIPTION WHERE ID_FORMATION=?) ORDER BY a.NOM, a.PRENOM");
    q.addBindValue(idFormation);
    q.exec();
    return q;
}

QSqlQuery Inscription::attestations()
{
    QSqlQuery q;
    q.prepare("SELECT a.ID_AGENT, a.NOM || ' ' || a.PRENOM, f.TITRE, f.FORMATEUR, f.LIEU, "
              "TO_CHAR(f.DATE_DEBUT,'DD/MM/YYYY'), TO_CHAR(f.DATE_FIN,'DD/MM/YYYY'), f.DUREE "
              "FROM INSCRIPTION i JOIN AGENT a ON a.ID_AGENT=i.ID_AGENT "
              "JOIN FORMATION f ON f.ID=i.ID_FORMATION "
              "WHERE f.STATUT='terminee' AND f.CERTIFICATION=1 AND i.ETAT='inscrit' "
              "AND i.PRESENCE=1 AND i.RESULTAT='reussi' ORDER BY f.DATE_FIN, a.NOM, a.PRENOM");
    q.exec();
    return q;
}
