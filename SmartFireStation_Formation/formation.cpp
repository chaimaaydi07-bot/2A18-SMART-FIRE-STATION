#include "formation.h"
#include <utility>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

Formation::Formation()
    : id(0), duree(0), nbPlaces(0) {}

Formation::Formation(int id, QString titre, QString type, QDate debut, QDate fin,
                     int duree, QString formateur, QString lieu, int places,
                     QString niveau, QString statut)
    : id(id), titre(titre), type(type), formateur(formateur), lieu(lieu),
      niveau(niveau), statut(statut), dateDebut(debut), dateFin(fin),
      duree(duree), nbPlaces(places) {}

bool Formation::ajouter()
{
    QSqlQuery q;
    q.prepare("INSERT INTO FORMATION (ID, TITRE, TYPE, DATE_DEBUT, DATE_FIN, DUREE, "
              "FORMATEUR, LIEU, NB_PLACES, NIVEAU, STATUT) "
              "VALUES (:id, :titre, :type, TO_DATE(:debut,'YYYY-MM-DD'), TO_DATE(:fin,'YYYY-MM-DD'), :duree, :formateur, :lieu, "
              ":places, :niveau, :statut)");
    q.bindValue(":id", id);
    q.bindValue(":titre", titre);
    q.bindValue(":type", type);
    q.bindValue(":debut", dateDebut.toString("yyyy-MM-dd"));
    q.bindValue(":fin", dateFin.toString("yyyy-MM-dd"));
    q.bindValue(":duree", duree);
    q.bindValue(":formateur", formateur);
    q.bindValue(":lieu", lieu);
    q.bindValue(":places", nbPlaces);
    q.bindValue(":niveau", niveau);
    q.bindValue(":statut", statut);
    if (!q.exec()) { qDebug() << q.lastError().text(); return false; }
    return true;
}

bool Formation::modifier()
{
    QSqlQuery q;
    q.prepare("UPDATE FORMATION SET TITRE=:titre, TYPE=:type, DATE_DEBUT=TO_DATE(:debut,'YYYY-MM-DD'), "
              "DATE_FIN=TO_DATE(:fin,'YYYY-MM-DD'), DUREE=:duree, FORMATEUR=:formateur, LIEU=:lieu, "
              "NB_PLACES=:places, NIVEAU=:niveau, STATUT=:statut WHERE ID=:id");
    q.bindValue(":id", id);
    q.bindValue(":titre", titre);
    q.bindValue(":type", type);
    q.bindValue(":debut", dateDebut.toString("yyyy-MM-dd"));
    q.bindValue(":fin", dateFin.toString("yyyy-MM-dd"));
    q.bindValue(":duree", duree);
    q.bindValue(":formateur", formateur);
    q.bindValue(":lieu", lieu);
    q.bindValue(":places", nbPlaces);
    q.bindValue(":niveau", niveau);
    q.bindValue(":statut", statut);
    if (!q.exec()) { qDebug() << q.lastError().text(); return false; }
    return q.numRowsAffected() > 0;
}

bool Formation::supprimer(int idSup)
{
    QSqlQuery q;
    q.prepare("DELETE FROM INSCRIPTION WHERE ID_FORMATION=:id");
    q.bindValue(":id", idSup);
    q.exec();
    q.prepare("DELETE FROM FORMATION WHERE ID=:id");
    q.bindValue(":id", idSup);
    if (!q.exec()) { qDebug() << q.lastError().text(); return false; }
    return q.numRowsAffected() > 0;
}

QSqlQueryModel* Formation::afficher()
{
    QSqlQueryModel *m = new QSqlQueryModel();
    m->setQuery("SELECT ID, TITRE, TYPE, TO_CHAR(DATE_DEBUT,'YYYY-MM-DD') AS DATE_DEBUT, TO_CHAR(DATE_FIN,'YYYY-MM-DD') AS DATE_FIN, DUREE, FORMATEUR, "
                "LIEU, NB_PLACES, NIVEAU, STATUT FROM FORMATION");
    m->setHeaderData(0, Qt::Horizontal, QObject::tr("ID"));
    m->setHeaderData(1, Qt::Horizontal, QObject::tr("Titre"));
    m->setHeaderData(2, Qt::Horizontal, QObject::tr("Type"));
    m->setHeaderData(3, Qt::Horizontal, QObject::tr("Début"));
    m->setHeaderData(4, Qt::Horizontal, QObject::tr("Fin"));
    m->setHeaderData(5, Qt::Horizontal, QObject::tr("Durée (h)"));
    m->setHeaderData(6, Qt::Horizontal, QObject::tr("Formateur"));
    m->setHeaderData(7, Qt::Horizontal, QObject::tr("Lieu"));
    m->setHeaderData(8, Qt::Horizontal, QObject::tr("Places"));
    m->setHeaderData(9, Qt::Horizontal, QObject::tr("Niveau"));
    m->setHeaderData(10, Qt::Horizontal, QObject::tr("Statut"));
    return m;
}

