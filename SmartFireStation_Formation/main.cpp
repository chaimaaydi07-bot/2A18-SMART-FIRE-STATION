#include "mainwindow.h"
#include "connection.h"
#include "splashscreen.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Connexion Oracle : si elle echoue (Oracle pas encore installe), l'interface
    // s'ouvre quand meme en MODE APERCU avec des donnees de demonstration.
    Connection c;
    const bool connecte = c.createConnection();

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
