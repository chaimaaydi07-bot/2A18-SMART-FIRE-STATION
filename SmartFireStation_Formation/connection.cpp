#include "connection.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

static QString g_erreur;

Connection::Connection()
    : db(QSqlDatabase::addDatabase("QODBC"))
{}

Connection::~Connection() { closeConnection(); }

Connection& Connection::createInstance()
{
    static Connection instance;
    return instance;
}

QString Connection::derniereErreur() { return g_erreur; }

// Connexion Oracle via ODBC.
// 1) Creez la source ODBC (voir README) et notez son nom -> NOM_SOURCE_ODBC
// 2) Remplacez l'utilisateur et le mot de passe par ceux de votre schema Oracle.
bool Connection::createConnection()
{
    g_erreur.clear();
    if (!QSqlDatabase::drivers().contains("QODBC")) {
        g_erreur = "Pilote Qt QODBC indisponible. Pilotes detectes : "
                   + QSqlDatabase::drivers().join(", ");
        return false;
    }
    if (db.isOpen()) return true;

    db.setDatabaseName(qEnvironmentVariable("SMARTFIRE_ODBC_DSN", "SmartFireStation"));
    db.setUserName(qEnvironmentVariable("SMARTFIRE_DB_USER", "smartfire"));
    db.setPassword(qEnvironmentVariable("SMARTFIRE_DB_PASSWORD", "smartfire2026"));

    if (!db.open()) {
        g_erreur = db.lastError().text();
        qDebug() << "Connexion Oracle ODBC echouee :" << g_erreur;
        return false;
    }
    qDebug() << "Connexion Oracle ODBC etablie :" << db.databaseName();
    return true;
}

void Connection::closeConnection()
{
    if (db.isValid() && db.isOpen()) db.close();
}
