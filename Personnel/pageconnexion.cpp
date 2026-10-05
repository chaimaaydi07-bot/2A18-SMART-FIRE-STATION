#include "pageconnexion.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QRandomGenerator>
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
const int VALIDITE_CODE_S  = 300;  // le code reçu par SMS est valable 5 minutes

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
QPushButton#lienOubli { background: transparent; border: none; color: #b3211c;
                        font-size: 12px; text-decoration: underline; }
QPushButton#lienOubli:hover { color: #d02a24; }
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
    auto *oubli = new QPushButton("Mot de passe oublié ?");
    oubli->setObjectName("lienOubli");
    oubli->setCursor(Qt::PointingHandCursor);
    connect(oubli, &QPushButton::clicked, this, &PageConnexion::motDePasseOublie);
    lc->addWidget(oubli, 0, Qt::AlignHCenter);

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

// ----------------------------------------------------------------------------
//  Mot de passe oublié : un code à 6 chiffres est envoyé par SMS à l'agent,
//  puis l'agent saisit ce code et choisit un nouveau mot de passe.
// ----------------------------------------------------------------------------
void PageConnexion::motDePasseOublie()
{
    // 1) Identifiant de l'agent
    bool ok = false;
    const QString id = QInputDialog::getText(this, "Mot de passe oublié",
                                             "Saisissez votre identifiant d'agent (ex. AGT-018) :",
                                             QLineEdit::Normal, edIdentifiant->text().trimmed(), &ok)
                           .trimmed().toUpper();
    if (!ok || id.isEmpty()) return;

    const Compte *compte = nullptr;
    for (const Compte &c : m_comptes)
        if (c.identifiant.compare(id, Qt::CaseInsensitive) == 0) compte = &c;
    if (!compte || compte->tel.trimmed().isEmpty()) {
        QMessageBox::warning(this, "Mot de passe oublié",
                             "Identifiant inconnu ou aucun numéro de téléphone enregistré.\n"
                             "Contactez le Responsable RH.");
        return;
    }
    const Compte c = *compte;   // copie : la liste peut changer pendant la saisie

    // 2) Envoi du code par SMS (mode simulation : le code est aussi affiché)
    const QString code = QString::number(QRandomGenerator::global()->bounded(100000, 1000000));
    const QDateTime expiration = QDateTime::currentDateTime().addSecs(VALIDITE_CODE_S);
    emit smsDemande(c.tel, QString("USPC - Votre code de réinitialisation est %1 "
                                   "(valable 5 minutes).").arg(code), c.nom);
    QString numero = c.tel;
    numero.remove(' ');
    QMessageBox::information(this, "Code envoyé",
                             QString("Un code à 6 chiffres a été envoyé par SMS au numéro se terminant par %1.\n\n"
                                     "Mode simulation — SMS reçu : « Votre code de réinitialisation est %2 ».")
                                 .arg(numero.right(3), code));

    // 3) Saisie du code et du nouveau mot de passe
    QDialog dlg(this);
    dlg.setWindowTitle("Nouveau mot de passe");
    auto *form = new QFormLayout(&dlg);
    auto *edCode = new QLineEdit;
    edCode->setMaxLength(6);
    edCode->setPlaceholderText("Code reçu par SMS");
    auto *edNouveau = new QLineEdit;
    edNouveau->setEchoMode(QLineEdit::Password);
    edNouveau->setPlaceholderText("6 caractères minimum");
    auto *edConfirm = new QLineEdit;
    edConfirm->setEchoMode(QLineEdit::Password);
    auto *lblMsg = new QLabel;
    lblMsg->setStyleSheet("color:#b3211c;");
    lblMsg->setWordWrap(true);
    form->addRow("Code :", edCode);
    form->addRow("Nouveau mot de passe :", edNouveau);
    form->addRow("Confirmation :", edConfirm);
    form->addRow(lblMsg);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    int essais = 0;
    connect(bb, &QDialogButtonBox::accepted, &dlg, [&] {
        if (QDateTime::currentDateTime() > expiration) {
            QMessageBox::warning(&dlg, "Code expiré", "Le code a expiré. Recommencez la procédure.");
            dlg.reject();
            return;
        }
        if (edCode->text().trimmed() != code) {
            if (++essais >= MAX_TENTATIVES) {
                QMessageBox::warning(&dlg, "Code incorrect",
                                     "Trop de codes incorrects. Recommencez la procédure.");
                dlg.reject();
                return;
            }
            lblMsg->setText(QString("Code incorrect (essai %1 sur %2).").arg(essais).arg(MAX_TENTATIVES));
            return;
        }
        if (edNouveau->text().size() < 6) {
            lblMsg->setText("Le mot de passe doit contenir au moins 6 caractères.");
            return;
        }
        if (edNouveau->text() != edConfirm->text()) {
            lblMsg->setText("Les deux mots de passe ne sont pas identiques.");
            return;
        }
        dlg.accept();
    });
    if (dlg.exec() != QDialog::Accepted) return;

    // 4) Enregistrement du nouveau mot de passe (chiffré)
    const QString nouveauHash = hacher(edNouveau->text());
    for (Compte &x : m_comptes)
        if (x.identifiant == c.identifiant) x.mdpHash = nouveauHash;
    emit motDePasseReinitialise(c.identifiant, nouveauHash);
    emit smsDemande(c.tel, "USPC - Votre mot de passe a été modifié. "
                           "Si vous n'êtes pas à l'origine de cette demande, contactez le Responsable RH.", c.nom);

    m_echecs = 0;
    edIdentifiant->setText(c.identifiant);
    edMdp->clear();
    lblErreur->clear();
    edMdp->setFocus();
    QMessageBox::information(this, "Mot de passe modifié",
                             "Votre mot de passe a été modifié. Vous pouvez vous connecter.");
}