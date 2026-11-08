#ifndef CONNEXION_H
#define CONNEXION_H
#include <QString>

// Connexion unique a la base Oracle commune via QODBC.
class Connexion {
public:
    static bool ouvrir();
    static QString derniereErreur();
};
#endif
