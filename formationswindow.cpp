#include "formationswindow.h"
#include "formation.h"
#include "inscriptiondialog.h"
#include "formationdialog.h"
#include "statsdialog.h"
#include <QSqlQueryModel>
#include <QSqlQuery>
#include <QLineEdit>
#include <QComboBox>
#include <QDateEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QTableView>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextDocument>
#include <QPdfWriter>
#include <QPageLayout>
#include <QBrush>
#include <QColor>
#include <QPixmap>
#include <QSoundEffect>
#include <QUrl>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QPainter>

// Modèle d'affichage : dates lisibles, taux de remplissage coloré (vert / orange / rouge).
// Colonnes : 0 id, 1 intitulé, 2 type, 3 formateur, 4 début, 5 fin, 6 durée, 7 lieu,
//            8 capacité (masquée), 9 inscrits, 10 certification, 11 statut, 12 places (masquée)
class FormationModel : public QSqlQueryModel {
public:
    using QSqlQueryModel::QSqlQueryModel;
    QVariant data(const QModelIndex &idx, int role = Qt::DisplayRole) const override
    {
        const int c = idx.column();
        if (role == Qt::DisplayRole) {
            const QVariant brut = QSqlQueryModel::data(idx, role);
            if (c == 4 || c == 5)
                return QDate::fromString(brut.toString(), Qt::ISODate).toString("dd/MM/yyyy");
            if (c == 9)
                return QString("%1 / %2").arg(brut.toInt())
                       .arg(QSqlQueryModel::data(index(idx.row(), 8), role).toInt());
            if (c == 10) return brut.toInt() ? "Oui" : "Non";
            if (c == 11) return Formation::libelleStatut(brut.toString());
        }
        if (role == Qt::BackgroundRole && c == 9) {
            const int ins = QSqlQueryModel::data(idx, Qt::DisplayRole).toInt();
            const int cap = QSqlQueryModel::data(index(idx.row(), 8), Qt::DisplayRole).toInt();
            if (cap > 0 && ins >= cap)        return QBrush(QColor("#F4B6B6"));   // complet
            if (cap > 0 && ins * 10 >= cap * 7) return QBrush(QColor("#FBD9A5")); // >= 70 %
            return QBrush(QColor("#C9EBC9"));
        }
        if (c == 11 && (role == Qt::BackgroundRole || role == Qt::ForegroundRole)) {
            const QString s = QSqlQueryModel::data(idx, Qt::DisplayRole).toString();
            QColor fond("#FDE3E6"), txt("#C8102E");                       // en cours / annulée
            if (s == "terminee")      { fond = QColor("#DDF3E1"); txt = QColor("#2E7D32"); }
            else if (s == "planifiee") { fond = QColor("#FFF0C9"); txt = QColor("#B26A00"); }
            return role == Qt::BackgroundRole ? QVariant(QBrush(fond)) : QVariant(QBrush(txt));
        }
        if (role == Qt::TextAlignmentRole && (c == 6 || c == 9 || c == 10 || c == 11))
            return int(Qt::AlignCenter);
        return QSqlQueryModel::data(idx, role);
    }
};

// Carte blanche arrondie avec ombre légère (comme les cartes du module Incidents)
static QFrame *creerCarte()
{
    auto *f = new QFrame; f->setObjectName("carte");
    auto *ombre = new QGraphicsDropShadowEffect(f);
    ombre->setBlurRadius(24); ombre->setOffset(0, 4); ombre->setColor(QColor(0, 0, 0, 30));
    f->setGraphicsEffect(ombre);
    return f;
}

static QLabel *creerBadge(const QString &texte, bool plein)
{
    auto *b = new QLabel(texte);
    b->setObjectName(plein ? "badgePlein" : "badge");
    b->setAlignment(Qt::AlignCenter);
    return b;
}

