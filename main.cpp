#include <QApplication>
#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QPainter>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>
#include <algorithm>
#include <vector>

struct Incident {
    int id;
    QString type;
    QDateTime date;
    QString address;
    QString gravity;
    int victims;
    QString description;
    QString status;
    bool archived = false;
};

static QString priority(const Incident &i)
{
    if (i.gravity == "Critique" || i.victims >= 5)
        return "Urgente";

    if (i.gravity == "Élevée" || i.victims >= 2)
        return "Haute";

    if (i.gravity == "Moyenne" || i.victims >= 1)
        return "Normale";

    return "Faible";
}

static QIcon makeIcon(ushort symbol,
                      const QColor &color,
                      int pixelSize = 23)
{
    QPixmap image(34, 34);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setPen(color);

    QFont font("Segoe MDL2 Assets");
    font.setPixelSize(pixelSize);
    painter.setFont(font);

    painter.drawText(
        image.rect(),
        Qt::AlignCenter,
        QString(QChar(symbol))
        );

    return QIcon(image);
}

static QLabel *caption(const QString &text)
{
    auto *label = new QLabel(text);
    label->setObjectName("fieldCaption");
    return label;
}

static QVBoxLayout *field(const QString &title,
                          QWidget *input)
{
    auto *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(7);
    layout->addWidget(caption(title));
    layout->addWidget(input);

    return layout;
}

static QFrame *card()
{
    auto *panel = new QFrame;
    panel->setObjectName("card");

    auto *shadow = new QGraphicsDropShadowEffect(panel);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 5);
    shadow->setColor(QColor(80, 35, 35, 22));
    panel->setGraphicsEffect(shadow);

    return panel;
}

class MainWindow : public QMainWindow
{
public:
    MainWindow()
    {
        setWindowTitle(
            "Smart Fire Station — Gestion des incidents"
            );

        resize(1460, 850);
        setMinimumSize(1040, 690);

        createUi();

        incidents = {
            {
                1,
                "Incendie",
                QDateTime::currentDateTime().addSecs(-7200),
                "Tunis",
                "Critique",
                5,
                "Incendie dans un bâtiment",
                "En cours"
            },
            {
                2,
                "Accident",
                QDateTime::currentDateTime().addDays(-1),
                "Ariana",
                "Élevée",
                2,
                "Accident de la route",
                "Signalé"
            },
            {
                3,
                "Fuite de gaz",
                QDateTime::currentDateTime().addDays(-2),
                "Ben Arous",
                "Moyenne",
                0,
                "Fuite signalée",
                "Terminé"
            },
            {
                4,
                "Sauvetage",
                QDateTime::currentDateTime().addDays(-3),
                "La Marsa",
                "Élevée",
                1,
                "Assistance à une personne",
                "En cours"
            }
        };

        refreshTable();
    }

private:
    std::vector<Incident> incidents;

    int nextId = 5;
    int selectedId = -1;

    QComboBox *typeInput = nullptr;
    QComboBox *gravityInput = nullptr;
    QDateTimeEdit *dateInput = nullptr;
    QSpinBox *victimsInput = nullptr;
    QLineEdit *addressInput = nullptr;
    QTextEdit *descriptionInput = nullptr;
    QComboBox *statusInput = nullptr;

    QLineEdit *searchInput = nullptr;
    QComboBox *filterInput = nullptr;
    QComboBox *sortInput = nullptr;
    QTableWidget *table = nullptr;

    QLabel *countLabel = nullptr;
    QLabel *alertLabel = nullptr;
    QLabel *alertDetails = nullptr;
    QPushButton *alertButton = nullptr;

    void createUi()
    {
        setStyleSheet(R"CSS(
            QMainWindow,
            QWidget#root,
            QScrollArea,
            QWidget#content {
                background: #f3f5f7;
            }

            QFrame#sidebar {
                background: qlineargradient(
                    x1:0,
                    y1:0,
                    x2:0,
                    y2:1,
                    stop:0 #3b0a12,
                    stop:0.42 #74121e,
                    stop:0.76 #ad1826,
                    stop:1 #d33a32
                );

                border-right: 1px solid #5e1019;
            }

