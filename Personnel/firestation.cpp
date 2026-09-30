#include "firestation.h"

#include <QApplication>
#include <QComboBox>
#include <QDateEdit>
#include <QFileDialog>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace {

// Valeurs par défaut affectées automatiquement à chaque nouvel agent
const QString DEF_RH   = "Mansouri Leila";
const QString DEF_CHEF = "Trabelsi Sami";

// Seuils des alertes (à adapter)
const int ALERTE_JOURS   = 30;  // alerte certification : X jours avant l'échéance
const int MAX_INTERV_48H = 5;   // surcharge : nombre d'interventions en 48 h
const int MIN_REPOS_H    = 11;  // repos réglementaire minimal (heures)

int ordreDispo(const QString &d)
{
    if (d == "Disponible") return 0;
    if (d == "En service") return 1;
    return 2;
}

// Un segment d'une barre de répartition
struct Seg {
    QString label;
    int     n;
    QColor  color;
};

// Graphique en barres verticales (charge de travail par agent)
class BarChart : public QWidget
{
public:
    explicit BarChart(const QList<Agent> &agents, QWidget *parent = nullptr)
        : QWidget(parent), m_agents(agents)
    {
        setMinimumHeight(250);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
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
            const int h = std::max(2, hMax * a.mois / max);
            const int x = int(slot * i + (slot - barW) / 2.0);
            const bool hot = a.interv48 >= MAX_INTERV_48H;

            QLinearGradient g(0, baseY - h, 0, baseY);
            g.setColorAt(0, hot ? QColor("#f5b041") : QColor("#d10000"));
            g.setColorAt(1, hot ? QColor("#c77d00") : QColor("#7a0000"));
            p.setPen(Qt::NoPen);
            p.setBrush(g);
            p.drawRoundedRect(QRect(x, baseY - h, barW, h), 3, 3);

            QFont bf = base; bf.setBold(true);
            p.setFont(bf);
            p.setPen(QColor("#222222"));
            p.drawText(QRect(int(slot * i), baseY - h - 20, int(slot), 18),
                       Qt::AlignCenter, QString::number(a.mois));

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
};

// Barre horizontale segmentée (répartitions)
class StackBar : public QWidget
{
public:
    explicit StackBar(const QList<Seg> &segs, QWidget *parent = nullptr)
        : QWidget(parent), m_segs(segs)
    {
        setFixedHeight(26);
        setMinimumWidth(100);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()), 6, 6);
        p.setClipPath(path);
        int total = 0;
        for (const Seg &s : m_segs) total += s.n;
        if (total <= 0) { p.fillRect(rect(), QColor("#eeeeee")); return; }
        double x = 0;
        for (const Seg &s : m_segs) {
            const double w = width() * double(s.n) / total;
            p.fillRect(QRectF(x, 0, w + 1, height()), s.color);
            x += w;
        }
    }

private:
    QList<Seg> m_segs;
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

