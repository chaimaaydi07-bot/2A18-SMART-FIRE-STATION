#include "mainwindow.h"
#include "connection.h"
#include "splashscreen.h"
#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Connexion Oracle : si elle echoue (Oracle pas encore installe), l'interface
    // s'ouvre quand meme en MODE APERCU avec des donnees de demonstration.
    Connection& c = Connection::createInstance();
    const bool connecte = c.createConnection();
    if (!connecte) {
        QMessageBox::warning(nullptr, "Connexion Oracle/ODBC",
                             "L'application demarrera en mode apercu.\n\n"
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