            QFrame#card {
                background: #ffffff;
                border: 1px solid #e7e9ed;
                border-radius: 18px;
            }

            QFrame#priorityCard {
                background: #fff5f4;
                border: 2px solid #f2c7c2;
                border-radius: 14px;
            }

            QLabel#heading {
                color: #231f27;
                font-size: 34px;
                font-weight: 800;
            }

            QLabel#subheading {
                color: #777984;
                font-size: 12px;
            }

            QLabel#cardTitle {
                color: #3b252a;
                font-size: 25px;
                font-weight: 800;
            }

            QLabel#titleIcon {
                color: #b51d2a;
                font-size: 29px;
                font-weight: 800;
            }

            QLabel#alertHeading {
                color: #a71928;
                font-size: 18px;
                font-weight: 800;
            }

            QLabel#alertText {
                color: #70444b;
                font-size: 12px;
            }

            QLabel#alertSymbol {
                color: #d0202f;
                font-size: 34px;
                font-weight: 800;
            }

            QLabel#fieldCaption {
                color: #3d3a42;
                font-size: 12px;
                font-weight: 700;
            }

            QLabel#footer {
                color: #ffd9d6;
                font-size: 10px;
                font-weight: 700;
            }

            QLabel#badge {
                color: #a71827;
                background: #fff0f1;
                border: 1px solid #f0c7cc;
                border-radius: 11px;
                padding: 6px 11px;
                font-size: 11px;
                font-weight: 700;
            }

            QLabel#count {
                background: #981a27;
                color: white;
                border-radius: 12px;
                padding: 7px 12px;
                font-size: 11px;
                font-weight: 800;
            }

            QPushButton#nav {
                color: #fff8f7;
                text-align: left;
                border: 1px solid transparent;
                border-radius: 10px;
                padding: 12px 13px;
                font-size: 13px;
            }

            QPushButton#nav:hover {
                background: #8f2632;
                border: 1px solid #cf7780;
            }

            QPushButton#navActive {
                background: #ffffff;
                color: #9d1724;
                text-align: left;
                border: 2px solid #f1bec4;
                border-radius: 11px;
                padding: 12px 13px;
                font-size: 14px;
                font-weight: 800;
            }

            QLineEdit,
            QTextEdit,
            QComboBox,
            QDateTimeEdit,
            QSpinBox {
                background: #fbfbfc;
                color: #26232a;
                border: 2px solid #dfe2e7;
                border-radius: 9px;
                padding: 9px;
                min-height: 23px;
                font-size: 12px;
            }

            QLineEdit:hover,
            QTextEdit:hover,
            QComboBox:hover,
            QDateTimeEdit:hover,
            QSpinBox:hover {
                border: 2px solid #c3a1a7;
                background: white;
            }

            QLineEdit:focus,
            QTextEdit:focus,
            QComboBox:focus,
            QDateTimeEdit:focus,
            QSpinBox:focus {
                border: 2px solid #bd1e2c;
                background: white;
            }

            QComboBox QAbstractItemView {
                background: white;
                color: #27232a;
                selection-background-color: #f8dfe2;
                selection-color: #8e1420;
            }

            QPushButton#primary {
                background: #b51c2a;
                color: white;
                border: 2px solid #8f1420;
                border-radius: 10px;
                padding: 11px 19px;
                font-size: 13px;
                font-weight: 800;
            }

            QPushButton#primary:hover {
                background: #cf2635;
                border-color: #a61725;
            }

            QPushButton#primary:pressed {
                background: #8f1420;
            }

            QPushButton#primary:disabled {
                background: #d8a5aa;
                border-color: #c8999e;
            }

            QPushButton#outline {
                background: white;
                color: #a61725;
                border: 2px solid #c94d59;
                border-radius: 10px;
                padding: 10px 16px;
                font-size: 13px;
                font-weight: 800;
            }

            QPushButton#outline:hover {
                background: #fff1f2;
                border-color: #9c1522;
            }

            QPushButton#subtle {
                background: #fff0f1;
                color: #a61725;
                border: 2px solid #e0a3aa;
                border-radius: 10px;
                padding: 10px 16px;
                font-size: 13px;
                font-weight: 800;
            }

            QPushButton#subtle:hover {
                background: #f9dce0;
                border-color: #b31b29;
            }

            QTableWidget {
                background: white;
                alternate-background-color: #fbf7f8;
                color: #302d34;
                border: 1px solid #e4e6ea;
                border-radius: 10px;
                gridline-color: #eceef1;
                selection-background-color: #ffe5e7;
                selection-color: #2c252a;
                font-size: 12px;
            }

            QHeaderView::section {
                background: #5b1722;
                color: white;
                border: none;
                border-right: 1px solid #762834;
                padding: 13px 5px;
                font-size: 11px;
                font-weight: 800;
            }

            QTableWidget::item {
                padding: 8px;
            }

            QLabel#gravityCritical {
                color: white;
                background: #c1121f;
                border-radius: 10px;
                padding: 6px 10px;
                font-size: 11px;
                font-weight: 800;
            }

            QLabel#gravityHigh {
                color: white;
                background: #e05a47;
                border-radius: 10px;
                padding: 6px 10px;
                font-size: 11px;
                font-weight: 800;
            }

            QLabel#gravityMedium {
                color: #8d4a00;
                background: #ffe0a3;
                border-radius: 10px;
                padding: 6px 10px;
                font-size: 11px;
                font-weight: 800;
            }

            QLabel#gravityLow {
                color: #24623d;
                background: #d9f1e2;
                border-radius: 10px;
                padding: 6px 10px;
                font-size: 11px;
                font-weight: 800;
            }

            QLabel#statusOpen {
                color: #8b5200;
                background: #fff0c7;
                border-radius: 9px;
                padding: 5px 8px;
                font-size: 11px;
                font-weight: 700;
            }

            QLabel#statusActive {
                color: #9c1724;
                background: #ffe1e4;
                border-radius: 9px;
                padding: 5px 8px;
                font-size: 11px;
                font-weight: 700;
            }

            QLabel#statusDone {
                color: #236440;
                background: #dcf3e5;
                border-radius: 9px;
                padding: 5px 8px;
                font-size: 11px;
                font-weight: 700;
            }

            QPushButton#tableAction {
                background: #fff4f5;
                color: #9e1724;
                border: 1px solid #efc9cd;
                border-radius: 7px;
                padding: 4px;
            }

            QPushButton#tableAction:hover {
                background: #f6d6da;
                border-color: #bd5964;
            }
        )CSS");

        auto *root = new QWidget;
        root->setObjectName("root");
        setCentralWidget(root);

        auto *outer = new QHBoxLayout(root);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);

        // SIDEBAR
        auto *sidebar = new QFrame;
        sidebar->setObjectName("sidebar");
        sidebar->setFixedWidth(218);
        outer->addWidget(sidebar);

        auto *side = new QVBoxLayout(sidebar);
        side->setContentsMargins(17, 24, 17, 24);
        side->setSpacing(8);

        // LOGO
        auto *logo = new QLabel;
        logo->setFixedSize(116, 116);
        logo->setAlignment(Qt::AlignCenter);
        logo->setStyleSheet(
            "background:white;"
            "border:2px solid #f0c5c9;"
            "border-radius:14px;"
            );

        const QPixmap picture(":/images/logo.png");

        if (!picture.isNull()) {
            logo->setPixmap(
                picture.scaled(
                    104,
                    104,
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation
                    )
                );
        }

        side->addWidget(logo, 0, Qt::AlignHCenter);
        side->addSpacing(18);

        const QStringList navTexts = {
            "Tableau de bord",
            "Incidents",
            "Interventions",
            "Pompiers",
            "Véhicules",
            "Équipements",
            "Rapports"
        };

        const ushort navSymbols[] = {
            0xE80F,
            0xE7BA,
            0xE77B,
            0xE716,
            0xE7F4,
            0xE713,
            0xE9D9
        };

        for (int index = 0;
             index < navTexts.size();
             ++index) {

            const QString name = navTexts[index];
            const bool active = name == "Incidents";

            auto *nav = new QPushButton(name);
            nav->setObjectName(
                active ? "navActive" : "nav"
                );

            nav->setIcon(
                makeIcon(
                    navSymbols[index],
                    QColor(active ? "#9d1724" : "#ffffff"),
                    24
                    )
                );

            nav->setIconSize(QSize(27, 27));
            nav->setMinimumHeight(50);
            nav->setCursor(Qt::PointingHandCursor);

            side->addWidget(nav);

            if (!active) {
                connect(
                    nav,
                    &QPushButton::clicked,
                    this,
                    [this, name] {
                        QMessageBox::information(
                            this,
                            name,
                            "Ce module sera intégré prochainement."
                            );
                    }
                    );
            }
        }

        side->addStretch();

        auto *footer = new QLabel("PROTECTION CIVILE");
        footer->setObjectName("footer");

        side->addWidget(
            footer,
            0,
            Qt::AlignHCenter
            );

        // ZONE PRINCIPALE
        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        outer->addWidget(scroll, 1);

        auto *content = new QWidget;
        content->setObjectName("content");
        scroll->setWidget(content);

        auto *page = new QVBoxLayout(content);
        page->setContentsMargins(28, 26, 28, 26);
        page->setSpacing(22);

        // ENTÊTE
        auto *header = new QHBoxLayout;
        auto *titles = new QVBoxLayout;

        auto *heading = new QLabel(
            "Gestion des incidents"
            );
        heading->setObjectName("heading");
        titles->addWidget(heading);

        auto *subtitle = new QLabel(
            "Créer, consulter et suivre "
            "les incidents en temps réel"
            );
        subtitle->setObjectName("subheading");
        titles->addWidget(subtitle);

        auto *titleLine = new QFrame;
        titleLine->setFixedSize(82, 4);
        titleLine->setStyleSheet(
            "background:#c51f2e;"
            "border:0;"
            "border-radius:2px;"
            );

        titles->addSpacing(5);
        titles->addWidget(titleLine);

        header->addLayout(titles);
        header->addStretch();

        auto *role = new QLabel(
            "Agent de coordination"
            );
        role->setStyleSheet(
            "color:#8e1521;"
            "background:white;"
            "border:2px solid #ebc5ca;"
            "border-radius:12px;"
            "padding:11px 17px;"
            "font-weight:bold;"
            );

        header->addWidget(
            role,
            0,
            Qt::AlignTop
            );

        page->addLayout(header);

        // DEUX COLONNES
        auto *columns = new QHBoxLayout;
        columns->setSpacing(18);
        page->addLayout(columns, 1);

        // FORMULAIRE
        auto *formCard = card();
        columns->addWidget(formCard, 47);

        auto *form = new QVBoxLayout(formCard);
        form->setContentsMargins(23, 23, 23, 23);
        form->setSpacing(15);

        auto *formTop = new QHBoxLayout;

        auto *formIcon = new QLabel("▣");
        formIcon->setObjectName("titleIcon");
        formTop->addWidget(formIcon);

        auto *formTitle = new QLabel(
            "Nouvel incident"
            );
        formTitle->setObjectName("cardTitle");
        formTop->addWidget(formTitle);

        formTop->addStretch();

        auto *badge = new QLabel("CRUD");
        badge->setObjectName("badge");
        formTop->addWidget(badge);

        form->addLayout(formTop);

        auto *formHint = new QLabel(
            "Saisir les informations de l'incident"
            );
        formHint->setObjectName("subheading");
        form->addWidget(formHint);

        typeInput = new QComboBox;
        typeInput->addItems({
            "Incendie",
            "Accident",
            "Fuite de gaz",
            "Sauvetage",
            "Autre"
        });

        gravityInput = new QComboBox;
        gravityInput->addItems({
            "Faible",
            "Moyenne",
            "Élevée",
            "Critique"
        });

        dateInput = new QDateTimeEdit(
            QDateTime::currentDateTime()
            );
        dateInput->setDisplayFormat(
            "dd/MM/yyyy HH:mm"
            );
        dateInput->setCalendarPopup(true);

        victimsInput = new QSpinBox;
        victimsInput->setRange(0, 999);

        addressInput = new QLineEdit;
        addressInput->setPlaceholderText(
            "Ex. Tunis, Centre-ville"
            );

        descriptionInput = new QTextEdit;
        descriptionInput->setFixedHeight(92);
        descriptionInput->setPlaceholderText(
            "Décrivez brièvement la situation "
            "et les risques observés..."
            );

        statusInput = new QComboBox;
        statusInput->addItems({
            "Signalé",
            "En cours",
            "Terminé"
        });

        auto *grid = new QGridLayout;
        grid->setHorizontalSpacing(14);
        grid->setVerticalSpacing(17);

        grid->addLayout(
            field("Type d'incident", typeInput),
            0,
            0
            );

        grid->addLayout(
            field("Niveau de gravité", gravityInput),
            0,
            1
            );

        grid->addLayout(
            field("Date et heure", dateInput),
            1,
            0
            );

        grid->addLayout(
            field("Nombre de victimes", victimsInput),
            1,
            1
            );

        grid->addLayout(
            field("Adresse", addressInput),
            2,
            0,
            1,
            2
            );

        grid->addLayout(
            field("Description", descriptionInput),
            3,
            0,
            1,
            2
            );

        grid->addLayout(
            field("Statut", statusInput),
            4,
            0,
            1,
            2
            );

        form->addLayout(grid);
        form->addStretch();

        // BOUTONS CRUD
        auto *actions = new QHBoxLayout;
        actions->setSpacing(10);

        auto *addButton = new QPushButton("Ajouter");
        auto *editButton = new QPushButton("Modifier");
        auto *archiveButton = new QPushButton("Archiver");

        addButton->setObjectName("primary");
        editButton->setObjectName("outline");
        archiveButton->setObjectName("subtle");

        addButton->setIcon(
            makeIcon(
                0xE710,
                QColor("#ffffff"),
                22
                )
            );

        editButton->setIcon(
            makeIcon(
                0xE70F,
                QColor("#a61725"),
                21
                )
            );

        archiveButton->setIcon(
            makeIcon(
                0xE7B8,
                QColor("#a61725"),
                21
                )
            );

        addButton->setIconSize(QSize(23, 23));
        editButton->setIconSize(QSize(23, 23));
        archiveButton->setIconSize(QSize(23, 23));

        addButton->setMinimumHeight(45);
        editButton->setMinimumHeight(45);
        archiveButton->setMinimumHeight(45);

        actions->addWidget(addButton);
        actions->addWidget(editButton);
        actions->addWidget(archiveButton);
        actions->addStretch();

        form->addLayout(actions);

        // LISTE DES INCIDENTS
        auto *listCard = card();
        columns->addWidget(listCard, 53);

        auto *list = new QVBoxLayout(listCard);
        list->setContentsMargins(23, 23, 23, 23);
        list->setSpacing(16);

        auto *listTop = new QHBoxLayout;

        auto *listIcon = new QLabel("☷");
        listIcon->setObjectName("titleIcon");
        listTop->addWidget(listIcon);

        auto *listTitle = new QLabel(
            "Liste des incidents"
            );
        listTitle->setObjectName("cardTitle");
        listTop->addWidget(listTitle);

        listTop->addStretch();

        countLabel = new QLabel;
        countLabel->setObjectName("count");
        listTop->addWidget(countLabel);

        list->addLayout(listTop);

        searchInput = new QLineEdit;
        searchInput->setPlaceholderText(
            "Rechercher par type ou adresse..."
            );

        searchInput->addAction(
            makeIcon(
                0xE721,
                QColor("#a71928"),
                20
                ),
            QLineEdit::LeadingPosition
            );

        filterInput = new QComboBox;
        filterInput->addItems({
            "Toutes gravités",
            "Faible",
            "Moyenne",
            "Élevée",
            "Critique"
        });

        auto *searchLine = new QHBoxLayout;
        searchLine->addWidget(searchInput, 2);
        searchLine->addWidget(filterInput, 1);

        list->addLayout(searchLine);

        sortInput = new QComboBox;
        sortInput->addItems({
            "Date : récente d'abord",
            "Date : ancienne d'abord",
            "Gravité : élevée d'abord",
            "Gravité : faible d'abord"
        });

        auto *sortLine = new QHBoxLayout;
        sortLine->addWidget(
            caption("Trier par :")
            );
        sortLine->addWidget(sortInput);
        sortLine->addStretch();

        list->addLayout(sortLine);

        // TABLEAU
        table = new QTableWidget(0, 6);

        table->setHorizontalHeaderLabels({
            "ID",
            "TYPE",
            "ADRESSE",
            "GRAVITÉ",
            "STATUT",
            "ACTION"
        });

        table->horizontalHeader()
            ->setSectionResizeMode(
                QHeaderView::Stretch
                );

        table->horizontalHeader()
            ->setSectionResizeMode(
                0,
                QHeaderView::ResizeToContents
                );

        table->setEditTriggers(
            QAbstractItemView::NoEditTriggers
            );

        table->setSelectionBehavior(
            QAbstractItemView::SelectRows
            );

        table->setSelectionMode(
            QAbstractItemView::SingleSelection
            );

        table->verticalHeader()->hide();
        table->setAlternatingRowColors(true);
        table->setMinimumHeight(280);

        list->addWidget(table, 1);

        // ALERTE
        auto *alertCard = new QFrame;
        alertCard->setObjectName("priorityCard");

        auto *alertRow = new QHBoxLayout(alertCard);
        alertRow->setContentsMargins(18, 17, 18, 17);
        alertRow->setSpacing(14);

        auto *alertSymbol = new QLabel("⚠");
        alertSymbol->setObjectName("alertSymbol");
        alertRow->addWidget(alertSymbol);

        auto *alertColumn = new QVBoxLayout;

        auto *alertHeading = new QLabel(
            "Alerte de priorité"
            );
        alertHeading->setObjectName("alertHeading");
        alertColumn->addWidget(alertHeading);

        alertLabel = new QLabel;
        alertLabel->setObjectName("alertText");
        alertLabel->setWordWrap(true);
        alertColumn->addWidget(alertLabel);

        alertDetails = new QLabel;
        alertDetails->setObjectName("alertText");
        alertDetails->setWordWrap(true);
        alertColumn->addWidget(alertDetails);

        alertRow->addLayout(alertColumn, 1);

        alertButton = new QPushButton(
            "Voir les détails  ›"
            );
        alertButton->setObjectName("primary");

        alertButton->setIcon(
            makeIcon(
                0xE76C,
                QColor("#ffffff"),
                20
                )
            );

        alertButton->setIconSize(QSize(22, 22));
        alertButton->setMinimumHeight(43);

        alertRow->addWidget(alertButton);
        list->addWidget(alertCard);

        // BOUTONS STATISTIQUES / PDF
        auto *bottom = new QHBoxLayout;

        auto *statsButton = new QPushButton(
            "Statistiques"
            );

        auto *pdfButton = new QPushButton(
            "Exporter en PDF"
            );

        statsButton->setObjectName("outline");
        pdfButton->setObjectName("outline");

        statsButton->setIcon(
            makeIcon(
                0xE9D9,
                QColor("#a61725"),
                20
                )
            );

        pdfButton->setIcon(
            makeIcon(
                0xE749,
                QColor("#a61725"),
                20
                )
            );

        statsButton->setIconSize(QSize(22, 22));
        pdfButton->setIconSize(QSize(22, 22));

        statsButton->setMinimumHeight(43);
        pdfButton->setMinimumHeight(43);

        bottom->addStretch();
        bottom->addWidget(statsButton);
        bottom->addWidget(pdfButton);

        list->addLayout(bottom);

        // CONNEXIONS
        connect(
            addButton,
            &QPushButton::clicked,
            this,
            [this] {
                addIncident();
            }
            );

        connect(
            editButton,
            &QPushButton::clicked,
            this,
            [this] {
                editIncident();
            }
            );

        connect(
            archiveButton,
            &QPushButton::clicked,
            this,
            [this] {
                archiveIncident();
            }
            );

        connect(
            searchInput,
            &QLineEdit::textChanged,
            this,
            [this] {
                refreshTable();
            }
            );

        connect(
            filterInput,
            &QComboBox::currentIndexChanged,
            this,
            [this] {
                refreshTable();
            }
            );

        connect(
            sortInput,
            &QComboBox::currentIndexChanged,
            this,
            [this] {
                refreshTable();
            }
            );

        connect(
            table,
            &QTableWidget::cellClicked,
            this,
            [this](int row, int) {
                chooseRow(row);
            }
            );

        connect(
            statsButton,
            &QPushButton::clicked,
            this,
            [this] {
                showStatistics();
            }
            );

        connect(
            pdfButton,
            &QPushButton::clicked,
            this,
            [this] {
                exportPdf();
            }
            );

        connect(
            alertButton,
            &QPushButton::clicked,
            this,
            [this] {
                showUrgentIncident();
            }
            );
    }

    Incident *selected()
    {
        for (auto &i : incidents) {
            if (i.id == selectedId && !i.archived)
                return &i;
        }

        return nullptr;
    }

    void clearForm()
    {
        selectedId = -1;

        table->clearSelection();
        typeInput->setCurrentIndex(0);
        gravityInput->setCurrentIndex(0);
        dateInput->setDateTime(
            QDateTime::currentDateTime()
            );
        victimsInput->setValue(0);
        addressInput->clear();
        descriptionInput->clear();
        statusInput->setCurrentIndex(0);
    }

    void chooseRow(int row)
    {
        if (row < 0 || !table->item(row, 0))
            return;

        selectedId =
            table->item(row, 0)
                ->data(Qt::UserRole)
                .toInt();

        Incident *i = selected();

        if (!i)
            return;

        typeInput->setCurrentText(i->type);
        gravityInput->setCurrentText(i->gravity);
        dateInput->setDateTime(i->date);
        victimsInput->setValue(i->victims);
        addressInput->setText(i->address);
        descriptionInput->setPlainText(
            i->description
            );
        statusInput->setCurrentText(i->status);
    }

    bool validate()
    {
        if (addressInput->text()
                .trimmed()
                .isEmpty()) {

            QMessageBox::warning(
                this,
                "Adresse manquante",
                "Saisissez l'adresse de l'incident."
                );

            addressInput->setFocus();

            return false;
        }

        return true;
    }

    void copyFormTo(Incident &i)
    {
        i.type = typeInput->currentText();
        i.gravity = gravityInput->currentText();
        i.date = dateInput->dateTime();
        i.victims = victimsInput->value();

        i.address =
            addressInput->text().trimmed();

        i.description =
            descriptionInput
                ->toPlainText()
                .trimmed();

        i.status = statusInput->currentText();
    }

    void addIncident()
    {
        if (!validate())
            return;

        Incident i;
        i.id = nextId++;

        copyFormTo(i);
        incidents.push_back(i);

        clearForm();
        refreshTable();
    }

    void editIncident()
    {
        Incident *i = selected();

        if (!i) {
            QMessageBox::information(
                this,
                "Modifier",
                "Sélectionnez d'abord "
                "un incident dans la liste."
                );

            return;
        }

        if (!validate())
            return;

        copyFormTo(*i);

        clearForm();
        refreshTable();
    }

    void archiveIncident()
    {
        Incident *i = selected();

        if (!i) {
            QMessageBox::information(
                this,
                "Archiver",
                "Sélectionnez d'abord "
                "un incident dans la liste."
                );

            return;
        }

        if (QMessageBox::question(
                this,
                "Archiver",
                "Archiver l'incident sélectionné ?"
                ) != QMessageBox::Yes) {

            return;
        }

        i->archived = true;

        clearForm();
        refreshTable();
    }

    void showUrgentIncident()
    {
        int urgentId = -1;

        for (const auto &i : incidents) {
            if (!i.archived
                && priority(i) == "Urgente"
                && i.status != "Terminé") {

                urgentId = i.id;
                break;
            }
        }

        if (urgentId < 0)
            return;

        searchInput->clear();
        filterInput->setCurrentIndex(0);
        refreshTable();

        for (int row = 0;
             row < table->rowCount();
             ++row) {

            const int id =
                table->item(row, 0)
                    ->data(Qt::UserRole)
                    .toInt();

            if (id == urgentId) {
                table->selectRow(row);
                chooseRow(row);

                table->scrollToItem(
                    table->item(row, 0)
                    );

                return;
            }
        }
    }

    void refreshTable()
    {
        std::vector<const Incident *> visible;

        const QString search =
            searchInput->text().trimmed();

        const QString filter =
            filterInput->currentText();

        for (const auto &i : incidents) {
            if (i.archived)
                continue;

            if (filter != "Toutes gravités"
                && i.gravity != filter) {

                continue;
            }

            if (!search.isEmpty()
                && !i.type.contains(
                    search,
                    Qt::CaseInsensitive
                    )
                && !i.address.contains(
                    search,
                    Qt::CaseInsensitive
                    )) {

                continue;
            }

            visible.push_back(&i);
        }

        auto gravityRank =
            [](const QString &gravity) {

                if (gravity == "Critique")
                    return 4;

                if (gravity == "Élevée")
                    return 3;

                if (gravity == "Moyenne")
                    return 2;

                return 1;
            };

        const int sort =
            sortInput->currentIndex();

        std::stable_sort(
            visible.begin(),
            visible.end(),
            [sort, &gravityRank](
                const Incident *a,
                const Incident *b
                ) {
                if (sort == 0)
                    return a->date > b->date;

                if (sort == 1)
                    return a->date < b->date;

                if (sort == 2) {
                    return gravityRank(a->gravity)
                    > gravityRank(b->gravity);
                }

                return gravityRank(a->gravity)
                       < gravityRank(b->gravity);
            }
            );

        table->setRowCount(0);

        for (const Incident *i : visible) {
            const int row = table->rowCount();
            table->insertRow(row);

            const QStringList values = {
                QString("INC-%1")
            .arg(
                i->id,
                3,
                10,
                QChar('0')
                ),
                i->type,
                i->address
        };

        for (int col = 0;
             col < values.size();
             ++col) {

            auto *cell =
                new QTableWidgetItem(
                    values[col]
                    );

            cell->setData(
                Qt::UserRole,
                i->id
                );

            table->setItem(
                row,
                col,
                cell
                );
        }

        // BADGE GRAVITÉ
        auto *gravityCell = new QWidget;
        gravityCell->setStyleSheet(
            "background:transparent;"
            );

        auto *gravityLayout =
            new QHBoxLayout(gravityCell);

        gravityLayout->setContentsMargins(
            5,
            4,
            5,
            4
            );

        auto *gravityBadge =
            new QLabel(i->gravity);

        gravityBadge->setAlignment(
            Qt::AlignCenter
            );

        gravityBadge->setObjectName(
            i->gravity == "Critique"
                ? "gravityCritical"
                : i->gravity == "Élevée"
                      ? "gravityHigh"
                      : i->gravity == "Moyenne"
                            ? "gravityMedium"
                            : "gravityLow"
            );

        gravityLayout->addWidget(
            gravityBadge
            );

        gravityLayout->addStretch();

        table->setCellWidget(
            row,
            3,
            gravityCell
            );

        // BADGE STATUT
        auto *statusWidget = new QWidget;
        statusWidget->setStyleSheet(
            "background:transparent;"
            );

        auto *statusLayout =
            new QHBoxLayout(statusWidget);

        statusLayout->setContentsMargins(
            4,
            5,
            4,
            5
            );

        auto *statusBadge =
            new QLabel(i->status);

        statusBadge->setAlignment(
            Qt::AlignCenter
            );

        statusBadge->setObjectName(
            i->status == "Terminé"
                ? "statusDone"
                : i->status == "En cours"
                      ? "statusActive"
                      : "statusOpen"
            );

        statusLayout->addWidget(statusBadge);
        statusLayout->addStretch();

        table->setCellWidget(
            row,
            4,
            statusWidget
            );

        // ACTIONS
        auto *actionCell = new QWidget;
        actionCell->setStyleSheet(
            "background:transparent;"
            );

        auto *actionLayout =
            new QHBoxLayout(actionCell);

        actionLayout->setContentsMargins(
            0,
            0,
            0,
            0
            );

        actionLayout->setSpacing(4);

        auto *viewButton =
            new QPushButton;

        viewButton->setToolTip(
            "Afficher et modifier"
            );

        viewButton->setObjectName(
            "tableAction"
            );

        viewButton->setIcon(
            makeIcon(
                0xE890,
                QColor("#9e1724"),
                19
                )
            );

        viewButton->setIconSize(
            QSize(20, 20)
            );

        auto *archiveButton =
            new QPushButton;

        archiveButton->setToolTip(
            "Archiver"
            );

        archiveButton->setObjectName(
            "tableAction"
            );

        archiveButton->setIcon(
            makeIcon(
                0xE7B8,
                QColor("#9e1724"),
                19
                )
            );

        archiveButton->setIconSize(
            QSize(20, 20)
            );

        actionLayout->addWidget(viewButton);
        actionLayout->addWidget(archiveButton);

        table->setCellWidget(
            row,
            5,
            actionCell
            );

        const int incidentId = i->id;

        connect(
            viewButton,
            &QPushButton::clicked,
            this,
            [this, incidentId, row] {
                selectedId = incidentId;
                table->selectRow(row);
                chooseRow(row);
            }
            );

        connect(
            archiveButton,
            &QPushButton::clicked,
            this,
            [this, incidentId] {
                selectedId = incidentId;
                archiveIncident();
            }
            );

        table->setRowHeight(row, 50);
    }

    countLabel->setText(
        QString("%1 incidents")
            .arg(
                int(visible.size())
                )
        );

    int urgent = 0;
    const Incident *firstUrgent = nullptr;

    for (const auto &i : incidents) {
        if (!i.archived
            && priority(i) == "Urgente"
            && i.status != "Terminé") {

            if (!firstUrgent)
                firstUrgent = &i;

            ++urgent;
        }
    }

    if (urgent > 0) {
        alertLabel->setText(
            QString(
                "%1 incident(s) urgents nécessitent "
                "une intervention immédiate."
                ).arg(urgent)
            );
    } else {
        alertLabel->setText(
            "Aucun incident urgent en attente."
            );
    }

    if (firstUrgent) {
        alertDetails->setText(
            QString("INC-%1 — %2 — %3")
                .arg(
                    firstUrgent->id,
                    3,
                    10,
                    QChar('0')
                    )
                .arg(
                    firstUrgent->type,
                    firstUrgent->address
                    )
            );
    } else {
        alertDetails->setText(
            "La liste des incidents est à jour."
            );
    }

    alertButton->setEnabled(
        firstUrgent != nullptr
        );
}

