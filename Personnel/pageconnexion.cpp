#include "pageconnexion.h"
#include "logoanime.h"

#include <QCheckBox>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QInputDialog>
#include <QLocale>
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
PageConnexion { background: #f6f4f1; }

/* ---- Panneau gauche (identité de l'USPC) ---- */
#panneau { background: #3a0b12; border-top: 6px solid #b3211c; }
#panneau QLabel { color: white; background: transparent; }
#panneau QFrame#barreRouge { background: #b3211c; border-radius: 2px; }
#panneau QLabel#sigle { font-size: 22px; font-weight: bold; letter-spacing: 2px; }
#panneau QLabel#tutelle { font-size: 12px; color: #e9c9c6; }
#panneau QLabel#nomComplet { font-size: 30px; font-weight: bold; }
#panneau QLabel#presentation { font-size: 15px; color: #f1dcda; }
#panneau QLabel#puce { border: 1px solid rgba(255,255,255,0.22); border-radius: 8px;
                       padding: 8px 12px; font-size: 13px; }
#panneau QLabel#infoSysteme { font-size: 12px; color: #e9c9c6; }
#panneau QFrame#anneau { background: rgba(255,255,255,0.08); border-radius: 102px; }

/* ---- Partie droite ---- */
QLabel#date { color: #5c5650; font-size: 13px; }
QLabel#langue { background: white; border: 1px solid #d9d3cc; border-radius: 8px;
                padding: 6px 12px; font-weight: bold; color: #1e1b19; }
#carte { background: white; border: 1px solid #e4ded7; border-radius: 14px; }
#carte QLabel { background: transparent; color: #1e1b19; }
#carte QLabel#surtitre { color: #9e1b17; font-size: 11px; font-weight: bold; letter-spacing: 2px; }
#carte QLabel#titreConnexion { font-size: 28px; font-weight: bold; }
#carte QLabel#aide { color: #5c5650; font-size: 13px; }
#carte QLabel#etiquette { color: #1e1b19; font-size: 13px; font-weight: bold; }
#carte QLabel#erreurConnexion { color: #b3211c; font-size: 12px; }
#carte QFrame#avertissement { background: #fbf3e4; border: 1px solid #f0ddb8; border-radius: 9px; }
#carte QLabel#texteAvertissement { color: #5a4520; font-size: 12px; }
QLabel#mention { color: #6b645d; font-size: 12px; }
QLabel#piedPage { color: #8a847e; font-size: 11px; }
QLineEdit { background: #faf8f6; border: 1px solid #d9d3cc; border-radius: 9px;
            padding: 0 12px; min-height: 44px; font-size: 14px; color: #1e1b19; }
QLineEdit:focus { border: 1px solid #b3211c; background: white; }
QLineEdit:disabled { color: #aaa; }
QToolButton#oeil { background: transparent; border: none; color: #9e1b17;
                   font-weight: bold; padding: 6px 10px; }
QToolButton#oeil:hover { color: #6e0f0c; }
QCheckBox { color: #3d3833; font-size: 13px; spacing: 8px; }
QPushButton#btnConnexion { background: #b3211c; color: white; border: none; border-radius: 9px;
                           min-height: 48px; font-size: 15px; font-weight: bold; }
QPushButton#btnConnexion:hover { background: #9e1b17; }
QPushButton#btnConnexion:disabled { background: #d9a09d; }
QPushButton#lienOubli { background: transparent; border: none; color: #9e1b17;
                        font-size: 13px; font-weight: bold; }
QPushButton#lienOubli:hover { color: #6e0f0c; text-decoration: underline; }
)";
}