QSqlQueryModel* Formation::rechercher(const QString &critere)
{
    QSqlQueryModel *m = new QSqlQueryModel();
    QSqlQuery q;
    q.prepare("SELECT ID, TITRE, TYPE, TO_CHAR(DATE_DEBUT,'YYYY-MM-DD') AS DATE_DEBUT, TO_CHAR(DATE_FIN,'YYYY-MM-DD') AS DATE_FIN, DUREE, FORMATEUR, "
              "LIEU, NB_PLACES, NIVEAU, STATUT FROM FORMATION "
              "WHERE TO_CHAR(ID) LIKE :c OR TITRE LIKE :c OR TYPE LIKE :c "
              "OR FORMATEUR LIKE :c OR LIEU LIKE :c OR STATUT LIKE :c");
    q.bindValue(":c", "%" + critere + "%");
    q.exec();
    m->setQuery(std::move(q));
    m->setHeaderData(0, Qt::Horizontal, QObject::tr("ID"));
    m->setHeaderData(1, Qt::Horizontal, QObject::tr("Titre"));
    m->setHeaderData(2, Qt::Horizontal, QObject::tr("Type"));
    m->setHeaderData(3, Qt::Horizontal, QObject::tr("Début"));
    m->setHeaderData(4, Qt::Horizontal, QObject::tr("Fin"));
    m->setHeaderData(5, Qt::Horizontal, QObject::tr("Durée (h)"));
    m->setHeaderData(6, Qt::Horizontal, QObject::tr("Formateur"));
    m->setHeaderData(7, Qt::Horizontal, QObject::tr("Lieu"));
    m->setHeaderData(8, Qt::Horizontal, QObject::tr("Places"));
    m->setHeaderData(9, Qt::Horizontal, QObject::tr("Niveau"));
    m->setHeaderData(10, Qt::Horizontal, QObject::tr("Statut"));
    return m;
}

QSqlQueryModel* Formation::trier(const QString &colonne, bool croissant)
{
    // La colonne vient d'une liste fermee (pas de saisie libre) : on la valide quand meme.
    static const QStringList autorisees =
        {"ID","TITRE","TYPE","DATE_DEBUT","DATE_FIN","DUREE","FORMATEUR","LIEU","NB_PLACES","NIVEAU","STATUT"};
    QString col = autorisees.contains(colonne) ? colonne : "ID";

    QSqlQueryModel *m = new QSqlQueryModel();
    m->setQuery("SELECT ID, TITRE, TYPE, TO_CHAR(DATE_DEBUT,'YYYY-MM-DD') AS DATE_DEBUT, TO_CHAR(DATE_FIN,'YYYY-MM-DD') AS DATE_FIN, DUREE, FORMATEUR, "
                "LIEU, NB_PLACES, NIVEAU, STATUT FROM FORMATION ORDER BY " + col +
                (croissant ? " ASC" : " DESC"));
    m->setHeaderData(0, Qt::Horizontal, QObject::tr("ID"));
    m->setHeaderData(1, Qt::Horizontal, QObject::tr("Titre"));
    m->setHeaderData(2, Qt::Horizontal, QObject::tr("Type"));
    m->setHeaderData(3, Qt::Horizontal, QObject::tr("Début"));
    m->setHeaderData(4, Qt::Horizontal, QObject::tr("Fin"));
    m->setHeaderData(5, Qt::Horizontal, QObject::tr("Durée (h)"));
    m->setHeaderData(6, Qt::Horizontal, QObject::tr("Formateur"));
    m->setHeaderData(7, Qt::Horizontal, QObject::tr("Lieu"));
    m->setHeaderData(8, Qt::Horizontal, QObject::tr("Places"));
    m->setHeaderData(9, Qt::Horizontal, QObject::tr("Niveau"));
    m->setHeaderData(10, Qt::Horizontal, QObject::tr("Statut"));
    return m;
}

QMap<QString,int> Formation::statistiquesParType()
{
    QMap<QString,int> res;
    QSqlQuery q("SELECT TYPE, COUNT(*) FROM FORMATION GROUP BY TYPE");
    while (q.next())
        res.insert(q.value(0).toString(), q.value(1).toInt());
    return res;
}

bool Formation::existe(int idTest)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM FORMATION WHERE ID=:id");
    q.bindValue(":id", idTest);
    return q.exec() && q.next() && q.value(0).toInt() > 0;
}

