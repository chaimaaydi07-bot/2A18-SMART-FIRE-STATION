#include "connection.h"
#include <QSqlError>

Connection::Connection()
{
    db = QSqlDatabase::addDatabase("QODBC");
    db.setDatabaseName("SmartFireStation");
    db.setUserName("smartfire");
}

Connection::~Connection()
{
    closeConnection();
}

Connection& Connection::createInstance()
{
    static Connection instance;
    return instance;
}

bool Connection::createConnection(const QString& motDePasse)
{
    if (db.isOpen())
        return true;

    return db.open("smartfire", motDePasse);
}

QString Connection::lastError() const
{
    return db.lastError().text();
}

void Connection::closeConnection()
{
    if (db.isValid() && db.isOpen())
        db.close();

    // Libérer la référence avant la destruction de QApplication.
    db = QSqlDatabase();
}