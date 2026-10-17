#include <QApplication>
#include <QPalette>
#include <QStackedWidget>
#include <QIcon>
#include <QColor>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QDebug>

#include "connection.h"
#include "firestation.h"
#include "pageconnexion.h"

// Recalcule la taille après un changement de page.
static void recalerFenetre(QStackedWidget &fenetre)
{
    if (fenetre.isMaximized()) {
        fenetre.showNormal();
        fenetre.showMaximized();
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setStyle("Fusion");
    app.setWindowIcon(QIcon(":/logo_USPC.png"));

    // Palette claire.
    QPalette p;
    p.setColor(QPalette::Window, QColor("#f4f2ef"));
    p.setColor(QPalette::WindowText, QColor("#222222"));
    p.setColor(QPalette::Base, Qt::white);
    p.setColor(QPalette::AlternateBase, QColor("#faf9f7"));
    p.setColor(QPalette::Text, QColor("#222222"));
    p.setColor(QPalette::Button, Qt::white);
    p.setColor(QPalette::ButtonText, QColor("#222222"));
    p.setColor(QPalette::PlaceholderText, QColor("#999999"));
    p.setColor(QPalette::ToolTipBase, Qt::white);
    p.setColor(QPalette::ToolTipText, QColor("#222222"));
    p.setColor(QPalette::Highlight, QColor("#f2a10c"));
    p.setColor(QPalette::HighlightedText, QColor("#222222"));
    app.setPalette(p);

    // Accès à l'unique instance de Connection.
    Connection &base = Connection::createInstance();

    // Mot de passe demandé au démarrage.
    while (true) {
        bool accepte = false;

        const QString motDePasse = QInputDialog::getText(
            nullptr,
            "Connexion Oracle",
            "Mot de passe Oracle de l'utilisateur smartfire :",
            QLineEdit::Password,
            QString(),
            &accepte
            );

        if (!accepte) {
            base.closeConnection();
            return 0;
        }

        if (base.createConnection(motDePasse)) {
            qDebug() << "Connexion Oracle établie pour smartfire.";

            QMessageBox::information(
                nullptr,
                "Base de données",
                "Connexion Oracle réussie avec smartfire."
                );

            break;
        }

        QMessageBox::warning(
            nullptr,
            "Échec de connexion Oracle",
            QString("Impossible de se connecter à Oracle.\n\n")
                + base.lastError()
                + "\n\nCliquez sur OK pour réessayer."
            );
    }

    int resultat = 0;

    // Les fenêtres seront détruites avant de fermer la base.
    {
        QStackedWidget fenetre;
        fenetre.setWindowTitle("USPC — Smart Fire Station");

        PageConnexion connexion;
        FireStation application;

        connexion.setComptes(application.comptes());
        fenetre.addWidget(&connexion);
        fenetre.addWidget(&application);

        // Connexion de l'agent à l'application.
        QObject::connect(
            &connexion,
            &PageConnexion::connexionReussie,
            &fenetre,
            [&](const QString &id,
                const QString &nom,
                const QString &role) {
                application.appliquerRole(id, nom, role);
                fenetre.setCurrentWidget(&application);
                recalerFenetre(fenetre);
            }
            );

        // Mot de passe oublié.
        QObject::connect(
            &connexion,
            &PageConnexion::smsDemande,
            &application,
            &FireStation::envoyerSms
            );

        QObject::connect(
            &connexion,
            &PageConnexion::motDePasseReinitialise,
            &application,
            &FireStation::changerMotDePasse
            );

        // Déconnexion de l'agent.
        QObject::connect(
            &application,
            &FireStation::deconnexion,
            &fenetre,
            [&] {
                connexion.setComptes(application.comptes());
                connexion.reinitialiser();
                fenetre.setCurrentWidget(&connexion);
                recalerFenetre(fenetre);
            }
            );

        fenetre.resize(1280, 820);
        fenetre.showMaximized();

        resultat = app.exec();
    }

    // Fermer la connexion avant de détruire QApplication.
    base.closeConnection();

    return resultat;
}