// Fenêtre de confirmation (boutons en français, style de l'application)
static bool confirmer(QWidget *parent, const QString &titre, const QString &texte,
                      const QString &libelleOk, const QString &detail = QString())
{
    QMessageBox box(QMessageBox::Warning, titre, texte, QMessageBox::NoButton, parent);
    if (!detail.isEmpty()) box.setInformativeText(detail);
    QPushButton *ok = box.addButton(libelleOk, QMessageBox::AcceptRole);
    QPushButton *non = box.addButton("Annuler", QMessageBox::RejectRole);
    ok->setObjectName("btnAjouter"); non->setObjectName("btnModifier");
    ok->setCursor(Qt::PointingHandCursor); non->setCursor(Qt::PointingHandCursor);
    box.setDefaultButton(non);
    box.exec();
    return box.clickedButton() == ok;
}

FormationsWindow::FormationsWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("Smart Fire Station — Gestion des formations");
    setWindowIcon(QIcon(":/images/logo.png"));
    resize(1500, 800);
    setStyleSheet(R"(
        QMainWindow, #contenu { background:#F3F4F8; }
        #sidebar { background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #4A0D14, stop:1 #B01E2E); }
        #logoBox { background:white; border-radius:14px; }
        #sidebar QPushButton { background:transparent; color:#F5DADD; text-align:left;
            padding:11px 14px; border:none; border-radius:10px; font-size:13px; }
        #sidebar QPushButton:hover { background:rgba(255,255,255,0.12); }
        #sidebar QPushButton#navActif { background:white; color:#8E1B2A; font-weight:bold; }
        #titrePage { font-size:30px; font-weight:bold; color:#1E1E24; }
        #sousTitre { color:#6B6B76; font-size:11px; }
        #filet { background:#B01E2E; max-height:3px; min-height:3px; max-width:90px; }
        #carte { background:white; border-radius:18px; }
        #titreCarte { font-size:20px; font-weight:bold; color:#1E1E24; }
        #aide { color:#8A8A95; font-size:10px; }
        #carte QLabel { color:#3A3A44; font-size:11px; }
        #carte QLineEdit, #carte QComboBox, #carte QDateEdit, #carte QSpinBox {
            background:#FAFAFC; border:1px solid #D9D9E1; border-radius:8px; padding:7px 9px; min-height:20px; }
        #carte QLineEdit:focus, #carte QComboBox:focus, #carte QDateEdit:focus, #carte QSpinBox:focus {
            border:1px solid #B01E2E; }
        #carte QCheckBox { color:#3A3A44; spacing:8px; }
        #badge { background:#FDECEF; color:#B01E2E; border:1px solid #F1B7BF; border-radius:12px;
            padding:4px 14px; font-weight:bold; font-size:10px; }
        #badgePlein { background:#7A1422; color:white; border-radius:14px; padding:6px 16px;
            font-weight:bold; font-size:11px; }
        QPushButton#btnAjouter, QPushButton#btnPdf, QPushButton#btnSupprimer, QPushButton#btnModifier,
        QPushButton#btnVider, QPushButton#btnSec {
            border-radius:9px; padding:10px 16px; font-weight:bold; font-size:12px; }
        QPushButton#btnAjouter { background:#B01E2E; color:white; border:none; }
        QPushButton#btnAjouter:hover { background:#8E1B2A; }
        QPushButton#btnPdf { background:#7A1422; color:white; border:none; }
        QPushButton#btnPdf:hover { background:#5E0F1A; }
        QPushButton#btnSupprimer { background:white; color:#C8102E; border:1.5px solid #C8102E; }
        QPushButton#btnSupprimer:hover { background:#FDECEF; }
        QPushButton#btnModifier, QPushButton#btnVider, QPushButton#btnSec {
            background:white; color:#7A1422; border:1px solid #D9B3B9; }
        QPushButton#btnModifier:hover, QPushButton#btnVider:hover, QPushButton#btnSec:hover { background:#FDECEF; }
        QTableView { background:white; alternate-background-color:#FBF3F4; border:none;
            gridline-color:#EFE3E5; selection-background-color:#F6C9CF; selection-color:#1E1E24; }
        QHeaderView::section { background:#5A0F1A; color:white; padding:9px 6px; border:none;
            font-weight:bold; font-size:10px; }
        QScrollBar:vertical { background:transparent; width:10px; }
        QScrollBar::handle:vertical { background:#D9B3B9; border-radius:5px; }
        QScrollBar::add-line, QScrollBar::sub-line { height:0; }
    )");

    // ================= SIDEBAR =================
    auto *sidebar = new QWidget; sidebar->setObjectName("sidebar"); sidebar->setFixedWidth(210);
    auto *sl = new QVBoxLayout(sidebar); sl->setContentsMargins(16, 20, 16, 16); sl->setSpacing(6);
    auto *logoBox = new QLabel; logoBox->setObjectName("logoBox");
    logoBox->setPixmap(QPixmap(":/images/logo.png").scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoBox->setAlignment(Qt::AlignCenter); logoBox->setFixedHeight(130);
    sl->addWidget(logoBox); sl->addSpacing(18);
    const QStringList menu = {"Tableau de bord", "Incidents", "Interventions", "Pompiers",
                              "Formations", "Véhicules", "Équipements", "Rapports"};
    for (const QString &m : menu) {
        auto *b = new QPushButton(m);
        b->setCursor(Qt::PointingHandCursor);
        if (m == "Formations") b->setObjectName("navActif");
        sl->addWidget(b);
    }
    sl->addStretch();

    // ================= FORMULAIRE (carte gauche) =================
    m_intitule = new QLineEdit; m_formateur = new QLineEdit; m_lieu = new QLineEdit;
    m_intitule->setPlaceholderText("Ex. Secourisme niveau 1");
    m_formateur->setPlaceholderText("Nom du formateur");
    m_lieu->setPlaceholderText("Ex. Centre de formation, Tunis");
    m_type = new QComboBox; m_type->addItems(Formation::types()); m_type->setEditable(true);
    m_debut = new QDateEdit(QDate::currentDate()); m_fin = new QDateEdit(QDate::currentDate());
    for (QDateEdit *d : {m_debut, m_fin}) { d->setCalendarPopup(true); d->setDisplayFormat("dd/MM/yyyy"); }
    m_duree = new QSpinBox; m_duree->setRange(0, 1000); m_duree->setSuffix(" h");
    m_capacite = new QSpinBox; m_capacite->setRange(0, 500);
    m_certif = new QCheckBox("Délivre une certification");
    m_annulee = new QCheckBox("Formation annulée");

    auto *grille = new QGridLayout; grille->setHorizontalSpacing(12); grille->setVerticalSpacing(4);
    auto champ = [&](int r, int c, const QString &lib, QWidget *w, int span = 1) {
        grille->addWidget(new QLabel(lib), r * 2, c, 1, span);
        grille->addWidget(w, r * 2 + 1, c, 1, span);
    };
    champ(0, 0, "Intitulé *", m_intitule, 2);
    champ(1, 0, "Type *", m_type);            champ(1, 1, "Formateur", m_formateur);
    champ(2, 0, "Date de début", m_debut);    champ(2, 1, "Date de fin", m_fin);
    champ(3, 0, "Durée", m_duree);            champ(3, 1, "Capacité (places)", m_capacite);
    champ(4, 0, "Lieu", m_lieu, 2);
    grille->addWidget(m_certif, 10, 0, 1, 2);
    grille->addWidget(m_annulee, 11, 0, 1, 2);

    // Boutons demandés : Ajouter / Supprimer / Export PDF  (+ Modifier / Vider)
    auto *bAjouter = new QPushButton("＋  Ajouter");      bAjouter->setObjectName("btnAjouter");
    auto *bSupprimer = new QPushButton("🗑  Supprimer");   bSupprimer->setObjectName("btnSupprimer");
    auto *bPdf = new QPushButton("⬇  Export PDF");        bPdf->setObjectName("btnPdf");
    auto *bModifier = new QPushButton("Modifier");        bModifier->setObjectName("btnModifier");
    auto *bVider = new QPushButton("Vider");              bVider->setObjectName("btnVider");
    for (auto *b : {bAjouter, bSupprimer, bPdf, bModifier, bVider}) b->setCursor(Qt::PointingHandCursor);
    auto *ligne1 = new QHBoxLayout; ligne1->addWidget(bAjouter); ligne1->addWidget(bSupprimer);
    auto *ligne2 = new QHBoxLayout; ligne2->addWidget(bModifier); ligne2->addWidget(bVider);

    auto *carteForm = creerCarte(); carteForm->setMinimumWidth(400); carteForm->setMaximumWidth(460);
    auto *lf = new QVBoxLayout(carteForm); lf->setContentsMargins(22, 20, 22, 20); lf->setSpacing(10);
    auto *entete = new QHBoxLayout;
    auto *t1 = new QLabel("▪  Détails de la formation"); t1->setObjectName("titreCarte");
    entete->addWidget(t1); entete->addStretch(); entete->addWidget(creerBadge("CRUD", false));
    auto *aide = new QLabel("Sélectionnez une formation pour la consulter ou la modifier"); aide->setObjectName("aide");
    lf->addLayout(entete); lf->addWidget(aide); lf->addLayout(grille);
    lf->addSpacing(6); lf->addLayout(ligne1); lf->addWidget(bPdf); lf->addLayout(ligne2); lf->addStretch();

    // ================= LISTE (carte droite) =================
    m_recherche = new QLineEdit; m_recherche->setPlaceholderText("Rechercher (intitulé, type, formateur, statut)…");
    m_recherche->setClearButtonEnabled(true);
    m_tri = new QComboBox;
    m_tri->addItem("Date de début", "date_debut"); m_tri->addItem("Type", "type");
    m_tri->addItem("Statut", "statut");            m_tri->addItem("Places disponibles", "places");
    m_ordre = new QPushButton("↑ Croissant"); m_ordre->setObjectName("btnSec");
    m_ordre->setCursor(Qt::PointingHandCursor);

    m_model = new FormationModel(this);
    m_table = new QTableView;
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(40);

    auto *bPart = new QPushButton("Participants…"), *bStats = new QPushButton("Statistiques");
    for (auto *b : {bPart, bStats}) { b->setObjectName("btnSec"); b->setCursor(Qt::PointingHandCursor); }
    m_resume = new QLabel; m_resume->setObjectName("aide");
    m_badgeNb = creerBadge("0 formation", true);

    auto *carteListe = creerCarte();
    auto *ll = new QVBoxLayout(carteListe); ll->setContentsMargins(22, 20, 22, 20); ll->setSpacing(12);
    auto *entete2 = new QHBoxLayout;
    auto *t2 = new QLabel("▦  Liste des formations"); t2->setObjectName("titreCarte");
    entete2->addWidget(t2); entete2->addStretch(); entete2->addWidget(m_badgeNb);
    auto *haut = new QHBoxLayout;
    haut->addWidget(m_recherche, 1); haut->addWidget(new QLabel("Trier par :"));
    haut->addWidget(m_tri); haut->addWidget(m_ordre);
    auto *bas = new QHBoxLayout;
    bas->addWidget(m_resume, 1); bas->addWidget(bPart); bas->addWidget(bStats);
    ll->addLayout(entete2); ll->addLayout(haut); ll->addWidget(m_table, 1); ll->addLayout(bas);

    // ================= CONTENU =================
    auto *contenu = new QWidget; contenu->setObjectName("contenu");
    auto *cl = new QVBoxLayout(contenu); cl->setContentsMargins(28, 22, 28, 22); cl->setSpacing(14);
    auto *titre = new QLabel("Gestion des formations"); titre->setObjectName("titrePage");
    auto *sous = new QLabel("Planifier, consulter et suivre les formations des pompiers"); sous->setObjectName("sousTitre");
    auto *filet = new QFrame; filet->setObjectName("filet"); filet->setFixedWidth(90);
    auto *cartes = new QHBoxLayout; cartes->setSpacing(18);
    cartes->addWidget(carteForm); cartes->addWidget(carteListe, 1);
    cl->addWidget(titre); cl->addWidget(sous); cl->addWidget(filet); cl->addLayout(cartes, 1);

    auto *centre = new QWidget; auto *principal = new QHBoxLayout(centre);
    principal->setContentsMargins(0, 0, 0, 0); principal->setSpacing(0);
    principal->addWidget(sidebar); principal->addWidget(contenu, 1);
    setCentralWidget(centre);

    connect(bAjouter, &QPushButton::clicked, this, &FormationsWindow::ajouter);
    connect(bModifier, &QPushButton::clicked, this, &FormationsWindow::modifier);
    connect(bSupprimer, &QPushButton::clicked, this, &FormationsWindow::supprimer);
    connect(bVider, &QPushButton::clicked, this, [this] {
        if (confirmer(this, "Vider le formulaire", "Voulez-vous vider tous les champs du formulaire ?",
                      "Vider", "La sélection dans la liste sera aussi annulée."))
            viderFormulaire();
    });
    connect(bPart, &QPushButton::clicked, this, &FormationsWindow::participants);
    connect(bPdf, &QPushButton::clicked, this, &FormationsWindow::exporterPdf);
    connect(bStats, &QPushButton::clicked, this, [this] { StatsDialog(this).exec(); });
    connect(m_recherche, &QLineEdit::textChanged, this, [this] { rafraichir(); });   // temps réel
    connect(m_tri, &QComboBox::currentIndexChanged, this, [this] { rafraichir(); });
    connect(m_ordre, &QPushButton::clicked, this, [this] {
        m_croissant = !m_croissant;
        m_ordre->setText(m_croissant ? "↑ Croissant" : "↓ Décroissant");
        rafraichir();
    });
    // Son de clic sur tous les boutons de la fenêtre
    m_clic = new QSoundEffect(this);
    m_clic->setSource(QUrl("qrc:/sounds/click.wav"));
    m_clic->setVolume(0.6f);
    for (QPushButton *b : findChildren<QPushButton *>())
        connect(b, &QPushButton::clicked, m_clic, [this] { m_clic->play(); });

    rafraichir();
    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this] { remplirFormulaire(); });
    connect(m_debut, &QDateEdit::dateChanged, this, [this](const QDate &d) {
        if (m_fin->date() < d) m_fin->setDate(d);
    });
}

