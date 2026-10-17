#ifndef CONNECTION_H
#define CONNECTION_H

#include <QSqlDatabase>
#include <QString>

class Connection
{
public:
    // Accès à l'unique instance du Singleton.
    static Connection& createInstance();

    bool createConnection(const QString& motDePasse);
    QString lastError() const;
    void closeConnection();

    // Interdire la copie du Singleton.
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

private:
    Connection();
    ~Connection();

    QSqlDatabase db;
};

#endif // CONNECTION_H