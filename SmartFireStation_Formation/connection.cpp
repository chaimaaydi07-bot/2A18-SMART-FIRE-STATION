#include "connection.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

Connection::Connection() {}

// Connexion Oracle via ODBC.
// 1) Creez la source ODBC (voir README) et notez son nom -> NOM_SOURCE_ODBC
// 2) Remplacez l'utilisateur et le mot de passe par ceux de votre schema Oracle.
bool Connection::createConnection()
{
    const QString NOM_SOURCE_ODBC = "Source_Projet2A";
    const QString UTILISATEUR     = "votre_user";
    const QString MOT_DE_PASSE    = "votre_mdp";

    QSqlDatabase db = QSqlDatabase::addDatabase("QODBC");
    db.setDatabaseName(NOM_SOURCE_ODBC);
    db.setUserName(UTILISATEUR);
    db.setPassword(MOT_DE_PASSE);

    if (!db.open()) {
        qDebug() << "Connexion Oracle echouee :" << db.lastError().text();
        return false;
    }
    return true;
}

void Connection::closeConnection()
{
    QSqlDatabase::database().close();
}
