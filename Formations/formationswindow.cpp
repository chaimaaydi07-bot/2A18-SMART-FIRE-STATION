#include "formationswindow.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QFrame>
#include <QPixmap>
#include <QPdfWriter>
#include <QPainter>
#include <QDialog>
#include <QCalendarWidget>
#include <QTextCharFormat>
#include <QIntValidator>
#include <QRegularExpressionValidator>
#include <QStatusBar>
#include <QGraphicsDropShadowEffect>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QStandardItemModel>
#include <QTableWidgetItem>
#include <QDateEdit>
#include <QSpinBox>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>

// ======================================================================
//  Feuille de style (charte rouge du projet Smart Fire Station)
// ======================================================================
static const char *STYLE = R"(
QMainWindow, #contenu { background:#f7f3f3; }
QWidget { font-family:'Segoe UI','Calibri'; font-size:10pt; color:#2b2b2b; }

/* ---- Barre laterale ---- */
#sidebar { background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #3a0b14, stop:1 #c0222f); }
#sidebar QLabel { color:white; background:transparent; }
#nomApp { font-weight:bold; font-size:8pt; letter-spacing:1px; }
#sidebar QPushButton { background:transparent; color:white; border:none; text-align:left;
                       padding:9px 12px; border-radius:16px; font-size:9pt; }
#sidebar QPushButton:hover { background:rgba(255,255,255,40); }
#sidebar QPushButton[actif="true"] { background:white; color:#7a0f1c; font-weight:bold;
                       border:2px solid #f3b6bd; }

/* ---- Cartes ---- */
#carte { background:white; border:1px solid #eadada; border-radius:14px; }
#titre { color:#a3121f; font-size:17pt; font-weight:bold; background:transparent; }
#sousTitre, #labelChamp { color:#6b6b6b; font-size:8pt; background:transparent; }
#labelChamp { color:#333; font-weight:bold; font-size:8.5pt; }