void showStatistics()
{
    QDialog dialog(this);

    dialog.setWindowTitle(
        "Statistiques — Gestion des incidents"
        );

    dialog.resize(560, 420);

    dialog.setStyleSheet(
        "QDialog{background:#f3f5f7;}"
        "QLabel{color:#302b2b;}"
        "QProgressBar{"
        "border:1px solid #dec5c9;"
        "background:#ece3e5;"
        "border-radius:5px;"
        "height:17px;"
        "}"
        "QProgressBar::chunk{"
        "background:#b51c2a;"
        "border-radius:5px;"
        "}"
        );

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(
        26,
        24,
        26,
        24
        );

    auto *title = new QLabel(
        "Répartition des incidents par type"
        );

    title->setStyleSheet(
        "font-size:20px;"
        "font-weight:bold;"
        "color:#991824;"
        );

    layout->addWidget(title);

    int total = 0;

    for (const auto &i : incidents) {
        if (!i.archived)
            ++total;
    }

    layout->addWidget(
        new QLabel(
            QString("%1 incidents actifs")
                .arg(total)
            )
        );

    const QStringList types = {
        "Incendie",
        "Accident",
        "Fuite de gaz",
        "Sauvetage",
        "Autre"
    };

    for (const QString &type : types) {
        int count = 0;

        for (const auto &i : incidents) {
            if (!i.archived
                && i.type == type) {

                ++count;
            }
        }

        layout->addWidget(
            new QLabel(
                QString("%1 (%2)")
                    .arg(type)
                    .arg(count)
                )
            );

        auto *bar = new QProgressBar;

        bar->setRange(
            0,
            std::max(1, total)
            );

        bar->setValue(count);
        bar->setTextVisible(false);

        layout->addWidget(bar);
    }

    layout->addStretch();
    dialog.exec();
}

