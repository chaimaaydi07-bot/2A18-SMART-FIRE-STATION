#include "formation.h"
#include <QSqlDatabase>
#include <QSqlError>
#include <QVariant>
#include <QStringList>

QStringList Formation::types()
{
    return {"Secourisme", "Lutte contre l'incendie", "Sauvetage",
            "Matières dangereuses", "Conduite d'engins"};
}

QString Formation::libelleStatut(const QString &code)
{
    if (code == "planifiee") return "Planifiée";
    if (code == "en_cours")  return "En cours";
    if (code == "terminee")  return "Terminée";
    if (code == "annulee")   return "Annulée";
    return code;
}

QString Formation::statutAuto(const QDate &debut, const QDate &fin)
{
    const QDate aujourdHui = QDate::currentDate();
    if (aujourdHui < debut) return "planifiee";
    if (aujourdHui > fin) return "terminee";
    return "en_cours";
}

void Formation::majStatuts()
{
    if (!QSqlDatabase::database().isOpen()) return;
    QSqlQuery q;
    q.prepare("UPDATE FORMATION SET STATUT = CASE "
              "WHEN DATE_DEBUT > :debut THEN 'planifiee' "
              "WHEN DATE_FIN < :fin THEN 'terminee' ELSE 'en_cours' END "
              "WHERE STATUT <> 'annulee'");
    q.bindValue(":debut", QDate::currentDate());
    q.bindValue(":fin", QDate::currentDate());
    q.exec();
}

QString Formation::valider() const
{
    if (id < 0) return "L'identifiant de la formation est invalide.";
    if (intitule.trimmed().size() < 3 || intitule.trimmed().size() > 100)
        return "L'intitulé doit contenir entre 3 et 100 caractères.";
    if (type.trimmed().isEmpty() || type.trimmed().size() > 50)
        return "Le type est obligatoire (50 caractères maximum).";
    if (formateur.trimmed().size() > 100) return "Le nom du formateur ne peut pas dépasser 100 caractères.";
    if (lieu.trimmed().size() > 100) return "Le lieu ne peut pas dépasser 100 caractères.";
    if (!dateDebut.isValid() || !dateFin.isValid()) return "Les dates sont obligatoires.";
    if (dateFin < dateDebut) return "La date de fin doit être égale ou postérieure à la date de début.";
    if (duree <= 0 || duree > 99999) return "La durée doit être comprise entre 1 et 99 999 heures.";
    if (capacite <= 0 || capacite > 99999) return "La capacité doit être comprise entre 1 et 99 999 places.";
    if (!statut.isEmpty() && statut != "annulee") return "Le statut de la formation est invalide.";
    return {};
}

bool Formation::ajouter(QString *erreur)
{
    auto echouer = [erreur](const QString &message) { if (erreur) *erreur = message; return false; };
    const QString validation = valider();
    if (!validation.isEmpty()) return echouer(validation);
    if (!QSqlDatabase::database().isOpen()) return echouer("La connexion Oracle n'est pas ouverte.");

    QSqlQuery doublon;
    doublon.prepare("SELECT COUNT(*) FROM FORMATION WHERE LOWER(TITRE)=LOWER(?) AND DATE_DEBUT=?");
    doublon.addBindValue(intitule.trimmed());
    doublon.addBindValue(dateDebut);
    if (!doublon.exec()) return echouer(doublon.lastError().text());
    if (doublon.next() && doublon.value(0).toInt() > 0)
        return echouer("Une formation avec le même intitulé et la même date de début existe déjà.");

    QSqlQuery sequence;
    if (!sequence.exec("SELECT SEQ_FORMATION.NEXTVAL FROM DUAL") || !sequence.next())
        return echouer(sequence.lastError().text().isEmpty()
                       ? "La séquence SEQ_FORMATION est absente. Exécutez le script du modèle Oracle."
                       : sequence.lastError().text());
    id = sequence.value(0).toInt();
    statut = statut == "annulee" ? "annulee" : statutAuto(dateDebut, dateFin);

    QSqlQuery q;
    q.prepare("INSERT INTO FORMATION(ID,TITRE,TYPE,FORMATEUR,DATE_DEBUT,DATE_FIN,DUREE,LIEU,NB_PLACES,CERTIFICATION,STATUT) "
              "VALUES(?,?,?,?,?,?,?,?,?,?,?)");
    q.addBindValue(id); q.addBindValue(intitule.trimmed()); q.addBindValue(type.trimmed());
    q.addBindValue(formateur.trimmed()); q.addBindValue(dateDebut); q.addBindValue(dateFin);
    q.addBindValue(duree); q.addBindValue(lieu.trimmed()); q.addBindValue(capacite);
    q.addBindValue(certification ? 1 : 0); q.addBindValue(statut);
    if (!q.exec()) return echouer(q.lastError().text());
    if (erreur) erreur->clear();
    return true;
}

