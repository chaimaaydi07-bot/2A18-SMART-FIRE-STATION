#include "connexion.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDate>

static QString g_erreur;

QString Connexion::derniereErreur() { return g_erreur; }

bool Connexion::ouvrir(const QString &chemin)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(chemin);
    if (!db.open()) { g_erreur = db.lastError().text(); return false; }
    QSqlQuery q;
    q.exec("PRAGMA foreign_keys = ON");
    if (!creerTables()) return false;
    donneesDemo();
    return true;
}

bool Connexion::creerTables()
{
    const QStringList sql = {
        "CREATE TABLE IF NOT EXISTS formation ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " intitule TEXT NOT NULL, type TEXT NOT NULL, formateur TEXT,"
        " date_debut TEXT NOT NULL, date_fin TEXT NOT NULL,"
        " duree INTEGER NOT NULL CHECK(duree > 0), lieu TEXT,"
        " capacite INTEGER NOT NULL CHECK(capacite > 0),"
        " certification INTEGER NOT NULL DEFAULT 0,"
        " statut TEXT NOT NULL DEFAULT 'planifiee')",
        // Tables du module Gestion du personnel (simplifiées) nécessaires aux liens
        "CREATE TABLE IF NOT EXISTS agent ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " nom TEXT NOT NULL, prenom TEXT NOT NULL, grade TEXT)",
        "CREATE TABLE IF NOT EXISTS certification ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " id_agent INTEGER NOT NULL REFERENCES agent(id) ON DELETE CASCADE,"
        " nom TEXT NOT NULL, date_obtention TEXT, date_echeance TEXT)",
        "CREATE TABLE IF NOT EXISTS inscription ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " id_formation INTEGER NOT NULL REFERENCES formation(id) ON DELETE CASCADE,"
        " id_agent INTEGER NOT NULL REFERENCES agent(id) ON DELETE CASCADE,"
        " etat TEXT NOT NULL DEFAULT 'inscrit',"      // inscrit | attente
        " presence INTEGER,"                            // NULL | 0 | 1
        " resultat TEXT NOT NULL DEFAULT 'en_attente',"// en_attente | reussi | echoue
        " UNIQUE(id_formation, id_agent))"
    };
    QSqlQuery q;
    for (const QString &s : sql)
        if (!q.exec(s)) { g_erreur = q.lastError().text(); return false; }
    return true;
}

void Connexion::donneesDemo()
{
    QSqlQuery q;
    q.exec("SELECT COUNT(*) FROM agent");
    if (q.next() && q.value(0).toInt() > 0) return;

    const QDate t = QDate::currentDate();
    auto d = [&](int jours) { return t.addDays(jours).toString(Qt::ISODate); };

    const QStringList agents = {"Ben Salah|Karim|Sergent", "Trabelsi|Sami|Caporal",
        "Gharbi|Nour|Lieutenant", "Jlassi|Walid|Sapeur", "Mansouri|Ines|Sergent",
        "Haddad|Omar|Sapeur"};
    for (const QString &a : agents) {
        const QStringList p = a.split('|');
        q.prepare("INSERT INTO agent(nom,prenom,grade) VALUES(?,?,?)");
        q.addBindValue(p[0]); q.addBindValue(p[1]); q.addBindValue(p[2]); q.exec();
    }
    struct C { int ag; const char *nom; int jours; };
    const C certs[] = {{1,"Secourisme",30},{2,"Secourisme",400},{3,"Lutte contre l'incendie",60},
        {4,"Sauvetage",-10},{5,"Matières dangereuses",200},{6,"Conduite d'engins",45}};
    for (const C &c : certs) {
        q.prepare("INSERT INTO certification(id_agent,nom,date_obtention,date_echeance) VALUES(?,?,?,?)");
        q.addBindValue(c.ag); q.addBindValue(QString::fromUtf8(c.nom));
        q.addBindValue(d(c.jours - 730)); q.addBindValue(d(c.jours)); q.exec();
    }
    struct F { const char *i, *ty, *fo; int deb, fin, h; const char *lieu; int cap, cert; };
    const F forms[] = {
        {"Recyclage SST","Secourisme","Dr Mejri",10,11,14,"Salle de formation",4,1},
        {"Feux d'entrepôt","Lutte contre l'incendie","Cne Riahi",20,22,24,"Terrain d'exercice",6,1},
        {"Sauvetage en milieu périlleux","Sauvetage","Lt Chebbi",-30,-28,24,"Centre régional",5,1}};
    for (const F &f : forms) {
        q.prepare("INSERT INTO formation(intitule,type,formateur,date_debut,date_fin,duree,lieu,capacite,certification,statut)"
                  " VALUES(?,?,?,?,?,?,?,?,?,?)");
        q.addBindValue(QString::fromUtf8(f.i)); q.addBindValue(QString::fromUtf8(f.ty));
        q.addBindValue(QString::fromUtf8(f.fo)); q.addBindValue(d(f.deb)); q.addBindValue(d(f.fin));
        q.addBindValue(f.h); q.addBindValue(QString::fromUtf8(f.lieu)); q.addBindValue(f.cap);
        q.addBindValue(f.cert); q.addBindValue(f.deb > 0 ? "planifiee" : "terminee"); q.exec();
    }
    // Historique de la formation terminée (pour les statistiques)
    q.exec("INSERT INTO inscription(id_formation,id_agent,etat,presence,resultat) VALUES"
           "(3,4,'inscrit',1,'reussi'),(3,5,'inscrit',1,'echoue'),(3,6,'inscrit',0,'en_attente')");
}