int Formation::nbInscrits(int idFormation)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM INSCRIPTION WHERE ID_FORMATION=:id");
    q.bindValue(":id", idFormation);
    return (q.exec() && q.next()) ? q.value(0).toInt() : 0;
}

// Enregistre la presence d'un agent a une formation (inscription deja existante)
bool Formation::enregistrerPresence(int idFormation, int idPompier)
{
    if (!existe(idFormation)) return false;

    QSqlQuery p;
    p.prepare("SELECT NB_PLACES FROM FORMATION WHERE ID=:id");
    p.bindValue(":id", idFormation);
    if (!p.exec() || !p.next()) return false;
    if (nbInscrits(idFormation) >= p.value(0).toInt()) return false; // formation complete

    QSqlQuery q;
    q.prepare("INSERT INTO INSCRIPTION (ID_FORMATION, ID_POMPIER, DATE_PRESENCE) "
              "VALUES (:f, :p, TO_DATE(:d,'YYYY-MM-DD'))");
    q.bindValue(":f", idFormation);
    q.bindValue(":p", idPompier);
    q.bindValue(":d", QDate::currentDate().toString("yyyy-MM-dd"));
    return q.exec();
}

// Recherche + tri combines (utilise par l'interface)
QSqlQueryModel* Formation::lister(const QString &critere, const QString &colonne, bool croissant)
{
    static const QStringList autorisees =
        {"ID","TITRE","TYPE","DATE_DEBUT","DATE_FIN","DUREE","FORMATEUR","LIEU","NB_PLACES","NIVEAU","STATUT"};
    QString col = autorisees.contains(colonne) ? colonne : "ID";

    QString sql = "SELECT ID, TITRE, TYPE, TO_CHAR(DATE_DEBUT,'YYYY-MM-DD') AS DATE_DEBUT, TO_CHAR(DATE_FIN,'YYYY-MM-DD') AS DATE_FIN, DUREE, FORMATEUR, LIEU, "
                  "NB_PLACES, NIVEAU, STATUT FROM FORMATION";
    bool filtre = !critere.trimmed().isEmpty();
    if (filtre)
        sql += " WHERE TO_CHAR(ID) LIKE :c OR TITRE LIKE :c OR TYPE LIKE :c "
               "OR FORMATEUR LIKE :c OR LIEU LIKE :c OR STATUT LIKE :c";
    sql += " ORDER BY " + col + (croissant ? " ASC" : " DESC");

    QSqlQuery q;
    q.prepare(sql);
    if (filtre) q.bindValue(":c", "%" + critere.trimmed() + "%");
    q.exec();
    QSqlQueryModel *m = new QSqlQueryModel();
    m->setQuery(std::move(q));
    return m;
}
// Formations planifiees qui commencent dans les "jours" prochains jours
QSqlQueryModel* Formation::prochaines(int jours)
{
    QSqlQuery q;
    q.prepare("SELECT ID, TITRE, TYPE, LIEU, TO_CHAR(DATE_DEBUT,'YYYY-MM-DD') AS DATE_DEBUT, FORMATEUR "
              "FROM FORMATION WHERE STATUT=:statut "
              "AND DATE_DEBUT BETWEEN TO_DATE(:a,'YYYY-MM-DD') AND TO_DATE(:b,'YYYY-MM-DD') "
              "ORDER BY DATE_DEBUT");
    q.bindValue(":statut", "Planifiée");
    q.bindValue(":a", QDate::currentDate().toString("yyyy-MM-dd"));
    q.bindValue(":b", QDate::currentDate().addDays(jours).toString("yyyy-MM-dd"));
    q.exec();
    QSqlQueryModel *m = new QSqlQueryModel();
    m->setQuery(std::move(q));
    return m;
}
// Charge dans l'objet courant la formation dont l'identifiant est donne
bool Formation::recuperer(int idRech)
{
    QSqlQuery q;
    q.prepare("SELECT TITRE, TYPE, TO_CHAR(DATE_DEBUT,'YYYY-MM-DD'), TO_CHAR(DATE_FIN,'YYYY-MM-DD'), "
              "DUREE, FORMATEUR, LIEU, NB_PLACES, NIVEAU, STATUT FROM FORMATION WHERE ID=:id");
    q.bindValue(":id", idRech);
    if (!q.exec() || !q.next()) return false;
    id = idRech;
    titre = q.value(0).toString();
    type = q.value(1).toString();
    dateDebut = QDate::fromString(q.value(2).toString(), "yyyy-MM-dd");
    dateFin = QDate::fromString(q.value(3).toString(), "yyyy-MM-dd");
    duree = q.value(4).toInt();
    formateur = q.value(5).toString();
    lieu = q.value(6).toString();
    nbPlaces = q.value(7).toInt();
    niveau = q.value(8).toString();
    statut = q.value(9).toString();
    return true;
}
