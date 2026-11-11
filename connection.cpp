#include "connection.h"
#include <QSqlError>
#include <QStringList>

static QString g_erreur;

Connection::Connection()
    : db(QSqlDatabase::addDatabase("QODBC"))
{}

Connection::~Connection()
{
    closeConnection();
}

Connection &Connection::createInstance()
{
    static Connection instance;
    return instance;
}

QString Connection::derniereErreur()
{
    return g_erreur;
}

bool Connection::createConnection()
{
    g_erreur.clear();
    if (!QSqlDatabase::drivers().contains("QODBC")) {
        g_erreur = "Pilote Qt QODBC indisponible. Pilotes détectés : "
                   + QSqlDatabase::drivers().join(", ");
        return false;
    }
    if (db.isOpen()) return true;

    const QString motDePasse = qEnvironmentVariable("SMARTFIRE_DB_PASSWORD");
    if (motDePasse.isEmpty()) {
        g_erreur = "Configurez SMARTFIRE_DB_PASSWORD avant de lancer l'application.";
        return false;
    }
    db.setDatabaseName(qEnvironmentVariable("SMARTFIRE_ODBC_DSN", "SmartFireStation"));
    db.setUserName(qEnvironmentVariable("SMARTFIRE_DB_USER", "smartfire"));
    db.setPassword(motDePasse);
    if (!db.open()) {
        g_erreur = db.lastError().text();
        return false;
    }
    return true;
}

void Connection::closeConnection()
{
    if (db.isValid() && db.isOpen()) db.close();
}