QLineEdit, QComboBox, QDateEdit, QSpinBox {
    background:#faf7f7; border:1px solid #ddd3d3; border-radius:9px; padding:7px 9px; min-height:20px; }
QLineEdit:focus, QComboBox:focus, QDateEdit:focus, QSpinBox:focus { border:1px solid #b3202e; }
QComboBox::drop-down, QDateEdit::drop-down { border:none; width:22px; }

/* ---- Boutons principaux ---- */
#btnAjouter { color:white; font-weight:bold; border:2px solid #e0a07a; border-radius:9px; padding:10px;
    background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #c0222f, stop:1 #8e1320); }
#btnAjouter:hover { background:#d32a38; }
#btnModifier { color:#8e1320; font-weight:bold; background:white; border:2px solid #f0b27a;
    border-radius:9px; padding:10px; }
#btnModifier:hover { background:#fff3ea; }
#btnSupprimerForm { color:white; font-weight:bold; border:2px solid #5a0a14; border-radius:9px; padding:10px;
    background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #6d1220, stop:1 #3a0b14); }
#btnSupprimerForm:hover { background:#8e1320; }
#btnOutil { background:white; color:#8e1320; border:1px solid #e6c9cc; border-radius:9px; padding:7px 14px; font-weight:bold; }
#btnOutil:hover { background:#fdeeee; }
#btnTrier { color:white; font-weight:bold; border:none; border-radius:9px; padding:7px 16px; background:#a3121f; }
#btnTrier:hover { background:#c0222f; }
#btnLien { background:transparent; border:none; color:#a3121f; text-decoration:underline; }

/* ---- Badge compteur ---- */
#badge { background:#8e1320; color:white; border-radius:12px; padding:5px 14px; font-weight:bold; font-size:8.5pt; }

/* ---- Tableau ---- */
QTableWidget { background:white; border:1px solid #e6d5d5; gridline-color:#eadada;
               alternate-background-color:#fffafa; selection-background-color:#fde4e4; selection-color:#2b2b2b; }
QTableWidget::item:selected { background:#fde4e4; color:#2b2b2b; }
QHeaderView::section { background:#6d1220; color:white; font-weight:bold; font-size:8pt;
                       border:none; border-right:1px solid #8a2a38; padding:8px 4px; }
QPushButton#btnVoir { background:#fdeeee; color:#a3121f; border:1px solid #f0c6c9; border-radius:6px; padding:4px 10px; font-size:8pt; }
QPushButton#btnVoir:hover { background:#fbdada; }
QPushButton#btnModif { background:#fff3ea; color:#a65f00; border:1px solid #f0b27a; border-radius:6px; padding:4px 10px; font-size:8pt; font-weight:bold; }
QPushButton#btnModif:hover { background:#fde3cc; }
QPushButton#btnSupp { background:#8e1320; color:white; border:none; border-radius:6px; padding:5px 10px; font-weight:bold; font-size:8pt; }
QPushButton#btnSupp:hover { background:#b3202e; }

/* ---- Alerte ---- */
#alerte { background:#fdeeee; border:1px solid #f0b8bc; border-radius:14px; }
#alerte QLabel { background:transparent; }
#alerteTitre { font-weight:bold; font-size:11pt; color:#7a0f1c; }
#btnDetails { color:white; font-weight:bold; border:2px solid #e0a07a; border-radius:9px; padding:9px 16px;
    background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #c0222f, stop:1 #8e1320); }
)";


// ----------------------------------------------------------------------
// Donnees de demonstration (utilisees quand Oracle n'est pas connecte)
// colonnes : ID, TITRE, TYPE, DEBUT, FIN, DUREE, FORMATEUR, LIEU, PLACES, NIVEAU, STATUT
void FormationsWindow::initDemo()
{
    const QDate j = QDate::currentDate();
    auto d = [&](int n) { return j.addDays(n).toString("yyyy-MM-dd"); };
    demo = {
        {"1", "Premiers secours PSC1", "Secourisme", d(3), d(4), "14", "Karim Ben Ali", "Salle A", "20", "Débutant", "Planifiée"},
        {"2", "Manipulation des extincteurs", "Extincteurs", d(10), d(10), "6", "Sami Trabelsi", "Cour", "15", "Débutant", "Planifiée"},
        {"3", "Lutte contre incendie urbain", "Incendie", d(17), d(21), "30", "Nour Haddad", "Terrain", "12", "Avancé", "Planifiée"},
        {"4", "Évacuation de bâtiments", "Évacuation", d(-20), d(-19), "10", "Leila Mansour", "Caserne centrale", "18", "Intermédiaire", "Terminée"},
    };
}

int FormationsWindow::demoIndex(int id) const
{
    for (int i = 0; i < demo.size(); ++i) if (demo[i][0].toInt() == id) return i;
    return -1;
}

QStringList FormationsWindow::ligneDemo(const Formation &f)
{
    return {QString::number(f.getId()), f.getTitre(), f.getType(), f.getDateDebut().toString("yyyy-MM-dd"),
            f.getDateFin().toString("yyyy-MM-dd"), QString::number(f.getDuree()), f.getFormateur(), f.getLieu(),
            QString::number(f.getNbPlaces()), f.getNiveau(), f.getStatut()};
}

QAbstractItemModel* FormationsWindow::modeleApercu(const QString &critere) const
{
    QStandardItemModel *m = new QStandardItemModel;
    for (const QStringList &r : demo) {
        if (!critere.isEmpty() && !r.join(' ').contains(critere, Qt::CaseInsensitive)) continue;
        QList<QStandardItem*> ligne;
        for (const QString &v : r) ligne << new QStandardItem(v);
        m->appendRow(ligne);
    }
    return m;
}

// Lueur rouge autour d'un texte (comme « Nouvel incident » / « Liste des incidents »)
void FormationsWindow::effetLueur(QWidget *w)
{
    QGraphicsDropShadowEffect *e = new QGraphicsDropShadowEffect(w);
    e->setColor(QColor(255, 40, 40, 200));
    e->setBlurRadius(18);
    e->setOffset(0, 0);
    w->setGraphicsEffect(e);
}

void FormationsWindow::setModeApercu(bool actif)
{
    modeApercu = actif;
    if (actif)
        statusBar()->showMessage("Mode aperçu : Oracle non connecté — données de démonstration (les modifications restent en mémoire).");
    rafraichir();
}

// ======================================================================
FormationsWindow::FormationsWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Smart Fire Station — Gestion des formations");
    resize(1320, 760);
    setStyleSheet(STYLE);
    initDemo();
    construireUI();
    rafraichir();
}

QWidget* FormationsWindow::champ(const QString &label, QWidget *w)
{
    QWidget *c = new QWidget;
    QVBoxLayout *l = new QVBoxLayout(c);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(4);
    QLabel *lb = new QLabel(label);
    lb->setObjectName("labelChamp");
    l->addWidget(lb);
    l->addWidget(w);
    return c;
}

// ---------------------------------------------------------------- UI
void FormationsWindow::construireUI()
{
    QWidget *racine = new QWidget;
    setCentralWidget(racine);
    QHBoxLayout *h = new QHBoxLayout(racine);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);

    h->addWidget(construireSidebar());

    QWidget *contenu = new QWidget;
    contenu->setObjectName("contenu");
    QHBoxLayout *hc = new QHBoxLayout(contenu);
    hc->setContentsMargins(18, 18, 18, 18);
    hc->setSpacing(18);
    hc->addWidget(construireFormulaire(), 4);
    hc->addWidget(construireListe(), 6);
    h->addWidget(contenu, 1);
}

QWidget* FormationsWindow::construireSidebar()
{
    QWidget *sb = new QWidget;
    sb->setObjectName("sidebar");
    sb->setFixedWidth(230);
    QVBoxLayout *l = new QVBoxLayout(sb);
    l->setContentsMargins(14, 22, 14, 14);
    l->setSpacing(6);

    // Logo du projet
    QLabel *logo = new QLabel;
    logo->setPixmap(QPixmap(":/formations/images/logo.png").scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignCenter);
    l->addWidget(logo);

    QLabel *nom = new QLabel("SMART FIRE STATION");
    nom->setObjectName("nomApp");
    nom->setAlignment(Qt::AlignCenter);
    l->addWidget(nom);
    l->addSpacing(14);

    struct Item { const char *texte; bool actif; };
    const Item items[] = {
        {"⚠  Incidents", false},
        {"▪  Gestion des bâtiments\n    à risque", false},
        {"👤  Personnel", false},
        {"🚒  Véhicules", false},
        {"🛠  Équipements", false},
        {"📄  Rapports", false},
        {"✦  Gestion des\n    formations", true},
    };
    for (const Item &it : items) {
        QPushButton *b = new QPushButton(it.texte);
        b->setProperty("actif", it.actif);
        b->setCursor(Qt::PointingHandCursor);
        if (!it.actif) {
            connect(b, &QPushButton::clicked, this, [this]() {
                QMessageBox::information(this, "Smart Fire Station",
                    "Ce module est développé par un autre membre de l'équipe.");
            });
        }
        l->addWidget(b);
    }
    l->addStretch();

    return sb;
}

QWidget* FormationsWindow::construireFormulaire()
{
    QFrame *carte = new QFrame;
    carte->setObjectName("carte");
    QVBoxLayout *l = new QVBoxLayout(carte);
    l->setContentsMargins(22, 20, 22, 20);
    l->setSpacing(12);

    QLabel *t = new QLabel("Nouvelle formation");
    t->setObjectName("titre");
    effetLueur(t);
    QLabel *st = new QLabel("Saisir les informations de la formation");
    st->setObjectName("sousTitre");
    l->addWidget(t);
    l->addWidget(st);

    leId = new QLineEdit;
    leId->setPlaceholderText("Généré automatiquement");
    leId->setValidator(new QIntValidator(1, 999999, this));
    leId->setReadOnly(true);
    leTitre = new QLineEdit;
    leTitre->setPlaceholderText("Ex. Premiers secours PSC1");
    leFormateur = new QLineEdit;
    leFormateur->setPlaceholderText("Nom du formateur");
    leFormateur->setValidator(new QRegularExpressionValidator(QRegularExpression("[A-Za-zÀ-ÿ \\-']{0,50}"), this));
    leLieu = new QLineEdit;
    leLieu->setPlaceholderText("Ex. Tunis, Caserne centrale");

    cbType = new QComboBox;
    cbType->addItems({"Secourisme", "Incendie", "Extincteurs", "Évacuation",
                      "Matières dangereuses", "Sauvetage aquatique", "Conduite véhicule"});
    cbNiveau = new QComboBox;
    cbNiveau->addItems({"Débutant", "Intermédiaire", "Avancé"});
    cbStatut = new QComboBox;
    cbStatut->addItems({"Planifiée", "En cours", "Terminée", "Annulée"});

    deDebut = new QDateEdit(QDate::currentDate());
    deFin = new QDateEdit(QDate::currentDate().addDays(1));
    for (QDateEdit *d : {deDebut, deFin}) { d->setCalendarPopup(true); d->setDisplayFormat("dd/MM/yyyy"); }
    sbDuree = new QSpinBox; sbDuree->setRange(0, 500); sbDuree->setSuffix(" h");
    sbPlaces = new QSpinBox; sbPlaces->setRange(0, 500);

    QGridLayout *g = new QGridLayout;
    g->setHorizontalSpacing(14);
    g->setVerticalSpacing(10);
    g->addWidget(champ("Identifiant", leId), 0, 0);
    g->addWidget(champ("Statut", cbStatut), 0, 1);
    g->addWidget(champ("Titre de la formation", leTitre), 1, 0, 1, 2);
    g->addWidget(champ("Type de formation", cbType), 2, 0);
    g->addWidget(champ("Niveau", cbNiveau), 2, 1);
    g->addWidget(champ("Date de début", deDebut), 3, 0);
    g->addWidget(champ("Date de fin", deFin), 3, 1);
    g->addWidget(champ("Durée (heures)", sbDuree), 4, 0);
    g->addWidget(champ("Nombre de places", sbPlaces), 4, 1);
    g->addWidget(champ("Formateur", leFormateur), 5, 0, 1, 2);
    g->addWidget(champ("Lieu", leLieu), 6, 0, 1, 2);
    l->addLayout(g);

    QPushButton *bAj = new QPushButton("+  Ajouter");
    bAj->setObjectName("btnAjouter");
    QPushButton *bMod = new QPushButton("✎  Modifier");
    bMod->setObjectName("btnModifier");
    QPushButton *bSupF = new QPushButton("🗑  Supprimer");
    bSupF->setObjectName("btnSupprimerForm");
    for (auto *b : {bAj, bMod, bSupF}) b->setCursor(Qt::PointingHandCursor);
    QHBoxLayout *bl = new QHBoxLayout;
    bl->setSpacing(14);
    bl->addWidget(bAj);
    bl->addWidget(bMod);
    bl->addWidget(bSupF);
    l->addLayout(bl);

    QLabel *outilsTitre = new QLabel("Outils de gestion");
    outilsTitre->setObjectName("labelChamp");
    l->addWidget(outilsTitre);
    QPushButton *bStats = new QPushButton("Statistiques");
    QPushButton *bCal = new QPushButton("Calendrier");
    for (QPushButton *b : {bStats, bCal}) {
        b->setObjectName("btnOutil");
        b->setCursor(Qt::PointingHandCursor);
    }
    QGridLayout *outils = new QGridLayout;
    outils->setHorizontalSpacing(8);
    outils->setVerticalSpacing(8);
    outils->addWidget(bStats, 0, 0);
    outils->addWidget(bCal, 0, 1);
    l->addLayout(outils);

    QPushButton *bVider = new QPushButton("Vider le formulaire");
    bVider->setObjectName("btnLien");
    bVider->setCursor(Qt::PointingHandCursor);
    l->addWidget(bVider, 0, Qt::AlignCenter);

    connect(bAj, &QPushButton::clicked, this, &FormationsWindow::on_ajouter);
    connect(bMod, &QPushButton::clicked, this, &FormationsWindow::on_modifier);
    connect(bSupF, &QPushButton::clicked, this, &FormationsWindow::on_supprimerForm);
    connect(bStats, &QPushButton::clicked, this, &FormationsWindow::on_stats);
    connect(bCal, &QPushButton::clicked, this, &FormationsWindow::on_calendrier);
    connect(bVider, &QPushButton::clicked, this, &FormationsWindow::on_vider);
    return carte;
}

QWidget* FormationsWindow::construireListe()
{
    QFrame *carte = new QFrame;
    carte->setObjectName("carte");
    QVBoxLayout *l = new QVBoxLayout(carte);
    l->setContentsMargins(22, 20, 22, 20);
    l->setSpacing(12);

    // en-tete : titre + compteur
    QHBoxLayout *entete = new QHBoxLayout;
    QLabel *t = new QLabel("Liste des formations");
    t->setObjectName("titre");
    effetLueur(t);
    lbCompteur = new QLabel("0 formation(s)");
    lbCompteur->setObjectName("badge");
    entete->addWidget(t);
    entete->addStretch();
    entete->addWidget(lbCompteur, 0, Qt::AlignTop);
    l->addLayout(entete);

    // recherche + filtre
    leRecherche = new QLineEdit;
    leRecherche->setPlaceholderText("🔎 Rechercher par titre, type, formateur ou ID...");
    cbFiltre = new QComboBox;
    cbFiltre->addItems({"Tous niveaux", "Débutant", "Intermédiaire", "Avancé"});
    QHBoxLayout *r1 = new QHBoxLayout;
    r1->addWidget(leRecherche, 1);
    r1->addWidget(cbFiltre);
    l->addLayout(r1);

    // tri + outils
    cbTri = new QComboBox;
    cbTri->addItem("Date : récente d'abord", "DATE_DEBUT|DESC");
    cbTri->addItem("Date : ancienne d'abord", "DATE_DEBUT|ASC");
    cbTri->addItem("Titre : A → Z", "TITRE|ASC");
    cbTri->addItem("Durée : croissante", "DUREE|ASC");
    cbTri->addItem("Durée : décroissante", "DUREE|DESC");
    cbTri->addItem("Places : décroissantes", "NB_PLACES|DESC");
    QPushButton *bTrier = new QPushButton("⇅  Trier");
    bTrier->setObjectName("btnTrier");
    QPushButton *bPdf = new QPushButton("PDF");
    for (auto *b : {bPdf}) b->setObjectName("btnOutil");
    for (auto *b : {bTrier, bPdf}) b->setCursor(Qt::PointingHandCursor);

    QHBoxLayout *r2 = new QHBoxLayout;
    r2->addWidget(new QLabel("Trier par :"));
    r2->addWidget(cbTri);
    r2->addWidget(bTrier);
    r2->addStretch();
    r2->addWidget(bPdf);
    l->addLayout(r2);

    // tableau
    table = new QTableWidget(0, 7);
    table->setHorizontalHeaderLabels({"ID", "TITRE", "TYPE", "DÉBUT", "NIVEAU", "STATUT", "ACTION"});
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(44);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    QHeaderView *hh = table->horizontalHeader();
    hh->setSectionResizeMode(QHeaderView::Stretch);
    hh->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    l->addWidget(table, 1);

    // alerte de priorite
    QFrame *alerte = new QFrame;
    alerte->setObjectName("alerte");
    QHBoxLayout *al = new QHBoxLayout(alerte);
    al->setContentsMargins(18, 12, 18, 12);
    QLabel *ico = new QLabel("⚠");
    ico->setStyleSheet("font-size:28pt;color:#2b2b2b;");
    QVBoxLayout *txt = new QVBoxLayout;
    QLabel *at = new QLabel("Alerte de priorité");
    at->setObjectName("alerteTitre");
    lbAlerteTexte = new QLabel;
    lbAlerteTexte->setWordWrap(true);
    lbAlerteDetail = new QLabel;
    lbAlerteDetail->setStyleSheet("font-size:8.5pt;");
    txt->addWidget(at);
    txt->addWidget(lbAlerteTexte);
    txt->addWidget(lbAlerteDetail);
    QPushButton *bDet = new QPushButton("Voir les détails  ›");
    bDet->setObjectName("btnDetails");
    bDet->setCursor(Qt::PointingHandCursor);
    al->addWidget(ico);
    al->addLayout(txt, 1);
    al->addWidget(bDet);
    l->addWidget(alerte);

    // connexions
    connect(leRecherche, &QLineEdit::textChanged, this, [this]() { rafraichir(); });
    connect(cbFiltre, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { rafraichir(); });
    connect(bTrier, &QPushButton::clicked, this, &FormationsWindow::on_trier);
    connect(bPdf, &QPushButton::clicked, this, &FormationsWindow::on_pdf);
    connect(bDet, &QPushButton::clicked, this, &FormationsWindow::on_alerteDetails);
    return carte;
}

// ---------------------------------------------------------------- affichage
void FormationsWindow::rafraichir()
{
    QAbstractItemModel *m = modeApercu ? modeleApercu(leRecherche->text())
                                       : static_cast<QAbstractItemModel*>(Ftmp.lister(leRecherche->text(), triColonne, triCroissant));
    const QString filtre = cbFiltre->currentIndex() == 0 ? QString() : cbFiltre->currentText();

    table->setRowCount(0);
    int n = 0;
    for (int r = 0; r < m->rowCount(); ++r) {
        auto val = [&](int c) { return m->data(m->index(r, c)).toString(); };
        const QString niveau = val(9);
        if (!filtre.isEmpty() && niveau != filtre) continue;

        int row = table->rowCount();
        table->insertRow(row);
        const int id = val(0).toInt();
        const QDate d = QDate::fromString(val(3), "yyyy-MM-dd");

        const QStringList cellules = {QString("FOR-%1").arg(id, 3, 10, QChar('0')), val(1), val(2),
                                      d.toString("dd/MM/yyyy"), niveau, val(10)};
        for (int c = 0; c < cellules.size(); ++c) {
            QTableWidgetItem *it = new QTableWidgetItem(cellules[c]);
            if (c == 4) {   // couleur du niveau, comme la gravite des incidents
                if (niveau == "Avancé")             { it->setBackground(QColor("#fbd5d5")); it->setForeground(QColor("#b3202e")); }
                else if (niveau == "Intermédiaire") { it->setBackground(QColor("#fde7c8")); it->setForeground(QColor("#a65f00")); }
                else                                { it->setBackground(QColor("#d6f1de")); it->setForeground(QColor("#1b7a3a")); }
                QFont f = it->font(); f.setBold(true); it->setFont(f);
            }
            it->setData(Qt::UserRole, id);
            table->setItem(row, c, it);
        }

        // boutons Voir / Supprimer sur chaque ligne
        QWidget *actions = new QWidget;
        QHBoxLayout *al = new QHBoxLayout(actions);
        al->setContentsMargins(4, 4, 4, 4);
        al->setSpacing(6);
        QPushButton *bVoir = new QPushButton("Voir");
        bVoir->setObjectName("btnVoir");
        QPushButton *bModif = new QPushButton("✎ Modifier");
        bModif->setObjectName("btnModif");
        QPushButton *bSupp = new QPushButton("Supprimer");
        bSupp->setObjectName("btnSupp");
        bVoir->setCursor(Qt::PointingHandCursor);
        bModif->setCursor(Qt::PointingHandCursor);
        bSupp->setCursor(Qt::PointingHandCursor);
        al->addWidget(bVoir);
        al->addWidget(bModif);
        al->addWidget(bSupp);
        connect(bVoir, &QPushButton::clicked, this, [this, id]() { on_voir(id); });
        connect(bModif, &QPushButton::clicked, this, [this, id]() { on_modifierLigne(id); });
        connect(bSupp, &QPushButton::clicked, this, [this, id]() { on_supprimer(id); });
        table->setCellWidget(row, 6, actions);
        ++n;
    }
    delete m;
    lbCompteur->setText(QString("%1 formation(s)").arg(n));
    majAlerte();
}

// Formations planifiees dans les 7 prochains jours
void FormationsWindow::majAlerte()
{
    if (modeApercu) {
        lbAlerteTexte->setText("2 formation(s) commence(nt) dans les 7 prochains jours (démo).");
        lbAlerteDetail->setText("FOR-001 — Premiers secours PSC1 — Salle A");
        return;
    }
    QSqlQueryModel *m = Ftmp.prochaines(7);
    int n = m->rowCount();
    QString premiere;
    if (n > 0)
        premiere = QString("FOR-%1 — %2 — %3")
                       .arg(m->data(m->index(0, 0)).toInt(), 3, 10, QChar('0'))
                       .arg(m->data(m->index(0, 1)).toString(),
                            m->data(m->index(0, 3)).toString());
    delete m;
    if (n == 0) {
        lbAlerteTexte->setText("Aucune formation urgente dans les 7 prochains jours.");
        lbAlerteDetail->clear();
    } else {
        lbAlerteTexte->setText(QString("%1 formation(s) commence(nt) dans les 7 prochains jours.").arg(n));
        lbAlerteDetail->setText(premiere);
    }
}


void FormationsWindow::on_alerteDetails()
{
    QSqlQueryModel *m = Ftmp.prochaines(7);
    QString txt;
    for (int i = 0; i < m->rowCount(); ++i)
        txt += QString("• %1 — %2 — %3 (%4)\n")
                   .arg(m->data(m->index(i, 1)).toString(),
                        QDate::fromString(m->data(m->index(i, 4)).toString(), "yyyy-MM-dd").toString("dd/MM/yyyy"),
                        m->data(m->index(i, 3)).toString(),
                        m->data(m->index(i, 5)).toString());
    delete m;
    QMessageBox::information(this, "Formations à venir", txt.isEmpty() ? "Aucune formation à venir." : txt);
}
// ---------------------------------------------------------------- controle de saisie
bool FormationsWindow::controleSaisie(QString &erreur)
{
    if (leTitre->text().trimmed().size() < 3) { erreur = "Le titre doit contenir au moins 3 caractères."; return false; }
    if (deFin->date() < deDebut->date())      { erreur = "La date de fin doit être postérieure à la date de début."; return false; }
    if (sbDuree->value() <= 0)                { erreur = "La durée doit être supérieure à 0."; return false; }
    if (leFormateur->text().trimmed().isEmpty()) { erreur = "Le formateur est obligatoire."; return false; }
    if (leLieu->text().trimmed().isEmpty())   { erreur = "Le lieu est obligatoire."; return false; }
    if (sbPlaces->value() <= 0)               { erreur = "Le nombre de places doit être supérieur à 0."; return false; }
    return true;
}

Formation FormationsWindow::lireFormulaire() const
{
    return Formation(leId->text().toInt(), leTitre->text().trimmed(), cbType->currentText(),
                     deDebut->date(), deFin->date(), sbDuree->value(),
                     leFormateur->text().trimmed(), leLieu->text().trimmed(),
                     sbPlaces->value(), cbNiveau->currentText(), cbStatut->currentText());
}

// ---------------------------------------------------------------- CRUD
void FormationsWindow::on_ajouter()
{
    QString err;
    if (!controleSaisie(err)) { QMessageBox::warning(this, "Saisie invalide", err); return; }
    Formation f = lireFormulaire();
    if (modeApercu) {
        int prochainId = 1;
        for (const QStringList &ligne : demo)
            prochainId = qMax(prochainId, ligne[0].toInt() + 1);
        for (const QStringList &ligne : demo) {
            if (ligne[1].trimmed().compare(f.getTitre().trimmed(), Qt::CaseInsensitive) == 0) {
                QMessageBox::warning(this, "Titre déjà utilisé", "Une formation porte déjà ce titre.");
                return;
            }
        }
        f.setId(prochainId);
        demo << ligneDemo(f);
        rafraichir();
        QMessageBox::information(this, "Succès", QString("Formation FOR-%1 ajoutée (mode aperçu).")
                                     .arg(f.getId(), 3, 10, QChar('0')));
        on_vider();
        return;
    }
    if (f.titreExiste(f.getTitre())) {
        QMessageBox::warning(this, "Titre déjà utilisé", "Une formation porte déjà ce titre.");
        return;
    }
    if (f.ajouter()) {
        rafraichir();
        QMessageBox::information(this, "Succès", QString("Formation FOR-%1 ajoutée.")
                                     .arg(f.getId(), 3, 10, QChar('0')));
        on_vider();
    } else {
        QMessageBox::critical(this, "Erreur Oracle", "Échec de l'ajout :\n" + Formation::derniereErreur());
    }
}

void FormationsWindow::on_modifier()
{
    if (leId->text().isEmpty()) {
        QMessageBox::warning(this, "Modification", "Sélectionnez une formation dans la liste avant de la modifier.");
        return;
    }
    QString err;
    if (!controleSaisie(err)) { QMessageBox::warning(this, "Saisie invalide", err); return; }
    Formation f = lireFormulaire();
    if (modeApercu) {
        int i = demoIndex(f.getId());
        if (i < 0) { QMessageBox::warning(this, "Introuvable", "Aucune formation avec cet identifiant.\nCliquez sur « Voir » dans la liste."); return; }
        for (int j = 0; j < demo.size(); ++j) {
            if (j != i && demo[j][1].trimmed().compare(f.getTitre().trimmed(), Qt::CaseInsensitive) == 0) {
                QMessageBox::warning(this, "Titre déjà utilisé", "Une autre formation porte déjà ce titre.");
                return;
            }
        }
        demo[i] = ligneDemo(f);
        rafraichir();
        QMessageBox::information(this, "Succès", "Formation modifiée (mode aperçu).");
        return;
    }
    if (!f.existe(f.getId())) {
        QMessageBox::warning(this, "Introuvable", "Aucune formation avec cet identifiant.\nCliquez sur « Voir » dans la liste.");
        return;
    }
    if (f.titreExiste(f.getTitre(), f.getId())) {
        QMessageBox::warning(this, "Titre déjà utilisé", "Une autre formation porte déjà ce titre.");
        return;
    }
    if (f.modifier()) { rafraichir(); QMessageBox::information(this, "Succès", "Formation modifiée."); }
    else QMessageBox::critical(this, "Erreur Oracle", "Échec de la modification :\n" + Formation::derniereErreur());
}


// Bouton « Modifier » de la ligne : ouvre une fenetre pre-remplie, enregistre directement
void FormationsWindow::on_modifierLigne(int id)
{
    QList<QVariant> v;   // TITRE, TYPE, DEBUT, FIN, DUREE, FORMATEUR, LIEU, PLACES, NIVEAU, STATUT
    if (modeApercu) {
        int i = demoIndex(id);
        if (i < 0) return;
        for (int c = 1; c <= 10; ++c) v << demo[i][c];
    } else {
        Formation r;                       // lecture depuis Oracle (SELECT ... WHERE ID=:id)
        if (!r.recuperer(id)) { QMessageBox::critical(this, "Erreur Oracle", "Formation introuvable ou erreur SQL :\n" + Formation::derniereErreur()); return; }
        v << r.getTitre() << r.getType()
          << r.getDateDebut().toString("yyyy-MM-dd") << r.getDateFin().toString("yyyy-MM-dd")
          << r.getDuree() << r.getFormateur() << r.getLieu() << r.getNbPlaces()
          << r.getNiveau() << r.getStatut();
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QString("Modifier la formation FOR-%1").arg(id, 3, 10, QChar('0')));
    dlg.setMinimumWidth(420);
    QFormLayout *fl = new QFormLayout(&dlg);

    QLineEdit *eTitre = new QLineEdit(v[0].toString());
    QComboBox *eType = new QComboBox;
    for (int i = 0; i < cbType->count(); ++i) eType->addItem(cbType->itemText(i));
    eType->setCurrentText(v[1].toString());
    QDateEdit *eDebut = new QDateEdit(QDate::fromString(v[2].toString(), "yyyy-MM-dd"));
    QDateEdit *eFin = new QDateEdit(QDate::fromString(v[3].toString(), "yyyy-MM-dd"));
    for (QDateEdit *d : {eDebut, eFin}) { d->setCalendarPopup(true); d->setDisplayFormat("dd/MM/yyyy"); }
    QSpinBox *eDuree = new QSpinBox; eDuree->setRange(0, 500); eDuree->setSuffix(" h"); eDuree->setValue(v[4].toInt());
    QLineEdit *eFormateur = new QLineEdit(v[5].toString());
    QLineEdit *eLieu = new QLineEdit(v[6].toString());
    QSpinBox *ePlaces = new QSpinBox; ePlaces->setRange(0, 500); ePlaces->setValue(v[7].toInt());
    QComboBox *eNiveau = new QComboBox;
    for (int i = 0; i < cbNiveau->count(); ++i) eNiveau->addItem(cbNiveau->itemText(i));
    eNiveau->setCurrentText(v[8].toString());
    QComboBox *eStatut = new QComboBox;
    for (int i = 0; i < cbStatut->count(); ++i) eStatut->addItem(cbStatut->itemText(i));
    eStatut->setCurrentText(v[9].toString());

    fl->addRow("Titre", eTitre);
    fl->addRow("Type", eType);
    fl->addRow("Niveau", eNiveau);
    fl->addRow("Statut", eStatut);
    fl->addRow("Date de début", eDebut);
    fl->addRow("Date de fin", eFin);
    fl->addRow("Durée", eDuree);
    fl->addRow("Places", ePlaces);
    fl->addRow("Formateur", eFormateur);
    fl->addRow("Lieu", eLieu);
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Save)->setText("Enregistrer");
    bb->button(QDialogButtonBox::Cancel)->setText("Annuler");
    fl->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;

    if (eTitre->text().trimmed().size() < 3)   { QMessageBox::warning(this, "Saisie invalide", "Le titre doit contenir au moins 3 caractères."); return; }
    if (eFin->date() < eDebut->date())         { QMessageBox::warning(this, "Saisie invalide", "La date de fin doit être postérieure à la date de début."); return; }
    if (eDuree->value() <= 0 || ePlaces->value() <= 0) { QMessageBox::warning(this, "Saisie invalide", "Durée et nombre de places doivent être supérieurs à 0."); return; }
    if (eFormateur->text().trimmed().isEmpty() || eLieu->text().trimmed().isEmpty()) { QMessageBox::warning(this, "Saisie invalide", "Formateur et lieu sont obligatoires."); return; }

    Formation f(id, eTitre->text().trimmed(), eType->currentText(), eDebut->date(), eFin->date(), eDuree->value(),
                eFormateur->text().trimmed(), eLieu->text().trimmed(), ePlaces->value(), eNiveau->currentText(), eStatut->currentText());
    if (modeApercu) {
        int i = demoIndex(id);
        for (int j = 0; j < demo.size(); ++j) {
            if (j != i && demo[j][1].trimmed().compare(f.getTitre().trimmed(), Qt::CaseInsensitive) == 0) {
                QMessageBox::warning(this, "Titre déjà utilisé", "Une autre formation porte déjà ce titre.");
                return;
            }
        }
        if (i >= 0) demo[i] = ligneDemo(f);
        rafraichir();
        QMessageBox::information(this, "Succès", "Formation modifiée (mode aperçu).");
        return;
    }
    if (f.titreExiste(f.getTitre(), id)) {
        QMessageBox::warning(this, "Titre déjà utilisé", "Une autre formation porte déjà ce titre.");
        return;
    }
    if (f.modifier()) { rafraichir(); QMessageBox::information(this, "Succès", "Formation modifiée."); }
    else QMessageBox::critical(this, "Erreur Oracle", "Échec de la modification :\n" + Formation::derniereErreur());
}

