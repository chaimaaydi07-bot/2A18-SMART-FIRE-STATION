#include <QApplication>
#include <QPalette>
#include <QStackedWidget>
#include <QIcon>
#include <QColor>
#include <QSqlDatabase>
#include <QSqlError>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QDebug>

#include "firestation.h"
#include "pageconnexion.h"

// Recalcule la taille de la fenêtre après un changement de page.
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

    // Palette claire forcée.
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

    // Connexion Oracle via la source ODBC créée dans Windows.
    QSqlDatabase db = QSqlDatabase::addDatabase("QODBC");
    db.setDatabaseName("SmartFireStation");
    db.setUserName("smartfire");

    // Le mot de passe est saisi au démarrage, sans être écrit dans le code.
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

        if (!accepte)
            return 0;

        if (db.open("smartfire", motDePasse)) {
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
            "Impossible de se connecter à Oracle.\n\n"
                + db.lastError().text()
                + "\n\nCliquez sur OK pour réessayer."
            );
    }

    // Une seule fenêtre : connexion puis application.
    QStackedWidget fenetre;
    fenetre.setWindowTitle("USPC — Smart Fire Station");

    PageConnexion connexion;
    FireStation application;

    connexion.setComptes(application.comptes());
    fenetre.addWidget(&connexion);
    fenetre.addWidget(&application);

    // Connexion à l'application.
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

    // Déconnexion de l'application.
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

    return app.exec();
}