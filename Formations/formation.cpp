#include "formation.h"
#include <utility>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QSqlDatabase>

static QString g_erreur;
QString Formation::derniereErreur() { return g_erreur; }

// Execute une requete d'ecriture : verifie la connexion, trace la commande SQL, garde l'erreur
static bool executer(QSqlQuery &q)
{
    g_erreur.clear();
    if (!QSqlDatabase::database().isOpen()) {
        g_erreur = "Connexion Oracle non ouverte.";
        qDebug() << g_erreur;
        return false;
    }
    qDebug().noquote() << "[SQL]" << q.lastQuery();
    // QSqlQuery(sql) executes immediately. Do not execute an already active
    // ODBC statement a second time: some Oracle ODBC drivers report S1010.
    if (q.isActive()) return true;
    if (q.lastError().isValid() || !q.exec()) {
        g_erreur = q.lastError().text();
        qDebug().noquote() << "[SQL ERREUR]" << g_erreur;
        return false;
    }
    return true;
}

static QSqlQueryModel* modelePour(QSqlQuery &q)
{
    QSqlQueryModel *m = new QSqlQueryModel();
    if (!QSqlDatabase::database().isOpen()) {
        g_erreur = "Connexion Oracle non ouverte.";
        m->setQuery(QString());
        return m;
    }
    // Queries constructed with SQL text may already have executed.
    if (!q.isActive() && (q.lastError().isValid() || !q.exec())) {
        g_erreur = q.lastError().text();
        qDebug().noquote() << "[SQL ERREUR]" << g_erreur;
        m->setQuery(QString());
        return m;
    } else g_erreur.clear();
    m->setQuery(std::move(q));
    return m;
}

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
    QSqlQuery sequence("SELECT SEQ_FORMATION_ID.NEXTVAL FROM DUAL");
    if (!executer(sequence) || !sequence.next()) return false;
    const int nouvelId = sequence.value(0).toInt();

    QSqlQuery q;
    q.prepare("INSERT INTO FORMATION (ID, TITRE, TYPE, DATE_DEBUT, DATE_FIN, DUREE, "
              "FORMATEUR, LIEU, NB_PLACES, NIVEAU, STATUT) "
              "VALUES (:id, :titre, :type, TO_DATE(:debut,'YYYY-MM-DD'), TO_DATE(:fin,'YYYY-MM-DD'), :duree, :formateur, :lieu, "
              ":places, :niveau, :statut)");
    q.bindValue(":id", nouvelId);
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
    if (!executer(q)) return false;

    id = nouvelId;
    return true;
}

bool Formation::titreExiste(const QString &titreRecherche, int idExclu)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM FORMATION "
              "WHERE UPPER(TRIM(TITRE)) = UPPER(TRIM(:titre)) AND ID <> :id");
    q.bindValue(":titre", titreRecherche.trimmed());
    q.bindValue(":id", idExclu);
    return executer(q) && q.next() && q.value(0).toInt() > 0;
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
    if (!executer(q)) return false;
    if (q.numRowsAffected() == 0) {
        g_erreur = "Aucune formation ne correspond à cet identifiant.";
        return false;
    }
    return true;
}

bool Formation::supprimer(int idSup)
{
    QSqlQuery q;
    q.prepare("DELETE FROM FORMATION WHERE ID=:id");
    q.bindValue(":id", idSup);
    if (!executer(q)) return false;
    if (q.numRowsAffected() == 0) {
        g_erreur = "Aucune formation ne correspond à cet identifiant.";
        return false;
    }
    return true;
}

