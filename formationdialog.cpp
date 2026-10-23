#include "formationdialog.h"
#include "formation.h"
#include <QLineEdit>
#include <QComboBox>
#include <QDateEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>

FormationDialog::FormationDialog(int idFormation, QWidget *parent) : QDialog(parent), m_id(idFormation)
{
    const bool modif = m_id > 0;
    setWindowTitle(modif ? "Modifier la formation" : "Nouvelle formation");
    setModal(true);
    setMinimumWidth(480);
    setStyleSheet(R"(
        QDialog { background:white; }
        QLabel { color:#3A3A44; font-size:11px; }
        QLabel#titre { font-size:20px; font-weight:bold; color:#1E1E24; }
        QLabel#aide { color:#8A8A95; font-size:10px; }
        QLineEdit, QComboBox, QDateEdit, QSpinBox {
            background:#FAFAFC; border:1px solid #D9D9E1; border-radius:8px; padding:7px 9px; min-height:20px; }
        QLineEdit:focus, QComboBox:focus, QDateEdit:focus, QSpinBox:focus { border:1px solid #B01E2E; }
        QCheckBox { color:#3A3A44; spacing:8px; }
        QPushButton { border-radius:9px; padding:10px 22px; font-weight:bold; font-size:12px; }
        QPushButton#btnOk { background:#B01E2E; color:white; border:none; }
        QPushButton#btnOk:hover { background:#8E1B2A; }
        QPushButton#btnAnnuler { background:white; color:#7A1422; border:1px solid #D9B3B9; }
        QPushButton#btnAnnuler:hover { background:#FDECEF; }
    )");

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
    m_annulee->setVisible(modif);

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

    auto *titre = new QLabel(modif ? "✎  Modifier la formation" : "＋  Nouvelle formation"); titre->setObjectName("titre");
    auto *aide = new QLabel("Saisir les informations de la formation (* obligatoire)"); aide->setObjectName("aide");

    auto *bOk = new QPushButton(modif ? "Enregistrer" : "Ajouter");   bOk->setObjectName("btnOk");   bOk->setDefault(true);
    auto *bAnnuler = new QPushButton("Annuler"); bAnnuler->setObjectName("btnAnnuler");
    bOk->setCursor(Qt::PointingHandCursor); bAnnuler->setCursor(Qt::PointingHandCursor);
    auto *boutons = new QHBoxLayout; boutons->addStretch(); boutons->addWidget(bAnnuler); boutons->addWidget(bOk);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(24, 22, 24, 20); lay->setSpacing(10);
    lay->addWidget(titre); lay->addWidget(aide); lay->addLayout(grille);
    lay->addSpacing(8); lay->addLayout(boutons);

    connect(bOk, &QPushButton::clicked, this, &QDialog::accept);
    connect(bAnnuler, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_debut, &QDateEdit::dateChanged, this, [this](const QDate &d) {
        if (m_fin->date() < d) m_fin->setDate(d);
    });
    if (modif) {
        Formation f;
        if (Formation::charger(m_id, f)) {
            m_intitule->setText(f.intitule); m_type->setCurrentText(f.type);
            m_formateur->setText(f.formateur); m_lieu->setText(f.lieu);
            m_debut->setDate(f.dateDebut); m_fin->setDate(f.dateFin);
            m_duree->setValue(f.duree); m_capacite->setValue(f.capacite);
            m_certif->setChecked(f.certification); m_annulee->setChecked(f.statut == "annulee");
        }
    }
    m_intitule->setFocus();
}

void FormationDialog::accept()
{
    Formation f;
    f.intitule = m_intitule->text(); f.type = m_type->currentText().trimmed();
    f.formateur = m_formateur->text(); f.lieu = m_lieu->text();
    f.dateDebut = m_debut->date(); f.dateFin = m_fin->date();
    f.duree = m_duree->value(); f.capacite = m_capacite->value();
    f.certification = m_certif->isChecked();
    f.id = m_id;
    f.statut = m_annulee->isChecked() ? "annulee" : "";

    const QString e = f.valider();
    if (!e.isEmpty()) { QMessageBox::warning(this, "Saisie invalide", e); return; }

    QString err;
    if (m_id > 0) {
        if (!f.modifier(&err)) { QMessageBox::warning(this, "Modification impossible", err); return; }
    } else {
        if (!f.ajouter(&err)) { QMessageBox::warning(this, "Ajout impossible", err); return; }
    }

    QDialog::accept();   // ferme la fenêtre uniquement si l'enregistrement a réussi
}