QFrame *carteKpi(const QString &n, const QString &txt, const QString &couleur)
{
    QVBoxLayout *l = nullptr;
    QFrame *f = nouvelleCarte(&l);
    auto *num = new QLabel(n);
    num->setStyleSheet(QString("font-size:26px;font-weight:bold;color:%1;").arg(couleur));
    l->addWidget(num);
    l->addWidget(etiquette(txt, "sous"));
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

const char *STYLE = R"(
QWidget { color: #222; }
QComboBox QAbstractItemView { background: white; color: #222;
    selection-background-color: #f2a10c; selection-color: #222; }
QTableWidget { color: #222; }
QCalendarWidget QWidget { color: #222; background: white; }
QMainWindow, #main { background: #f4f2ef; }
#statsRoot { background: #f4f2ef; }
#sidebar { background: #b3211c; }
#sidebar QPushButton { color: white; background: transparent; border: none;
    text-align: left; padding: 10px 22px; font-size: 14px; }
#sidebar QPushButton:checked { background: rgba(255,255,255,0.22); font-weight: bold; }
#logo { background: white; border-radius: 75px; }
#titre { font-size: 26px; font-weight: bold; color: #222; }
#sous { color: #777; }
#card { background: white; border: 1px solid #e4e0da; border-radius: 10px; }
#card QLabel { background: transparent; }
#h3 { font-size: 16px; font-weight: bold; }
QLabel[champ="true"] { color: #777; font-size: 12px; }
QLineEdit, QComboBox, QDateEdit { background: #faf9f7; border: 1px solid #e4e0da;
    border-radius: 6px; padding: 6px 8px; min-height: 20px; }
QComboBox:disabled { color: #aaa; }
#certRow { background: #faf9f7; border: 1px solid #e4e0da; border-radius: 8px; }
#certRow QLabel { background: transparent; }
#certRow QLineEdit, #certRow QDateEdit { background: white; }
QPushButton { background: white; border: 1px solid #e4e0da; border-radius: 6px; padding: 8px 14px; }
QPushButton:disabled { color: #aaa; }
QPushButton#orange { background: #f2a10c; border: none; font-weight: bold; color: #222; }
QPushButton#rouge  { background: #b3211c; border: none; font-weight: bold; color: white; }
QPushButton#rouge:disabled { background: #d9a09d; }
QPushButton#retour { color: #b3211c; font-weight: bold; border: 1px solid #e8b4b1; }
QToolButton { background: white; border: 1px solid #e4e0da; border-radius: 6px; padding: 4px 8px; }
QTableWidget { background: white; border: none; gridline-color: transparent; }
QHeaderView::section { background: white; border: none; border-bottom: 1px solid #e4e0da;
    color: #777; font-weight: bold; padding: 6px; }
#alerte { background: #fbe4e2; border-radius: 8px; }
#alerteInfo { background: #e3eefa; border-radius: 8px; }
#alerte QLabel, #alerteInfo QLabel { background: transparent; }
#pastille { background: #b3211c; color: white; border-radius: 5px; font-weight: bold; }
#erreur { color: #b3211c; font-size: 12px; }
)";

} // namespace

// ============================================================================
//  Construction
// ============================================================================
FireStation::FireStation(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("Smart Fire Station — Gestion du personnel");
    setStyleSheet(STYLE);
    chargerDonnees();

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
    content->addWidget(etiquette("Module 5 — Gestion du personnel et fonctionnalités innovantes", "sous"));

    auto *bar = new QHBoxLayout;
    bar->addWidget(new QLabel("Trier par :"));
    cbTri = new QComboBox;
    cbTri->addItems({"Disponibilité", "Nom", "Grade"});
    bar->addWidget(cbTri);
    auto *btnPdf = new QPushButton("↓ Exporter le planning de garde (PDF)");
    btnPdf->setObjectName("orange");
    auto *btnStats = new QPushButton("Statistiques (charge de travail)");
    btnStats->setObjectName("orange");
    bar->addWidget(btnPdf);
    bar->addWidget(btnStats);
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

    root->addWidget(pages, 1);
    setCentralWidget(central);

    connect(btnPdf,   &QPushButton::clicked, this, &FireStation::exporterPdf);
    connect(btnStats, &QPushButton::clicked, this, &FireStation::afficherStats);
    connect(navPersonnel, &QPushButton::clicked, this, [this] { pages->setCurrentIndex(0); });
    connect(cbTri, &QComboBox::currentTextChanged, this, [this] { rafraichir(); });

    reinitialiser();
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

    auto *logo = new QLabel;
    logo->setObjectName("logo");
    logo->setFixedSize(150, 150);
    logo->setAlignment(Qt::AlignCenter);
    logo->setPixmap(QPixmap(":/logo.png").scaled(104, 104, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    l->addWidget(logo, 0, Qt::AlignHCenter);
    l->addSpacing(20);

    const QStringList items = {"Tableau de bord", "Incidents", "Interventions", "Personnel",
                               "Véhicules", "Équipements", "Rapports"};
    for (const QString &t : items) {
        auto *b = new QPushButton(t);
        b->setCheckable(true);
        b->setAutoExclusive(true);
        b->setChecked(t == "Personnel");
        b->setCursor(Qt::PointingHandCursor);
        if (t == "Personnel") navPersonnel = b;
        l->addWidget(b);
    }
    l->addStretch();
    return side;
}

QWidget *FireStation::creerFormulaire()
{
    auto *card = new QFrame;
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

    champ("Fonction");
    cbFonction = new QComboBox;
    cbFonction->addItems({"Pompier", "Chauffeur", "Mécanicien", "Administratif"});
    l->addWidget(cbFonction);

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
    cbGrade->addItems({"Sapeur", "Caporal", "Sergent", "Lieutenant", "Capitaine"});
    cbSpec = new QComboBox;
    cbSpec->addItems({"Incendie", "Sauvetage", "Secourisme", "Chimique"});
    auto *lG = new QLabel("Grade");      lG->setProperty("champ", "true");
    auto *lS = new QLabel("Spécialité"); lS->setProperty("champ", "true");
    cG->addWidget(lG); cG->addWidget(cbGrade);
    cS->addWidget(lS); cS->addWidget(cbSpec);
    rGr->addLayout(cG); rGr->addLayout(cS);
    l->addLayout(rGr);

    champ("Téléphone");
    edTel = new QLineEdit;
    edTel->setPlaceholderText("+216 20 123 456");
    l->addWidget(edTel);

    champ("Disponibilité");
    cbDispo = new QComboBox;
    cbDispo->addItems({"Disponible", "En service", "Repos"});
    l->addWidget(cbDispo);

    champ("Certifications (nom, date d'obtention, date d'échéance)");
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setFixedHeight(180);
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

    connect(cbFonction, &QComboBox::currentTextChanged, this, [this] { onFonctionChanged(); });
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
    cbFiltreFonction = new QComboBox;
    cbFiltreFonction->addItems({"Toutes fonctions", "Pompier", "Chauffeur", "Mécanicien", "Administratif"});
    rf->addWidget(edRecherche, 1);
    rf->addWidget(cbFiltreFonction);
    l->addLayout(rf);

    tblAgents = new QTableWidget(0, 7);
    tblAgents->setHorizontalHeaderLabels({"ID", "NOM", "FONCTION", "GRADE", "SPÉCIALITÉ",
                                          "DISPONIBILITÉ", "ACTION"});
    tblAgents->verticalHeader()->hide();
    tblAgents->setShowGrid(false);
    tblAgents->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tblAgents->setSelectionBehavior(QAbstractItemView::SelectRows);
    tblAgents->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
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
    connect(cbFiltreFonction, &QComboBox::currentTextChanged, this, [this] { rafraichir(); });
    return wrap;
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
    auto *left = new QVBoxLayout;
    left->setSpacing(2);
    left->addWidget(new QLabel("<span style='color:#777'>Personnel&nbsp;&nbsp;›&nbsp;&nbsp;</span>"
                               "<b style='color:#b3211c'>Statistiques</b>"));
    left->addWidget(etiquette("Statistiques — Gestion du personnel", "titre"));
    left->addWidget(etiquette("Charge de travail, disponibilité et répartition de l'effectif", "sous"));
    head->addLayout(left, 1);

    m_btnStatsRetour = new QPushButton("‹ Retour à la liste");
    m_btnStatsRetour->setObjectName("retour");
    m_btnStatsExport = new QPushButton("↓ Exporter en PDF");
    head->addWidget(m_btnStatsRetour, 0, Qt::AlignTop);
    head->addWidget(m_btnStatsExport, 0, Qt::AlignTop);
    v->addLayout(head);
    connect(m_btnStatsRetour, &QPushButton::clicked, this, [this] { pages->setCurrentIndex(0); });
    connect(m_btnStatsExport, &QPushButton::clicked, this, &FireStation::exporterStatsPdf);

    // --- calculs à partir des données réelles
    const int total = int(m_agents.size());
    int nDispo = 0, nServ = 0, nRepos = 0, nPomp = 0;
    bool hot = false;
    for (const Agent &a : m_agents) {
        if (a.dispo == "Disponible") ++nDispo;
        else if (a.dispo == "En service") ++nServ;
        else ++nRepos;
        if (a.fonction == "Pompier") ++nPomp;
        if (a.interv48 >= MAX_INTERV_48H) hot = true;
    }
    QList<Agent> tri = m_agents;
    std::stable_sort(tri.begin(), tri.end(),
                     [](const Agent &x, const Agent &y) { return x.mois > y.mois; });

    // --- cartes de chiffres
    auto *kp = new QHBoxLayout;
    kp->setSpacing(12);
    kp->addWidget(carteKpi(QString::number(total),  "Effectif total", "#b3211c"), 1);
    kp->addWidget(carteKpi(QString::number(nDispo), "Disponibles",    "#1e8e5a"), 1);
    kp->addWidget(carteKpi(QString::number(nServ),  "En service",     "#c46a00"), 1);
    kp->addWidget(carteKpi(QString::number(nRepos), "En repos",       "#777777"), 1);
    v->addLayout(kp);

    // --- ligne du milieu : graphique + répartitions
    auto *mid = new QHBoxLayout;
    mid->setSpacing(14);

    QVBoxLayout *lc = nullptr;
    QFrame *cChart = nouvelleCarte(&lc);
    lc->addWidget(etiquette("Charge de travail par agent", "h3"));
    lc->addWidget(etiquette("Nombre d'interventions — 30 derniers jours", "sous"));
    lc->addWidget(new BarChart(tri), 1);
    if (hot)
        lc->addWidget(etiquette(QString("Orange : agent en risque de surcharge (%1 interventions ou plus en 48 h)")
                                    .arg(MAX_INTERV_48H), "sous"));
    mid->addWidget(cChart, 11);

    QList<Seg> gr = {{"Capitaine", 0, QColor("#8b0000")}, {"Lieutenant", 0, QColor("#c00000")},
                     {"Sergent", 0, QColor("#e06a6a")},   {"Caporal", 0, QColor("#f0b8b8")},
                     {"Sapeur", 0, QColor("#f8dada")}};
    QList<Seg> sp = {{"Incendie", 0, QColor("#1e5fb3")},  {"Sauvetage", 0, QColor("#4f93e0")},
                     {"Chimique", 0, QColor("#8fb8ea")},  {"Secourisme", 0, QColor("#c5daf5")}};
    for (const Agent &a : m_agents) {
        if (a.fonction != "Pompier") continue;
        for (Seg &s : gr) if (s.label == a.grade)      ++s.n;
        for (Seg &s : sp) if (s.label == a.specialite) ++s.n;
    }
    QList<Seg> grV, spV;
    for (const Seg &s : gr) if (s.n > 0) grV.append(s);
    for (const Seg &s : sp) if (s.n > 0) spV.append(s);
    const QString sur = QString("Sur les %1 %2").arg(nPomp).arg(nPomp > 1 ? "pompiers" : "pompier");

    QVBoxLayout *lr = nullptr;
    QFrame *cRight = nouvelleCarte(&lr);
    ajouterDistribution(lr, "Répartition par grade", sur, grV, "Aucun pompier enregistré.");
    lr->addSpacing(18);
    ajouterDistribution(lr, "Répartition par spécialité", sur, spV, "Aucun pompier enregistré.");
    lr->addStretch();
    mid->addWidget(cRight, 10);
    v->addLayout(mid);

    // --- disponibilité de l'effectif
    QList<Seg> dv = {{"Disponible", nDispo, QColor("#1e8e5a")},
                     {"En service", nServ,  QColor("#c46a00")},
                     {"Repos",      nRepos, QColor("#9a9a9a")}};
    QList<Seg> dvV;
    for (const Seg &s : dv) if (s.n > 0) dvV.append(s);
    QVBoxLayout *ld = nullptr;
    QFrame *cDispo = nouvelleCarte(&ld);
    ajouterDistribution(ld, "Disponibilité de l'effectif", "Répartition actuelle des statuts",
                        dvV, "Aucun agent.");
    v->addWidget(cDispo);
    v->addStretch();
    return root;
}

void FireStation::afficherStats()
{
    QWidget *w = creerStatsWidget();
    statsScroll->setWidget(w);   // remplace (et supprime) l'ancienne page
    m_statsContent = w;
    pages->setCurrentIndex(1);
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
    auto ag = [&](QString id, QString nom, QString pre, QString fn, QString gr, QString sp,
                  QString tel, QString dispo, QList<Certification> certs,
                  int i48, int repos, int mois) {
        Agent a;
        a.id = id; a.nom = nom; a.prenom = pre; a.fonction = fn; a.grade = gr;
        a.specialite = sp; a.tel = tel; a.dispo = dispo; a.certs = certs;
        a.interv48 = i48; a.reposH = repos; a.mois = mois;
        a.rh = DEF_RH; a.chef = DEF_CHEF;
        m_agents.append(a);
    };
    ag("POM-014", "Ben Ali", "Karim", "Pompier", "Sergent", "Sauvetage", "+216 20 123 456",
       "Disponible", {{"Sauvetage", t.addDays(-300), t.addDays(65)}}, 2, 14, 14);
    ag("POM-015", "Trabelsi", "Sami", "Pompier", "Capitaine", "Incendie", "+216 22 555 010",
       "En service", {{"Incendie", t.addDays(-200), t.addDays(160)}}, 6, 9, 9);
    ag("POM-016", "Gharbi", "Amine", "Mécanicien", "", "", "+216 98 765 432",
       "Disponible", {}, 0, 20, 16);
    ag("POM-017", "Jlassi", "Nour", "Pompier", "Sergent", "Secourisme", "+216 50 321 987",
       "Repos", {{"Secourisme", t.addDays(-353), t.addDays(12)}}, 1, 30, 6);
    ag("POM-018", "Mansouri", "Leila", "Administratif", "", "", "+216 71 000 111",
       "Disponible", {}, 0, 24, 0);
    ag("POM-019", "Sassi", "Mohamed", "Pompier", "Caporal", "Incendie", "+216 55 111 222",
       "Disponible", {{"Incendie", t.addDays(-100), t.addDays(250)}}, 2, 16, 11);
    ag("POM-020", "Amri", "Hedi", "Pompier", "Lieutenant", "Chimique", "+216 52 333 444",
       "En service", {{"Chimique", t.addDays(-150), t.addDays(200)}}, 1, 18, 4);
    ag("POM-021", "Ben Salah", "Walid", "Pompier", "Sapeur", "Sauvetage", "+216 53 555 666",
       "Disponible", {}, 3, 15, 8);
}

QString FireStation::nouvelId() const
{
    int max = 0;
    for (const Agent &a : m_agents)
        max = std::max(max, a.id.mid(4).toInt());
    return QString("POM-%1").arg(max + 1, 3, 10, QChar('0'));
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
void FireStation::onFonctionChanged()
{
    const bool p = cbFonction->currentText() == "Pompier";
    cbGrade->setEnabled(p);
    cbSpec->setEnabled(p);
}

void FireStation::ajouterCertLigne(const Certification &c)
{
    auto *frame = new QFrame;
    frame->setObjectName("certRow");
    auto *g = new QGridLayout(frame);
    g->setContentsMargins(8, 8, 8, 8);
    g->setHorizontalSpacing(6);
    g->setVerticalSpacing(3);

    auto *nom = new QLineEdit(c.nom);
    nom->setPlaceholderText("Nom de la certification");
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
    CertRow r;
    r.frame = frame; r.nom = nom; r.obt = obt; r.ech = ech;
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
    a.nom      = edNom->text().trimmed();
    a.prenom   = edPrenom->text().trimmed();
    a.fonction = cbFonction->currentText();
    const bool p = a.fonction == "Pompier";
    a.grade      = p ? cbGrade->currentText() : QString();
    a.specialite = p ? cbSpec->currentText()  : QString();
    a.tel        = edTel->text().trimmed();
    a.dispo      = cbDispo->currentText();

    if (a.nom.isEmpty() || a.prenom.isEmpty()) { erreur = "Nom et prénom obligatoires."; return false; }
    if (!QRegularExpression("^\\+?[0-9 ]{8,15}$").match(a.tel).hasMatch()) {
        erreur = "Numéro de téléphone invalide."; return false;
    }

    a.certs.clear();
    for (const CertRow &r : m_certRows) {
        Certification c;
        c.nom       = r.nom->text().trimmed();
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
    a.id   = nouvelId();
    a.rh   = DEF_RH;        // affectation automatique
    a.chef = DEF_CHEF;
    m_agents.append(a);
    reinitialiser();
    rafraichir();
}

void FireStation::modifier()
{
    const int i = indexParId(m_editingId);
    if (i < 0) return;
    Agent a; QString err;
    if (!lireFormulaire(a, err)) { lblErreur->setText(err); return; }
    Agent &old = m_agents[i];
    old.nom = a.nom; old.prenom = a.prenom; old.fonction = a.fonction; old.grade = a.grade;
    old.specialite = a.specialite; old.tel = a.tel; old.dispo = a.dispo; old.certs = a.certs;
    reinitialiser();
    rafraichir();
}

void FireStation::reinitialiser()
{
    m_editingId.clear();
    lblFormTitle->setText("＋ Nouvel agent");
    edNom->clear(); edPrenom->clear(); edTel->clear();
    cbFonction->setCurrentIndex(0);
    cbDispo->setCurrentIndex(0);
    viderCerts();
    ajouterCertLigne();          // une ligne de saisie toujours visible
    lblErreur->clear();
    onFonctionChanged();
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
    cbFonction->setCurrentText(a.fonction);
    onFonctionChanged();
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

    QMessageBox box(this);
    box.setWindowTitle("Fiche agent");
    box.setTextFormat(Qt::RichText);
    box.setText(QString("<h3>%1 — %2 %3</h3>"
                        "<p>%4 %5 %6<br>Tél : %7<br>Disponibilité : %8<br>"
                        "Responsable RH : %9<br>Chef de caserne : %10<br>"
                        "Interventions (48 h) : %11 — Repos : %12 h</p>"
                        "<b>Certifications</b><ul>%13</ul>")
                    .arg(a.id, a.nom.toHtmlEscaped(), a.prenom.toHtmlEscaped(),
                         a.fonction, a.grade, a.specialite, a.tel, a.dispo,
                         a.rh)
                    .arg(a.chef).arg(a.interv48).arg(a.reposH).arg(certs));
    box.exec();
}

void FireStation::supprimer(const QString &id)
{
    if (QMessageBox::question(this, "Supprimer", "Supprimer l'agent " + id + " ?")
        != QMessageBox::Yes) return;
    const int i = indexParId(id);
    if (i >= 0) m_agents.removeAt(i);
    if (m_editingId == id) reinitialiser();
    rafraichir();
}

// ============================================================================
//  Liste, tri, recherche, alertes
// ============================================================================
void FireStation::rafraichir()
{
    const QString q = edRecherche->text().trimmed().toLower();
    const QString f = cbFiltreFonction->currentIndex() == 0 ? QString() : cbFiltreFonction->currentText();

    QList<const Agent *> L;
    for (const Agent &a : m_agents) {
        if (!f.isEmpty() && a.fonction != f) continue;
        const QString hay = (a.nom + " " + a.prenom + " " + a.grade + " " + a.specialite + " " + a.dispo).toLower();
        if (!q.isEmpty() && !hay.contains(q)) continue;
        L << &a;
    }
    const QString tri = cbTri->currentText();
    std::stable_sort(L.begin(), L.end(), [&](const Agent *x, const Agent *y) {
        if (tri == "Nom")   return x->nom < y->nom;
        if (tri == "Grade") return x->grade < y->grade;
        return ordreDispo(x->dispo) < ordreDispo(y->dispo);
    });

    tblAgents->setRowCount(0);
    for (const Agent *a : L) {
        const int r = tblAgents->rowCount();
        tblAgents->insertRow(r);
        tblAgents->setRowHeight(r, 44);
        const QStringList vals = {a->id, a->nom + " " + a->prenom, a->fonction,
                                  a->grade.isEmpty() ? "—" : a->grade,
                                  a->specialite.isEmpty() ? "—" : a->specialite};
        for (int c = 0; c < vals.size(); ++c)
            tblAgents->setItem(r, c, new QTableWidgetItem(vals[c]));

        auto *disp = new QTableWidgetItem(a->dispo);
        disp->setTextAlignment(Qt::AlignCenter);
        if (a->dispo == "Disponible")      { disp->setBackground(QColor("#dff3e5")); disp->setForeground(QColor("#1e7a3c")); }
        else if (a->dispo == "En service") { disp->setBackground(QColor("#fdebd0")); disp->setForeground(QColor("#a86200")); }
        else                               { disp->setBackground(QColor("#eeeeee")); disp->setForeground(QColor("#777777")); }
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
        connect(mk("Modifier", "Modifier"), &QToolButton::clicked, this, [this, id] { chargerDansFormulaire(id); });
        connect(mk("Suppr.", "Supprimer"),  &QToolButton::clicked, this, [this, id] { supprimer(id); });
        tblAgents->setCellWidget(r, 6, w);
    }
    mettreAJourAlertes();
}

void FireStation::ajouterAlerte(const QString &titre, const QString &detail,
                                bool info, const QString &idAgent)
{
    auto *f = new QFrame;
    f->setObjectName(info ? "alerteInfo" : "alerte");
    auto *h = new QHBoxLayout(f);
    h->setContentsMargins(12, 10, 12, 10);

    auto *p = new QLabel(info ? "✓" : "!");
    p->setObjectName("pastille");
    p->setFixedSize(24, 24);
    p->setAlignment(Qt::AlignCenter);
    h->addWidget(p);

    auto *t = new QLabel(QString("<b>%1</b><br><span style='color:#666'>%2</span>")
                             .arg(titre.toHtmlEscaped(), detail.toHtmlEscaped()));
    t->setWordWrap(true);
    h->addWidget(t, 1);

    if (!idAgent.isEmpty()) {
        auto *b = new QPushButton("Voir ›");
        connect(b, &QPushButton::clicked, this, [this, idAgent] { consulter(idAgent); });
        h->addWidget(b);
    }
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
    for (const Agent &a : m_agents) {
        const QString who = QString("%1 — %2 %3").arg(a.id, a.nom, a.prenom);

        // Innovation 1 : renouvellement des certifications
        for (const Certification &c : a.certs) {
            const int d = int(today.daysTo(c.echeance));
            if (d <= ALERTE_JOURS) {
                ajouterAlerte("Certification à renouveler — alerte automatique",
                              QString("%1 — %2 %3").arg(who, c.nom,
                                                        d < 0 ? QString("expirée depuis %1 jour(s)").arg(-d)
                                                              : QString("expire dans %1 jour(s)").arg(d)),
                              false, a.id);
                ++n;
            }
        }
        // Innovation 2 : surcharge et repos réglementaire
        if (a.interv48 >= MAX_INTERV_48H) {
            ajouterAlerte("Risque de surcharge",
                          QString("%1 — %2 interventions en 48 h (seuil %3)")
                              .arg(who).arg(a.interv48).arg(MAX_INTERV_48H), false, a.id);
            ++n;
        }
        if (a.fonction == "Pompier" && a.reposH < MIN_REPOS_H) {
            ajouterAlerte("Repos réglementaire non respecté",
                          QString("%1 — %2 h de repos (minimum %3 h)")
                              .arg(who).arg(a.reposH).arg(MIN_REPOS_H), false, a.id);
            ++n;
        }
    }
    if (n == 0) ajouterAlerte("Aucune alerte", "Tout est en ordre.", true, QString());
}

// ============================================================================
//  Export PDF du planning de garde
// ============================================================================
void FireStation::exporterPdf()
{
    const QString path = QFileDialog::getSaveFileName(this, "Exporter le planning de garde",
                                                      "planning_garde.pdf", "PDF (*.pdf)");
    if (path.isEmpty()) return;

    QString rows;
    for (const Agent &a : m_agents) {
        if (a.dispo == "Repos") continue;
        rows += QString("<tr><td>%1 %2</td><td>%3</td><td>%4</td><td>%5</td><td>%6</td><td>%7</td></tr>")
                    .arg(a.nom.toHtmlEscaped(), a.prenom.toHtmlEscaped(), a.fonction,
                         a.grade.isEmpty() ? "—" : a.grade, a.tel, a.chef, a.dispo);
    }
    const QString html = QString(
                             "<h2>Planning de garde — Caserne centrale</h2><p>Édité le %1</p>"
                             "<table border='1' cellspacing='0' cellpadding='5' width='100%'>"
                             "<tr bgcolor='#dddddd'><th>Agent</th><th>Fonction</th><th>Grade</th>"
                             "<th>Téléphone</th><th>Chef de caserne</th><th>Statut</th></tr>%2</table>")
                             .arg(QDate::currentDate().toString("dd/MM/yyyy"), rows);

    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    QTextDocument doc;
    doc.setHtml(html);
    doc.print(&writer);
    QMessageBox::information(this, "Export", "Planning exporté :\n" + path);
}