void FormationsWindow::rafraichir()
{
    Formation::majStatuts();
    const int idAvant = idSelectionne();
    QSqlQuery q = Formation::liste(m_recherche->text(), m_tri->currentData().toString(), m_croissant);
    m_model->setQuery(std::move(q));
    const char *titres[] = {"ID", "Intitulé", "Type", "Formateur", "Début", "Fin", "Durée (h)",
                            "Lieu", "Capacité", "Inscrits / Capacité", "Certification", "Statut", "Places"};
    for (int i = 0; i < 13; ++i) m_model->setHeaderData(i, Qt::Horizontal, QString::fromUtf8(titres[i]));
    m_table->hideColumn(0); m_table->hideColumn(8); m_table->hideColumn(12);
    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_resume->setText(QString("%1 formation(s) affichée(s)").arg(m_model->rowCount()));
    m_badgeNb->setText(QString("%1 formation%2").arg(m_model->rowCount()).arg(m_model->rowCount() > 1 ? "s" : ""));
    if (idAvant > 0)
        for (int r = 0; r < m_model->rowCount(); ++r)
            if (m_model->data(m_model->index(r, 0), Qt::EditRole).toInt() == idAvant) {
                m_table->selectRow(r); break;
            }
}

int FormationsWindow::idSelectionne() const
{
    const QModelIndex i = m_table->currentIndex();
    if (!i.isValid()) return -1;
    return m_model->QSqlQueryModel::data(m_model->index(i.row(), 0)).toInt();
}

