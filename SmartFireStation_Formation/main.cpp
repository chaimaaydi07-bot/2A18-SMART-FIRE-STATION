#include "mainwindow.h"
#include "connection.h"
#include "splashscreen.h"
#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Test de connexion, comme dans l'atelier Meyers Singleton.
    // En cas d'echec, l'interface reste accessible en mode apercu.
    Connection& c = Connection::createInstance();
    const bool connecte = c.createConnection();
    if (connecte) {
        QMessageBox::information(nullptr, "Base de donnees",
                                 "Connexion Oracle/ODBC reussie.");
    } else {
        QMessageBox::warning(nullptr, "Connexion Oracle/ODBC",
                             "Echec de connexion. L'application demarrera en mode apercu.\n\n"
                             + Connection::derniereErreur());
    }

    MainWindow *w = new MainWindow;
    w->setModeApercu(!connecte);

    SplashScreen *splash = new SplashScreen;
    QObject::connect(splash, &SplashScreen::termine, [w, splash]() {
        w->show();
        splash->deleteLater();
    });
    splash->demarrer(3000);

    return a.exec();
}