// Bouton « Supprimer » du formulaire : supprime la formation dont l'identifiant est saisi
void FormationsWindow::on_supprimerForm()
{
    if (leId->text().isEmpty()) {
        QMessageBox::warning(this, "Suppression", "Saisissez un identifiant ou cliquez sur « Voir » dans la liste.");
        return;
    }
    const int id = leId->text().toInt();
    const bool existe = modeApercu ? demoIndex(id) >= 0 : Ftmp.existe(id);
    if (!existe) { QMessageBox::warning(this, "Introuvable", "Aucune formation avec cet identifiant."); return; }
    on_supprimer(id);
}

void FormationsWindow::on_supprimer(int id)
{
    if (QMessageBox::question(this, "Confirmation",
            QString("Supprimer la formation FOR-%1 ?").arg(id, 3, 10, QChar('0'))) != QMessageBox::Yes)
        return;
    if (modeApercu) {
        int i = demoIndex(id);
        if (i >= 0) demo.removeAt(i);
        if (leId->text().toInt() == id) on_vider();
        rafraichir();
        return;
    }
    if (Ftmp.supprimer(id)) {
        if (leId->text().toInt() == id) on_vider();
        rafraichir();
    } else {
        QMessageBox::critical(this, "Erreur Oracle", "Échec de la suppression :\n" + Formation::derniereErreur());
    }
}