void FormationsWindow::remplirFormulaire()
{
    Formation f;
    if (!Formation::charger(idSelectionne(), f)) return;
    m_intitule->setText(f.intitule); m_type->setCurrentText(f.type);
    m_formateur->setText(f.formateur); m_lieu->setText(f.lieu);
    m_debut->setDate(f.dateDebut); m_fin->setDate(f.dateFin);
    m_duree->setValue(f.duree); m_capacite->setValue(f.capacite);
    m_certif->setChecked(f.certification); m_annulee->setChecked(f.statut == "annulee");
}

void FormationsWindow::viderFormulaire()
{
    m_table->clearSelection(); m_table->setCurrentIndex(QModelIndex());
    m_intitule->clear(); m_formateur->clear(); m_lieu->clear();
    m_type->setCurrentIndex(0);
    m_debut->setDate(QDate::currentDate()); m_fin->setDate(QDate::currentDate());
    m_duree->setValue(0); m_capacite->setValue(0);
    m_certif->setChecked(false); m_annulee->setChecked(false);
}

bool FormationsWindow::lireFormulaire(Formation &f)
{
    f.intitule = m_intitule->text(); f.type = m_type->currentText().trimmed();
    f.formateur = m_formateur->text(); f.lieu = m_lieu->text();
    f.dateDebut = m_debut->date(); f.dateFin = m_fin->date();
    f.duree = m_duree->value(); f.capacite = m_capacite->value();
    f.certification = m_certif->isChecked();
    f.statut = m_annulee->isChecked() ? "annulee" : "";
    const QString e = f.valider();
    if (!e.isEmpty()) { QMessageBox::warning(this, "Saisie invalide", e); return false; }
    return true;
}