void exportPdf()
{
    QString path =
        QFileDialog::getSaveFileName(
            this,
            "Enregistrer la liste des incidents",
            "incidents.pdf",
            "PDF (*.pdf)"
            );

    if (path.isEmpty())
        return;

    if (!path.endsWith(
            ".pdf",
            Qt::CaseInsensitive)) {

        path += ".pdf";
    }

    QFile file(path);

    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(
            this,
            "Export PDF",
            "Impossible de créer le fichier PDF."
            );

        return;
    }

    QString html =
        "<h1 style='color:#b51c2a'>"
        "SMART FIRE STATION"
        "</h1>"

        "<h2>Liste des incidents</h2>"

        "<table width='100%' "
        "border='1' "
        "cellspacing='0' "
        "cellpadding='6'>"

        "<tr style='"
        "background:#5b1722;"
        "color:white'>"

        "<th>ID</th>"
        "<th>Type</th>"
        "<th>Date</th>"
        "<th>Adresse</th>"
        "<th>Gravité</th>"
        "<th>Victimes</th>"
        "<th>Statut</th>"
        "<th>Priorité</th>"

        "</tr>";

    for (const auto &i : incidents) {
        if (i.archived)
            continue;

        const QStringList values = {
            QString("INC-%1")
        .arg(
            i.id,
            3,
            10,
            QChar('0')
            ),
            i.type,
            i.date.toString(
                "dd/MM/yyyy HH:mm"
                ),
            i.address,
            i.gravity,
            QString::number(i.victims),
            i.status,
            priority(i)
    };

    html += "<tr>";

    for (const QString &value : values) {
        html +=
            "<td>"
            + value.toHtmlEscaped()
            + "</td>";
    }

    html += "</tr>";
}

html += "</table>";

{
    QPdfWriter pdf(&file);

    pdf.setPageSize(
        QPageSize(QPageSize::A4)
        );

    pdf.setPageOrientation(
        QPageLayout::Landscape
        );

    pdf.setPageMargins(
        QMarginsF(12, 12, 12, 12)
        );

    QTextDocument document;
    document.setHtml(html);
    document.print(&pdf);
}

file.close();

QMessageBox::information(
    this,
    "Export PDF",
    "PDF enregistré :\n" + path
    );
}
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setStyle("Fusion");
    app.setFont(QFont("Segoe UI", 10));

    MainWindow window;
    window.show();

    return app.exec();
}