// « Voir » : charge la formation dans le formulaire (pour la consulter puis la modifier)
// « Voir » : charge la formation dans le formulaire (pour la consulter puis la modifier)
void FormationsWindow::on_voir(int id)
{
    if (modeApercu) {
        int i = demoIndex(id);
        if (i < 0) return;
        const QStringList &r = demo[i];
        leId->setText(r[0]); leTitre->setText(r[1]); cbType->setCurrentText(r[2]);
        deDebut->setDate(QDate::fromString(r[3], "yyyy-MM-dd")); deFin->setDate(QDate::fromString(r[4], "yyyy-MM-dd"));
        sbDuree->setValue(r[5].toInt()); leFormateur->setText(r[6]); leLieu->setText(r[7]);
        sbPlaces->setValue(r[8].toInt()); cbNiveau->setCurrentText(r[9]); cbStatut->setCurrentText(r[10]);
        return;
    }
    Formation f;
    if (!f.recuperer(id)) return;
    leId->setText(QString::number(id));
    leTitre->setText(f.getTitre());
    cbType->setCurrentText(f.getType());
    deDebut->setDate(f.getDateDebut());
    deFin->setDate(f.getDateFin());
    sbDuree->setValue(f.getDuree());
    leFormateur->setText(f.getFormateur());
    leLieu->setText(f.getLieu());
    sbPlaces->setValue(f.getNbPlaces());
    cbNiveau->setCurrentText(f.getNiveau());
    cbStatut->setCurrentText(f.getStatut());
}



