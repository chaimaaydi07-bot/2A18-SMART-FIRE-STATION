#include <QApplication>
#include <QPalette>
#include <QStackedWidget>
#include "firestation.h"
#include "pageconnexion.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    app.setWindowIcon(QIcon(":/logo_USPC.png"));

    // Palette claire forcée (ignore le mode sombre de Windows)
    QPalette p;
    p.setColor(QPalette::Window,          QColor("#f4f2ef"));
    p.setColor(QPalette::WindowText,      QColor("#222222"));
    p.setColor(QPalette::Base,            Qt::white);
    p.setColor(QPalette::AlternateBase,   QColor("#faf9f7"));
    p.setColor(QPalette::Text,            QColor("#222222"));
    p.setColor(QPalette::Button,          Qt::white);
    p.setColor(QPalette::ButtonText,      QColor("#222222"));
    p.setColor(QPalette::PlaceholderText, QColor("#999999"));
    p.setColor(QPalette::ToolTipBase,     Qt::white);
    p.setColor(QPalette::ToolTipText,     QColor("#222222"));
    p.setColor(QPalette::Highlight,       QColor("#f2a10c"));
    p.setColor(QPalette::HighlightedText, QColor("#222222"));
    app.setPalette(p);

    // Une seule fenêtre : page 0 = connexion, page 1 = application
    QStackedWidget fenetre;
    fenetre.setWindowTitle("USPC — Smart Fire Station");
    PageConnexion connexion;
    FireStation application;
    connexion.setComptes(application.comptes());
    fenetre.addWidget(&connexion);
    fenetre.addWidget(&application);

    // Connexion réussie : on adapte l'application au rôle puis on l'affiche
    QObject::connect(&connexion, &PageConnexion::connexionReussie, &fenetre,
                     [&](const QString &id, const QString &nom, const QString &role) {
                         application.appliquerRole(id, nom, role);
                         fenetre.setCurrentWidget(&application);
                     });

    // Déconnexion : retour à la page de connexion (liste des comptes mise à jour)
    QObject::connect(&application, &FireStation::deconnexion, &fenetre, [&] {
        connexion.setComptes(application.comptes());
        connexion.reinitialiser();
        fenetre.setCurrentWidget(&connexion);
    });

    fenetre.resize(1280, 820);
    fenetre.show();
    return app.exec();
}