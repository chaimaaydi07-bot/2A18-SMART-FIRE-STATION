#include "inscription.h"
#include "formation.h"
#include <QSqlQuery>
#include <QVariant>

QString Inscription::libelleResultat(const QString &c)
{
    if (c == "reussi") return "Réussi";
    if (c == "echoue") return "Échoué";
    return "En attente";
}

QString Inscription::libelleEtat(const QString &c)
{
    return c == "attente" ? "Liste d'attente" : "Inscrit";
}

static QString nomAgent(int idAgent)
{
    QSqlQuery q;
    q.prepare("SELECT nom || ' ' || prenom FROM agent WHERE id=?");
    q.addBindValue(idAgent); q.exec();
    return q.next() ? q.value(0).toString() : QString();
}

QString Inscription::conflit(int idAgent, int idFormation)
{
    QSqlQuery q;
    q.prepare("SELECT f2.intitule FROM inscription i"
              " JOIN formation f2 ON f2.id = i.id_formation"
              " JOIN formation f1 ON f1.id = :f"
              " WHERE i.id_agent = :a AND i.etat = 'inscrit' AND f2.id <> f1.id"
              " AND f2.statut <> 'annulee'"
              " AND f2.date_debut <= f1.date_fin AND f2.date_fin >= f1.date_debut LIMIT 1");
    q.bindValue(":f", idFormation); q.bindValue(":a", idAgent); q.exec();
    return q.next() ? q.value(0).toString() : QString();
}

Retour Inscription::inscrire(int idFormation, int idAgent)
{
    Formation f;
    if (!Formation::charger(idFormation, f)) return {false, "Formation introuvable."};
    if (f.statut == "annulee")  return {false, "Cette formation est annulée."};
    if (f.statut == "terminee") return {false, "Cette formation est terminée."};

    QSqlQuery d;
    d.prepare("SELECT COUNT(*) FROM inscription WHERE id_formation=? AND id_agent=?");
    d.addBindValue(idFormation); d.addBindValue(idAgent); d.exec();
    if (d.next() && d.value(0).toInt() > 0)
        return {false, "Cet agent est déjà inscrit (ou en liste d'attente) pour cette formation."};

    const QString c = conflit(idAgent, idFormation);
    if (!c.isEmpty())
        return {false, QString("Conflit de calendrier : %1 est déjà inscrit à « %2 » sur cette période.")
                       .arg(nomAgent(idAgent), c)};

    const bool complet = Formation::placesRestantes(idFormation) <= 0;
    QSqlQuery q;
    q.prepare("INSERT INTO inscription(id_formation,id_agent,etat) VALUES(?,?,?)");
    q.addBindValue(idFormation); q.addBindValue(idAgent);
    q.addBindValue(complet ? "attente" : "inscrit");
    if (!q.exec()) return {false, "Échec de l'inscription."};
    return {true, complet ? QString("Formation complète : %1 est placé en liste d'attente.").arg(nomAgent(idAgent))
                          : QString("%1 est inscrit.").arg(nomAgent(idAgent))};
}

Retour Inscription::desinscrire(int idInscription)
{
    QSqlQuery q;
    q.prepare("SELECT id_formation, etat FROM inscription WHERE id=?");
    q.addBindValue(idInscription); q.exec();
    if (!q.next()) return {false, "Inscription introuvable."};
    const int idF = q.value(0).toInt();
    const bool etaitInscrit = q.value(1).toString() == "inscrit";

    Formation f; Formation::charger(idF, f);
    if (f.statut == "terminee" || f.statut == "annulee")
        return {false, "Impossible de modifier les inscriptions d'une formation terminée ou annulée."};

    QSqlQuery del;
    del.prepare("DELETE FROM inscription WHERE id=?");
    del.addBindValue(idInscription); del.exec();

    QString msg = "Désinscription effectuée.";
    if (etaitInscrit) {   // promotion automatique depuis la liste d'attente
        QSqlQuery w;
        w.prepare("SELECT id, id_agent FROM inscription WHERE id_formation=? AND etat='attente' ORDER BY id");
        w.addBindValue(idF); w.exec();
        while (w.next()) {
            const int idAgent = w.value(1).toInt();
            if (!conflit(idAgent, idF).isEmpty()) continue;
            QSqlQuery up;
            up.prepare("UPDATE inscription SET etat='inscrit' WHERE id=?");
            up.addBindValue(w.value(0)); up.exec();
            msg += QString(" %1 passe de la liste d'attente à inscrit.").arg(nomAgent(idAgent));
            break;
        }
    }
    return {true, msg};
}

