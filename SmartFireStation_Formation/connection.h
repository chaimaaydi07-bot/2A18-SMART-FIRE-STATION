#ifndef CONNECTION_H
#define CONNECTION_H

#include <QSqlDatabase>

class Connection
{
public:
    Connection();
    bool createConnection();
    void closeConnection();
};

#endif // CONNECTION_H
