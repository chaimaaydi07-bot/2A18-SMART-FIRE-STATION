#include "firestation.h"
#include "logoanime.h"

#include <QApplication>
#include <QComboBox>
#include <QDateEdit>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QListWidget>
#include <QMessageBox>
#include <QMovie>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <algorithm>

namespace {

// ----------------------------------------------------------------------------
//  Paramètres généraux
// ----------------------------------------------------------------------------
const QString UNITE    = "Unité Spéciale de la Protection Civile (USPC)";


const int ALERTE_JOURS        = 30;  // SMS + alerte : X jours avant l'échéance
const int RENOUVELLEMENT_JOURS = 90; // une formation est proposée X jours avant l'échéance

// ----------------------------------------------------------------------------
//  Listes de valeurs adaptées à l'USPC
// ----------------------------------------------------------------------------
const QStringList POSTES = {"Pompier", "Chauffeur", "Maître-chien", "Secouriste",
                            "Mécanicien", "Administratif", "Dispatcher",
                            "Responsable des équipements", "Responsable RH", "Chef de caserne"};
const QStringList POSTES_OPERATIONNELS = {"Pompier", "Chauffeur", "Maître-chien",
                                          "Secouriste", "Chef de caserne"};
const QStringList GRADES = {"Agent", "Caporal", "Sergent", "Adjudant", "Lieutenant", "Capitaine"};
const QStringList SPECIALITES = {"Sauvetage-déblaiement", "Recherche cynophile",
                                 "Risques chimiques (NRBC)", "Secourisme",
                                 "Lutte contre l'incendie"};
const QStringList DISPOS = {"Disponible", "En intervention", "En formation", "En repos", "En congé"};

// Noms des certifications (identiques dans les fiches agents et le catalogue)
const QString C_PSE1  = "PSE1 — Premiers secours";
const QString C_PSE2  = "PSE2 — Secours en équipe";
const QString C_SD1   = "Sauvetage-déblaiement (SD1)";
const QString C_SD2   = "Sauvetage-déblaiement (SD2)";
const QString C_CYNO  = "Recherche cynophile";
const QString C_NRBC  = "Risques chimiques (NRBC)";
const QString C_COND  = "Conduite d'engins d'intervention";
const QString C_MAINT = "Maintenance des véhicules d'intervention";
const QString C_FDF   = "Lutte contre l'incendie (FDF)";
const QString C_GOC   = "Gestion opérationnelle et commandement (GOC)";

// ----------------------------------------------------------------------------
//  Droits d'accès : modules autorisés pour chaque rôle (= poste)
//  Pour ajouter un module validé plus tard, il suffit de l'ajouter ici.
// ----------------------------------------------------------------------------
const QStringList MODULES = {"Incidents", "Véhicules", "Équipements", "Personnel"};
const QMap<QString, QStringList> ACCES = {
    {"Chef de caserne",             {"Incidents", "Véhicules", "Équipements", "Personnel"}},
    {"Dispatcher",                  {"Incidents"}},
    {"Mécanicien",                  {"Véhicules"}},
    {"Responsable des équipements", {"Équipements"}},
    {"Responsable RH",              {"Personnel"}},
    };

bool estOperationnel(const QString &poste) { return POSTES_OPERATIONNELS.contains(poste); }
bool aUnGrade(const QString &poste)        { return poste != "Administratif"; }

int ordreDispo(const QString &d)
{
    const int i = int(DISPOS.indexOf(d));
    return i < 0 ? 99 : i;
}

QColor couleurDispo(const QString &d)
{
    if (d == "Disponible")      return QColor("#1e8e5a");
    if (d == "En intervention") return QColor("#c46a00");
    if (d == "En formation")    return QColor("#2f6fb3");
    if (d == "En repos")        return QColor("#8a8a8a");
    return QColor("#b5b5b5");   // En congé
}

QColor fondDispo(const QString &d)
{
    if (d == "Disponible")      return QColor("#dff3e5");
    if (d == "En intervention") return QColor("#fdebd0");
    if (d == "En formation")    return QColor("#e3eefa");
    return QColor("#eeeeee");
}

// ----------------------------------------------------------------------------
//  Animations
// ----------------------------------------------------------------------------
// Apparition en fondu d'un widget (avec un délai optionnel pour les effets en cascade)
void fondu(QWidget *w, int ms = 400, int delai = 0)
{
    auto *eff = new QGraphicsOpacityEffect(w);
    eff->setOpacity(0.0);
    w->setGraphicsEffect(eff);
    auto *anim = new QPropertyAnimation(eff, "opacity", w);
    anim->setDuration(ms);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    // l'effet est retiré à la fin pour garder un rendu net
    QObject::connect(anim, &QPropertyAnimation::finished, w, [w, eff] {
        if (w->graphicsEffect() == eff) w->setGraphicsEffect(nullptr);
    });
    QTimer::singleShot(delai, anim, [anim] { anim->start(QAbstractAnimation::DeleteWhenStopped); });
}

// Animation de progression 0 → 1 (barres qui « poussent »)
void animerProgression(QWidget *w, double *prog, int ms, int delai = 0)
{
    auto *a = new QVariantAnimation(w);
    a->setStartValue(0.0);
    a->setEndValue(1.0);
    a->setDuration(ms);
    a->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(a, &QVariantAnimation::valueChanged, w, [w, prog](const QVariant &v) {
        *prog = v.toDouble();
        w->update();
    });
    QTimer::singleShot(delai, a, [a] { a->start(QAbstractAnimation::DeleteWhenStopped); });
}

// ----------------------------------------------------------------------------
//  Graphiques
// ----------------------------------------------------------------------------
struct Seg {
    QString label;
    int     n;
    QColor  color;
};

// Graphique en barres verticales (charge de travail par agent), animé
class BarChart : public QWidget
{
public:
    explicit BarChart(const QList<Agent> &agents, QWidget *parent = nullptr)
        : QWidget(parent), m_agents(agents)
    {
        setMinimumHeight(250);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        animerProgression(this, &m_prog, 900, 250);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const int n = int(m_agents.size());
        if (n == 0) { p.drawText(rect(), Qt::AlignCenter, "Aucun agent"); return; }

        int max = 1;
        for (const Agent &a : m_agents) max = std::max(max, a.mois);

        const int top = 26, bottom = 30;
        const int baseY = height() - bottom;
        const int hMax  = baseY - top;
        const double slot = double(width()) / n;
        const int barW = int(std::min(40.0, slot * 0.55));
        const QFont base = font();
        const double ps = base.pointSizeF() > 0 ? base.pointSizeF() : 9.0;

        for (int i = 0; i < n; ++i) {
            const Agent &a = m_agents[i];
            const int h = std::max(2, int(hMax * a.mois / max * m_prog));
            const int x = int(slot * i + (slot - barW) / 2.0);

            QLinearGradient g(0, baseY - h, 0, baseY);
            g.setColorAt(0, QColor("#d10000"));
            g.setColorAt(1, QColor("#7a0000"));
            p.setPen(Qt::NoPen);
            p.setBrush(g);
            p.drawRoundedRect(QRect(x, baseY - h, barW, h), 3, 3);

            QFont bf = base; bf.setBold(true);
            p.setFont(bf);
            p.setPen(QColor("#222222"));
            p.drawText(QRect(int(slot * i), baseY - h - 20, int(slot), 18),
                       Qt::AlignCenter, QString::number(qRound(a.mois * m_prog)));

            QFont sf = base; sf.setPointSizeF(std::max(7.0, ps - 1.5));
            p.setFont(sf);
            p.setPen(QColor("#777777"));
            const QString nm = QFontMetrics(sf).elidedText(
                a.nom + " " + a.prenom.left(1) + ".", Qt::ElideRight, int(slot) - 4);
            p.drawText(QRect(int(slot * i), baseY + 6, int(slot), 18), Qt::AlignCenter, nm);
        }
        p.setPen(QColor("#e4e0da"));
        p.drawLine(0, baseY, width(), baseY);
    }

private:
    QList<Agent> m_agents;
    double m_prog = 0.0;
};

// Barre horizontale segmentée (répartitions), animée
class StackBar : public QWidget
{
public:
    explicit StackBar(const QList<Seg> &segs, QWidget *parent = nullptr)
        : QWidget(parent), m_segs(segs)
    {
        setFixedHeight(26);
        setMinimumWidth(100);
        animerProgression(this, &m_prog, 800, 300);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()), 6, 6);
        p.setClipPath(path);
        p.fillRect(rect(), QColor("#f1efec"));
        int total = 0;
        for (const Seg &s : m_segs) total += s.n;
        if (total <= 0) return;
        const double W = width() * m_prog;
        double x = 0;
        for (const Seg &s : m_segs) {
            const double w = W * double(s.n) / total;
            p.fillRect(QRectF(x, 0, w + 1, height()), s.color);
            x += w;
        }
    }

private:
    QList<Seg> m_segs;
    double m_prog = 0.0;
};

QFrame *nouvelleCarte(QVBoxLayout **out)
{
    auto *f = new QFrame;
    f->setObjectName("card");
    auto *l = new QVBoxLayout(f);
    l->setContentsMargins(22, 18, 22, 18);
    l->setSpacing(4);
    *out = l;
    return f;
}

QLabel *etiquette(const QString &txt, const QString &objName)
{
    auto *l = new QLabel(txt);
    l->setObjectName(objName);
    return l;
}

// Carte de chiffre clé avec compteur animé
QFrame *carteKpi(int n, const QString &txt, const QString &couleur, int delai = 0)
{
    QVBoxLayout *l = nullptr;
    QFrame *f = nouvelleCarte(&l);
    auto *num = new QLabel("0");
    num->setStyleSheet(QString("font-size:26px;font-weight:bold;color:%1;").arg(couleur));
    l->addWidget(num);
    l->addWidget(etiquette(txt, "sous"));

    auto *a = new QVariantAnimation(num);
    a->setStartValue(0);
    a->setEndValue(n);
    a->setDuration(700);
    a->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(a, &QVariantAnimation::valueChanged, num,
                     [num](const QVariant &v) { num->setText(QString::number(v.toInt())); });
    QTimer::singleShot(delai, a, [a] { a->start(QAbstractAnimation::DeleteWhenStopped); });
    return f;
}