void FormationsWindow::on_vider()
{
    leId->clear(); leTitre->clear(); leFormateur->clear(); leLieu->clear();
    sbDuree->setValue(0); sbPlaces->setValue(0);
    deDebut->setDate(QDate::currentDate());
    deFin->setDate(QDate::currentDate().addDays(1));
    cbType->setCurrentIndex(0); cbNiveau->setCurrentIndex(0); cbStatut->setCurrentIndex(0);
}

// ---------------------------------------------------------------- metiers
void FormationsWindow::on_trier()
{
    const QStringList p = cbTri->currentData().toString().split('|');
    triColonne = p[0];
    triCroissant = (p[1] == "ASC");
    rafraichir();
}

void FormationsWindow::on_pdf()
{
    QString chemin = QFileDialog::getSaveFileName(this, "Exporter en PDF", "formations.pdf", "PDF (*.pdf)");
    if (chemin.isEmpty()) return;

    QAbstractItemModel *m = modeApercu ? modeleApercu(leRecherche->text())
                                       : static_cast<QAbstractItemModel*>(Ftmp.lister(leRecherche->text(), triColonne, triCroissant));

    QPdfWriter pdf(chemin);
    pdf.setPageSize(QPageSize(QPageSize::A4));
    pdf.setPageOrientation(QPageLayout::Landscape);
    pdf.setResolution(300);
    QPainter p(&pdf);

    p.drawPixmap(300, 200, QPixmap(":/formations/images/logo.png").scaled(250, 250, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    p.setPen(QColor("#a3121f"));
    p.setFont(QFont("Arial", 22, QFont::Bold));
    p.drawText(650, 400, "Smart Fire Station - Liste des formations");
    p.setPen(Qt::black);
    p.setFont(QFont("Arial", 9));
    p.drawText(650, 520, "Édité le " + QDate::currentDate().toString("dd/MM/yyyy"));

    const QStringList titres = {"ID", "Titre", "Type", "Début", "Fin", "Durée", "Formateur", "Lieu", "Statut"};
    const int cols[] = {0, 1, 2, 3, 4, 5, 6, 7, 10};
    const int larg[] = {500, 2300, 1600, 1200, 1200, 700, 1800, 1500, 1200};
    int y = 900, x = 300;

    p.setFont(QFont("Arial", 9, QFont::Bold));
    p.setBrush(QColor("#6d1220"));
    for (int i = 0; i < 9; ++i) {
        p.setPen(Qt::black);
        p.drawRect(x, y, larg[i], 350);
        p.setPen(Qt::white);
        p.fillRect(x, y, larg[i], 350, QColor("#6d1220"));
        p.drawText(QRect(x + 20, y, larg[i] - 40, 350), Qt::AlignVCenter, titres[i]);
        x += larg[i];
    }
    y += 350;
    p.setPen(Qt::black);
    p.setBrush(Qt::NoBrush);
    p.setFont(QFont("Arial", 9));
    for (int r = 0; r < m->rowCount(); ++r) {
        if (y > pdf.height() - 600) { pdf.newPage(); y = 400; }
        x = 300;
        for (int i = 0; i < 9; ++i) {
            p.drawRect(x, y, larg[i], 300);
            p.drawText(QRect(x + 20, y, larg[i] - 40, 300), Qt::AlignVCenter, m->data(m->index(r, cols[i])).toString());
            x += larg[i];
        }
        y += 300;
    }
    p.end();
    delete m;
    QMessageBox::information(this, "PDF", "Export réalisé :\n" + chemin);
}

void FormationsWindow::on_stats()
{
    QMap<QString,int> stats;
    if (modeApercu) {
        QAbstractItemModel *m = modeleApercu(QString());
        for (int r = 0; r < m->rowCount(); ++r)
            stats[m->data(m->index(r, 2)).toString()]++;
        delete m;
    } else {
        stats = Ftmp.statistiquesParType();
    }
    if (stats.isEmpty()) { QMessageBox::information(this, "Statistiques", "Aucune donnée à afficher."); return; }
    int total = 0;
    for (int v : stats) total += v;

    QPieSeries *serie = new QPieSeries();
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        QPieSlice *s = serie->append(QString("%1 (%2%)").arg(it.key()).arg(100.0 * it.value() / total, 0, 'f', 1), it.value());
        s->setLabelVisible(true);
    }
    QChart *chart = new QChart();
    chart->addSeries(serie);
    chart->setTitle("Répartition des formations par type");
    chart->legend()->setAlignment(Qt::AlignRight);
    QChartView *vue = new QChartView(chart);
    vue->setRenderHint(QPainter::Antialiasing);

    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle("Statistiques");
    dlg->resize(680, 460);
    QVBoxLayout *l = new QVBoxLayout(dlg);
    l->addWidget(vue);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void FormationsWindow::on_calendrier()
{
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle("Calendrier des formations");
    dlg->resize(420, 420);
    QVBoxLayout *l = new QVBoxLayout(dlg);
    QCalendarWidget *cal = new QCalendarWidget;
    QLabel *info = new QLabel("Cliquez sur un jour surligné.");
    info->setWordWrap(true);

    QTextCharFormat fmt;
    fmt.setBackground(QColor("#ffcdd2"));
    fmt.setFontWeight(QFont::Bold);
    if (modeApercu) {
        QAbstractItemModel *m = modeleApercu(QString());
        for (int r = 0; r < m->rowCount(); ++r) {
            QDate d = QDate::fromString(m->data(m->index(r, 3)).toString(), "yyyy-MM-dd");
            QDate f = QDate::fromString(m->data(m->index(r, 4)).toString(), "yyyy-MM-dd");
            for (; d.isValid() && d <= f; d = d.addDays(1)) cal->setDateTextFormat(d, fmt);
        }
        delete m;
    } else {
        QSqlQueryModel *m = Ftmp.calendrier();
        for (int row = 0; row < m->rowCount(); ++row) {
            QDate d = QDate::fromString(m->data(m->index(row, 0)).toString(), "yyyy-MM-dd");
            QDate f = QDate::fromString(m->data(m->index(row, 1)).toString(), "yyyy-MM-dd");
            for (; d.isValid() && d <= f; d = d.addDays(1)) cal->setDateTextFormat(d, fmt);
        }
        delete m;
    }
    const bool apercu = modeApercu;
    connect(cal, &QCalendarWidget::clicked, dlg, [info, apercu, this](const QDate &date) {
        if (apercu) {
            QAbstractItemModel *m = modeleApercu(QString());
            QString t;
            for (int r = 0; r < m->rowCount(); ++r) {
                QDate d = QDate::fromString(m->data(m->index(r, 3)).toString(), "yyyy-MM-dd");
                QDate f = QDate::fromString(m->data(m->index(r, 4)).toString(), "yyyy-MM-dd");
                if (date >= d && date <= f)
                    t += "• " + m->data(m->index(r, 1)).toString() + " — " + m->data(m->index(r, 7)).toString() + "\n";
            }
            delete m;
            info->setText(t.isEmpty() ? "Aucune formation ce jour-là." : t);
            return;
        }
        QSqlQueryModel *r = Ftmp.formationsDuJour(date);
        QString t;
        for (int row = 0; row < r->rowCount(); ++row)
            t += "• " + r->data(r->index(row, 0)).toString() + " — " + r->data(r->index(row, 1)).toString() + "\n";
        delete r;
        info->setText(t.isEmpty() ? "Aucune formation ce jour-là." : t);
    });
    l->addWidget(cal);
    l->addWidget(info);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void FormationsWindow::on_gestionInscriptions()
{
    if (modeApercu) {
        QMessageBox::information(this, "Inscriptions",
                                 "Connectez-vous à Oracle pour gérer les inscriptions.");
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Gestion des inscriptions aux formations");
    dlg.resize(720, 500);
    QVBoxLayout *layout = new QVBoxLayout(&dlg);

    QLabel *intro = new QLabel("Les inscriptions sont enregistrées dans la table Oracle INSCRIRE.");
    intro->setObjectName("sousTitre");
    layout->addWidget(intro);

    QGridLayout *form = new QGridLayout;
    QSpinBox *idFormation = new QSpinBox;
    idFormation->setRange(1, 999999999);
    idFormation->setPrefix("Formation #");
    QSpinBox *idAgent = new QSpinBox;
    idAgent->setRange(1, 999999999);
    idAgent->setPrefix("Agent #");
    QDateEdit *dateInscription = new QDateEdit(QDate::currentDate());
    dateInscription->setCalendarPopup(true);
    dateInscription->setDisplayFormat("dd/MM/yyyy");
    QComboBox *statutInscription = new QComboBox;
    statutInscription->addItem("Inscrit", "INSCRIT");
    statutInscription->addItem("Confirmé", "CONFIRME");
    statutInscription->addItem("En attente", "ATTENTE");
    statutInscription->addItem("Annulé", "ANNULE");
    QComboBox *presence = new QComboBox;
    presence->addItem("Non renseignée", -1);
    presence->addItem("Absent", 0);
    presence->addItem("Présent", 1);
    QComboBox *resultat = new QComboBox;
    resultat->addItem("En attente", "EN_ATTENTE");
    resultat->addItem("Réussi", "REUSSI");
    resultat->addItem("Échec", "ECHEC");
    form->addWidget(champ("Identifiant formation", idFormation), 0, 0);
    form->addWidget(champ("Identifiant agent", idAgent), 0, 1);
    form->addWidget(champ("Date d'inscription", dateInscription), 0, 2);
    form->addWidget(champ("Statut", statutInscription), 1, 0);
    form->addWidget(champ("Présence", presence), 1, 1);
    form->addWidget(champ("Résultat", resultat), 1, 2);
    layout->addLayout(form);

    QTableWidget *tableInscriptions = new QTableWidget(0, 6);
    tableInscriptions->setHorizontalHeaderLabels({"ID formation", "ID agent", "Date d'inscription",
                                                  "Statut", "Présence", "Résultat"});
    tableInscriptions->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableInscriptions->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableInscriptions->setSelectionMode(QAbstractItemView::SingleSelection);
    tableInscriptions->setAlternatingRowColors(true);
    tableInscriptions->verticalHeader()->setVisible(false);
    tableInscriptions->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(tableInscriptions, 1);

    QLabel *etat = new QLabel;
    etat->setWordWrap(true);
    layout->addWidget(etat);

    QHBoxLayout *actions = new QHBoxLayout;
    QPushButton *ajouter = new QPushButton("Ajouter l'inscription");
    ajouter->setObjectName("btnAjouter");
    QPushButton *modifier = new QPushButton("Modifier l'inscription");
    modifier->setObjectName("btnModifier");
    QPushButton *supprimer = new QPushButton("Supprimer");
    supprimer->setObjectName("btnSupprimerForm");
    QPushButton *fermer = new QPushButton("Fermer");
    for (QPushButton *button : {ajouter, modifier, supprimer, fermer})
        button->setCursor(Qt::PointingHandCursor);
    actions->addWidget(ajouter);
    actions->addWidget(modifier);
    actions->addWidget(supprimer);
    actions->addStretch();
    actions->addWidget(fermer);
    layout->addLayout(actions);

    auto rafraichirInscriptions = [&]() {
        QSqlQueryModel *model = Inscription().afficher();
        tableInscriptions->setRowCount(0);
        for (int row = 0; row < model->rowCount(); ++row) {
            tableInscriptions->insertRow(row);
            for (int column = 0; column < model->columnCount(); ++column) {
                QTableWidgetItem *item = new QTableWidgetItem(
                    model->data(model->index(row, column)).toString());
                tableInscriptions->setItem(row, column, item);
            }
        }
        if (!Inscription::derniereErreur().isEmpty())
            etat->setText("Erreur Oracle : " + Inscription::derniereErreur());
        else
            etat->setText(QString("%1 inscription(s) chargée(s) depuis Oracle.").arg(model->rowCount()));
        delete model;
    };

    connect(tableInscriptions, &QTableWidget::itemSelectionChanged, &dlg, [&]() {
        const int row = tableInscriptions->currentRow();
        if (row < 0) return;
        idFormation->setValue(tableInscriptions->item(row, 0)->text().toInt());
        idAgent->setValue(tableInscriptions->item(row, 1)->text().toInt());
        const QDate value = QDate::fromString(tableInscriptions->item(row, 2)->text(), "yyyy-MM-dd");
        if (value.isValid()) dateInscription->setDate(value);
        statutInscription->setCurrentIndex(statutInscription->findData(tableInscriptions->item(row, 3)->text()));
        const QString presenceText = tableInscriptions->item(row, 4)->text();
        presence->setCurrentIndex(presence->findData(presenceText.isEmpty() ? -1 : presenceText.toInt()));
        resultat->setCurrentIndex(resultat->findData(tableInscriptions->item(row, 5)->text()));
    });
    connect(ajouter, &QPushButton::clicked, &dlg, [&]() {
        Inscription inscription(idFormation->value(), idAgent->value(), dateInscription->date(),
                               statutInscription->currentData().toString(),
                               presence->currentData().toInt(), resultat->currentData().toString());
        const bool ok = inscription.ajouter();
        const QString erreur = Inscription::derniereErreur();
        rafraichirInscriptions();
        etat->setText(ok ? "Inscription ajoutée dans Oracle."
                         : "Échec de l'ajout : " + erreur);
    });
    connect(modifier, &QPushButton::clicked, &dlg, [&]() {
        if (tableInscriptions->currentRow() < 0) {
            etat->setText("Sélectionnez une inscription dans le tableau.");
            return;
        }
        Inscription inscription(idFormation->value(), idAgent->value(), dateInscription->date(),
                               statutInscription->currentData().toString(),
                               presence->currentData().toInt(), resultat->currentData().toString());
        const bool ok = inscription.modifier();
        const QString erreur = Inscription::derniereErreur();
        rafraichirInscriptions();
        etat->setText(ok ? "Inscription modifiée dans Oracle."
                         : "Échec de la modification : " + erreur);
    });
    connect(supprimer, &QPushButton::clicked, &dlg, [&]() {
        if (tableInscriptions->currentRow() < 0) {
            etat->setText("Sélectionnez une inscription dans le tableau.");
            return;
        }
        Inscription inscription(idFormation->value(), idAgent->value());
        const bool ok = inscription.supprimer();
        const QString erreur = Inscription::derniereErreur();
        rafraichirInscriptions();
        etat->setText(ok ? "Inscription supprimée d'Oracle."
                         : "Échec de la suppression : " + erreur);
    });
    connect(fermer, &QPushButton::clicked, &dlg, &QDialog::accept);

    rafraichirInscriptions();
    dlg.exec();
}
