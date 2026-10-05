#include "pageconnexion.h"

#include <QCryptographicHash>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
const int MAX_TENTATIVES   = 3;    // nombre d'essais avant blocage
const int DUREE_BLOCAGE_S  = 30;   // durée du blocage en secondes

const char *STYLE_CONNEXION = R"(
PageConnexion { background: #f4f2ef; }
#panneau { background: #b3211c; }
#panneau QLabel { color: white; background: transparent; }
#panneau QLabel#logoRond { background: white; border-radius: 110px; }
#titreApp { font-size: 26px; font-weight: bold; }
#sousTitreApp { font-size: 14px; color: rgba(255,255,255,0.85); }
#carte { background: white; border: 1px solid #e4e0da; border-radius: 12px; }
#carte QLabel { background: transparent; color: #222; }
#carte QLabel#titreConnexion { font-size: 24px; font-weight: bold; }
#carte QLabel#aide { color: #777; font-size: 12px; }
#carte QLabel#etiquette { color: #555; font-size: 12px; font-weight: bold; }
#carte QLabel#erreurConnexion { color: #b3211c; font-size: 12px; }
QLineEdit { background: #faf9f7; border: 1px solid #e4e0da; border-radius: 6px;
            padding: 8px 10px; font-size: 14px; color: #222; }
QLineEdit:focus { border: 1px solid #f2a10c; background: white; }
QLineEdit:disabled { color: #aaa; }
QToolButton { background: white; border: 1px solid #e4e0da; border-radius: 6px; padding: 6px 10px; color: #222; }
QToolButton:hover { border: 1px solid #f2a10c; }
QPushButton#btnConnexion { background: #b3211c; color: white; border: none; border-radius: 6px;
                           padding: 10px; font-size: 15px; font-weight: bold; }
QPushButton#btnConnexion:hover { background: #d02a24; }
QPushButton#btnConnexion:disabled { background: #d9a09d; }
)";
}

PageConnexion::PageConnexion(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(STYLE_CONNEXION);

    auto *racine = new QHBoxLayout(this);
    racine->setContentsMargins(0, 0, 0, 0);
    racine->setSpacing(0);

    // --- Panneau gauche : logo et nom de l'application
    auto *panneau = new QFrame;
    panneau->setObjectName("panneau");
    panneau->setMinimumWidth(420);
    auto *lp = new QVBoxLayout(panneau);
    lp->setContentsMargins(40, 40, 40, 40);
    lp->addStretch();
    auto *logo = new QLabel;
    logo->setObjectName("logoRond");
    logo->setFixedSize(220, 220);
    logo->setAlignment(Qt::AlignCenter);
    logo->setPixmap(QPixmap(":/logo_USPC.png").scaled(150, 150, Qt::KeepAspectRatio,
                                                      Qt::SmoothTransformation));
    lp->addWidget(logo, 0, Qt::AlignHCenter);
    lp->addSpacing(24);
    auto *titre = new QLabel("Smart Fire Station");
    titre->setObjectName("titreApp");
    titre->setAlignment(Qt::AlignCenter);
    lp->addWidget(titre);
    auto *sousTitre = new QLabel("Unité Spéciale de la Protection Civile");
    sousTitre->setObjectName("sousTitreApp");
    sousTitre->setAlignment(Qt::AlignCenter);
    lp->addWidget(sousTitre);
    lp->addStretch();
    racine->addWidget(panneau, 2);

    // --- Partie droite : carte de connexion centrée
    auto *droite = new QWidget;
    auto *ld = new QVBoxLayout(droite);
    ld->addStretch();
    auto *carte = new QFrame;
    carte->setObjectName("carte");
    carte->setFixedWidth(400);
    auto *lc = new QVBoxLayout(carte);
    lc->setContentsMargins(32, 32, 32, 32);
    lc->setSpacing(6);

    auto *titreC = new QLabel("Connexion");
    titreC->setObjectName("titreConnexion");
    lc->addWidget(titreC);
    auto *aide = new QLabel("Connectez-vous avec votre identifiant d'agent.");
    aide->setObjectName("aide");
    lc->addWidget(aide);
    lc->addSpacing(16);

    auto *e1 = new QLabel("Identifiant");
    e1->setObjectName("etiquette");
    lc->addWidget(e1);
    edIdentifiant = new QLineEdit;
    edIdentifiant->setPlaceholderText("ex. AGT-018");
    lc->addWidget(edIdentifiant);
    lc->addSpacing(8);

    auto *e2 = new QLabel("Mot de passe");
    e2->setObjectName("etiquette");
    lc->addWidget(e2);
    auto *ligneMdp = new QHBoxLayout;
    edMdp = new QLineEdit;
    edMdp->setEchoMode(QLineEdit::Password);
    edMdp->setPlaceholderText("Mot de passe");
    btnOeil = new QToolButton;
    btnOeil->setText("Afficher");
    btnOeil->setCursor(Qt::PointingHandCursor);
    ligneMdp->addWidget(edMdp, 1);
    ligneMdp->addWidget(btnOeil);
    lc->addLayout(ligneMdp);

    lblErreur = new QLabel;
    lblErreur->setObjectName("erreurConnexion");
    lblErreur->setWordWrap(true);
    lblErreur->setMinimumHeight(34);
    lc->addWidget(lblErreur);

    btnConnexion = new QPushButton("Se connecter");
    btnConnexion->setObjectName("btnConnexion");
    btnConnexion->setCursor(Qt::PointingHandCursor);
    lc->addWidget(btnConnexion);
    lc->addSpacing(8);
    auto *oubli = new QLabel("Mot de passe oublié ? Contactez le Responsable RH.");
    oubli->setObjectName("aide");
    oubli->setAlignment(Qt::AlignCenter);
    lc->addWidget(oubli);

    ld->addWidget(carte, 0, Qt::AlignHCenter);
    ld->addStretch();
    racine->addWidget(droite, 3);

    // --- Minuteur du blocage (décompte chaque seconde)
    m_minuteur = new QTimer(this);
    m_minuteur->setInterval(1000);
    connect(m_minuteur, &QTimer::timeout, this, [this] {
        if (--m_secondesRestantes <= 0) {
            m_minuteur->stop();
            m_echecs = 0;
            edIdentifiant->setEnabled(true);
            edMdp->setEnabled(true);
            btnConnexion->setEnabled(true);
            lblErreur->clear();
            edMdp->setFocus();
            return;
        }
        lblErreur->setText(QString("Trop de tentatives échouées. Réessayez dans %1 s.")
                               .arg(m_secondesRestantes));
    });

    connect(btnConnexion, &QPushButton::clicked, this, &PageConnexion::seConnecter);
    connect(edIdentifiant, &QLineEdit::returnPressed, this, [this] { edMdp->setFocus(); });
    connect(edMdp, &QLineEdit::returnPressed, this, &PageConnexion::seConnecter);   // touche Entrée
    connect(btnOeil, &QToolButton::clicked, this, &PageConnexion::afficherMasquerMdp);
}

QString PageConnexion::hacher(const QString &motDePasse)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(motDePasse.toUtf8(), QCryptographicHash::Sha256).toHex());
}

void PageConnexion::setComptes(const QList<Compte> &comptes)
{
    m_comptes = comptes;
}

void PageConnexion::reinitialiser()
{
    edIdentifiant->clear();
    edMdp->clear();
    edMdp->setEchoMode(QLineEdit::Password);
    btnOeil->setText("Afficher");
    if (!m_minuteur->isActive()) lblErreur->clear();
    edIdentifiant->setFocus();
}

void PageConnexion::afficherMasquerMdp()
{
    const bool masque = edMdp->echoMode() == QLineEdit::Password;
    edMdp->setEchoMode(masque ? QLineEdit::Normal : QLineEdit::Password);
    btnOeil->setText(masque ? "Masquer" : "Afficher");
}

void PageConnexion::seConnecter()
{
    if (m_minuteur->isActive()) return;               // compte bloqué

    const QString id  = edIdentifiant->text().trimmed().toUpper();
    const QString mdp = edMdp->text();
    if (id.isEmpty() || mdp.isEmpty()) {
        lblErreur->setText("Veuillez saisir votre identifiant et votre mot de passe.");
        return;
    }

    // Vérification : l'identifiant existe et le mot de passe chiffré correspond
    const QString h = hacher(mdp);
    for (const Compte &c : m_comptes) {
        if (c.identifiant.compare(id, Qt::CaseInsensitive) == 0 && c.mdpHash == h) {
            m_echecs = 0;
            lblErreur->clear();
            emit connexionReussie(c.identifiant, c.nom, c.role);
            return;
        }
    }

    // Échec : même message que l'identifiant ou le mot de passe soit faux (sécurité)
    ++m_echecs;
    edMdp->clear();
    if (m_echecs >= MAX_TENTATIVES) {
        bloquer();
        return;
    }
    lblErreur->setText(QString("Identifiant ou mot de passe incorrect. Tentative %1 sur %2.")
                           .arg(m_echecs).arg(MAX_TENTATIVES));
}

void PageConnexion::bloquer()
{
    m_secondesRestantes = DUREE_BLOCAGE_S;
    edIdentifiant->setEnabled(false);
    edMdp->setEnabled(false);
    btnConnexion->setEnabled(false);
    lblErreur->setText(QString("Trop de tentatives échouées. Réessayez dans %1 s.")
                           .arg(m_secondesRestantes));
    m_minuteur->start();
}