void FormationsWindow::ajouter()
{
    // Ouvre la fenêtre de saisie ; l'insertion en base est faite par le dialogue
    FormationDialog dlg(0, this);
    if (dlg.exec() == QDialog::Accepted) {
        rafraichir();
        viderFormulaire();
    }
}

void FormationsWindow::modifier()
{
    const int id = idSelectionne();
    if (id < 0) { QMessageBox::information(this, "Modifier", "Sélectionnez une formation dans la liste."); return; }
    // Ouvre la fenêtre préremplie ; l'enregistrement est fait par le dialogue
    FormationDialog dlg(id, this);
    if (dlg.exec() == QDialog::Accepted) {
        rafraichir();
        remplirFormulaire();
    }
}

void FormationsWindow::supprimer()
{
    const int id = idSelectionne();
    if (id < 0) { QMessageBox::information(this, "Supprimer", "Sélectionnez une formation dans la liste."); return; }
    Formation f; Formation::charger(id, f);
    const int n = Formation::inscrits(id);
    const QString detail = n ? QString("Cette formation compte %1 inscrit(s) : leurs inscriptions seront aussi supprimées.\nCette action est définitive.").arg(n)
                             : "Cette action est définitive.";
    if (!confirmer(this, "Supprimer la formation",
                   QString("Voulez-vous vraiment supprimer la formation « %1 » ?").arg(f.intitule),
                   "Supprimer", detail))
        return;
    Formation::supprimer(id);
    viderFormulaire(); rafraichir();
}