bool Formation::modifier(QString *erreur)
{
    auto echouer = [erreur](const QString &message) { if (erreur) *erreur = message; return false; };
    const QString validation = valider();
    if (!validation.isEmpty()) return echouer(validation);
    if (id <= 0) return echouer("Sélectionnez une formation à modifier.");
    if (!QSqlDatabase::database().isOpen()) return echouer("La connexion Oracle n'est pas ouverte.");
    const int nombreInscrits = inscrits(id);
    if (capacite < nombreInscrits)
        return echouer(QString("La capacité ne peut pas être inférieure au nombre d'inscrits (%1).").arg(nombreInscrits));

    QSqlQuery doublon;
    doublon.prepare("SELECT COUNT(*) FROM FORMATION WHERE LOWER(TITRE)=LOWER(?) AND DATE_DEBUT=? AND ID<>?");
    doublon.addBindValue(intitule.trimmed()); doublon.addBindValue(dateDebut); doublon.addBindValue(id);
    if (!doublon.exec()) return echouer(doublon.lastError().text());
    if (doublon.next() && doublon.value(0).toInt() > 0)
        return echouer("Une autre formation avec le même intitulé et la même date de début existe déjà.");

    statut = statut == "annulee" ? "annulee" : statutAuto(dateDebut, dateFin);
    QSqlQuery q;
    q.prepare("UPDATE FORMATION SET TITRE=?,TYPE=?,FORMATEUR=?,DATE_DEBUT=?,DATE_FIN=?,DUREE=?,"
              "LIEU=?,NB_PLACES=?,CERTIFICATION=?,STATUT=? WHERE ID=?");
    q.addBindValue(intitule.trimmed()); q.addBindValue(type.trimmed()); q.addBindValue(formateur.trimmed());
    q.addBindValue(dateDebut); q.addBindValue(dateFin); q.addBindValue(duree);
    q.addBindValue(lieu.trimmed()); q.addBindValue(capacite); q.addBindValue(certification ? 1 : 0);
    q.addBindValue(statut); q.addBindValue(id);
    if (!q.exec()) return echouer(q.lastError().text());
    if (q.numRowsAffected() == 0) return echouer("Formation introuvable.");
    if (erreur) erreur->clear();
    return true;
}

bool Formation::supprimer(int idFormation)
{
    if (idFormation <= 0 || !QSqlDatabase::database().isOpen()) return false;
    QSqlQuery q;
    q.prepare("DELETE FROM FORMATION WHERE ID=?");
    q.addBindValue(idFormation);
    return q.exec() && q.numRowsAffected() > 0;
}

bool Formation::charger(int idFormation, Formation &f)
{
    if (idFormation <= 0 || !QSqlDatabase::database().isOpen()) return false;
    QSqlQuery q;
    q.prepare("SELECT TITRE,TYPE,FORMATEUR,TO_CHAR(DATE_DEBUT,'YYYY-MM-DD'),TO_CHAR(DATE_FIN,'YYYY-MM-DD'),"
              "DUREE,LIEU,NB_PLACES,CERTIFICATION,STATUT FROM FORMATION WHERE ID=?");
    q.addBindValue(idFormation);
    if (!q.exec() || !q.next()) return false;
    f.id = idFormation; f.intitule = q.value(0).toString(); f.type = q.value(1).toString();
    f.formateur = q.value(2).toString();
    f.dateDebut = QDate::fromString(q.value(3).toString(), Qt::ISODate);
    f.dateFin = QDate::fromString(q.value(4).toString(), Qt::ISODate);
    f.duree = q.value(5).toInt(); f.lieu = q.value(6).toString(); f.capacite = q.value(7).toInt();
    f.certification = q.value(8).toInt() != 0; f.statut = q.value(9).toString();
    return true;
}

