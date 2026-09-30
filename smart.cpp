#include "smart.h"
#include <QLabel>
#include <QPixmap>
#include <QComboBox>
#include <QDir>
#include <QDate>
#include <QDateEdit>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QButtonGroup>
#include <QPrinter>
#include <QTextDocument>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

smart::smart(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Smart Fire Station");
     resize(1400, 850);




    QWidget *central = new QWidget;
    setCentralWidget(central);


    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // BARRE DU HAUT
    QFrame *top = new QFrame;
    top->setFixedHeight(65);
    top->setStyleSheet("background:#c40000;");

    QHBoxLayout *topLayout = new QHBoxLayout(top);
    QLabel *logoImg = new QLabel;
    logoImg->setPixmap(QPixmap(":/images/logo.png").scaled(50, 50, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoImg->setStyleSheet("background: transparent;");
    topLayout->addWidget(logoImg);

    QLabel *logo = new QLabel("♨  SMART FIRE STATION");
    logo->setStyleSheet(
        "color:white;"
        "font-size:16px;"
        "font-weight:bold;"
        );

    topLayout->addWidget(logo);
    topLayout->addStretch();

    QLabel *admin = new QLabel("Admin  ▾");
    admin->setStyleSheet("color:white;font-size:12px;");
    topLayout->addWidget(admin);

    mainLayout->addWidget(top);

    // CORPS
    QHBoxLayout *body = new QHBoxLayout;
    body->setContentsMargins(0,0,0,0);
    body->setSpacing(0);

    // MENU GAUCHE
    QFrame *menu = new QFrame;
    menu->setFixedWidth(185);
    menu->setStyleSheet("background:#a90000;");

    QVBoxLayout *menuLayout = new QVBoxLayout(menu);

    QStringList menuItems = {
        "⌂   Accueil",
        "♨   Incidents",
        "▣   Véhicules",
        "⚒   Équipements",
        "♟   Personnel",
        "↗   Interventions",
        "▥   Rapports"
    };

    for(int i = 0; i < menuItems.size(); i++)
    {
        QPushButton *button = new QPushButton(menuItems[i]);
        button->setFixedHeight(42);

        button->setStyleSheet(
            "QPushButton{"
            "color:white;"
            "background:transparent;"
            "border:0;"
            "text-align:left;"
            "padding-left:15px;"
            "font-weight:bold;"
            "}"
            "QPushButton:hover{"
            "background:#c40000;"
            "}"
            );

        menuLayout->addWidget(button);
    }

    menuLayout->addStretch();

    body->addWidget(menu);

    // CONTENU
    QWidget *content = new QWidget;
    QVBoxLayout *contentLayout = new QVBoxLayout(content);

    QLabel *title = new QLabel("⚒  Gestion des équipements");
    title->setStyleSheet(
        "color:#b40000;"
        "font-size:20px;"
        "font-weight:bold;"
        );

    contentLayout->addWidget(title);

    QLabel *description = new QLabel(
        "Consulter, ajouter, modifier et gérer tout le matériel de la caserne."
        );

    description->setStyleSheet("color:#777;");
    contentLayout->addWidget(description);

    // SPLITTER
    QSplitter *splitter = new QSplitter(Qt::Horizontal);

    // FORMULAIRE
    QFrame *form = new QFrame;
    form->setStyleSheet(
        "QFrame{"
        "background:white;"
        "border:1px solid #ddd;"
        "border-radius:5px;"
        "}"
        );

    QVBoxLayout *formLayout = new QVBoxLayout(form);

    QLabel *formTitle =
        new QLabel("Nouveau / Modifier Équipement");

    formTitle->setStyleSheet(
        "color:#b40000;"
        "font-size:14px;"
        "font-weight:bold;"
        );

    formLayout->addWidget(formTitle);

    nom = new QLineEdit;
    nom->setPlaceholderText("Nom de l'équipement");

    type = new QComboBox;
    type->addItems({
        "Sélectionner un type",
        "Protection",
        "Secours",
        "Communication",
        "Médical",
        "Outillage"
    });

    marque = new QLineEdit;
    marque->setPlaceholderText("Marque");

    modele = new QLineEdit;
    modele->setPlaceholderText("Modèle");

    quantite = new QSpinBox;
    quantite->setRange(1,9999);

    etat = new QComboBox;
    etat->addItems({
        "Disponible",
        "Hors service",
        "En maintenance"
    });

    dateAchat = new QDateEdit(QDate::currentDate());
    dateAchat->setCalendarPopup(true);

    dateMaintenance = new QDateEdit(QDate::currentDate());
    dateMaintenance->setCalendarPopup(true);

    prochaineMaintenance =
        new QDateEdit(QDate::currentDate().addMonths(6));

    prochaineMaintenance->setCalendarPopup(true);

    remarques = new QLineEdit;
    remarques->setPlaceholderText("Remarques");

    formLayout->addWidget(new QLabel("Nom de l'équipement"));
    formLayout->addWidget(nom);

    formLayout->addWidget(new QLabel("Type"));
    formLayout->addWidget(type);

    formLayout->addWidget(new QLabel("Marque"));
    formLayout->addWidget(marque);

    formLayout->addWidget(new QLabel("Modèle"));
    formLayout->addWidget(modele);

    formLayout->addWidget(new QLabel("Quantité"));
    formLayout->addWidget(quantite);

    formLayout->addWidget(new QLabel("État"));
    formLayout->addWidget(etat);

    formLayout->addWidget(new QLabel("Date d'achat"));
    formLayout->addWidget(dateAchat);

    formLayout->addWidget(new QLabel("Date dernière maintenance"));
    formLayout->addWidget(dateMaintenance);

    formLayout->addWidget(new QLabel("Prochaine maintenance"));
    formLayout->addWidget(prochaineMaintenance);

    formLayout->addWidget(new QLabel("Remarques"));
    formLayout->addWidget(remarques);

    formLayout->addStretch();

    QHBoxLayout *buttons = new QHBoxLayout;

    QPushButton *add = new QPushButton("Ajouter");
    QPushButton *edit = new QPushButton("Modifier");
    QPushButton *archive = new QPushButton("Archiver");
    QString btnStyle =
        "QPushButton { background:white; color:#c40000; border:1px solid #c40000;"
        "border-radius:6px; padding:4px 10px; font-weight:bold; }"
        "QPushButton:hover { background:#f5dcdc; }"
        "QPushButton:checked { background:#c40000; color:white; }";

    add->setStyleSheet(btnStyle);
    edit->setStyleSheet(btnStyle);
    archive->setStyleSheet(btnStyle);

    add->setFixedHeight(34);
    edit->setFixedHeight(34);
    archive->setFixedHeight(34);

    QButtonGroup *group = new QButtonGroup(this);
    group->setExclusive(true);
    for (QPushButton *b : {add, edit, archive}) {
        b->setCheckable(true);
        group->addButton(b);
    }
    add->setChecked(true);


add->setFixedHeight(34);
edit->setFixedHeight(34);
archive->setFixedHeight(34);


group->setExclusive(true);
for (QPushButton *b : {add, edit, archive}) {
    b->setCheckable(true);
    group->addButton(b);
}
add->setChecked(true);



    buttons->addWidget(add);
    buttons->addWidget(edit);
    buttons->addWidget(archive);

    formLayout->addLayout(buttons);

    splitter->addWidget(form);

    // TABLEAU
    QFrame *tableFrame = new QFrame;
    tableFrame->setStyleSheet(
        "QFrame{"
        "background:white;"
        "border:1px solid #ddd;"
        "}"
        );

    QVBoxLayout *tableLayout = new QVBoxLayout(tableFrame);

    QLabel *listTitle =
        new QLabel("☷  Liste des équipements");

    listTitle->setStyleSheet(
        "color:#b40000;"
        "font-size:14px;"
        "font-weight:bold;"
        );

    tableLayout->addWidget(listTitle);

    recherche = new QLineEdit;
    recherche->setPlaceholderText(
        "Rechercher un équipement..."
        );
    QPushButton *btnPdf  = new QPushButton("⬇ Exporter en PDF");
    QPushButton *btnStat = new QPushButton("Statistiques");

    QString orangeStyle =
        "QPushButton { background:#f5a623; color:black; border:none;"
        "border-radius:6px; padding:8px 14px; font-weight:bold; }"
        "QPushButton:hover { background:#e0941a; }";
    btnPdf->setStyleSheet(orangeStyle);
    btnStat->setStyleSheet(orangeStyle);

    QHBoxLayout *toolsLayout = new QHBoxLayout;
    toolsLayout->addWidget(btnPdf);
    toolsLayout->addWidget(btnStat);
    toolsLayout->addStretch();
    QLabel *lblTri = new QLabel("Trier par :");
    triCombo = new QComboBox;
    triCombo->addItems({"Nom", "Marque", "Disponibilité"});
    triCombo->setCurrentIndex(-1);
    triCombo->setPlaceholderText("Choisir un critère");
    toolsLayout->addWidget(lblTri);
    toolsLayout->addWidget(triCombo);
    tableLayout->addLayout(toolsLayout);

    tableLayout->addWidget(recherche);

    table = new QTableWidget(0, 9);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(8, QHeaderView::Fixed);
    table->setColumnWidth(8, 200);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    table->setHorizontalHeaderLabels({
        "Nom",
        "Type",
        "Marque",
        "Modèle",
        "Quantité",
        "État",
        "Date achat",
        "Maintenance",
        "Actions"
    });

    table->horizontalHeader()
        ->setStretchLastSection(true);

    table->verticalHeader()->setVisible(false);

    QStringList noms = {
        "Extincteur CO2",
        "Casque",
        "Appareil respiratoire",
        "Tenue de feu",
        "Gants",
        "Bottes",
        "Scie à béton",
        "Lampe torche"
    };
    QStringList marques = {
        "Rosenbauer",
        "MSA",
        "Dräger",
        "Bullard",
        "Ziamatic",
        "Holmatro",
        "Husqvarna",
        "Petzl"
    };

    QStringList etats = {
        "Disponible",
        "En maintenance",
        "Hors service",
        "Disponible",
        "En mission",
        "Disponible",
        "En maintenance",
        "En mission"
    };

    for(int i = 0; i < noms.size(); i++)

    {
        int row = table->rowCount();
        table->insertRow(row);

        table->setItem(
            row,0,
            new QTableWidgetItem(noms[i])
            );

        table->setItem(
            row,1,
            new QTableWidgetItem("Protection")
            );

        table->setItem(
            row,2,
            new QTableWidgetItem(marques[i])
            );

        table->setItem(
            row,3,
            new QTableWidgetItem("PSS 7000")
            );

        table->setItem(
            row,4,
            new QTableWidgetItem(QString::number((i+1)*5))
            );

        table->setItem(
            row,5,
            new QTableWidgetItem(etats[i])
            );

        table->setItem(
            row,6,
            new QTableWidgetItem(
                QDate::currentDate()
                    .toString("dd/MM/yyyy")
                )
            );

        table->setItem(
            row,7,
            new QTableWidgetItem(
                QDate::currentDate()
                    .toString("dd/MM/yyyy")
                )
            );
        QWidget *actions = new QWidget;
        QHBoxLayout *actLayout = new QHBoxLayout(actions);
        actLayout->setContentsMargins(2, 2, 2, 2);
        actLayout->setSpacing(4);

        QPushButton *btnEdit = new QPushButton("Modif.");
        QPushButton *btnDel  = new QPushButton("Suppr.");

        btnEdit->setStyleSheet("QPushButton{background:white;color:#c40000;border:1px solid #c40000;border-radius:4px;padding:2px 4px;font-size:11px;}"
                               "QPushButton:hover{background:#c40000;color:white;}");
        btnDel->setStyleSheet("QPushButton{background:#c40000;color:white;border:none;border-radius:4px;padding:2px 4px;font-size:11px;}"
                              "QPushButton:hover{background:#8f0000;}");

        btnEdit->setMinimumWidth(60);
        btnDel->setMinimumWidth(60);


        actLayout->addWidget(btnEdit);
        actLayout->addWidget(btnDel);
        connect(btnDel, &QPushButton::clicked, this, [=]() {
            int r = table->indexAt(actions->mapTo(table->viewport(), QPoint(0, 0))).row();
            if (r < 0) return;
            if (QMessageBox::question(this, "Suppression",
                                      "Supprimer cet équipement ?") == QMessageBox::Yes)
                table->removeRow(r);
        });

        connect(btnEdit, &QPushButton::clicked, this, [=]() {
            int r = table->indexAt(actions->mapTo(table->viewport(), QPoint(0, 0))).row();
            if (r < 0) return;
            nom->setText(table->item(r, 0)->text());
            type->setCurrentText(table->item(r, 1)->text());
            marque->setText(table->item(r, 2)->text());
            modele->setText(table->item(r, 3)->text());
            quantite->setValue(table->item(r, 4)->text().toInt());
            etat->setCurrentText(table->item(r, 5)->text());
            table->selectRow(r);
        });

        table->setCellWidget(row, 8, actions);
    }

    tableLayout->addWidget(table);


    connect(btnPdf, &QPushButton::clicked, this, [=]() {
        QString file = QFileDialog::getSaveFileName(this, "Exporter en PDF",
                                                    QDir::homePath() + "/Documents/equipements.pdf", "PDF (*.pdf)");
        if (file.isEmpty()) return;

        QString html = "<h2 style='color:#c40000'>Liste des équipements</h2>"
                       "<table border='1' cellspacing='0' cellpadding='4'><tr>";
        for (int c = 0; c < 8; c++)
            html += "<th>" + table->horizontalHeaderItem(c)->text() + "</th>";
        html += "</tr>";
        for (int r = 0; r < table->rowCount(); r++) {
            html += "<tr>";
            for (int c = 0; c < 8; c++)
                html += "<td>" + (table->item(r, c) ? table->item(r, c)->text() : "") + "</td>";
            html += "</tr>";
        }
        html += "</table>";

        QTextDocument doc;
        doc.setHtml(html);
        QPrinter printer(QPrinter::PrinterResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(file);
        doc.print(&printer);
        QMessageBox::information(this, "PDF", "Export réussi !");
    });

    connect(btnStat, &QPushButton::clicked, this, [=]() {
        int total = table->rowCount();
        int dispo = 0, quantiteTotale = 0;
        for (int r = 0; r < total; r++) {
            if (table->item(r, 5) && table->item(r, 5)->text() == "Disponible") dispo++;
            if (table->item(r, 4)) quantiteTotale += table->item(r, 4)->text().toInt();
        }
        QMessageBox::information(this, "Statistiques",
                                 QString("Nombre d'équipements : %1\nDisponibles : %2\nAutres états : %3\nQuantité totale : %4")
                                     .arg(total).arg(dispo).arg(total - dispo).arg(quantiteTotale));
    });

    splitter->addWidget(tableFrame);

    splitter->setSizes({350,850});

    contentLayout->addWidget(splitter);

    body->addWidget(content);

    mainLayout->addLayout(body);

    connect(add,&QPushButton::clicked,
            this,&smart::ajouter);

    connect(edit,&QPushButton::clicked,
            this,&smart::modifier);

    connect(archive,&QPushButton::clicked,
            this,&smart::archiver);

    connect(recherche,&QLineEdit::textChanged,
            this,&smart::rechercher);
    connect(triCombo, &QComboBox::currentIndexChanged,
            this, &smart::trier);
}
void smart::ajouterLigne(const QString &n, const QString &t, const QString &m,
                         const QString &mod, int q, const QString &e,
                         const QString &da, const QString &dm, const QString &pm)
{
    int row = table->rowCount();
    table->insertRow(row);

    table->setItem(row, 0, new QTableWidgetItem(n));
    table->setItem(row, 1, new QTableWidgetItem(t));
    table->setItem(row, 2, new QTableWidgetItem(m));
    table->setItem(row, 3, new QTableWidgetItem(mod));
    table->setItem(row, 4, new QTableWidgetItem(QString::number(q)));
    table->setItem(row, 5, new QTableWidgetItem(e));
    table->setItem(row, 6, new QTableWidgetItem(da));
    table->setItem(row, 7, new QTableWidgetItem(dm));

    QWidget *actions = new QWidget;
    QHBoxLayout *actLayout = new QHBoxLayout(actions);
    actLayout->setContentsMargins(2, 2, 2, 2);
    actLayout->setSpacing(4);

    QPushButton *btnEdit = new QPushButton("Modif.");
    QPushButton *btnDel  = new QPushButton("Suppr.");
    btnEdit->setStyleSheet("QPushButton{background:white;color:#c40000;border:1px solid #c40000;border-radius:4px;padding:2px 4px;font-size:11px;}"
                           "QPushButton:hover{background:#c40000;color:white;}");
    btnDel->setStyleSheet("QPushButton{background:#c40000;color:white;border:none;border-radius:4px;padding:2px 4px;font-size:11px;}"
                          "QPushButton:hover{background:#8f0000;}");
    btnEdit->setMinimumWidth(60);
    btnDel->setMinimumWidth(60);
    actLayout->addWidget(btnEdit);
    actLayout->addWidget(btnDel);

    connect(btnDel, &QPushButton::clicked, this, [=]() {
        int r = table->indexAt(actions->mapTo(table->viewport(), QPoint(0, 0))).row();
        if (r < 0) return;
        if (QMessageBox::question(this, "Suppression", "Supprimer cet équipement ?") == QMessageBox::Yes)
            table->removeRow(r);
    });

    connect(btnEdit, &QPushButton::clicked, this, [=]() {
        int r = table->indexAt(actions->mapTo(table->viewport(), QPoint(0, 0))).row();
        if (r < 0) return;
        nom->setText(table->item(r, 0)->text());
        type->setCurrentText(table->item(r, 1)->text());
        marque->setText(table->item(r, 2)->text());
        modele->setText(table->item(r, 3)->text());
        quantite->setValue(table->item(r, 4)->text().toInt());
        etat->setCurrentText(table->item(r, 5)->text());
        table->selectRow(r);
    });

    table->setCellWidget(row, 8, actions);
    }




    void smart::ajouter()
    {
        if (nom->text().trimmed().isEmpty() || type->currentIndex() == 0) {
            QMessageBox::warning(this, "Erreur", "Veuillez saisir le nom et choisir un type.");
            return;
        }

        ajouterLigne(nom->text(), type->currentText(), marque->text(), modele->text(),
                     quantite->value(), etat->currentText(),
                     dateAchat->date().toString("dd/MM/yyyy"),
                     dateMaintenance->date().toString("dd/MM/yyyy"),
                     prochaineMaintenance->date().toString("dd/MM/yyyy"));

        nom->clear();
        marque->clear();
        modele->clear();
        remarques->clear();
        quantite->setValue(1);
        type->setCurrentIndex(0);
    }



void smart::modifier()

{
    int r = table->currentRow();
    if (r < 0) {
        QMessageBox::warning(this, "Erreur", "Sélectionnez d'abord une ligne (bouton Modif.).");
        return;
    }
    table->item(r, 0)->setText(nom->text());
    table->item(r, 1)->setText(type->currentText());
    table->item(r, 2)->setText(marque->text());
    table->item(r, 3)->setText(modele->text());
    table->item(r, 4)->setText(QString::number(quantite->value()));
    table->item(r, 5)->setText(etat->currentText());
}

void smart::archiver()
{
    int row = table->currentRow();

    if(row >= 0)
        table->removeRow(row);
}

void smart::rechercher()
{
    QString texte = recherche->text();

    for(int row = 0; row < table->rowCount(); row++)
    {
        bool found = false;

        for(int col = 0; col < table->columnCount(); col++)
        {
            if(table->item(row,col) &&
                table->item(row,col)
                    ->text()
                    .contains(texte,Qt::CaseInsensitive))
            {
                found = true;
                break;
            }
        }

        table->setRowHidden(row,!found);
    }
    QLabel *logo = new QLabel(this);
    logo->setPixmap(QPixmap(":/images/logo.png").scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->move(50, 700);
    logo->show();
}
void smart::trier()
{
    int colonne;
    switch (triCombo->currentIndex()) {
    case 0: colonne = 0; break;
    case 1: colonne = 2; break;
    case 2: colonne = 5; break;
    default: return;
    }

    table->sortItems(colonne, Qt::AscendingOrder);
    rechercher();
}