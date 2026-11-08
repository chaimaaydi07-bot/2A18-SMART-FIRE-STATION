#include "connexion.h"
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlDriver>

static QString g_erreur;

QString Connexion::derniereErreur()
{
    return g_erreur;
}

bool Connexion::ouvrir()
{
    g_erreur.clear();
    const QString dsn = qEnvironmentVariable("SMARTFIRE_ODBC_DSN", "SmartFireStation");
    const QString user = qEnvironmentVariable("SMARTFIRE_DB_USER", "smartfire");
    const QString password = qEnvironmentVariable("SMARTFIRE_DB_PASSWORD");
    if (password.isEmpty()) {
        g_erreur = "Configurez la variable d'environnement SMARTFIRE_DB_PASSWORD avant de lancer l'application.";
        return false;
    }

    if (!QSqlDatabase::drivers().contains("QODBC")) {
        g_erreur = "Le pilote Qt QODBC est introuvable. Pilotes disponibles : "
                   + QSqlDatabase::drivers().join(", ");
        return false;
    }

    QSqlDatabase db;
    if (QSqlDatabase::contains(QSqlDatabase::defaultConnection)) {
        db = QSqlDatabase::database();
        if (db.isOpen()) return true;
        if (db.driverName() != "QODBC") {
            g_erreur = "La connexion Qt par défaut utilise un pilote différent de QODBC.";
            return false;
        }
    } else {
        db = QSqlDatabase::addDatabase("QODBC");
    }

    db.setDatabaseName(dsn);
    db.setUserName(user);
    db.setPassword(password);
    if (!db.open()) {
        g_erreur = db.lastError().text();
        return false;
    }
    return true;
}
