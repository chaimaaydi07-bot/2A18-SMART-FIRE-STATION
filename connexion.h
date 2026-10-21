#ifndef CONNEXION_H
#define CONNEXION_H
#include <QString>

// Accès à la base SQLite locale (créée automatiquement au premier lancement).
class Connexion {
public:
    static bool ouvrir(const QString &chemin = "smartfire.db");
    static QString derniereErreur();
private:
    static bool creerTables();
    static void donneesDemo();
};
#endif
