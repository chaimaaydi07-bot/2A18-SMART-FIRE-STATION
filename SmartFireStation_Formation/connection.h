#ifndef CONNECTION_H
#define CONNECTION_H

#include <QSqlDatabase>
#include <QString>

class Connection
{
public:
    static Connection& createInstance();
    bool createConnection();
    void closeConnection();
    static QString derniereErreur();

private:
    Connection();
    ~Connection();
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    QSqlDatabase db;
};

#endif // CONNECTION_H
