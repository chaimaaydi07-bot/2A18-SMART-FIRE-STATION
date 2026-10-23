#include "formation.h"
#include <QSqlError>
#include <QVariant>

QStringList Formation::types()
{
    return {"Secourisme", "Lutte contre l'incendie", "Sauvetage",
            "Matières dangereuses", "Conduite d'engins"};
}

QString Formation::libelleStatut(const QString &c)
{
    if (c == "planifiee") return "Planifiée";
    if (c == "en_cours")  return "En cours";
    if (c == "terminee")  return "Terminée";
    if (c == "annulee")   return "Annulée";
    return c;
}

QString Formation::statutAuto(const QDate &debut, const QDate &fin)
{
    const QDate t = QDate::currentDate();
    if (t < debut) return "planifiee";
    if (t > fin)   return "terminee";
    return "en_cours";
}

void Formation::majStatuts()
{
    QSqlQuery q;
    q.prepare("UPDATE formation SET statut = CASE"
              " WHEN :t < date_debut THEN 'planifiee'"
              " WHEN :t > date_fin THEN 'terminee' ELSE 'en_cours' END"
              " WHERE statut <> 'annulee'");
    q.bindValue(":t", QDate::currentDate().toString(Qt::ISODate));
    q.exec();
}

QString Formation::valider() const
{
    if (intitule.trimmed().isEmpty()) return "L'intitulé est obligatoire.";
    if (type.trimmed().isEmpty())     return "Le type est obligatoire.";
    if (!dateDebut.isValid() || !dateFin.isValid()) return "Les dates sont obligatoires.";
    if (dateFin < dateDebut) return "La date de fin doit être postérieure ou égale à la date de début.";
    if (duree <= 0)     return "La durée doit être strictement positive.";
    if (capacite <= 0)  return "La capacité doit être strictement positive.";
    return QString();
}

bool Formation::ajouter(QString *erreur)
{
    const QString e = valider();
    if (!e.isEmpty()) { *erreur = e; return false; }

    QSqlQuery d;   // prévention des doublons
    d.prepare("SELECT COUNT(*) FROM formation WHERE lower(intitule)=lower(?) AND date_debut=?");
    d.addBindValue(intitule.trimmed()); d.addBindValue(dateDebut.toString(Qt::ISODate)); d.exec();
    if (d.next() && d.value(0).toInt() > 0) {
        *erreur = "Une formation avec le même intitulé et la même date de début existe déjà.";
        return false;
    }
    if (statut != "annulee") statut = statutAuto(dateDebut, dateFin);

    QSqlQuery q;
    q.prepare("INSERT INTO formation(intitule,type,formateur,date_debut,date_fin,duree,lieu,capacite,certification,statut)"
              " VALUES(?,?,?,?,?,?,?,?,?,?)");
    q.addBindValue(intitule.trimmed()); q.addBindValue(type); q.addBindValue(formateur.trimmed());
    q.addBindValue(dateDebut.toString(Qt::ISODate)); q.addBindValue(dateFin.toString(Qt::ISODate));
    q.addBindValue(duree); q.addBindValue(lieu.trimmed()); q.addBindValue(capacite);
    q.addBindValue(certification ? 1 : 0); q.addBindValue(statut);
    if (!q.exec()) { *erreur = q.lastError().text(); return false; }
    id = q.lastInsertId().toInt();
    return true;
}