QString legendeHtml(const QList<Seg> &segs)
{
    QStringList parts;
    for (const Seg &s : segs)
        parts << QString("<span style='color:%1;font-size:15px'>&#9679;</span>&nbsp;%2&nbsp;(%3)")
                     .arg(s.color.name(), s.label).arg(s.n);
    return parts.join("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
}

void ajouterDistribution(QVBoxLayout *l, const QString &titre, const QString &sous,
                         const QList<Seg> &segs, const QString &vide)
{
    l->addWidget(etiquette(titre, "h3"));
    l->addWidget(etiquette(sous, "sous"));
    l->addSpacing(8);
    if (segs.isEmpty()) {
        l->addWidget(etiquette(vide, "sous"));
        return;
    }
    l->addWidget(new StackBar(segs));
    l->addSpacing(6);
    auto *lg = new QLabel(legendeHtml(segs));
    lg->setWordWrap(true);
    l->addWidget(lg);
}

// En-tête commun des pages secondaires (fil d'Ariane + titre + sous-titre)
QVBoxLayout *enTetePage(const QString &page, const QString &titre, const QString &sous)
{
    auto *left = new QVBoxLayout;
    left->setSpacing(2);
    left->addWidget(new QLabel(QString("<span style='color:#777'>Personnel&nbsp;&nbsp;›&nbsp;&nbsp;</span>"
                                       "<b style='color:#b3211c'>%1</b>").arg(page)));
    left->addWidget(etiquette(titre, "titre"));
    left->addWidget(etiquette(sous, "sous"));
    return left;
}

const char *STYLE = R"(
QWidget { color: #222; }
QComboBox QAbstractItemView { background: white; color: #222;
    selection-background-color: #f2a10c; selection-color: #222; }
QTableWidget { color: #222; }
QCalendarWidget QWidget { color: #222; background: white; }
QMainWindow, #main { background: #f4f2ef; }
#statsRoot { background: #f4f2ef; }
#sidebar { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
    stop:0 #27050c, stop:0.32 #59101a, stop:0.68 #a71928, stop:1 #d53d39); }
#sidebar QPushButton { color: white; background: transparent; border: 1px solid transparent;
    border-radius: 10px; text-align: left; padding: 10px 16px; margin: 0 12px; font-size: 14px; }
#sidebar QPushButton:hover { background: rgba(255,255,255,0.14); border: 1px solid #cf7780; }
#sidebar QPushButton:checked { background: white; color: #981a27; font-weight: bold;
    border: 2px solid #f1bec4; }
#sidebar QLabel#acteur { color: rgba(255,255,255,0.85); font-size: 12px; padding: 0 22px; }
#sidebar QPushButton#deco { color: white; border: 1px solid rgba(255,255,255,0.6); border-radius: 6px;
    margin: 10px 22px 0 22px; padding: 7px; text-align: center; font-size: 13px; }
#sidebar QPushButton#deco:hover { background: rgba(255,255,255,0.18); }
#aVenir { font-size: 18px; color: #777; }
#logo { background: white; border-radius: 75px; }
#titre { font-size: 26px; font-weight: bold; color: #222; }
#sous { color: #777; }
#card { background: white; border: 1px solid #e4e0da; border-radius: 10px; }
#card QLabel { background: transparent; }
#h3 { font-size: 16px; font-weight: bold; }
QLabel[champ="true"] { color: #777; font-size: 12px; }
QLineEdit, QComboBox, QDateEdit { background: #faf9f7; border: 1px solid #e4e0da;
    border-radius: 6px; padding: 6px 8px; min-height: 20px; }
QLineEdit:focus, QComboBox:focus, QDateEdit:focus { border: 1px solid #f2a10c; background: white; }
QComboBox:disabled { color: #aaa; }
#certRow { background: #faf9f7; border: 1px solid #e4e0da; border-radius: 8px; }
#certRow QLabel { background: transparent; }
#certRow QLineEdit, #certRow QDateEdit { background: white; }
QPushButton { background: white; border: 1px solid #e4e0da; border-radius: 6px; padding: 8px 14px; }
QPushButton:hover { background: #fbf7f1; border: 1px solid #f2a10c; }
QPushButton:pressed { background: #f3ece2; }
QPushButton:disabled { color: #aaa; }
QPushButton#orange { background: #f2a10c; border: none; font-weight: bold; color: #222; }
QPushButton#orange:hover { background: #ffb52e; }
QPushButton#orange:pressed { background: #d98e00; }
QPushButton#rouge  { background: #b3211c; border: none; font-weight: bold; color: white; }
QPushButton#rouge:hover { background: #d02a24; }
QPushButton#rouge:disabled { background: #d9a09d; }
QPushButton#retour { color: #b3211c; font-weight: bold; border: 1px solid #e8b4b1; }
QPushButton#retour:hover { background: #fbe4e2; }
QToolButton { background: white; border: 1px solid #e4e0da; border-radius: 6px; padding: 4px 8px; }
QToolButton:hover { background: #fdf3e1; border: 1px solid #f2a10c; }
QTableWidget { background: white; border: none; gridline-color: transparent; }
QTableWidget::item:hover { background: #fdf6ea; }
QTableWidget::item:selected { background: #fde9c4; color: #222; }
QHeaderView::section { background: white; border: none; border-bottom: 1px solid #e4e0da;
    color: #777; font-weight: bold; padding: 6px; }
#alerte { background: #fbe4e2; border-radius: 8px; }
#alerteInfo { background: #e3eefa; border-radius: 8px; }
#alerte QLabel, #alerteInfo QLabel { background: transparent; }
#alerte QLabel#pastille { background: #b3211c; color: white; border-radius: 12px; font-weight: bold; }
#alerteInfo QLabel#pastilleInfo { background: #2f6fb3; color: white; border-radius: 12px; font-weight: bold; }
#erreur { color: #b3211c; font-size: 12px; }
#badgeUrgent { background: #fbe4e2; color: #b3211c; border-radius: 6px; padding: 2px 8px; font-weight: bold; }
#badgeNormal { background: #fdebd0; color: #a86200; border-radius: 6px; padding: 2px 8px; font-weight: bold; }
#badgeOk { background: #dff3e5; color: #1e7a3c; border-radius: 6px; padding: 2px 8px; font-weight: bold; }
QStatusBar { background: white; color: #555; border-top: 1px solid #e4e0da; }
)";

} // namespace

// ============================================================================
//  Construction
// ============================================================================
FireStation::FireStation(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("USPC — Gestion du personnel");
    setWindowIcon(QIcon(":/logo_USPC.png"));
    setStyleSheet(STYLE);
    chargerDonnees();
    chargerSessions();

    auto *central = new QWidget;
    central->setObjectName("main");
    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(creerSidebar());

    pages = new QStackedWidget;

    // Page 0 : formulaire + liste
    auto *page0 = new QWidget;
    auto *content = new QVBoxLayout(page0);
    content->setContentsMargins(22, 20, 22, 20);
    content->setSpacing(10);

    content->addWidget(etiquette("Gestion du personnel", "titre"));

    auto *bar = new QHBoxLayout;
    bar->addWidget(new QLabel("Trier par :"));
    cbTri = new QComboBox;
    cbTri->addItems({"Disponibilité", "Nom", "Grade"});
    bar->addWidget(cbTri);
    auto *btnPdf = new QPushButton("↓ Planning de garde (PDF)");
    btnPdf->setObjectName("orange");
    auto *btnStats = new QPushButton("Statistiques");
    btnStats->setObjectName("orange");
    auto *btnForm = new QPushButton("Formations recommandées");
    btnForm->setObjectName("orange");
    auto *btnSms = new QPushButton("Journal SMS");
    for (QPushButton *b : {btnPdf, btnStats, btnForm, btnSms}) {
        b->setCursor(Qt::PointingHandCursor);
        bar->addWidget(b);
    }
    bar->addStretch();
    content->addLayout(bar);

    auto *corps = new QHBoxLayout;
    corps->setSpacing(16);
    corps->addWidget(creerFormulaire(), 0, Qt::AlignTop);
    corps->addWidget(creerListe(), 1);
    content->addLayout(corps, 1);
    pages->addWidget(page0);

    // Page 1 : statistiques (reconstruite à chaque ouverture)
    statsScroll = new QScrollArea;
    statsScroll->setWidgetResizable(true);
    statsScroll->setFrameShape(QFrame::NoFrame);
    pages->addWidget(statsScroll);

    // Page 2 : formations recommandées (reconstruite à chaque ouverture)
    formScroll = new QScrollArea;
    formScroll->setWidgetResizable(true);
    formScroll->setFrameShape(QFrame::NoFrame);
    pages->addWidget(formScroll);

    // Page 3 : module d'un coéquipier (affiché après l'intégration)
    lblAVenir = new QLabel;
    lblAVenir->setObjectName("aVenir");
    lblAVenir->setAlignment(Qt::AlignCenter);
    pages->addWidget(lblAVenir);

    // Page 4 : module Incidents (coequipiere)
    pageIncidents = creerModuleIncidents();
    pages->addWidget(pageIncidents);

    root->addWidget(pages, 1);
    setCentralWidget(central);
    statusBar()->showMessage("SMS : mode simulation (voir le Journal SMS)");

    connect(btnPdf,   &QPushButton::clicked, this, &FireStation::exporterPdf);
    connect(btnStats, &QPushButton::clicked, this, &FireStation::afficherStats);
    connect(btnForm,  &QPushButton::clicked, this, &FireStation::afficherFormations);
    connect(btnSms,   &QPushButton::clicked, this, &FireStation::afficherJournalSms);
    connect(navPersonnel, &QPushButton::clicked, this, [this] { allerPage(0); });
    connect(cbTri, &QComboBox::currentTextChanged, this, [this] { rafraichir(); });

    reinitialiser();
    verifierEcheancesSms();     // Innovation 1 : SMS automatiques au démarrage
    rafraichir();
}

QWidget *FireStation::creerSidebar()
{
    auto *side = new QWidget;
    side->setObjectName("sidebar");
    side->setFixedWidth(210);
    auto *l = new QVBoxLayout(side);
    l->setContentsMargins(0, 18, 0, 18);
    l->setSpacing(0);

    // Logo animé (flamme + gyrophares en boucle)
    auto *logo = new LogoAnime(":/logo_USPC.png", false, true);   // net, dessiné par Qt
    logo->setFixedSize(150, 150);
    l->addWidget(logo, 0, Qt::AlignHCenter);
    l->addSpacing(20);

    // Un bouton par module validé ; ils sont affichés ou masqués selon le rôle
    for (const QString &t : MODULES) {
        auto *b = new QPushButton(t);
        b->setCheckable(true);
        b->setAutoExclusive(true);
        b->setChecked(t == "Personnel");
        b->setCursor(Qt::PointingHandCursor);
        if (t == "Personnel") navPersonnel = b;
        else if (t == "Incidents")
            connect(b, &QPushButton::clicked, this, [this] { allerPage(4); });
        else connect(b, &QPushButton::clicked, this, [this, t] {
                lblAVenir->setText(QString("Module %1\n\n(ajouté lors de l'intégration)").arg(t));
                allerPage(3);
            });
        m_boutonsModules.insert(t, b);
        l->addWidget(b);
    }
    l->addStretch();

    // Utilisateur connecté + déconnexion
    lblActeur = new QLabel;
    lblActeur->setObjectName("acteur");
    l->addWidget(lblActeur);
    auto *btnDeco = new QPushButton("Se déconnecter");
    btnDeco->setObjectName("deco");
    btnDeco->setCursor(Qt::PointingHandCursor);
    connect(btnDeco, &QPushButton::clicked, this, &FireStation::deconnexion);
    l->addWidget(btnDeco);
    return side;
}

QWidget *FireStation::creerFormulaire()
{
    auto *card = new QFrame;
    m_carteFormulaire = card;
    card->setObjectName("card");
    card->setFixedWidth(350);
    auto *l = new QVBoxLayout(card);
    l->setContentsMargins(16, 14, 16, 14);
    l->setSpacing(4);

    lblFormTitle = new QLabel("＋ Nouvel agent");
    lblFormTitle->setObjectName("h3");
    l->addWidget(lblFormTitle);

    auto champ = [&](const QString &t) {
        auto *lb = new QLabel(t);
        lb->setProperty("champ", "true");
        l->addWidget(lb);
    };

    champ("Poste");
    cbPoste = new QComboBox;
    cbPoste->addItems(POSTES);
    l->addWidget(cbPoste);

    auto *rNom = new QHBoxLayout;
    auto *cNom = new QVBoxLayout, *cPre = new QVBoxLayout;
    edNom = new QLineEdit;    edPrenom = new QLineEdit;
    auto *lN = new QLabel("Nom");    lN->setProperty("champ", "true");
    auto *lP = new QLabel("Prénom"); lP->setProperty("champ", "true");
    cNom->addWidget(lN); cNom->addWidget(edNom);
    cPre->addWidget(lP); cPre->addWidget(edPrenom);
    rNom->addLayout(cNom); rNom->addLayout(cPre);
    l->addLayout(rNom);

    auto *rGr = new QHBoxLayout;
    auto *cG = new QVBoxLayout, *cS = new QVBoxLayout;
    cbGrade = new QComboBox;
    cbGrade->addItems(GRADES);
    cbSpec = new QComboBox;
    cbSpec->addItems(SPECIALITES);
    cbSpec->setMinimumContentsLength(10);
    cbSpec->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    auto *lG = new QLabel("Grade");      lG->setProperty("champ", "true");
    auto *lS = new QLabel("Spécialité"); lS->setProperty("champ", "true");
    cG->addWidget(lG); cG->addWidget(cbGrade);
    cS->addWidget(lS); cS->addWidget(cbSpec);
    rGr->addLayout(cG, 1); rGr->addLayout(cS, 1);
    l->addLayout(rGr);

    champ("Téléphone (utilisé pour les SMS)");
    edTel = new QLineEdit;
    edTel->setPlaceholderText("+216 20 123 456");
    l->addWidget(edTel);

    champ("Mot de passe (si l'agent utilise l'application)");
    edMdp = new QLineEdit;
    edMdp->setEchoMode(QLineEdit::Password);
    edMdp->setPlaceholderText("Laisser vide pour ne pas changer");
    l->addWidget(edMdp);

    champ("Disponibilité");
    cbDispo = new QComboBox;
    cbDispo->addItems(DISPOS);
    l->addWidget(cbDispo);

    champ("Certifications (nom, date d'obtention, date d'échéance)");
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setFixedHeight(180);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setStyleSheet("background: transparent;");
    auto *box = new QWidget;
    certsLayout = new QVBoxLayout(box);
    certsLayout->setContentsMargins(0, 0, 0, 0);
    certsLayout->setSpacing(6);
    certsLayout->addStretch();
    scroll->setWidget(box);
    l->addWidget(scroll);

    auto *bAddC = new QPushButton("+ Ajouter une certification");
    l->addWidget(bAddC);

    lblErreur = new QLabel;
    lblErreur->setObjectName("erreur");
    lblErreur->setWordWrap(true);
    l->addWidget(lblErreur);

    auto *rb = new QHBoxLayout;
    btnAjouter = new QPushButton("+ Ajouter");
    btnAjouter->setObjectName("rouge");
    btnModifier = new QPushButton("Modifier");
    auto *btnReset = new QPushButton("Réinitialiser");
    rb->addWidget(btnAjouter); rb->addWidget(btnModifier); rb->addWidget(btnReset);
    l->addLayout(rb);

    connect(cbPoste, &QComboBox::currentTextChanged, this, [this] { onPosteChanged(); });
    connect(bAddC, &QPushButton::clicked, this, [this] { ajouterCertLigne(); });
    connect(btnAjouter,  &QPushButton::clicked, this, &FireStation::ajouter);
    connect(btnModifier, &QPushButton::clicked, this, &FireStation::modifier);
    connect(btnReset,    &QPushButton::clicked, this, &FireStation::reinitialiser);
    return card;
}

QWidget *FireStation::creerListe()
{
    auto *wrap = new QWidget;
    auto *lw = new QVBoxLayout(wrap);
    lw->setContentsMargins(0, 0, 0, 0);
    lw->setSpacing(10);

    auto *card = new QFrame;
    card->setObjectName("card");
    auto *l = new QVBoxLayout(card);
    l->setContentsMargins(16, 14, 16, 14);

    l->addWidget(etiquette("☰ Liste du personnel", "h3"));

    auto *rf = new QHBoxLayout;
    edRecherche = new QLineEdit;
    edRecherche->setPlaceholderText("Rechercher par nom, grade, spécialité ou disponibilité…");
    cbFiltrePoste = new QComboBox;
    cbFiltrePoste->addItem("Tous les postes");
    cbFiltrePoste->addItems(POSTES);
    rf->addWidget(edRecherche, 1);
    rf->addWidget(cbFiltrePoste);
    l->addLayout(rf);

    tblAgents = new QTableWidget(0, 7);
    tblAgents->setHorizontalHeaderLabels({"ID", "NOM", "POSTE", "GRADE", "SPÉCIALITÉ",
                                          "DISPONIBILITÉ", "ACTION"});
    tblAgents->verticalHeader()->hide();
    tblAgents->setShowGrid(false);
    tblAgents->setMouseTracking(true);
    tblAgents->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tblAgents->setSelectionBehavior(QAbstractItemView::SelectRows);
    tblAgents->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tblAgents->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tblAgents->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    tblAgents->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    l->addWidget(tblAgents, 1);
    lw->addWidget(card, 1);

    auto *zone = new QWidget;
    alertesLayout = new QVBoxLayout(zone);
    alertesLayout->setContentsMargins(0, 0, 0, 0);
    alertesLayout->setSpacing(8);
    lw->addWidget(zone);

    connect(edRecherche, &QLineEdit::textChanged, this, [this] { rafraichir(); });
    connect(cbFiltrePoste, &QComboBox::currentTextChanged, this, [this] { rafraichir(); });
    return wrap;
}

void FireStation::allerPage(int index)
{
    pages->setCurrentIndex(index);
    pages->currentWidget()->update();   // redessine immédiatement la nouvelle page
}

// ============================================================================
//  Page Statistiques
// ============================================================================
QWidget *FireStation::creerStatsWidget()
{
    auto *root = new QWidget;
    root->setObjectName("statsRoot");
    auto *v = new QVBoxLayout(root);
    v->setContentsMargins(24, 20, 24, 20);
    v->setSpacing(14);

    // --- en-tête
    auto *head = new QHBoxLayout;
    head->addLayout(enTetePage("Statistiques", "Statistiques — Gestion du personnel",
                               "Charge de travail, disponibilité et répartition de l'effectif"), 1);
    m_btnStatsRetour = new QPushButton("‹ Retour à la liste");
    m_btnStatsRetour->setObjectName("retour");
    m_btnStatsExport = new QPushButton("↓ Exporter en PDF");
    head->addWidget(m_btnStatsRetour, 0, Qt::AlignTop);
    head->addWidget(m_btnStatsExport, 0, Qt::AlignTop);
    v->addLayout(head);
    connect(m_btnStatsRetour, &QPushButton::clicked, this, [this] { allerPage(0); });
    connect(m_btnStatsExport, &QPushButton::clicked, this, &FireStation::exporterStatsPdf);

    // --- calculs à partir des données
    const int total = int(m_agents.size());
    int nDispo = 0, nInterv = 0, nIndispo = 0, nOp = 0;
    for (const Agent &a : m_agents) {
        if (a.dispo == "Disponible") ++nDispo;
        else if (a.dispo == "En intervention") ++nInterv;
        else ++nIndispo;
        if (estOperationnel(a.poste)) ++nOp;
    }
    QList<Agent> tri = m_agents;
    std::stable_sort(tri.begin(), tri.end(),
                     [](const Agent &x, const Agent &y) { return x.mois > y.mois; });

    // --- cartes de chiffres (compteurs animés, apparition en cascade)
    auto *kp = new QHBoxLayout;
    kp->setSpacing(12);
    QList<QFrame *> kpis = {
                             carteKpi(total,    "Effectif total",                       "#b3211c", 100),
                             carteKpi(nDispo,   "Disponibles",                          "#1e8e5a", 180),
                             carteKpi(nInterv,  "En intervention",                      "#c46a00", 260),
                             carteKpi(nIndispo, "Indisponibles (repos, congé, formation)", "#777777", 340)};
    for (int i = 0; i < kpis.size(); ++i) {
        kp->addWidget(kpis[i], 1);
        fondu(kpis[i], 400, 80 * i);
    }
    v->addLayout(kp);

    // --- ligne du milieu : graphique + répartitions
    auto *mid = new QHBoxLayout;
    mid->setSpacing(14);

    QVBoxLayout *lc = nullptr;
    QFrame *cChart = nouvelleCarte(&lc);
    lc->addWidget(etiquette("Charge de travail par agent", "h3"));
    lc->addWidget(etiquette("Nombre d'interventions — 30 derniers jours", "sous"));
    lc->addWidget(new BarChart(tri), 1);
    mid->addWidget(cChart, 11);

    QList<Seg> gr = {{"Capitaine", 0, QColor("#8b0000")}, {"Lieutenant", 0, QColor("#b00000")},
                     {"Adjudant", 0, QColor("#d43a3a")},  {"Sergent", 0, QColor("#e57373")},
                     {"Caporal", 0, QColor("#f0b0b0")},   {"Agent", 0, QColor("#f8dada")}};
    QList<Seg> sp = {{"Sauvetage-déblaiement", 0, QColor("#1e5fb3")},
                     {"Recherche cynophile", 0, QColor("#3d7fd0")},
                     {"Risques chimiques (NRBC)", 0, QColor("#6fa3e3")},
                     {"Secourisme", 0, QColor("#9fc2ee")},
                     {"Lutte contre l'incendie", 0, QColor("#cfe0f7")}};
    for (const Agent &a : m_agents) {
        if (!estOperationnel(a.poste)) continue;
        for (Seg &s : gr) if (s.label == a.grade)      ++s.n;
        for (Seg &s : sp) if (s.label == a.specialite) ++s.n;
    }
    QList<Seg> grV, spV;
    for (const Seg &s : gr) if (s.n > 0) grV.append(s);
    for (const Seg &s : sp) if (s.n > 0) spV.append(s);
    const QString sur = QString("Sur les %1 agents opérationnels").arg(nOp);

    QVBoxLayout *lr = nullptr;
    QFrame *cRight = nouvelleCarte(&lr);
    ajouterDistribution(lr, "Répartition par grade", sur, grV, "Aucun agent opérationnel.");
    lr->addSpacing(18);
    ajouterDistribution(lr, "Répartition par spécialité", sur, spV, "Aucun agent opérationnel.");
    lr->addStretch();
    mid->addWidget(cRight, 10);
    v->addLayout(mid);
    fondu(cChart, 450, 250);
    fondu(cRight, 450, 330);

    // --- disponibilité de l'effectif
    QList<Seg> dvV;
    for (const QString &d : DISPOS) {
        int n = 0;
        for (const Agent &a : m_agents) if (a.dispo == d) ++n;
        if (n > 0) dvV.append({d, n, couleurDispo(d)});
    }
    QVBoxLayout *ld = nullptr;
    QFrame *cDispo = nouvelleCarte(&ld);
    ajouterDistribution(ld, "Disponibilité de l'effectif", "Répartition actuelle des statuts",
                        dvV, "Aucun agent.");
    v->addWidget(cDispo);
    fondu(cDispo, 450, 420);
    v->addStretch();
    return root;
}

void FireStation::afficherStats()
{
    QWidget *w = creerStatsWidget();
    statsScroll->setWidget(w);   // remplace (et supprime) l'ancienne page
    m_statsContent = w;
    allerPage(1);
}

void FireStation::exporterStatsPdf()
{
    if (!m_statsContent) return;
    const QString path = QFileDialog::getSaveFileName(this, "Exporter les statistiques",
                                                      "statistiques_personnel.pdf", "PDF (*.pdf)");
    if (path.isEmpty()) return;

    // les boutons de navigation ne doivent pas apparaître dans le PDF
    m_btnStatsRetour->hide();
    m_btnStatsExport->hide();

    QWidget *w = m_statsContent;
    QPdfWriter writer(path);
    writer.setPageLayout(QPageLayout(QPageSize(QPageSize::A4), QPageLayout::Landscape,
                                     QMarginsF(10, 10, 10, 10)));
    QPainter p(&writer);
    const QRect vp = p.viewport();
    const double sc = std::min(double(vp.width()) / w->width(), double(vp.height()) / w->height());
    p.scale(sc, sc);
    w->render(&p);
    p.end();

    m_btnStatsRetour->show();
    m_btnStatsExport->show();
    QMessageBox::information(this, "Export", "Statistiques exportées :\n" + path);
}

// ============================================================================
//  Données de démonstration (à remplacer par votre base de données)
// ============================================================================
void FireStation::chargerDonnees()
{
    const QDate t = QDate::currentDate();
    auto ag = [&](QString id, QString nom, QString pre, QString poste, QString gr, QString sp,
                  QString tel, QString dispo, QList<Certification> certs, int mois) {
        Agent a;
        a.id = id; a.nom = nom; a.prenom = pre; a.poste = poste; a.grade = gr;
        a.specialite = sp; a.tel = tel; a.dispo = dispo; a.certs = certs; a.mois = mois;
        m_agents.append(a);
    };
    ag("AGT-014", "Ben Ali", "Karim", "Pompier", "Sergent", "Sauvetage-déblaiement",
       "+216 20 123 456", "Disponible",
       {{C_PSE1, t.addDays(-165), t.addDays(200)}, {C_SD1, t.addDays(-300), t.addDays(65)}}, 14);
    ag("AGT-015", "Trabelsi", "Sami", "Chef de caserne", "Capitaine", "Sauvetage-déblaiement",
       "+216 22 555 010", "En intervention",
       {{C_PSE1, t.addDays(-200), t.addDays(160)}, {C_SD1, t.addDays(-400), t.addDays(300)},
        {C_SD2, t.addDays(-300), t.addDays(400)}, {C_GOC, t.addDays(-200), t.addDays(500)}}, 9);
    ag("AGT-016", "Gharbi", "Amine", "Mécanicien", "Caporal", "",
       "+216 98 765 432", "Disponible", {}, 16);
    ag("AGT-017", "Jlassi", "Nour", "Secouriste", "Sergent", "Secourisme",
       "+216 50 321 987", "En repos", {{C_PSE1, t.addDays(-353), t.addDays(12)}}, 6);
    ag("AGT-018", "Mansouri", "Leila", "Responsable RH", "Lieutenant", "",
       "+216 71 000 111", "Disponible", {}, 0);
    ag("AGT-022", "Hammami", "Rim", "Administratif", "", "",
       "+216 71 000 222", "Disponible", {}, 0);
    ag("AGT-019", "Sassi", "Mohamed", "Maître-chien", "Caporal", "Recherche cynophile",
       "+216 55 111 222", "Disponible",
       {{C_PSE1, t.addDays(-100), t.addDays(250)}, {C_CYNO, t.addDays(-320), t.addDays(45)}}, 11);
    ag("AGT-020", "Amri", "Hedi", "Pompier", "Lieutenant", "Risques chimiques (NRBC)",
       "+216 52 333 444", "En formation",
       {{C_PSE1, t.addDays(-150), t.addDays(200)}, {C_NRBC, t.addDays(-370), t.addDays(-5)}}, 4);
    ag("AGT-021", "Ben Salah", "Walid", "Chauffeur", "Agent", "Lutte contre l'incendie",
       "+216 53 555 666", "En congé", {}, 8);
    ag("AGT-023", "Mejri", "Khaled", "Dispatcher", "Sergent", "",
       "+216 58 444 777", "Disponible", {}, 0);
    ag("AGT-024", "Ferchichi", "Sonia", "Responsable des équipements", "Adjudant", "",
       "+216 97 222 888", "Disponible", {}, 0);
    ag("AGT-025", "Dridi", "Youssef", "Pompier", "Adjudant", "Lutte contre l'incendie",
       "+216 21 600 111", "Disponible",
       {{C_PSE1, t.addDays(-90), t.addDays(275)}, {C_FDF, t.addDays(-120), t.addDays(245)}}, 12);
    ag("AGT-026", "Khelifi", "Anis", "Chauffeur", "Caporal", "Sauvetage-déblaiement",
       "+216 24 700 222", "Disponible",
       {{C_PSE1, t.addDays(-60), t.addDays(305)}, {C_COND, t.addDays(-200), t.addDays(165)}}, 10);
    ag("AGT-027", "Hamdi", "Ines", "Secouriste", "Agent", "Secourisme",
       "+216 26 800 333", "Disponible",
       {{C_PSE1, t.addDays(-30), t.addDays(335)}, {C_PSE2, t.addDays(-30), t.addDays(335)}}, 7);
    ag("AGT-028", "Saidi", "Bilel", "Pompier", "Caporal", "Sauvetage-déblaiement",
       "+216 29 900 444", "Disponible",
       {{C_PSE1, t.addDays(-45), t.addDays(320)}, {C_SD1, t.addDays(-45), t.addDays(320)}}, 9);

    // Comptes de démonstration (mots de passe chiffrés, jamais stockés en clair)
    const QMap<QString, QString> mdpDemo = {
        {"AGT-018", "rh2026"},       // Responsable RH
        {"AGT-015", "chef2026"},     // Chef de caserne
        {"AGT-023", "disp2026"},     // Dispatcher
        {"AGT-016", "meca2026"},     // Mécanicien
        {"AGT-024", "equip2026"},    // Responsable des équipements
    };
    for (Agent &a : m_agents)
        if (mdpDemo.contains(a.id)) a.mdpHash = PageConnexion::hacher(mdpDemo.value(a.id));
}

// ============================================================================
//  Authentification : comptes et droits d'accès
// ============================================================================
QList<Compte> FireStation::comptes() const
{
    QList<Compte> liste;
    for (const Agent &a : m_agents)
        if (!a.mdpHash.isEmpty())
            liste.append({a.id, a.nom + " " + a.prenom, a.poste, a.mdpHash, a.tel});
    return liste;
}

void FireStation::majUtilisateurConnecte()
{
    // Le nom est relu dans la liste des agents : s'il est modifié, le menu suit
    const int i = indexParId(m_idConnecte);
    if (i < 0) return;
    const Agent &a = m_agents[i];
    lblActeur->setText(QString("Connecté :<br><b>%1</b><br>%2")
                           .arg((a.nom + " " + a.prenom).toHtmlEscaped(), m_roleConnecte));
}

void FireStation::appliquerRole(const QString &identifiant, const QString &nom, const QString &role)
{
    m_idConnecte = identifiant;
    m_roleConnecte = role;
    lblActeur->setText(QString("Connecté :<br><b>%1</b><br>%2").arg(nom.toHtmlEscaped(), role));

    // Menu : seuls les modules autorisés pour ce rôle sont visibles
    const QStringList autorises = ACCES.value(role);
    QPushButton *premier = nullptr;
    for (const QString &m : MODULES) {
        QPushButton *b = m_boutonsModules.value(m);
        b->setVisible(autorises.contains(m));
        if (!premier && autorises.contains(m)) premier = b;
    }

    // Module Personnel : le Responsable RH gère, les autres rôles consultent seulement
    m_lectureSeule = role != "Responsable RH";
    m_carteFormulaire->setVisible(!m_lectureSeule);
    reinitialiser();
    rafraichir();

    if (premier) { premier->setChecked(true); premier->click(); }
    else {
        lblAVenir->setText("Aucun module n'est autorisé pour ce rôle.");
        allerPage(3);
    }
}

// Catalogue des sessions de formation (Innovation 2)
void FireStation::chargerSessions()
{
    const QDate t = QDate::currentDate();
    // ENPC = École Nationale de la Protection Civile
    const QString ENPC  = "ENPC — Jbel Jloud, Tunis";
    const QString ZRIBA = "Centre de formation de Zriba";
    const QString BASE  = "Base de l'USPC — Naassen";
    auto s = [&](QString id, QString cert, QString lieu, int jours, int places) {
        SessionFormation f;
        f.id = id; f.certification = cert; f.lieu = lieu;
        f.date = t.addDays(jours); f.places = places;
        m_sessions.append(f);
    };
    s("SES-01", C_PSE1,  ENPC,  10,  6);
    s("SES-02", C_PSE1,  ZRIBA, 35, 10);
    s("SES-03", C_PSE2,  ENPC,  21,  8);
    s("SES-04", C_SD1,   ZRIBA, 28, 12);
    s("SES-05", C_SD2,   ZRIBA, 45,  6);
    s("SES-06", C_CYNO,  BASE,  18,  4);
    s("SES-07", C_NRBC,  ENPC,  14,  5);
    s("SES-08", C_COND,  ENPC,  30,  6);
    s("SES-09", C_MAINT, ENPC,  40,  4);
    s("SES-10", C_FDF,   ZRIBA, 25, 10);
    s("SES-11", C_GOC,   ENPC,  60,  8);
}

QString FireStation::nouvelId() const
{
    int max = 0;
    for (const Agent &a : m_agents)
        max = std::max(max, a.id.mid(4).toInt());
    return QString("AGT-%1").arg(max + 1, 3, 10, QChar('0'));
}

int FireStation::indexParId(const QString &id) const
{
    for (int i = 0; i < m_agents.size(); ++i)
        if (m_agents[i].id == id) return i;
    return -1;
}

// ============================================================================
//  Formulaire
// ============================================================================
void FireStation::onPosteChanged()
{
    const QString poste = cbPoste->currentText();
    cbGrade->setEnabled(aUnGrade(poste));
    cbSpec->setEnabled(estOperationnel(poste));
}

void FireStation::ajouterCertLigne(const Certification &c)
{
    auto *frame = new QFrame;
    frame->setObjectName("certRow");
    auto *g = new QGridLayout(frame);
    g->setContentsMargins(8, 8, 8, 8);
    g->setHorizontalSpacing(6);
    g->setVerticalSpacing(3);

    auto *nom = new QComboBox;
    nom->setEditable(true);      // liste des certifications connues + saisie libre
    nom->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    nom->setMinimumContentsLength(12);
    QStringList connues;
    for (const SessionFormation &s : m_sessions)
        if (!connues.contains(s.certification)) connues << s.certification;
    nom->addItems(connues);
    nom->setCurrentText(c.nom);
    nom->lineEdit()->setPlaceholderText("Nom de la certification");
    g->addWidget(nom, 0, 0, 1, 3);

    auto *l1 = new QLabel("Obtention");
    l1->setProperty("champ", "true");
    auto *l2 = new QLabel("Échéance");
    l2->setProperty("champ", "true");
    g->addWidget(l1, 1, 0);
    g->addWidget(l2, 1, 1);

    auto mk = [](const QDate &d) {
        auto *de = new QDateEdit(d);
        de->setCalendarPopup(true);
        de->setDisplayFormat("dd/MM/yyyy");
        return de;
    };
    auto *obt = mk(c.obtention.isValid() ? c.obtention : QDate::currentDate());
    auto *ech = mk(c.echeance.isValid()  ? c.echeance  : QDate::currentDate().addYears(1));
    g->addWidget(obt, 2, 0);
    g->addWidget(ech, 2, 1);

    auto *del = new QToolButton;
    del->setText("✕");
    del->setToolTip("Retirer cette certification");
    g->addWidget(del, 2, 2);
    g->setColumnStretch(0, 1);
    g->setColumnStretch(1, 1);

    connect(del, &QToolButton::clicked, this, [this, frame] { retirerCert(frame); });

    certsLayout->insertWidget(certsLayout->count() - 1, frame);
    fondu(frame, 300);
    CertRow r;
    r.frame = frame; r.nom = nom->lineEdit(); r.obt = obt; r.ech = ech;
    m_certRows.append(r);
}

void FireStation::retirerCert(QFrame *frame)
{
    for (int i = 0; i < m_certRows.size(); ++i) {
        if (m_certRows[i].frame == frame) { m_certRows.removeAt(i); break; }
    }
    certsLayout->removeWidget(frame);
    frame->hide();
    frame->deleteLater();
}

void FireStation::viderCerts()
{
    for (const CertRow &r : m_certRows) {
        certsLayout->removeWidget(r.frame);
        r.frame->hide();
        r.frame->deleteLater();
    }
    m_certRows.clear();
}

bool FireStation::lireFormulaire(Agent &a, QString &erreur)
{
    a.nom    = edNom->text().trimmed();
    a.prenom = edPrenom->text().trimmed();
    a.poste  = cbPoste->currentText();
    a.grade      = aUnGrade(a.poste)       ? cbGrade->currentText() : QString();
    a.specialite = estOperationnel(a.poste) ? cbSpec->currentText()  : QString();
    a.tel        = edTel->text().trimmed();
    a.dispo      = cbDispo->currentText();

    if (a.nom.isEmpty() || a.prenom.isEmpty()) { erreur = "Nom et prénom obligatoires."; return false; }
    if (!QRegularExpression("^\\+?[0-9 ]{8,15}$").match(a.tel).hasMatch()) {
        erreur = "Numéro de téléphone invalide (nécessaire pour les SMS)."; return false;
    }
    const QString mdp = edMdp->text();
    if (!mdp.isEmpty() && mdp.size() < 6) {
        erreur = "Le mot de passe doit contenir au moins 6 caractères."; return false;
    }
    a.mdpHash = mdp.isEmpty() ? QString() : PageConnexion::hacher(mdp);   // jamais stocké en clair

    a.certs.clear();
    for (const CertRow &r : m_certRows) {
        Certification c;
        c.nom = r.nom->text().trimmed();
        if (c.nom.isEmpty()) continue;          // ligne vide = pas de certification
        c.obtention = r.obt->date();
        c.echeance  = r.ech->date();
        if (c.echeance <= c.obtention) {
            erreur = QString("« %1 » : l'échéance doit être postérieure à l'obtention.").arg(c.nom);
            return false;
        }
        a.certs.append(c);
    }
    return true;
}

void FireStation::ajouter()
{
    Agent a; QString err;
    if (!lireFormulaire(a, err)) { lblErreur->setText(err); return; }
    a.id = nouvelId();
    m_agents.append(a);
    reinitialiser();
    verifierEcheancesSms();
    rafraichir();
}

void FireStation::modifier()
{
    const int i = indexParId(m_editingId);
    if (i < 0) return;
    Agent a; QString err;
    if (!lireFormulaire(a, err)) { lblErreur->setText(err); return; }
    Agent &old = m_agents[i];
    old.nom = a.nom; old.prenom = a.prenom; old.poste = a.poste; old.grade = a.grade;
    old.specialite = a.specialite; old.tel = a.tel; old.dispo = a.dispo; old.certs = a.certs;
    if (!a.mdpHash.isEmpty()) old.mdpHash = a.mdpHash;   // mot de passe vide = inchangé
    reinitialiser();
    verifierEcheancesSms();
    rafraichir();
}

void FireStation::reinitialiser()
{
    m_editingId.clear();
    lblFormTitle->setText("＋ Nouvel agent");
    edNom->clear(); edPrenom->clear(); edTel->clear(); edMdp->clear();
    cbPoste->setCurrentIndex(0);
    cbGrade->setCurrentIndex(0);
    cbSpec->setCurrentIndex(0);
    cbDispo->setCurrentIndex(0);
    viderCerts();
    ajouterCertLigne();          // une ligne de saisie toujours visible
    lblErreur->clear();
    onPosteChanged();
    btnAjouter->setEnabled(true);
    btnModifier->setEnabled(false);
}

void FireStation::chargerDansFormulaire(const QString &id)
{
    const int i = indexParId(id);
    if (i < 0) return;
    const Agent &a = m_agents[i];
    m_editingId = id;
    lblFormTitle->setText("✎ Modifier " + id);
    edNom->setText(a.nom); edPrenom->setText(a.prenom); edTel->setText(a.tel);
    cbPoste->setCurrentText(a.poste);
    onPosteChanged();
    if (!a.grade.isEmpty())      cbGrade->setCurrentText(a.grade);
    if (!a.specialite.isEmpty()) cbSpec->setCurrentText(a.specialite);
    cbDispo->setCurrentText(a.dispo);
    viderCerts();
    for (const Certification &c : a.certs) ajouterCertLigne(c);
    if (a.certs.isEmpty()) ajouterCertLigne();
    lblErreur->clear();
    btnAjouter->setEnabled(false);
    btnModifier->setEnabled(true);
}

void FireStation::consulter(const QString &id)
{
    const int i = indexParId(id);
    if (i < 0) return;
    const Agent &a = m_agents[i];
    QString certs;
    for (const Certification &c : a.certs)
        certs += QString("<li>%1 — obtenue le %2, échéance %3</li>")
                     .arg(c.nom.toHtmlEscaped(), c.obtention.toString("dd/MM/yyyy"),
                          c.echeance.toString("dd/MM/yyyy"));
    if (certs.isEmpty()) certs = "<li>Aucune</li>";

    QString recos;
    for (const Recommandation &r : recommandationsPour(a)) {
        QString ou = "aucune session programmée";
        if (r.sessionIndex >= 0) {
            const SessionFormation &s = m_sessions[r.sessionIndex];
            ou = QString("%1, le %2").arg(s.lieu, s.date.toString("dd/MM/yyyy"));
        }
        recos += QString("<li><b>%1</b> (%2) — %3%4</li>")
                     .arg(r.certification.toHtmlEscaped(), r.statut, ou.toHtmlEscaped(),
                          r.inscrit ? " — <b>inscrit</b>" : "");
    }
    if (recos.isEmpty()) recos = "<li>Aucune, le profil est à jour.</li>";

    QMessageBox box(this);
    box.setWindowTitle("Fiche agent");
    box.setTextFormat(Qt::RichText);
    box.setText(QString("<h3>%1 — %2 %3</h3>"
                        "<p>Poste : %4<br>Grade : %5<br>Spécialité : %6<br>"
                        "Tél : %7<br>Disponibilité : %8</p>"
                        "<b>Certifications</b><ul>%9</ul>"
                        "<b>Formations recommandées</b><ul>%10</ul>")
                    .arg(a.id, a.nom.toHtmlEscaped(), a.prenom.toHtmlEscaped(), a.poste,
                         a.grade.isEmpty() ? "—" : a.grade,
                         a.specialite.isEmpty() ? "—" : a.specialite, a.tel, a.dispo)
                    .arg(certs, recos));
    box.exec();
}

void FireStation::supprimer(const QString &id)
{
    if (id == m_idConnecte) {
        QMessageBox::warning(this, "Supprimer",
                             "Vous ne pouvez pas supprimer votre propre compte pendant que vous êtes connecté.");
        return;
    }
    if (QMessageBox::question(this, "Supprimer", "Supprimer l'agent " + id + " ?")
        != QMessageBox::Yes) return;
    const int i = indexParId(id);
    if (i >= 0) m_agents.removeAt(i);
    for (SessionFormation &s : m_sessions)
        if (s.inscrits.removeAll(id) > 0) ++s.places;
    if (m_editingId == id) reinitialiser();
    rafraichir();
}

// ============================================================================
//  Liste, tri, recherche, alertes
// ============================================================================
void FireStation::rafraichir()
{
    const QString q = edRecherche->text().trimmed().toLower();
    const QString f = cbFiltrePoste->currentIndex() == 0 ? QString() : cbFiltrePoste->currentText();

    QList<const Agent *> L;
    for (const Agent &a : m_agents) {
        if (!f.isEmpty() && a.poste != f) continue;
        const QString hay = (a.nom + " " + a.prenom + " " + a.grade + " " + a.specialite + " " + a.dispo).toLower();
        if (!q.isEmpty() && !hay.contains(q)) continue;
        L << &a;
    }
    const QString tri = cbTri->currentText();
    std::stable_sort(L.begin(), L.end(), [&](const Agent *x, const Agent *y) {
        if (tri == "Nom")   return x->nom < y->nom;
        if (tri == "Grade") return GRADES.indexOf(x->grade) > GRADES.indexOf(y->grade);
        return ordreDispo(x->dispo) < ordreDispo(y->dispo);
    });

    tblAgents->setRowCount(0);
    for (const Agent *a : L) {
        const int r = tblAgents->rowCount();
        tblAgents->insertRow(r);
        tblAgents->setRowHeight(r, 44);
        const QStringList vals = {a->id, a->nom + " " + a->prenom, a->poste,
                                  a->grade.isEmpty() ? "—" : a->grade,
                                  a->specialite.isEmpty() ? "—" : a->specialite};
        for (int c = 0; c < vals.size(); ++c)
            tblAgents->setItem(r, c, new QTableWidgetItem(vals[c]));

        auto *disp = new QTableWidgetItem(a->dispo);
        disp->setTextAlignment(Qt::AlignCenter);
        disp->setBackground(fondDispo(a->dispo));
        disp->setForeground(couleurDispo(a->dispo).darker(115));
        tblAgents->setItem(r, 5, disp);

        auto *w = new QWidget;
        auto *hl = new QHBoxLayout(w);
        hl->setContentsMargins(2, 2, 2, 2);
        hl->setSpacing(4);
        const QString id = a->id;
        auto mk = [&](const QString &txt, const QString &tip) {
            auto *b = new QToolButton;
            b->setText(txt); b->setToolTip(tip); b->setCursor(Qt::PointingHandCursor);
            hl->addWidget(b);
            return b;
        };
        connect(mk("Voir", "Consulter"),    &QToolButton::clicked, this, [this, id] { consulter(id); });
        if (!m_lectureSeule) {   // modification réservée au Responsable RH
            connect(mk("Modifier", "Modifier"), &QToolButton::clicked, this, [this, id] { chargerDansFormulaire(id); });
            connect(mk("Suppr.", "Supprimer"),  &QToolButton::clicked, this, [this, id] { supprimer(id); });
        }
        tblAgents->setCellWidget(r, 6, w);
    }
    majUtilisateurConnecte();
    mettreAJourAlertes();
}

void FireStation::ajouterAlerte(const QString &titre, const QString &detail, bool info,
                                const QString &idAgent, const QString &actionTxt,
                                std::function<void()> action)
{
    auto *f = new QFrame;
    f->setObjectName(info ? "alerteInfo" : "alerte");
    auto *h = new QHBoxLayout(f);
    h->setContentsMargins(12, 10, 12, 10);

    auto *p = new QLabel(info ? "i" : "!");
    p->setObjectName(info ? "pastilleInfo" : "pastille");
    p->setFixedSize(24, 24);
    p->setAlignment(Qt::AlignCenter);
    h->addWidget(p);

    auto *t = new QLabel(QString("<b>%1</b><br><span style='color:#666'>%2</span>")
                             .arg(titre.toHtmlEscaped(), detail.toHtmlEscaped()));
    t->setWordWrap(true);
    h->addWidget(t, 1);

    if (action) {
        auto *b = new QPushButton(actionTxt);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, action);
        h->addWidget(b);
    }
    if (!idAgent.isEmpty()) {
        auto *b = new QPushButton("Voir ›");
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, [this, idAgent] { consulter(idAgent); });
        h->addWidget(b);
    }
    // apparition en cascade
    fondu(f, 350, 70 * alertesLayout->count());
    alertesLayout->addWidget(f);
}

void FireStation::mettreAJourAlertes()
{
    while (QLayoutItem *it = alertesLayout->takeAt(0)) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }
    const QDate today = QDate::currentDate();
    int n = 0;
    int nRecos = 0;
    QSet<QString> agentsRecos;
    for (const Agent &a : m_agents) {
        const QString who = QString("%1 — %2 %3").arg(a.id, a.nom, a.prenom);

        // Innovation 1 : renouvellement des certifications (SMS envoyé à l'agent)
        for (const Certification &c : a.certs) {
            const int d = int(today.daysTo(c.echeance));
            if (d > ALERTE_JOURS) continue;
            const QString etat = d < 0 ? QString("expirée depuis %1 jour(s)").arg(-d)
                                       : QString("expire dans %1 jour(s)").arg(d);
            const QString sms = m_smsEnvoyes.contains(cleSms(a, c))
                                    ? " — SMS de rappel envoyé à l'agent" : "";
            const Agent copie = a;
            const Certification cert = c;
            ajouterAlerte("Certification à renouveler",
                          QString("%1 — %2 %3%4").arg(who, c.nom, etat, sms), false, a.id,
                          "Renvoyer le SMS", [this, copie, cert] {
                              const int j = int(QDate::currentDate().daysTo(cert.echeance));
                              envoyerSms(copie.tel,
                                         QString("USPC - Rappel : votre certification \"%1\" %2 "
                                                 "(échéance %3). Contactez le service RH.")
                                             .arg(cert.nom,
                                                  j < 0 ? "est expirée" : QString("expire dans %1 jours").arg(j),
                                                  cert.echeance.toString("dd/MM/yyyy")),
                                         copie.nom + " " + copie.prenom);
                          });
            ++n;
        }
        // Innovation 2 : formations recommandées (résumé)
        for (const Recommandation &r : recommandationsPour(a)) {
            if (r.inscrit) continue;
            ++nRecos;
            agentsRecos.insert(a.id);
        }
    }
    if (nRecos > 0) {
        ajouterAlerte("Formations recommandées",
                      QString("%1 certification(s) à planifier pour %2 agent(s), avec lieu et date de session.")
                          .arg(nRecos).arg(agentsRecos.size()),
                      true, QString(), "Voir les formations ›", [this] { afficherFormations(); });
        ++n;
    }
    if (n == 0) ajouterAlerte("Aucune alerte", "Tout est en ordre.", true, QString());
}

// ============================================================================
//  Innovation 1 : SMS de renouvellement des certifications
// ============================================================================
QString FireStation::cleSms(const Agent &a, const Certification &c) const
{
    return a.id + "|" + c.nom + "|" + c.echeance.toString(Qt::ISODate);
}

// Envoie automatiquement un SMS pour chaque certification qui arrive à échéance
// (une seule fois par certification et par échéance).
void FireStation::verifierEcheancesSms()
{
    const QDate today = QDate::currentDate();
    for (const Agent &a : m_agents) {
        for (const Certification &c : a.certs) {
            const int d = int(today.daysTo(c.echeance));
            if (d > ALERTE_JOURS) continue;
            const QString cle = cleSms(a, c);
            if (m_smsEnvoyes.contains(cle)) continue;
            const QString msg = d < 0
                                    ? QString("USPC - Rappel : votre certification \"%1\" est expirée depuis le %2. "
                                              "Contactez le service RH pour la renouveler.")
                                          .arg(c.nom, c.echeance.toString("dd/MM/yyyy"))
                                    : QString("USPC - Rappel : votre certification \"%1\" expire le %2 (dans %3 jours). "
                                              "Contactez le service RH pour la renouveler.")
                                          .arg(c.nom, c.echeance.toString("dd/MM/yyyy")).arg(d);
            envoyerSms(a.tel, msg, a.nom + " " + a.prenom);
            m_smsEnvoyes.insert(cle);
        }
    }
}

// Envoi d'un SMS en mode simulation : le message est enregistré dans le Journal SMS.
// (Pour un envoi réel, il suffirait de remplacer le contenu de cette fonction
//  par l'appel au service SMS d'un opérateur.)
void FireStation::envoyerSms(const QString &tel, const QString &message, const QString &destinataire)
{
    QString numero = tel;
    numero.remove(' ');
    const QString horodatage = QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm");
    m_journalSms.prepend(QString("[%1] → %2 (%3) : %4").arg(horodatage, destinataire, numero, message));
    statusBar()->showMessage(QString("SMS simulé envoyé à %1 (%2)").arg(destinataire, numero), 5000);
}

// Réinitialisation du mot de passe depuis la page de connexion (code reçu par SMS)
void FireStation::changerMotDePasse(const QString &identifiant, const QString &mdpHash)
{
    const int i = indexParId(identifiant);
    if (i >= 0) m_agents[i].mdpHash = mdpHash;
}

void FireStation::afficherJournalSms()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Journal des SMS");
    dlg.resize(720, 400);
    auto *l = new QVBoxLayout(&dlg);
    l->addWidget(etiquette("Mode simulation : les SMS sont enregistrés dans ce journal.", "sous"));
    auto *liste = new QListWidget;
    liste->setWordWrap(true);
    liste->setAlternatingRowColors(true);
    if (m_journalSms.isEmpty()) liste->addItem("Aucun SMS envoyé pour le moment.");
    else liste->addItems(m_journalSms);
    l->addWidget(liste, 1);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    l->addWidget(bb);
    dlg.exec();
}

// ============================================================================
//  Innovation 2 : recommandation de certifications
// ============================================================================
// Pour un agent, détermine les certifications requises par son poste, sa
// spécialité et son grade, puis propose la prochaine session disponible
// (nom, lieu, date) pour celles qui manquent ou qui arrivent à échéance.
QList<Recommandation> FireStation::recommandationsPour(const Agent &a) const
{
    const QDate today = QDate::currentDate();
    const int g = int(GRADES.indexOf(a.grade));
    const bool op = estOperationnel(a.poste);

    // dernière certification connue portant ce nom
    auto trouver = [&](const QString &nom) -> const Certification * {
        const Certification *best = nullptr;
        for (const Certification &c : a.certs)
            if (c.nom.compare(nom, Qt::CaseInsensitive) == 0 && (!best || c.echeance > best->echeance))
                best = &c;
        return best;
    };

    // 1) certifications requises (nom, motif)
    QList<QPair<QString, QString>> requises;
    auto requiert = [&](const QString &cert, const QString &motif) {
        for (const auto &r : requises) if (r.first == cert) return;
        requises.append({cert, motif});
    };
    if (op)                            requiert(C_PSE1,  "obligatoire pour tout agent opérationnel");
    if (a.poste == "Responsable RH")     requiert(C_PSE1,  "premiers secours pour tout le personnel");
    if (a.poste == "Chauffeur")          requiert(C_COND,  "requise pour le poste Chauffeur");
    if (a.poste == "Maître-chien")       requiert(C_CYNO,  "requise pour le poste Maître-chien");
    if (a.poste == "Mécanicien")         requiert(C_MAINT, "requise pour le poste Mécanicien");
    if (op) {
        if (a.specialite == "Sauvetage-déblaiement") {
            requiert(C_SD1, "spécialité Sauvetage-déblaiement");
            if (trouver(C_SD1) && g >= GRADES.indexOf("Sergent"))
                requiert(C_SD2, "évolution de carrière (grade " + a.grade + ")");
        }
        if (a.specialite == "Recherche cynophile")      requiert(C_CYNO, "spécialité Recherche cynophile");
        if (a.specialite == "Risques chimiques (NRBC)") requiert(C_NRBC, "spécialité Risques chimiques");
        if (a.specialite == "Secourisme")               requiert(C_PSE2, "spécialité Secourisme");
        if (a.specialite == "Lutte contre l'incendie")  requiert(C_FDF,  "spécialité Lutte contre l'incendie");
        if (g >= GRADES.indexOf("Adjudant"))
            requiert(C_GOC, "requise à partir du grade Adjudant");
    }

    // 2) comparaison avec les certifications de l'agent + recherche de session
    QList<Recommandation> out;
    for (const auto &req : requises) {
        Recommandation r;
        r.agentId = a.id;
        r.certification = req.first;
        r.motif = req.second;

        if (const Certification *c = trouver(req.first)) {
            const int d = int(today.daysTo(c->echeance));
            if (d > RENOUVELLEMENT_JOURS) continue;           // encore valable : rien à proposer
            r.statut  = d < 0 ? QString("Expirée depuis %1 j").arg(-d) : QString("Expire dans %1 j").arg(d);
            r.urgente = d <= ALERTE_JOURS;
        } else {
            r.statut  = "Manquante";
            r.urgente = false;
        }

        // prochaine session à venir pour cette certification
        for (int i = 0; i < m_sessions.size(); ++i) {
            const SessionFormation &s = m_sessions[i];
            if (s.certification != req.first || s.date < today) continue;
            if (s.inscrits.contains(a.id)) { r.sessionIndex = i; r.inscrit = true; break; }
            if (s.places <= 0) continue;
            if (r.sessionIndex < 0 || s.date < m_sessions[r.sessionIndex].date) r.sessionIndex = i;
        }
        out.append(r);
    }
    return out;
}

void FireStation::inscrire(const QString &agentId, int sessionIndex)
{
    const int i = indexParId(agentId);
    if (i < 0 || sessionIndex < 0 || sessionIndex >= m_sessions.size()) return;
    SessionFormation &s = m_sessions[sessionIndex];
    const Agent &a = m_agents[i];
    if (s.places <= 0 || s.inscrits.contains(agentId)) return;

    if (QMessageBox::question(this, "Inscription",
                              QString("Inscrire %1 %2 à la formation :\n\n%3\n%4\nle %5 ?\n\n"
                                      "Une convocation sera envoyée par SMS.")
                                  .arg(a.nom, a.prenom, s.certification, s.lieu,
                                       s.date.toString("dd/MM/yyyy")))
        != QMessageBox::Yes) return;

    s.inscrits << agentId;
    --s.places;
    envoyerSms(a.tel,
               QString("USPC - Convocation : vous êtes inscrit(e) à la formation \"%1\" le %2, lieu : %3.")
                   .arg(s.certification, s.date.toString("dd/MM/yyyy"), s.lieu),
               a.nom + " " + a.prenom);
    afficherFormations();       // rafraîchit la page
    mettreAJourAlertes();
}

QWidget *FireStation::creerFormationsWidget()
{
    auto *root = new QWidget;
    root->setObjectName("statsRoot");
    auto *v = new QVBoxLayout(root);
    v->setContentsMargins(24, 20, 24, 20);
    v->setSpacing(14);

    // --- en-tête
    auto *head = new QHBoxLayout;
    head->addLayout(enTetePage("Formations", "Formations et certifications recommandées",
                               "Certifications à passer selon le poste, la spécialité et le grade de chaque agent"), 1);
    auto *retour = new QPushButton("‹ Retour à la liste");
    retour->setObjectName("retour");
    head->addWidget(retour, 0, Qt::AlignTop);
    v->addLayout(head);
    connect(retour, &QPushButton::clicked, this, [this] { allerPage(0); });

    // --- filtre par agent
    auto *rf = new QHBoxLayout;
    rf->addWidget(new QLabel("Agent :"));
    auto *cb = new QComboBox;
    cb->addItem("Tous les agents", QString());
    for (const Agent &a : m_agents)
        cb->addItem(QString("%1 — %2 %3").arg(a.id, a.nom, a.prenom), a.id);
    const int sel = cb->findData(m_formFiltre);
    cb->setCurrentIndex(sel < 0 ? 0 : sel);
    rf->addWidget(cb);
    rf->addStretch();
    v->addLayout(rf);
    connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, cb](int) {
        m_formFiltre = cb->currentData().toString();
        QTimer::singleShot(0, this, [this] { afficherFormations(); });
    });

    // --- calcul des recommandations
    QList<Recommandation> recos;
    for (const Agent &a : m_agents) {
        if (!m_formFiltre.isEmpty() && a.id != m_formFiltre) continue;
        recos += recommandationsPour(a);
    }
    std::stable_sort(recos.begin(), recos.end(), [](const Recommandation &x, const Recommandation &y) {
        if (x.inscrit != y.inscrit) return !x.inscrit;
        return x.urgente && !y.urgente;
    });
    int nUrg = 0, nIns = 0, nAVenir = 0;
    for (const Recommandation &r : recos) { if (r.urgente && !r.inscrit) ++nUrg; if (r.inscrit) ++nIns; }
    for (const SessionFormation &s : m_sessions) if (s.date >= QDate::currentDate()) ++nAVenir;

    auto *kp = new QHBoxLayout;
    kp->setSpacing(12);
    QList<QFrame *> kpis = {carteKpi(int(recos.size()) - nIns, "Certifications à planifier", "#b3211c", 100),
                            carteKpi(nUrg, "Urgentes (échéance proche ou dépassée)", "#c46a00", 180),
                            carteKpi(nIns, "Inscriptions effectuées", "#1e8e5a", 260),
                            carteKpi(nAVenir, "Sessions à venir", "#2f6fb3", 340)};
    for (int i = 0; i < kpis.size(); ++i) { kp->addWidget(kpis[i], 1); fondu(kpis[i], 400, 80 * i); }
    v->addLayout(kp);

    // --- tableau des recommandations
    QVBoxLayout *lt = nullptr;
    QFrame *cT = nouvelleCarte(&lt);
    lt->addWidget(etiquette("Certifications proposées", "h3"));
    lt->addWidget(etiquette("Pour chaque certification : nom, lieu et date de la prochaine session", "sous"));
    lt->addSpacing(6);

    auto *t = new QTableWidget(int(recos.size()), 7);
    t->setHorizontalHeaderLabels({"AGENT", "CERTIFICATION", "MOTIF", "ÉTAT", "LIEU", "DATE", "ACTION"});
    t->verticalHeader()->hide();
    t->setShowGrid(false);
    t->setMouseTracking(true);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    t->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    t->setWordWrap(true);

    for (int r = 0; r < recos.size(); ++r) {
        const Recommandation &rc = recos[r];
        const Agent &a = m_agents[indexParId(rc.agentId)];
        t->setRowHeight(r, 46);
        t->setItem(r, 0, new QTableWidgetItem(a.nom + " " + a.prenom));
        auto *ic = new QTableWidgetItem(rc.certification);
        QFont bf = ic->font(); bf.setBold(true); ic->setFont(bf);
        t->setItem(r, 1, ic);
        auto *im = new QTableWidgetItem(rc.motif);
        im->setForeground(QColor("#777777"));
        t->setItem(r, 2, im);

        auto *badge = new QLabel(rc.inscrit ? "Inscrit ✓" : rc.statut);
        badge->setObjectName(rc.inscrit ? "badgeOk" : (rc.urgente ? "badgeUrgent" : "badgeNormal"));
        auto *bw = new QWidget;
        auto *bl = new QHBoxLayout(bw);
        bl->setContentsMargins(4, 0, 4, 0);
        bl->addWidget(badge);
        bl->addStretch();
        t->setCellWidget(r, 3, bw);

        if (rc.sessionIndex >= 0) {
            const SessionFormation &s = m_sessions[rc.sessionIndex];
            t->setItem(r, 4, new QTableWidgetItem(s.lieu));
            t->setItem(r, 5, new QTableWidgetItem(QString("%1  (%2 pl.)")
                                                      .arg(s.date.toString("dd/MM/yyyy")).arg(s.places)));
        } else {
            auto *it = new QTableWidgetItem("Aucune session programmée");
            it->setForeground(QColor("#999999"));
            t->setItem(r, 4, it);
            t->setItem(r, 5, new QTableWidgetItem("—"));
        }

        if (!rc.inscrit && rc.sessionIndex >= 0 && !m_lectureSeule) {
            auto *b = new QPushButton("Inscrire");
            b->setObjectName("rouge");
            b->setCursor(Qt::PointingHandCursor);
            const QString id = rc.agentId;
            const int si = rc.sessionIndex;
            connect(b, &QPushButton::clicked, this, [this, id, si] { inscrire(id, si); });
            auto *aw = new QWidget;
            auto *al = new QHBoxLayout(aw);
            al->setContentsMargins(4, 4, 4, 4);
            al->addWidget(b);
            t->setCellWidget(r, 6, aw);
        }
    }
    if (recos.isEmpty())
        lt->addWidget(etiquette("Aucune certification à proposer : les profils sont à jour.", "sous"));
    else {
        t->setMinimumHeight(std::min(560, 46 * int(recos.size()) + 40));
        lt->addWidget(t);
    }
    v->addWidget(cT);

    // --- catalogue des sessions
    QVBoxLayout *lc = nullptr;
    QFrame *cC = nouvelleCarte(&lc);
    lc->addWidget(etiquette("Catalogue des sessions de formation", "h3"));
    lc->addWidget(etiquette("Sessions programmées par les centres de formation de la Protection civile", "sous"));
    lc->addSpacing(6);
    QList<int> idx;
    for (int i = 0; i < m_sessions.size(); ++i) idx << i;
    std::sort(idx.begin(), idx.end(), [this](int x, int y) { return m_sessions[x].date < m_sessions[y].date; });
    auto *tc = new QTableWidget(int(idx.size()), 5);
    tc->setHorizontalHeaderLabels({"DATE", "CERTIFICATION", "LIEU", "PLACES RESTANTES", "INSCRITS"});
    tc->verticalHeader()->hide();
    tc->setShowGrid(false);
    tc->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tc->setSelectionMode(QAbstractItemView::NoSelection);
    tc->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tc->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    for (int r = 0; r < idx.size(); ++r) {
        const SessionFormation &s = m_sessions[idx[r]];
        tc->setRowHeight(r, 36);
        tc->setItem(r, 0, new QTableWidgetItem(s.date.toString("dd/MM/yyyy")));
        tc->setItem(r, 1, new QTableWidgetItem(s.certification));
        tc->setItem(r, 2, new QTableWidgetItem(s.lieu));
        tc->setItem(r, 3, new QTableWidgetItem(QString::number(s.places)));
        QStringList noms;
        for (const QString &id : s.inscrits) {
            const int k = indexParId(id);
            if (k >= 0) noms << m_agents[k].nom;
        }
        tc->setItem(r, 4, new QTableWidgetItem(noms.isEmpty() ? "—" : noms.join(", ")));
    }
    tc->setMinimumHeight(36 * int(idx.size()) + 40);
    lc->addWidget(tc);
    v->addWidget(cC);
    v->addStretch();
    return root;
}

void FireStation::afficherFormations()
{
    const bool dejaOuverte = pages->currentIndex() == 2;
    formScroll->setWidget(creerFormationsWidget());
    if (!dejaOuverte) allerPage(2);
}

// ============================================================================
//  Export PDF du planning de garde
// ============================================================================
void FireStation::exporterPdf()
{
    // 1) Choix de la semaine et du nombre d'agents par garde
    QDialog dlg(this);
    dlg.setWindowTitle("Planning de garde hebdomadaire");
    auto *form = new QFormLayout(&dlg);
    const QDate auj = QDate::currentDate();
    auto *deSemaine = new QDateEdit(auj.addDays(8 - auj.dayOfWeek()));   // lundi prochain
    deSemaine->setCalendarPopup(true);
    deSemaine->setDisplayFormat("dd/MM/yyyy");
    auto *sbNombre = new QSpinBox;
    sbNombre->setRange(1, 8);
    sbNombre->setValue(3);
    form->addRow("Semaine du :", deSemaine);
    form->addRow("Agents par garde :", sbNombre);
    auto *aide = new QLabel("Le planning commence le lundi de la semaine choisie.\n"
                            "Chaque jour : garde de jour (08h-20h) et garde de nuit (20h-08h).");
    aide->setStyleSheet("color:#777;");
    form->addRow(aide);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(bb);
    if (dlg.exec() != QDialog::Accepted) return;

    const QDate lundi = deSemaine->date().addDays(1 - deSemaine->date().dayOfWeek());
    const QString path = QFileDialog::getSaveFileName(
        this, "Exporter le planning de garde",
        QString("planning_garde_%1.pdf").arg(lundi.toString("yyyy-MM-dd")), "PDF (*.pdf)");
    if (path.isEmpty()) return;

    if (genererPlanningPdf(path, lundi, sbNombre->value()))
        QMessageBox::information(this, "Export", "Planning exporté :\n" + path);
}

// Construit le planning de la semaine par rotation et l'exporte en PDF.
// Règles : un gradé (Sergent ou plus) par garde, un chauffeur si possible,
// pas deux gardes consécutives, agents en congé ou en formation exclus,
// et répartition équitable (on choisit d'abord ceux qui ont le moins de gardes).
bool FireStation::genererPlanningPdf(const QString &chemin, QDate debut, int agentsParGarde)
{
    // --- Agents pouvant être planifiés
    QList<const Agent *> equipe, exclus;
    for (const Agent &a : m_agents) {
        if (!estOperationnel(a.poste)) continue;
        if (a.dispo == "En congé" || a.dispo == "En formation") exclus << &a;
        else equipe << &a;
    }
    if (equipe.size() < 2) {
        QMessageBox::warning(this, "Planning", "Il faut au moins 2 agents opérationnels disponibles.");
        return false;
    }
    // Rotation d'une semaine à l'autre : l'ordre de départ des agents est décalé selon
    // le numéro de la semaine, pour que les équipes changent chaque semaine
    // (la même semaine redonne toujours le même planning, ce qui permet de le réimprimer).
    int annee = 0;
    const int semaine = debut.weekNumber(&annee);
    const int decalage = (semaine * 3 + annee) % int(equipe.size());
    std::rotate(equipe.begin(), equipe.begin() + decalage, equipe.end());
    // Une semaine sur deux, les chauffeurs commencent par une garde de nuit
    // (sinon un chauffeur seul ferait toujours les gardes de jour)
    const bool chauffeursDeNuit = semaine % 2 == 1;

    // Pour éviter deux gardes de suite, il faut au moins 2 équipes complètes
    const int parGarde = std::max(1, std::min(agentsParGarde, int(equipe.size()) / 2));

    // --- Construction des 14 gardes (7 jours x jour/nuit) par rotation
    const int sergent = int(GRADES.indexOf("Sergent"));
    // Plafond de gardes par agent sur la semaine (répartition équitable)
    const int plafond = (14 * parGarde + int(equipe.size()) - 1) / int(equipe.size());
    QMap<const Agent *, int> nbJour, nbNuit;
    QList<const Agent *> precedente;
    QVector<QList<const Agent *>> gardes(14);

    for (int g = 0; g < 14; ++g) {
        QList<const Agent *> choisis;
        auto nbTotal = [&](const Agent *a) { return nbJour.value(a) + nbNuit.value(a); };
        // candidats libres, triés : le moins de gardes d'abord
        auto candidats = [&](auto filtre) {
            QList<const Agent *> c;
            for (const Agent *a : equipe)
                if (!choisis.contains(a) && !precedente.contains(a) && nbTotal(a) < plafond && filtre(a))
                    c << a;
            std::stable_sort(c.begin(), c.end(), [&](const Agent *x, const Agent *y) {
                return nbTotal(x) < nbTotal(y);
            });
            return c;
        };
        // 1) un gradé, qui sera chef de garde
        const auto grades = candidats([&](const Agent *a) { return GRADES.indexOf(a->grade) >= sergent; });
        if (!grades.isEmpty()) choisis << grades.first();
        const bool sansChauffeur = (g == 0 && chauffeursDeNuit);
        // 2) un chauffeur pour conduire l'engin
        if (choisis.size() < parGarde && !sansChauffeur) {
            const auto chauffeurs = candidats([](const Agent *a) { return a->poste == "Chauffeur"; });
            if (!chauffeurs.isEmpty()) choisis << chauffeurs.first();
        }
        // 3) compléter l'équipe
        auto autres = candidats([&](const Agent *a) { return !(sansChauffeur && a->poste == "Chauffeur"); });
        while (choisis.size() < parGarde && !autres.isEmpty()) choisis << autres.takeFirst();
        // 4) en dernier recours (effectif trop faible), lever les contraintes
        for (const Agent *a : equipe)
            if (choisis.size() < parGarde && !choisis.contains(a)) choisis << a;

        for (const Agent *a : choisis) (g % 2 == 0 ? nbJour : nbNuit)[a]++;
        std::stable_sort(choisis.begin(), choisis.end(), [](const Agent *x, const Agent *y) {
            return GRADES.indexOf(x->grade) > GRADES.indexOf(y->grade);   // chef de garde en premier
        });
        gardes[g] = choisis;
        precedente = choisis;
    }

    // --- Mise en page HTML
    QString chef = "—", rh = "—";
    for (const Agent &ag : m_agents) {
        if (ag.poste == "Chef de caserne" && chef == "—") chef = ag.nom + " " + ag.prenom;
        if (ag.poste == "Responsable RH" && rh == "—")   rh   = ag.nom + " " + ag.prenom;
    }
    const QStringList jours = {"Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi", "Dimanche"};
    auto cellule = [](const QList<const Agent *> &l) {
        QStringList lignes;
        for (int i = 0; i < l.size(); ++i) {
            const Agent *a = l[i];
            lignes << QString("%1%2 %3 %4 <span style='color:#777777'>(%5)</span>")
                          .arg(i == 0 ? "&#9733; " : "", a->grade, a->nom.toHtmlEscaped(),
                               a->prenom.toHtmlEscaped(), a->poste);
        }
        return lignes.join("<br>");
    };

    QString lignesPlanning;
    for (int j = 0; j < 7; ++j) {
        const QDate d = debut.addDays(j);
        const QString fond = j >= 5 ? "#fdf3e1" : "#ffffff";   // week-end en couleur
        lignesPlanning += QString("<tr bgcolor='%1'><td><b>%2</b><br>%3</td><td>%4</td><td>%5</td></tr>")
                              .arg(fond, jours[j], d.toString("dd/MM/yyyy"),
                                   cellule(gardes[2 * j]), cellule(gardes[2 * j + 1]));
    }

    QString lignesBilan;
    QList<const Agent *> tries = equipe;
    std::stable_sort(tries.begin(), tries.end(), [](const Agent *x, const Agent *y) {
        return GRADES.indexOf(x->grade) > GRADES.indexOf(y->grade);
    });
    for (const Agent *a : tries)
        lignesBilan += QString("<tr><td>%1 %2</td><td>%3</td><td>%4</td><td>%5</td>"
                               "<td align='center'>%6</td><td align='center'>%7</td><td align='center'><b>%8</b></td></tr>")
                           .arg(a->nom.toHtmlEscaped(), a->prenom.toHtmlEscaped(), a->poste, a->grade, a->tel)
                           .arg(nbJour.value(a)).arg(nbNuit.value(a)).arg(nbJour.value(a) + nbNuit.value(a));

    QStringList nomsExclus;
    for (const Agent *a : exclus) nomsExclus << QString("%1 %2 (%3)").arg(a->nom, a->prenom, a->dispo);
    const QString remarque = parGarde < agentsParGarde
                                 ? QString("<p style='color:#b3211c'>Effectif insuffisant : %1 agent(s) par garde au lieu de %2.</p>")
                                       .arg(parGarde).arg(agentsParGarde)
                                 : QString();

    const QString html = QString(
                             "<table width='100%'><tr>"
                             "<td width='80'><img src='logo' width='70' height='70'></td>"
                             "<td><h2 style='color:#b3211c'>Planning de garde — semaine du %1 au %2</h2>"
                             "%3<br>Chef de caserne : %4 — Responsable RH : %5 — Édité le %6</td>"
                             "</tr></table><br>%7"
                             "<table border='1' cellspacing='0' cellpadding='5' width='100%'>"
                             "<tr bgcolor='#b3211c'><th><font color='white'>Jour</font></th>"
                             "<th><font color='white'>Garde de jour (08h – 20h)</font></th>"
                             "<th><font color='white'>Garde de nuit (20h – 08h)</font></th></tr>%8</table>"
                             "<p style='font-size:9pt;color:#555555'>&#9733; Chef de garde (agent le plus gradé). "
                             "Règles : un gradé (Sergent ou plus) par garde, un chauffeur si disponible, "
                             "pas deux gardes consécutives, répartition équitable.<br>"
                             "Agents non planifiés : %9</p>")
                             .arg(debut.toString("dd/MM/yyyy"), debut.addDays(6).toString("dd/MM/yyyy"), UNITE, chef, rh,
                                  QDate::currentDate().toString("dd/MM/yyyy"), remarque, lignesPlanning,
                                  nomsExclus.isEmpty() ? "aucun" : nomsExclus.join(", "))
                         + QString("<h3 style='page-break-before:always'>Récapitulatif des gardes par agent</h3>"
                                   "<table border='1' cellspacing='0' cellpadding='4' width='100%'>"
                                   "<tr bgcolor='#dddddd'><th>Agent</th><th>Poste</th><th>Grade</th><th>Téléphone</th>"
                                   "<th>Jour</th><th>Nuit</th><th>Total</th></tr>%1</table>")
                               .arg(lignesBilan);

    QPdfWriter writer(chemin);
    writer.setPageLayout(QPageLayout(QPageSize(QPageSize::A4), QPageLayout::Landscape,
                                     QMarginsF(12, 12, 12, 12)));
    QTextDocument doc;
    QFont police("Arial");
    police.setPointSize(9);
    doc.setDefaultFont(police);
    doc.addResource(QTextDocument::ImageResource, QUrl("logo"),
                    QImage(":/logo_USPC.png").scaled(240, 240, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    doc.setHtml(html);
    doc.print(&writer);
    return true;
}