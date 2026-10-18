#include "connection.h"

// Ctor
// Initialise l'attribut `db` avec le driver ODBC.
// On le fait dans le constructeur pour que `db` soit stocké dans le Singleton.
// Cela signifie que la même instance de `QSqlDatabase` sera utilisée partout dans l'application
// via le Singleton, au lieu de créer une nouvelle connexion à chaque fois.
Connection::Connection() {
    db = QSqlDatabase::addDatabase("QODBC");
}

//Dtor
// Ferme la connexion si elle est encore ouverte lorsque l'objet est détruit
Connection::~Connection() {
    if (db.isOpen())
        db.close(); // Libère proprement les ressources
}

// --- Accès à l'instance unique du Singleton ---
// Utilise la technique "Meyers Singleton" : instance statique locale
// Elle est créée la première fois qu'on appelle createInstance()
// et reste vivante pendant toute la durée du programme (thread-safe en C++11+)
Connection& Connection::createInstance() {
    static Connection instance;  // Instance unique du Singleton
    return instance; // Retourne une référence à cette instance
}


bool Connection::createConnection() {
    db.setDatabaseName("SmartFireStation");// saisissez le nom de votre source de données ici
    db.setUserName("smartfire");// saisissez votre nom d'utilisateur ici
    db.setPassword("smartfire2026");// saisissez votre mot de passe utilisateur ici

    if (db.open()) {
        qDebug() << "Connexion établie";
        return true;
    }

    qDebug() << "Échec de la connexion :" << db.lastError().text();
    return false;
}