QSqlQueryModel* Formation::afficher()
{
    QSqlQuery q("SELECT ID, TITRE, TYPE, TO_CHAR(DATE_DEBUT,'YYYY-MM-DD') AS DATE_DEBUT, TO_CHAR(DATE_FIN,'YYYY-MM-DD') AS DATE_FIN, DUREE, FORMATEUR, "
                "LIEU, NB_PLACES, NIVEAU, STATUT FROM FORMATION ORDER BY ID");
    QSqlQueryModel *m = modelePour(q);
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
    QSqlQuery q;
    q.prepare("SELECT ID, TITRE, TYPE, TO_CHAR(DATE_DEBUT,'YYYY-MM-DD') AS DATE_DEBUT, TO_CHAR(DATE_FIN,'YYYY-MM-DD') AS DATE_FIN, DUREE, FORMATEUR, "
              "LIEU, NB_PLACES, NIVEAU, STATUT FROM FORMATION "
              "WHERE TITRE LIKE :c OR TYPE LIKE :c "
              "OR FORMATEUR LIKE :c OR LIEU LIKE :c OR STATUT LIKE :c");
    q.bindValue(":c", "%" + critere + "%");
    QSqlQueryModel *m = modelePour(q);
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

    QSqlQuery q("SELECT ID, TITRE, TYPE, TO_CHAR(DATE_DEBUT,'YYYY-MM-DD') AS DATE_DEBUT, TO_CHAR(DATE_FIN,'YYYY-MM-DD') AS DATE_FIN, DUREE, FORMATEUR, "
                "LIEU, NB_PLACES, NIVEAU, STATUT FROM FORMATION ORDER BY " + col +
                (croissant ? " ASC" : " DESC"));
    QSqlQueryModel *m = modelePour(q);
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
    if (!executer(q)) return res;
    while (q.next())
        res.insert(q.value(0).toString(), q.value(1).toInt());
    return res;
}

bool Formation::existe(int idTest)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM FORMATION WHERE ID=:id");
    q.bindValue(":id", idTest);
    if (!executer(q) || !q.next()) return false;
    return q.value(0).toInt() > 0;
}

int Formation::nbInscrits(int idFormation)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM INSCRIRE "
              "WHERE ID=:id AND STATUT_INSCRIPTION IN ('Inscrit', 'Confirmé')");
    q.bindValue(":id", idFormation);
    return (executer(q) && q.next()) ? q.value(0).toInt() : 0;
}

// Enregistre la presence d'un agent dont l'inscription existe deja.
bool Formation::enregistrerPresence(int idFormation, int idAgent)
{
    QSqlQuery q;
    q.prepare("UPDATE INSCRIRE SET PRESENCE='Présent' "
              "WHERE ID=:f AND ID_AGENT=:a");
    q.bindValue(":f", idFormation);
    q.bindValue(":a", idAgent);
    if (!executer(q)) return false;
    if (q.numRowsAffected() == 0) {
        g_erreur = "Aucune inscription ne correspond à ces identifiants.";
        return false;
    }
    return true;
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
        sql += " WHERE TITRE LIKE :c OR TYPE LIKE :c "
               "OR FORMATEUR LIKE :c OR LIEU LIKE :c OR STATUT LIKE :c";
    sql += " ORDER BY " + col + (croissant ? " ASC" : " DESC");

    QSqlQuery q;
    q.prepare(sql);
    if (filtre) q.bindValue(":c", "%" + critere.trimmed() + "%");
    return modelePour(q);
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
    return modelePour(q);
}
// Charge dans l'objet courant la formation dont l'identifiant est donne
bool Formation::recuperer(int idRech)
{
    QSqlQuery q;
    q.prepare("SELECT TITRE, TYPE, TO_CHAR(DATE_DEBUT,'YYYY-MM-DD'), TO_CHAR(DATE_FIN,'YYYY-MM-DD'), "
              "DUREE, FORMATEUR, LIEU, NB_PLACES, NIVEAU, STATUT FROM FORMATION WHERE ID=:id");
    q.bindValue(":id", idRech);
    if (!executer(q) || !q.next()) return false;
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

QSqlQueryModel* Formation::calendrier()
{
    QSqlQuery q("SELECT TO_CHAR(DATE_DEBUT,'YYYY-MM-DD'), TO_CHAR(DATE_FIN,'YYYY-MM-DD') FROM FORMATION");
    return modelePour(q);
}

QSqlQueryModel* Formation::formationsDuJour(const QDate &date)
{
    QSqlQuery q;
    q.prepare("SELECT TITRE, LIEU FROM FORMATION WHERE :d BETWEEN DATE_DEBUT AND DATE_FIN");
    q.bindValue(":d", date);
    return modelePour(q);
}
