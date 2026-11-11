#ifndef CONNECTION_H
#define CONNECTION_H

#include <QSqlDatabase>
#include <QString>

class Connection final
{
public:
    static Connection &createInstance();
    bool createConnection();
    void closeConnection();
    static QString derniereErreur();

    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;

private:
    Connection();
    ~Connection();
    QSqlDatabase db;
};

#endif // CONNECTION_H