void FormationsWindow::participants()
{
    const int id = idSelectionne();
    if (id < 0) { QMessageBox::information(this, "Participants", "Sélectionnez une formation dans la liste."); return; }
    InscriptionDialog(id, this).exec();
    rafraichir();
}

void FormationsWindow::exporterPdf()
{
    const QString chemin = QFileDialog::getSaveFileName(this, "Exporter le planning", "planning_formations.pdf", "PDF (*.pdf)");
    if (chemin.isEmpty()) return;

    QString html = "<h2 style='color:#7A1422'>Planning des formations — Smart Fire Station</h2>"
                   "<p>Édité le " + QDate::currentDate().toString("dd/MM/yyyy") + "</p>"
                   "<table border='1' cellspacing='0' cellpadding='4' width='100%'><tr style='background:#5A0F1A;color:white'>";
    const int cols[] = {1, 2, 3, 4, 5, 6, 7, 9, 10, 11};
    for (int c : cols)
        html += "<th>" + m_model->headerData(c, Qt::Horizontal).toString().toHtmlEscaped() + "</th>";   // échappement HTML
    html += "</tr>";
    for (int r = 0; r < m_model->rowCount(); ++r) {
        html += "<tr>";
        for (int c : cols)
            html += "<td>" + m_model->data(m_model->index(r, c)).toString().toHtmlEscaped() + "</td>";
        html += "</tr>";
    }
    html += "</table>";

    QPdfWriter pdf(chemin);
    pdf.setPageSize(QPageSize(QPageSize::A4));
    pdf.setPageOrientation(QPageLayout::Landscape);
    pdf.setPageMargins(QMarginsF(12, 12, 12, 12), QPageLayout::Millimeter);
    QTextDocument doc; doc.setHtml(html);
    doc.print(&pdf);
    QMessageBox::information(this, "Export PDF", "Planning exporté :\n" + chemin);
}