bool Formation::modifier(QString *erreur)
{
    const QString e = valider();
    if (!e.isEmpty()) { *erreur = e; return false; }
    if (capacite < inscrits(id)) {
        *erreur = QString("La capacité ne peut pas être inférieure au nombre d'inscrits (%1).").arg(inscrits(id));
        return false;
    }
    QSqlQuery d;
    d.prepare("SELECT COUNT(*) FROM formation WHERE lower(intitule)=lower(?) AND date_debut=? AND id<>?");
    d.addBindValue(intitule.trimmed()); d.addBindValue(dateDebut.toString(Qt::ISODate)); d.addBindValue(id); d.exec();
    if (d.next() && d.value(0).toInt() > 0) {
        *erreur = "Une autre formation avec le même intitulé et la même date de début existe déjà.";
        return false;
    }
    if (statut != "annulee") statut = statutAuto(dateDebut, dateFin);

    QSqlQuery q;
    q.prepare("UPDATE formation SET intitule=?,type=?,formateur=?,date_debut=?,date_fin=?,duree=?,"
              "lieu=?,capacite=?,certification=?,statut=? WHERE id=?");
    q.addBindValue(intitule.trimmed()); q.addBindValue(type); q.addBindValue(formateur.trimmed());
    q.addBindValue(dateDebut.toString(Qt::ISODate)); q.addBindValue(dateFin.toString(Qt::ISODate));
    q.addBindValue(duree); q.addBindValue(lieu.trimmed()); q.addBindValue(capacite);
    q.addBindValue(certification ? 1 : 0); q.addBindValue(statut); q.addBindValue(id);
    if (!q.exec()) { *erreur = q.lastError().text(); return false; }
    return true;
}

bool Formation::supprimer(int id)
{
    QSqlQuery q;
    q.prepare("DELETE FROM formation WHERE id=?");
    q.addBindValue(id);
    return q.exec();
}

bool Formation::charger(int id, Formation &f)
{
    QSqlQuery q;
    q.prepare("SELECT intitule,type,formateur,date_debut,date_fin,duree,lieu,capacite,certification,statut"
              " FROM formation WHERE id=?");
    q.addBindValue(id);
    if (!q.exec() || !q.next()) return false;
    f.id = id; f.intitule = q.value(0).toString(); f.type = q.value(1).toString();
    f.formateur = q.value(2).toString();
    f.dateDebut = QDate::fromString(q.value(3).toString(), Qt::ISODate);
    f.dateFin = QDate::fromString(q.value(4).toString(), Qt::ISODate);
    f.duree = q.value(5).toInt(); f.lieu = q.value(6).toString(); f.capacite = q.value(7).toInt();
    f.certification = q.value(8).toInt() != 0; f.statut = q.value(9).toString();
    return true;
}

int Formation::inscrits(int idFormation)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM inscription WHERE id_formation=? AND etat='inscrit'");
    q.addBindValue(idFormation); q.exec();
    return q.next() ? q.value(0).toInt() : 0;
}

int Formation::placesRestantes(int idFormation)
{
    Formation f;
    if (!charger(idFormation, f)) return 0;
    return f.capacite - inscrits(idFormation);
}

// Recherche et tri réalisés par la base ; colonne de tri validée par liste blanche.
QSqlQuery Formation::liste(const QString &recherche, const QString &colonneTri, bool croissant)
{
    static const QStringList autorisees = {"date_debut", "type", "statut", "places", "intitule", "id"};
    const QString col = autorisees.contains(colonneTri) ? colonneTri : "date_debut";

    QSqlQuery q;
    q.prepare(QString(
        "SELECT * FROM ("
        " SELECT f.id AS id, f.intitule AS intitule, f.type AS type, f.formateur AS formateur,"
        "  f.date_debut AS date_debut, f.date_fin AS date_fin, f.duree AS duree, f.lieu AS lieu,"
        "  f.capacite AS capacite,"
        "  (SELECT COUNT(*) FROM inscription i WHERE i.id_formation=f.id AND i.etat='inscrit') AS inscrits,"
        "  f.certification AS certification, f.statut AS statut,"
        "  f.capacite - (SELECT COUNT(*) FROM inscription i WHERE i.id_formation=f.id AND i.etat='inscrit') AS places"
        " FROM formation f)"
        " WHERE intitule LIKE :r OR type LIKE :r OR formateur LIKE :r OR replace(statut,'_',' ') LIKE :r"
        " ORDER BY %1 %2, id").arg(col, croissant ? "ASC" : "DESC"));
    q.bindValue(":r", "%" + recherche.trimmed() + "%");
    q.exec();
    return q;
}