int Formation::inscrits(int idFormation)
{
    if (idFormation <= 0 || !QSqlDatabase::database().isOpen()) return 0;
    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM INSCRIPTION WHERE ID_FORMATION=? AND ETAT='inscrit'");
    q.addBindValue(idFormation);
    return q.exec() && q.next() ? q.value(0).toInt() : 0;
}

int Formation::placesRestantes(int idFormation)
{
    Formation f;
    return charger(idFormation, f) ? f.capacite - inscrits(idFormation) : 0;
}

QSqlQuery Formation::liste(const QString &recherche, const QString &colonneTri, bool croissant)
{
    static const QStringList colonnesAutorisees = {"date_debut", "type", "statut", "places", "intitule", "id"};
    const QString colonne = colonnesAutorisees.contains(colonneTri) ? colonneTri : "date_debut";
    QSqlQuery q;
    q.prepare(QString(
        "SELECT * FROM ("
        " SELECT f.ID AS ID, f.TITRE AS INTITULE, f.TYPE AS TYPE, f.FORMATEUR AS FORMATEUR,"
        " TO_CHAR(f.DATE_DEBUT,'YYYY-MM-DD') AS DATE_DEBUT, TO_CHAR(f.DATE_FIN,'YYYY-MM-DD') AS DATE_FIN,"
        " f.DUREE AS DUREE, f.LIEU AS LIEU, f.NB_PLACES AS CAPACITE,"
        " (SELECT COUNT(*) FROM INSCRIPTION i WHERE i.ID_FORMATION=f.ID AND i.ETAT='inscrit') AS INSCRITS,"
        " f.CERTIFICATION AS CERTIFICATION, f.STATUT AS STATUT,"
        " f.NB_PLACES-(SELECT COUNT(*) FROM INSCRIPTION i WHERE i.ID_FORMATION=f.ID AND i.ETAT='inscrit') AS PLACES"
        " FROM FORMATION f)"
        " WHERE LOWER(INTITULE) LIKE LOWER(?) OR LOWER(TYPE) LIKE LOWER(?)"
        " OR LOWER(NVL(FORMATEUR,' ')) LIKE LOWER(?) OR LOWER(REPLACE(STATUT,'_',' ')) LIKE LOWER(?)"
        " ORDER BY %1 %2, ID").arg(colonne, croissant ? "ASC" : "DESC"));
    const QString motif = "%" + recherche.trimmed() + "%";
    for (int i = 0; i < 4; ++i) q.addBindValue(motif);
    q.exec();
    return q;
}

QSqlQuery Formation::statistiques()
{
    QSqlQuery q;
    q.prepare("SELECT f.TYPE, COUNT(DISTINCT f.ID), "
              "NVL(SUM(CASE WHEN i.ETAT='inscrit' THEN 1 ELSE 0 END),0), "
              "NVL(SUM(CASE WHEN i.ETAT='inscrit' AND i.PRESENCE=1 THEN 1 ELSE 0 END),0), "
              "NVL(SUM(CASE WHEN i.ETAT='inscrit' AND i.PRESENCE=1 AND i.RESULTAT='reussi' THEN 1 ELSE 0 END),0) "
              "FROM FORMATION f LEFT JOIN INSCRIPTION i ON i.ID_FORMATION=f.ID "
              "WHERE f.STATUT<>'annulee' GROUP BY f.TYPE ORDER BY f.TYPE");
    q.exec();
    return q;
}

QSqlQuery Formation::afficher()
{
    return liste(QString(), "date_debut", true);
}