Retour Inscription::marquer(int idInscription, int presence, const QString &resultat)
{
    QSqlQuery q;
    q.prepare("SELECT id_formation, id_agent, etat, presence FROM inscription WHERE id=?");
    q.addBindValue(idInscription); q.exec();
    if (!q.next()) return {false, "Inscription introuvable."};
    const int idF = q.value(0).toInt(), idA = q.value(1).toInt();
    if (q.value(2).toString() != "inscrit") return {false, "Cet agent est en liste d'attente."};
    const int presenceActuelle = q.value(3).isNull() ? -1 : q.value(3).toInt();

    Formation f; Formation::charger(idF, f);
    if (f.statut == "planifiee") return {false, "La formation n'a pas encore commencé."};
    if (f.statut == "annulee")   return {false, "Cette formation est annulée."};

    const int presenceFinale = presence >= 0 ? presence : presenceActuelle;
    if (!resultat.isEmpty() && resultat != "en_attente" && presenceFinale != 1)
        return {false, "Le résultat ne peut être saisi que pour un agent présent."};

    QSqlQuery u;
    if (presence >= 0) {
        u.prepare("UPDATE inscription SET presence=?, resultat=CASE WHEN ?=0 THEN 'en_attente' ELSE resultat END WHERE id=?");
        u.addBindValue(presence); u.addBindValue(presence); u.addBindValue(idInscription); u.exec();
    }
    if (!resultat.isEmpty()) {
        u.prepare("UPDATE inscription SET resultat=? WHERE id=?");
        u.addBindValue(resultat); u.addBindValue(idInscription); u.exec();
    }

    // Mise à jour de la certification (module Gestion du personnel) après réussite
    if (resultat == "reussi" && f.certification) {
        const QString obtention = f.dateFin.toString(Qt::ISODate);
        const QString echeance  = f.dateFin.addYears(2).toString(Qt::ISODate);
        QSqlQuery c;
        c.prepare("SELECT id FROM certification WHERE id_agent=? AND nom=?");
        c.addBindValue(idA); c.addBindValue(f.type); c.exec();
        QSqlQuery w;
        if (c.next()) {
            w.prepare("UPDATE certification SET date_obtention=?, date_echeance=? WHERE id=?");
            w.addBindValue(obtention); w.addBindValue(echeance); w.addBindValue(c.value(0));
        } else {
            w.prepare("INSERT INTO certification(id_agent,nom,date_obtention,date_echeance) VALUES(?,?,?,?)");
            w.addBindValue(idA); w.addBindValue(f.type); w.addBindValue(obtention); w.addBindValue(echeance);
        }
        w.exec();
        return {true, "Résultat enregistré ; certification mise à jour (échéance " + QDate::fromString(echeance, Qt::ISODate).toString("dd/MM/yyyy") + ")."};
    }
    return {true, "Enregistré."};
}

QList<Recommandation> Inscription::recommandations(int idFormation)
{
    QList<Recommandation> res;
    Formation f;
    if (!Formation::charger(idFormation, f)) return res;
    QSqlQuery q;
    q.prepare("SELECT a.id, a.nom || ' ' || a.prenom, c.date_echeance FROM certification c"
              " JOIN agent a ON a.id = c.id_agent"
              " WHERE c.nom = :t AND c.date_echeance <= :lim"
              " AND a.id NOT IN (SELECT id_agent FROM inscription WHERE id_formation = :f)"
              " ORDER BY c.date_echeance");
    q.bindValue(":t", f.type);
    q.bindValue(":lim", QDate::currentDate().addDays(90).toString(Qt::ISODate));
    q.bindValue(":f", idFormation);
    q.exec();
    while (q.next())
        res.append({q.value(0).toInt(), q.value(1).toString(),
                    QDate::fromString(q.value(2).toString(), Qt::ISODate)});
    return res;
}