PageConnexion::PageConnexion(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(STYLE_CONNEXION);

    auto *racine = new QHBoxLayout(this);
    racine->setContentsMargins(0, 0, 0, 0);
    racine->setSpacing(0);

    // =====================================================================
    //  Panneau gauche : identité de l'USPC
    // =====================================================================
    auto *panneau = new QFrame;
    panneau->setObjectName("panneau");
    panneau->setMinimumWidth(440);
    auto *lp = new QVBoxLayout(panneau);
    lp->setContentsMargins(48, 36, 48, 32);
    lp->setSpacing(0);

    // En-tête officiel : barre rouge + sigle + organisme de tutelle
    auto *entete = new QHBoxLayout;
    entete->setSpacing(12);
    auto *barre = new QFrame;
    barre->setObjectName("barreRouge");
    barre->setFixedSize(4, 40);
    entete->addWidget(barre);
    auto *blocNom = new QVBoxLayout;
    blocNom->setSpacing(0);
    auto *sigle = new QLabel("USPC");
    sigle->setObjectName("sigle");
    auto *tutelle = new QLabel("Office National de la Protection Civile");
    tutelle->setObjectName("tutelle");
    blocNom->addWidget(sigle);
    blocNom->addWidget(tutelle);
    entete->addLayout(blocNom);
    entete->addStretch();
    lp->addLayout(entete);
    lp->addStretch(2);

    // Logo animé, centré, dans un léger anneau
    auto *anneau = new QFrame;
    anneau->setObjectName("anneau");
    anneau->setFixedSize(204, 204);
    auto *la = new QVBoxLayout(anneau);
    la->setContentsMargins(10, 10, 10, 10);
    auto *logo = new LogoAnime(":/logo_USPC.png", true, true);   // logo animé et net
    logo->setFixedSize(184, 184);
    la->addWidget(logo);
    lp->addWidget(anneau, 0, Qt::AlignHCenter);
    lp->addSpacing(28);

    auto *nomComplet = new QLabel("Unité Spéciale de la\nProtection Civile");
    nomComplet->setObjectName("nomComplet");
    lp->addWidget(nomComplet);
    lp->addSpacing(10);
    auto *presentation = new QLabel("Plateforme interne de gestion de la caserne : incidents, "
                                    "véhicules, équipements, personnel et formation.");
    presentation->setObjectName("presentation");
    presentation->setWordWrap(true);
    presentation->setMaximumWidth(420);
    lp->addWidget(presentation);
    lp->addSpacing(22);

    // Les 5 modules de l'application
    auto *grilleModules = new QGridLayout;
    grilleModules->setHorizontalSpacing(10);
    grilleModules->setVerticalSpacing(10);
    const QStringList modules = {"Incidents", "Véhicules", "Équipements", "Personnel", "Formation"};
    for (int i = 0; i < modules.size(); ++i) {
        auto *puce = new QLabel(QString("<span style='color:#f2a10c'>●</span>&nbsp;&nbsp;%1")
                                    .arg(modules[i]));
        puce->setObjectName("puce");
        grilleModules->addWidget(puce, i / 3, i % 3);
    }
    auto *blocModules = new QHBoxLayout;
    blocModules->addLayout(grilleModules);
    blocModules->addStretch();
    lp->addLayout(blocModules);
    lp->addStretch(3);

    // Informations système
    auto *infos = new QLabel("<span style='color:#4cc38a'>●</span>&nbsp; Système opérationnel"
                             "&nbsp;&nbsp;&nbsp;&nbsp;Base de Naassen, Ben Arous"
                             "&nbsp;&nbsp;&nbsp;&nbsp;Version 1.0");
    infos->setObjectName("infoSysteme");
    lp->addWidget(infos);
    racine->addWidget(panneau, 2);

    // =====================================================================
    //  Partie droite : date, carte de connexion, mentions
    // =====================================================================
    auto *droite = new QWidget;
    auto *ld = new QVBoxLayout(droite);
    ld->setContentsMargins(40, 24, 40, 24);

    auto *barreHaut = new QHBoxLayout;
    barreHaut->addStretch();
    QString texteDate = QLocale(QLocale::French).toString(QDate::currentDate(), "dddd d MMMM yyyy");
    texteDate[0] = texteDate[0].toUpper();
    auto *date = new QLabel(texteDate);
    date->setObjectName("date");
    barreHaut->addWidget(date);
    ld->addLayout(barreHaut);
    ld->addStretch();

    auto *carte = new QFrame;
    carte->setObjectName("carte");
    carte->setFixedWidth(440);
    auto *lc = new QVBoxLayout(carte);
    lc->setContentsMargins(36, 34, 36, 30);
    lc->setSpacing(6);

    auto *surtitre = new QLabel("ESPACE AGENT");
    surtitre->setObjectName("surtitre");
    lc->addWidget(surtitre);
    auto *titreC = new QLabel("Connexion");
    titreC->setObjectName("titreConnexion");
    lc->addWidget(titreC);
    auto *aide = new QLabel("Utilisez votre identifiant d'agent et votre mot de passe.");
    aide->setObjectName("aide");
    aide->setWordWrap(true);
    lc->addWidget(aide);
    lc->addSpacing(14);

    auto *e1 = new QLabel("Identifiant d'agent");
    e1->setObjectName("etiquette");
    lc->addWidget(e1);
    edIdentifiant = new QLineEdit;
    edIdentifiant->setPlaceholderText("ex. AGT-018");
    lc->addWidget(edIdentifiant);
    lc->addSpacing(10);

    auto *e2 = new QLabel("Mot de passe");
    e2->setObjectName("etiquette");
    lc->addWidget(e2);
    auto *ligneMdp = new QHBoxLayout;
    ligneMdp->setSpacing(4);
    edMdp = new QLineEdit;
    edMdp->setEchoMode(QLineEdit::Password);
    edMdp->setPlaceholderText("Mot de passe");
    btnOeil = new QToolButton;
    btnOeil->setObjectName("oeil");
    btnOeil->setText("Afficher");
    btnOeil->setCursor(Qt::PointingHandCursor);
    ligneMdp->addWidget(edMdp, 1);
    ligneMdp->addWidget(btnOeil);
    lc->addLayout(ligneMdp);
    lc->addSpacing(6);

    // Se souvenir de moi + mot de passe oublié
    auto *ligneOptions = new QHBoxLayout;
    auto *souvenir = new QCheckBox("Se souvenir de moi");
    souvenir->setObjectName("souvenir");
    souvenir->setCursor(Qt::PointingHandCursor);
    ligneOptions->addWidget(souvenir);
    ligneOptions->addStretch();
    auto *oubli = new QPushButton("Mot de passe oublié ?");
    oubli->setObjectName("lienOubli");
    oubli->setCursor(Qt::PointingHandCursor);
    connect(oubli, &QPushButton::clicked, this, &PageConnexion::motDePasseOublie);
    ligneOptions->addWidget(oubli);
    lc->addLayout(ligneOptions);

    lblErreur = new QLabel;
    lblErreur->setObjectName("erreurConnexion");
    lblErreur->setWordWrap(true);
    lblErreur->setMinimumHeight(28);
    lc->addWidget(lblErreur);

    btnConnexion = new QPushButton("Se connecter  →");
    btnConnexion->setObjectName("btnConnexion");
    btnConnexion->setCursor(Qt::PointingHandCursor);
    lc->addWidget(btnConnexion);
    lc->addSpacing(14);

    // Rappel de la règle de sécurité
    auto *avert = new QFrame;
    avert->setObjectName("avertissement");
    auto *lav = new QHBoxLayout(avert);
    lav->setContentsMargins(12, 10, 12, 10);
    auto *texteAvert = new QLabel(QString("Après %1 tentatives incorrectes, l'accès est bloqué "
                                          "pendant %2 secondes.")
                                      .arg(MAX_TENTATIVES).arg(DUREE_BLOCAGE_S));
    texteAvert->setObjectName("texteAvertissement");
    texteAvert->setWordWrap(true);
    texteAvert->setMinimumHeight(36);   // deux lignes visibles en entier
    lav->addWidget(texteAvert);
    lc->addWidget(avert);

    ld->addWidget(carte, 0, Qt::AlignHCenter);
    ld->addSpacing(16);
    auto *mention = new QLabel("Accès réservé au personnel autorisé de l'USPC.\n"
                               "Pas encore de compte ? Contactez le Responsable RH.");
    mention->setObjectName("mention");
    mention->setAlignment(Qt::AlignCenter);
    ld->addWidget(mention, 0, Qt::AlignHCenter);
    ld->addStretch();

    auto *pied = new QHBoxLayout;
    auto *copyright = new QLabel("© 2026 Unité Spéciale de la Protection Civile");
    copyright->setObjectName("piedPage");
    auto *usage = new QLabel("Application de bureau — usage interne");
    usage->setObjectName("piedPage");
    pied->addWidget(copyright);
    pied->addStretch();
    pied->addWidget(usage);
    ld->addLayout(pied);
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
    // « Se souvenir de moi » coché : on garde l'identifiant pour la prochaine connexion
    auto *souvenir = findChild<QCheckBox *>("souvenir");
    const bool garderId = souvenir && souvenir->isChecked() && !edIdentifiant->text().isEmpty();
    if (!garderId) edIdentifiant->clear();
    edMdp->clear();
    edMdp->setEchoMode(QLineEdit::Password);
    btnOeil->setText("Afficher");
    if (!m_minuteur->isActive()) lblErreur->clear();
    if (garderId) edMdp->setFocus(); else edIdentifiant->setFocus();
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