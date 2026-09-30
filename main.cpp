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

struct Incident
{
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

static QString priority(const Incident &incident)
{
    if (incident.gravity == "Critique" || incident.victims >= 5)
        return "Urgente";

    if (incident.gravity == "Élevée" || incident.victims >= 2)
        return "Haute";

    if (incident.gravity == "Moyenne" || incident.victims >= 1)
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

    painter.drawText(image.rect(),
                     Qt::AlignCenter,
                     QString(QChar(symbol)));

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
            "Smart Fire Station — Gestion des incidents");

        resize(1460, 850);
        setMinimumSize(1040, 690);

        createUi();

        incidents = {
            {1,
             "Incendie",
             QDateTime::currentDateTime().addSecs(-7200),
             "Tunis",
             "Critique",
             5,
             "Incendie dans un bâtiment",
             "En cours"},

            {2,
             "Accident",
             QDateTime::currentDateTime().addDays(-1),
             "Ariana",
             "Élevée",
             2,
             "Accident de la route",
             "Signalé"},

            {3,
             "Fuite de gaz",
             QDateTime::currentDateTime().addDays(-2),
             "Ben Arous",
             "Moyenne",
             0,
             "Fuite signalée",
             "Terminé"},

            {4,
             "Sauvetage",
             QDateTime::currentDateTime().addDays(-3),
             "La Marsa",
             "Élevée",
             1,
             "Assistance à une personne",
             "En cours"}
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
                    x1:0, y1:0, x2:0, y2:1,
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

            QLabel#gravityCritical {
                color: white;
                background: #c1121f;
                border-radius: 10px;
                padding: 6px 10px;
                font-weight: 800;
            }

            QLabel#gravityHigh {
                color: white;
                background: #e05a47;
                border-radius: 10px;
                padding: 6px 10px;
                font-weight: 800;
            }

            QLabel#gravityMedium {
                color: #8d4a00;
                background: #ffe0a3;
                border-radius: 10px;
                padding: 6px 10px;
                font-weight: 800;
            }

            QLabel#gravityLow {
                color: #24623d;
                background: #d9f1e2;
                border-radius: 10px;
                padding: 6px 10px;
                font-weight: 800;
            }

            QLabel#statusOpen {
                color: #8b5200;
                background: #fff0c7;
                border-radius: 9px;
                padding: 5px 8px;
                font-weight: 700;
            }

            QLabel#statusActive {
                color: #9c1724;
                background: #ffe1e4;
                border-radius: 9px;
                padding: 5px 8px;
                font-weight: 700;
            }

            QLabel#statusDone {
                color: #236440;
                background: #dcf3e5;
                border-radius: 9px;
                padding: 5px 8px;
                font-weight: 700;
            }

            QPushButton#tableAction {
                background: #fff4f5;
                color: #9e1724;
                border: 1px solid #efc9cd;
                border-radius: 7px;
                padding: 4px;
            }
        )CSS");

        auto *root = new QWidget;
        root->setObjectName("root");
        setCentralWidget(root);

        auto *outer = new QHBoxLayout(root);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);

        auto *sidebar = new QFrame;
        sidebar->setObjectName("sidebar");
        sidebar->setFixedWidth(218);
        outer->addWidget(sidebar);

        auto *side = new QVBoxLayout(sidebar);
        side->setContentsMargins(17, 24, 17, 24);
        side->setSpacing(8);

        auto *logo = new QLabel;
        logo->setFixedSize(116, 116);
        logo->setAlignment(Qt::AlignCenter);
        logo->setStyleSheet(
            "background:white;"
            "border:2px solid #f0c5c9;"
            "border-radius:14px;");

        const QPixmap picture(":/images/logo.png");

        if (!picture.isNull()) {
            logo->setPixmap(
                picture.scaled(
                    104,
                    104,
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation));
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
            nav->setObjectName(active ? "navActive" : "nav");

            nav->setIcon(
                makeIcon(
                    navSymbols[index],
                    QColor(active ? "#9d1724" : "#ffffff"),
                    24));

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
                            "Ce module sera intégré prochainement.");
                    });
            }
        }

        side->addStretch();

        auto *footer = new QLabel("PROTECTION CIVILE");
        footer->setObjectName("footer");
        side->addWidget(footer, 0, Qt::AlignHCenter);

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

        auto *header = new QHBoxLayout;
        auto *titles = new QVBoxLayout;

        auto *heading = new QLabel("Gestion des incidents");
        heading->setObjectName("heading");
        titles->addWidget(heading);

        auto *subtitle = new QLabel(
            "Créer, consulter et suivre les incidents en temps réel");

        subtitle->setObjectName("subheading");
        titles->addWidget(subtitle);

        auto *titleLine = new QFrame;
        titleLine->setFixedSize(82, 4);
        titleLine->setStyleSheet(
            "background:#c51f2e;"
            "border:0;"
            "border-radius:2px;");

        titles->addSpacing(5);
        titles->addWidget(titleLine);

        header->addLayout(titles);
        header->addStretch();

        auto *role = new QLabel("Agent de coordination");
        role->setStyleSheet(
            "color:#8e1521;"
            "background:white;"
            "border:2px solid #ebc5ca;"
            "border-radius:12px;"
            "padding:11px 17px;"
            "font-weight:bold;");

        header->addWidget(role, 0, Qt::AlignTop);
        page->addLayout(header);

        auto *columns = new QHBoxLayout;
        columns->setSpacing(18);
        page->addLayout(columns, 1);

        auto *formCard = card();
        columns->addWidget(formCard, 47);

        auto *form = new QVBoxLayout(formCard);
        form->setContentsMargins(23, 23, 23, 23);
        form->setSpacing(15);

        auto *formTop = new QHBoxLayout;

        auto *formIcon = new QLabel("▣");
        formIcon->setObjectName("titleIcon");
        formTop->addWidget(formIcon);

        auto *formTitle = new QLabel("Nouvel incident");
        formTitle->setObjectName("cardTitle");
        formTop->addWidget(formTitle);

        formTop->addStretch();

        auto *badge = new QLabel("CRUD");
        badge->setObjectName("badge");
        formTop->addWidget(badge);

        form->addLayout(formTop);

        auto *formHint = new QLabel(
            "Saisir les informations de l'incident");

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
            QDateTime::currentDateTime());

        dateInput->setDisplayFormat("dd/MM/yyyy HH:mm");
        dateInput->setCalendarPopup(true);

        victimsInput = new QSpinBox;
        victimsInput->setRange(0, 999);

        addressInput = new QLineEdit;
        addressInput->setPlaceholderText(
            "Ex. Tunis, Centre-ville");

        descriptionInput = new QTextEdit;
        descriptionInput->setFixedHeight(92);
        descriptionInput->setPlaceholderText(
            "Décrivez brièvement la situation et les risques observés...");

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
            0, 0);

        grid->addLayout(
            field("Niveau de gravité", gravityInput),
            0, 1);

        grid->addLayout(
            field("Date et heure", dateInput),
            1, 0);

        grid->addLayout(
            field("Nombre de victimes", victimsInput),
            1, 1);

        grid->addLayout(
            field("Adresse", addressInput),
            2, 0, 1, 2);

        grid->addLayout(
            field("Description", descriptionInput),
            3, 0, 1, 2);

        grid->addLayout(
            field("Statut", statusInput),
            4, 0, 1, 2);

        form->addLayout(grid);
        form->addStretch();

        auto *actions = new QHBoxLayout;
        actions->setSpacing(10);

        auto *addButton = new QPushButton("Ajouter");
        auto *editButton = new QPushButton("Modifier");
        auto *archiveButton = new QPushButton("Archiver");

        addButton->setObjectName("primary");
        editButton->setObjectName("outline");
        archiveButton->setObjectName("subtle");

        actions->addWidget(addButton);
        actions->addWidget(editButton);
        actions->addWidget(archiveButton);
        actions->addStretch();

        form->addLayout(actions);

        auto *listCard = card();
        columns->addWidget(listCard, 53);

        auto *list = new QVBoxLayout(listCard);
        list->setContentsMargins(23, 23, 23, 23);
        list->setSpacing(16);

        auto *listTop = new QHBoxLayout;

        auto *listIcon = new QLabel("☷");
        listIcon->setObjectName("titleIcon");
        listTop->addWidget(listIcon);

        auto *listTitle = new QLabel("Liste des incidents");
        listTitle->setObjectName("cardTitle");
        listTop->addWidget(listTitle);

        listTop->addStretch();

        countLabel = new QLabel;
        countLabel->setObjectName("count");
        listTop->addWidget(countLabel);

        list->addLayout(listTop);

        searchInput = new QLineEdit;
        searchInput->setPlaceholderText(
            "Rechercher par type ou adresse...");

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
        sortLine->addWidget(caption("Trier par :"));
        sortLine->addWidget(sortInput);
        sortLine->addStretch();
        list->addLayout(sortLine);

        table = new QTableWidget(0, 6);

        table->setHorizontalHeaderLabels({
            "ID",
            "TYPE",
            "ADRESSE",
            "GRAVITÉ",
            "STATUT",
            "ACTION"
        });

        table->horizontalHeader()->setSectionResizeMode(
            QHeaderView::Stretch);

        table->setEditTriggers(
            QAbstractItemView::NoEditTriggers);

        table->setSelectionBehavior(
            QAbstractItemView::SelectRows);

        table->setSelectionMode(
            QAbstractItemView::SingleSelection);

        table->verticalHeader()->hide();
        table->setAlternatingRowColors(true);
        table->setMinimumHeight(280);

        list->addWidget(table, 1);

        auto *alertCard = new QFrame;
        alertCard->setObjectName("priorityCard");

        auto *alertRow = new QHBoxLayout(alertCard);
        alertRow->setContentsMargins(18, 17, 18, 17);
        alertRow->setSpacing(14);

        auto *alertSymbol = new QLabel("⚠");
        alertSymbol->setObjectName("alertSymbol");
        alertRow->addWidget(alertSymbol);

        auto *alertColumn = new QVBoxLayout;

        auto *alertHeading = new QLabel("Alerte de priorité");
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

        alertButton = new QPushButton("Voir les détails  ›");
        alertButton->setObjectName("primary");
        alertRow->addWidget(alertButton);

        list->addWidget(alertCard);

        auto *bottom = new QHBoxLayout;

        auto *statsButton = new QPushButton("Statistiques");
        auto *pdfButton = new QPushButton("Exporter en PDF");

        statsButton->setObjectName("outline");
        pdfButton->setObjectName("outline");

        bottom->addStretch();
        bottom->addWidget(statsButton);
        bottom->addWidget(pdfButton);

        list->addLayout(bottom);

        connect(
            addButton,
            &QPushButton::clicked,
            this,
            [this] {
                addIncident();
            });

        connect(
            editButton,
            &QPushButton::clicked,
            this,
            [this] {
                editIncident();
            });

        connect(
            archiveButton,
            &QPushButton::clicked,
            this,
            [this] {
                archiveIncident();
            });

        connect(
            searchInput,
            &QLineEdit::textChanged,
            this,
            [this] {
                refreshTable();
            });

        connect(
            filterInput,
            &QComboBox::currentIndexChanged,
            this,
            [this] {
                refreshTable();
            });

        connect(
            sortInput,
            &QComboBox::currentIndexChanged,
            this,
            [this] {
                refreshTable();
            });

        connect(
            table,
            &QTableWidget::cellClicked,
            this,
            [this](int row, int) {
                chooseRow(row);
            });

        connect(
            statsButton,
            &QPushButton::clicked,
            this,
            [this] {
                showStatistics();
            });

        connect(
            pdfButton,
            &QPushButton::clicked,
            this,
            [this] {
                exportPdf();
            });

        connect(
            alertButton,
            &QPushButton::clicked,
            this,
            [this] {
                showUrgentIncident();
            });
    }

    Incident *selected()
    {
        for (auto &incident : incidents) {
            if (incident.id == selectedId &&
                !incident.archived) {
                return &incident;
            }
        }

        return nullptr;
    }

    void clearForm()
    {
        selectedId = -1;
        table->clearSelection();

        typeInput->setCurrentIndex(0);
        gravityInput->setCurrentIndex(0);
        dateInput->setDateTime(QDateTime::currentDateTime());
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

        Incident *incident = selected();

        if (!incident)
            return;

        typeInput->setCurrentText(incident->type);
        gravityInput->setCurrentText(incident->gravity);
        dateInput->setDateTime(incident->date);
        victimsInput->setValue(incident->victims);
        addressInput->setText(incident->address);
        descriptionInput->setPlainText(incident->description);
        statusInput->setCurrentText(incident->status);
    }

    bool validate()
    {
        if (addressInput->text().trimmed().isEmpty()) {
            QMessageBox::warning(
                this,
                "Adresse manquante",
                "Saisissez l'adresse de l'incident.");

            addressInput->setFocus();
            return false;
        }

        return true;
    }

    void copyFormTo(Incident &incident)
    {
        incident.type = typeInput->currentText();
        incident.gravity = gravityInput->currentText();
        incident.date = dateInput->dateTime();
        incident.victims = victimsInput->value();
        incident.address = addressInput->text().trimmed();
        incident.description =
            descriptionInput->toPlainText().trimmed();
        incident.status = statusInput->currentText();
    }

    void addIncident()
    {
        if (!validate())
            return;

        Incident incident;
        incident.id = nextId++;

        copyFormTo(incident);
        incidents.push_back(incident);

        clearForm();
        refreshTable();
    }

    void editIncident()
    {
        Incident *incident = selected();

        if (!incident) {
            QMessageBox::information(
                this,
                "Modifier",
                "Sélectionnez d'abord un incident dans la liste.");

            return;
        }

        if (!validate())
            return;

        copyFormTo(*incident);
        clearForm();
        refreshTable();
    }

    void archiveIncident()
    {
        Incident *incident = selected();

        if (!incident) {
            QMessageBox::information(
                this,
                "Archiver",
                "Sélectionnez d'abord un incident dans la liste.");

            return;
        }

        if (QMessageBox::question(
                this,
                "Archiver",
                "Archiver l'incident sélectionné ?")
            != QMessageBox::Yes) {
            return;
        }

        incident->archived = true;

        clearForm();
        refreshTable();
    }

    void showUrgentIncident()
    {
        int urgentId = -1;

        for (const auto &incident : incidents) {
            if (!incident.archived &&
                priority(incident) == "Urgente" &&
                incident.status != "Terminé") {

                urgentId = incident.id;
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
                table->scrollToItem(table->item(row, 0));
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

        for (const auto &incident : incidents) {
            if (incident.archived)
                continue;

            if (filter != "Toutes gravités" &&
                incident.gravity != filter) {
                continue;
            }

            if (!search.isEmpty() &&
                !incident.type.contains(
                    search,
                    Qt::CaseInsensitive) &&
                !incident.address.contains(
                    search,
                    Qt::CaseInsensitive)) {
                continue;
            }

            visible.push_back(&incident);
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

        const int sort = sortInput->currentIndex();

        std::stable_sort(
            visible.begin(),
            visible.end(),
            [sort, &gravityRank](
                const Incident *first,
                const Incident *second) {

                if (sort == 0)
                    return first->date > second->date;

                if (sort == 1)
                    return first->date < second->date;

                if (sort == 2) {
                    return gravityRank(first->gravity) >
                           gravityRank(second->gravity);
                }

                return gravityRank(first->gravity) <
                       gravityRank(second->gravity);
            });

        table->setRowCount(0);

        for (const Incident *incident : visible) {
            const int row = table->rowCount();
            table->insertRow(row);

            const QStringList values = {
                QString("INC-%1")
            .arg(
                incident->id,
                3,
                10,
                QChar('0')),

                incident->type,
                incident->address
        };

        for (int column = 0;
             column < values.size();
             ++column) {

            auto *cell =
                new QTableWidgetItem(values[column]);

            cell->setData(
                Qt::UserRole,
                incident->id);

            table->setItem(
                row,
                column,
                cell);
        }

        auto *gravityCell = new QWidget;
        gravityCell->setStyleSheet(
            "background:transparent;");

        auto *gravityLayout =
            new QHBoxLayout(gravityCell);

        gravityLayout->setContentsMargins(
            5, 4, 5, 4);

        auto *gravityBadge =
            new QLabel(incident->gravity);

        gravityBadge->setAlignment(Qt::AlignCenter);

        gravityBadge->setObjectName(
            incident->gravity == "Critique"
                ? "gravityCritical"
                : incident->gravity == "Élevée"
                      ? "gravityHigh"
                      : incident->gravity == "Moyenne"
                            ? "gravityMedium"
                            : "gravityLow");

        gravityLayout->addWidget(gravityBadge);
        gravityLayout->addStretch();

        table->setCellWidget(
            row,
            3,
            gravityCell);

        auto *statusWidget = new QWidget;

        auto *statusLayout =
            new QHBoxLayout(statusWidget);

        statusLayout->setContentsMargins(
            4, 5, 4, 5);

        auto *statusBadge =
            new QLabel(incident->status);

        statusBadge->setAlignment(Qt::AlignCenter);

        statusBadge->setObjectName(
            incident->status == "Terminé"
                ? "statusDone"
                : incident->status == "En cours"
                      ? "statusActive"
                      : "statusOpen");

        statusLayout->addWidget(statusBadge);
        statusLayout->addStretch();

        table->setCellWidget(
            row,
            4,
            statusWidget);

        auto *actionCell = new QWidget;

        auto *actionLayout =
            new QHBoxLayout(actionCell);

        actionLayout->setContentsMargins(
            0, 0, 0, 0);

        auto *viewButton = new QPushButton("Voir");
        viewButton->setObjectName("tableAction");

        auto *rowArchiveButton =
            new QPushButton("Archiver");

        rowArchiveButton->setObjectName("tableAction");

        actionLayout->addWidget(viewButton);
        actionLayout->addWidget(rowArchiveButton);

        table->setCellWidget(
            row,
            5,
            actionCell);

        const int incidentId = incident->id;

        connect(
            viewButton,
            &QPushButton::clicked,
            this,
            [this, incidentId, row] {
                selectedId = incidentId;
                table->selectRow(row);
                chooseRow(row);
            });

        connect(
            rowArchiveButton,
            &QPushButton::clicked,
            this,
            [this, incidentId] {
                selectedId = incidentId;
                archiveIncident();
            });

        table->setRowHeight(row, 50);
    }

    countLabel->setText(
        QString("%1 incidents")
            .arg(int(visible.size())));

    int urgent = 0;
    const Incident *firstUrgent = nullptr;

    for (const auto &incident : incidents) {
        if (!incident.archived &&
            priority(incident) == "Urgente" &&
            incident.status != "Terminé") {

            if (!firstUrgent)
                firstUrgent = &incident;

            ++urgent;
        }
    }

    if (urgent > 0) {
        alertLabel->setText(
            QString(
                "%1 incident(s) urgents nécessitent "
                "une intervention immédiate.")
                .arg(urgent));
    } else {
        alertLabel->setText(
            "Aucun incident urgent en attente.");
    }

    if (firstUrgent) {
        alertDetails->setText(
            QString("INC-%1 — %2 — %3")
                .arg(
                    firstUrgent->id,
                    3,
                    10,
                    QChar('0'))
                .arg(
                    firstUrgent->type,
                    firstUrgent->address));
    } else {
        alertDetails->setText(
            "La liste des incidents est à jour.");
    }

    alertButton->setEnabled(
        firstUrgent != nullptr);
}

void showStatistics()
{
    int total = 0;
    int critiques = 0;
    int enCours = 0;
    int termines = 0;

    const QStringList types = {
        "Incendie",
        "Accident",
        "Fuite de gaz",
        "Sauvetage"
    };

    const QStringList gravites = {
        "Critique",
        "Élevée",
        "Moyenne",
        "Faible"
    };

    const QStringList statuts = {
        "Signalé",
        "En cours",
        "Terminé"
    };

    std::vector<int> parType(types.size(), 0);
    std::vector<int> parGravite(gravites.size(), 0);
    std::vector<int> parStatut(statuts.size(), 0);

    std::vector<QString> nomsZones;
    std::vector<int> parZone;

    for (const Incident &incident : incidents) {
        if (incident.archived)
            continue;

        ++total;

        if (incident.gravity == "Critique")
            ++critiques;

        if (incident.status == "En cours")
            ++enCours;

        if (incident.status == "Terminé")
            ++termines;

        int index = types.indexOf(incident.type);

        if (index >= 0)
            ++parType[index];

        index = gravites.indexOf(incident.gravity);

        if (index >= 0)
            ++parGravite[index];

        index = statuts.indexOf(incident.status);

        if (index >= 0)
            ++parStatut[index];

        QString zone =
            incident.address.trimmed();

        if (zone.isEmpty())
            zone = "Non précisée";

        int zoneIndex = -1;

        for (int i = 0;
             i < static_cast<int>(nomsZones.size());
             ++i) {

            if (nomsZones[i].compare(
                    zone,
                    Qt::CaseInsensitive) == 0) {

                zoneIndex = i;
                break;
            }
        }

        if (zoneIndex == -1) {
            nomsZones.push_back(zone);
            parZone.push_back(1);
        } else {
            ++parZone[zoneIndex];
        }
    }

    QDialog dialog(this);

    dialog.setWindowTitle(
        "Statistiques — Gestion des incidents");

    dialog.resize(1280, 760);
    dialog.setMinimumSize(1050, 680);

    dialog.setStyleSheet(R"CSS(
            QDialog {
                background: #f6f2ef;
                font-family: "Segoe UI";
                color: #28252d;
            }

            QFrame#statisticsSidebar {
                background: qlineargradient(
                    x1:0, y1:0, x2:0, y2:1,
                    stop:0 #b90000,
                    stop:1 #970000
                );
                border: none;
            }

            QLabel#statisticsBrand {
                color: white;
                font-size: 17px;
                font-weight: 800;
            }

            QPushButton#statisticsNav {
                background: transparent;
                color: white;
                border: none;
                border-radius: 8px;
                text-align: left;
                padding: 10px 15px;
                font-size: 13px;
            }

            QPushButton#statisticsNavActive {
                background: rgba(255,255,255,48);
                color: white;
                border: none;
                border-radius: 8px;
                text-align: left;
                padding: 10px 15px;
                font-size: 13px;
                font-weight: 700;
            }

            QLabel#statisticsTitle {
                color: #25242b;
                font-size: 25px;
                font-weight: 800;
            }

            QLabel#statisticsSubtitle {
                color: #777077;
                font-size: 12px;
            }

            QFrame#statisticsCard {
                background: white;
                border: 1px solid #eee7e3;
                border-radius: 10px;
            }

            QLabel#statisticsCardTitle {
                color: #302d33;
                font-size: 14px;
                font-weight: 700;
            }

            QPushButton#statisticsTopButton {
                background: white;
                color: #9d1721;
                border: 1px solid #e1c5c8;
                border-radius: 7px;
                padding: 8px 13px;
                font-size: 11px;
                font-weight: 700;
            }
        )CSS");

    auto *root =
        new QHBoxLayout(&dialog);

    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *sidebar = new QFrame;
    sidebar->setObjectName("statisticsSidebar");
    sidebar->setFixedWidth(220);

    auto *sideLayout =
        new QVBoxLayout(sidebar);

    sideLayout->setContentsMargins(
        20, 24, 20, 24);

    auto *logo = new QLabel;

    QPixmap logoImage(
        ":/images/logo.png");

    if (!logoImage.isNull()) {
        logo->setPixmap(
            logoImage.scaled(
                92,
                92,
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation));
    }

    logo->setAlignment(Qt::AlignCenter);
    sideLayout->addWidget(logo);

    auto *brand =
        new QLabel("SMART FIRE\nSTATION");

    brand->setObjectName("statisticsBrand");
    brand->setAlignment(Qt::AlignCenter);

    sideLayout->addWidget(brand);
    sideLayout->addSpacing(22);

    const QStringList navigation = {
        "Tableau de bord",
        "Incidents",
        "Interventions",
        "Pompiers",
        "Véhicules",
        "Équipements",
        "Rapports"
    };

    for (int i = 0;
         i < navigation.size();
         ++i) {

        auto *button =
            new QPushButton(navigation[i]);

        button->setObjectName(
            i == 1
                ? "statisticsNavActive"
                : "statisticsNav");

        sideLayout->addWidget(button);

        if (i == 1) {
            connect(
                button,
                &QPushButton::clicked,
                &dialog,
                &QDialog::accept);
        }
    }

    sideLayout->addStretch();
    root->addWidget(sidebar);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *page = new QWidget;

    auto *pageLayout =
        new QVBoxLayout(page);

    pageLayout->setContentsMargins(
        28, 20, 28, 24);

    pageLayout->setSpacing(14);

    auto *header = new QHBoxLayout;
    auto *titles = new QVBoxLayout;

    auto *pageTitle =
        new QLabel(
            "Statistiques — Gestion des incidents");

    pageTitle->setObjectName("statisticsTitle");

    auto *subtitle =
        new QLabel(
            "Types, gravité, zones et suivi des incidents");

    subtitle->setObjectName("statisticsSubtitle");

    titles->addWidget(pageTitle);
    titles->addWidget(subtitle);

    auto *returnButton =
        new QPushButton("Retour à la liste");

    returnButton->setObjectName(
        "statisticsTopButton");

    auto *pdfButton =
        new QPushButton("Exporter en PDF");

    pdfButton->setObjectName(
        "statisticsTopButton");

    header->addLayout(titles);
    header->addStretch();
    header->addWidget(returnButton);
    header->addWidget(pdfButton);

    pageLayout->addLayout(header);

    auto createCounter =
        [&](const QString &number,
            const QString &text,
            const QString &color) {

            auto *panel = new QFrame;
            panel->setObjectName("statisticsCard");
            panel->setMinimumHeight(78);

            auto *layout =
                new QVBoxLayout(panel);

            layout->setContentsMargins(
                15, 9, 15, 9);

            auto *valueLabel =
                new QLabel(number);

            valueLabel->setStyleSheet(
                QString(
                    "font-size:25px;"
                    "font-weight:800;"
                    "color:%1;")
                    .arg(color));

            auto *textLabel =
                new QLabel(text);

            textLabel->setStyleSheet(
                "font-size:11px;"
                "color:#8a8487;");

            layout->addWidget(valueLabel);
            layout->addWidget(textLabel);

            return panel;
        };

    auto *counterLayout =
        new QHBoxLayout;

    counterLayout->addWidget(
        createCounter(
            QString::number(total),
            "Total des incidents",
            "#c80012"));

    counterLayout->addWidget(
        createCounter(
            QString::number(critiques),
            "Critiques",
            "#18a465"));

    counterLayout->addWidget(
        createCounter(
            QString::number(enCours),
            "En cours",
            "#e57c00"));

    counterLayout->addWidget(
        createCounter(
            QString::number(termines),
            "Terminés",
            "#7a7a7a"));

    pageLayout->addLayout(counterLayout);

    auto createCard =
        [&](const QString &title,
            const QString &hint) {

            auto *panel = new QFrame;
            panel->setObjectName(
                "statisticsCard");

            auto *layout =
                new QVBoxLayout(panel);

            layout->setContentsMargins(
                17, 12, 17, 12);

            auto *titleLabel =
                new QLabel(title);

            titleLabel->setObjectName(
                "statisticsCardTitle");

            auto *hintLabel =
                new QLabel(hint);

            hintLabel->setStyleSheet(
                "font-size:10px;"
                "color:#8a8487;");

            layout->addWidget(titleLabel);
            layout->addWidget(hintLabel);

            return panel;
        };

    auto makeBarChart =
        [&](const QStringList &labels,
            const std::vector<int> &values) {

            QPixmap pixmap(520, 220);
            pixmap.fill(Qt::transparent);

            QPainter chartPainter(&pixmap);

            chartPainter.setRenderHint(
                QPainter::Antialiasing);

            int maximum = 1;

            for (int value : values) {
                maximum =
                    std::max(maximum, value);
            }

            const int section =
                490 /
                std::max(
                    1,
                    static_cast<int>(
                        labels.size()));

            for (int i = 0;
                 i < labels.size();
                 ++i) {

                const int height =
                    125 *
                    values[i] /
                    maximum;

                const int x =
                    15 +
                    i * section +
                    (section - 44) / 2;

                const int bottom = 165;

                chartPainter.setPen(Qt::NoPen);
                chartPainter.setBrush(
                    QColor("#c90012"));

                if (height > 0) {
                    chartPainter.drawRoundedRect(
                        QRect(
                            x,
                            bottom - height,
                            44,
                            height),
                        4,
                        4);
                }

                chartPainter.setPen(
                    QColor("#342f32"));

                QFont valueFont(
                    "Segoe UI",
                    10);

                valueFont.setBold(true);
                chartPainter.setFont(valueFont);

                chartPainter.drawText(
                    QRect(
                        x - 8,
                        bottom - height - 25,
                        60,
                        20),
                    Qt::AlignCenter,
                    QString::number(values[i]));

                chartPainter.setPen(
                    QColor("#80787c"));

                chartPainter.setFont(
                    QFont("Segoe UI", 8));

                chartPainter.drawText(
                    QRect(
                        i * section,
                        177,
                        section,
                        35),
                    Qt::AlignHCenter |
                        Qt::TextWordWrap,
                    labels[i]);
            }

            return pixmap;
        };

    auto makeSegments =
        [&](const QStringList &labels,
            const std::vector<int> &values,
            const std::vector<QColor> &colors,
            int width) {

            QPixmap pixmap(width, 76);
            pixmap.fill(Qt::transparent);

            QPainter chartPainter(&pixmap);

            chartPainter.setRenderHint(
                QPainter::Antialiasing);

            int sum = 0;

            for (int value : values)
                sum += value;

            const int barWidth = width - 10;

            chartPainter.setPen(Qt::NoPen);
            chartPainter.setBrush(
                QColor("#ece8e7"));

            chartPainter.drawRoundedRect(
                QRect(
                    5,
                    4,
                    barWidth,
                    16),
                6,
                6);

            double x = 5;

            for (int i = 0;
                 i < static_cast<int>(
                     values.size());
                 ++i) {

                const double segmentWidth =
                    sum > 0
                        ? double(barWidth) *
                              values[i] /
                              sum
                        : 0;

                chartPainter.setBrush(
                    colors[
                        i % colors.size()]);

                chartPainter.drawRect(
                    QRectF(
                        x,
                        4,
                        segmentWidth,
                        16));

                x += segmentWidth;
            }

            int legendX = 5;
            int legendY = 34;

            chartPainter.setFont(
                QFont("Segoe UI", 8));

            for (int i = 0;
                 i < labels.size();
                 ++i) {

                QString label = labels[i];

                if (label.size() > 15) {
                    label =
                        label.left(13) + "...";
                }

                const QString legend =
                    QString("%1 (%2)")
                        .arg(label)
                        .arg(values[i]);

                const int itemWidth =
                    std::max(
                        90,
                        int(
                            legend.size() *
                                7 +
                            22));

                if (legendX + itemWidth >
                    width - 5) {

                    legendX = 5;
                    legendY += 25;
                }

                chartPainter.setBrush(
                    colors[
                        i % colors.size()]);

                chartPainter.drawEllipse(
                    QRect(
                        legendX,
                        legendY + 4,
                        8,
                        8));

                chartPainter.setPen(
                    QColor("#5f595c"));

                chartPainter.drawText(
                    QRect(
                        legendX + 13,
                        legendY,
                        itemWidth - 13,
                        19),
                    Qt::AlignLeft |
                        Qt::AlignVCenter,
                    legend);

                legendX += itemWidth;
            }

            return pixmap;
        };

    auto *middle = new QHBoxLayout;

    auto *typeCard =
        createCard(
            "Nombre d’incidents par type",
            "Répartition sur l’ensemble des incidents");

    typeCard->setMinimumHeight(280);

    auto *typeLayout =
        qobject_cast<QVBoxLayout *>(
            typeCard->layout());

    auto *typeChart = new QLabel;

    typeChart->setPixmap(
        makeBarChart(
            types,
            parType));

    typeChart->setAlignment(
        Qt::AlignCenter);

    typeLayout->addWidget(typeChart, 1);
    middle->addWidget(typeCard, 1);

    auto *rightCharts =
        new QVBoxLayout;

    auto *gravityCard =
        createCard(
            "Répartition par niveau de gravité",
            "Sur l’ensemble des incidents");

    auto *gravityLayout =
        qobject_cast<QVBoxLayout *>(
            gravityCard->layout());

    auto *gravityChart = new QLabel;

    gravityChart->setPixmap(
        makeSegments(
            gravites,
            parGravite,
            {
                QColor("#c90012"),
                QColor("#ef676f"),
                QColor("#f4a0a5"),
                QColor("#f8c9cc")
            },
            480));

    gravityChart->setAlignment(
        Qt::AlignCenter);

    gravityLayout->addWidget(
        gravityChart);

    rightCharts->addWidget(
        gravityCard);

    QStringList zonesAffichees;
    std::vector<int> nombresZones;

    int autresZones = 0;

    for (int i = 0;
         i < static_cast<int>(
             nomsZones.size());
         ++i) {

        if (i < 4) {
            zonesAffichees
                << nomsZones[i];

            nombresZones.push_back(
                parZone[i]);
        } else {
            autresZones +=
                parZone[i];
        }
    }

    if (autresZones > 0) {
        zonesAffichees << "Autres";
        nombresZones.push_back(
            autresZones);
    }

    auto *zoneCard =
        createCard(
            "Répartition par zone",
            "Sur l’ensemble des incidents");

    auto *zoneLayout =
        qobject_cast<QVBoxLayout *>(
            zoneCard->layout());

    auto *zoneChart = new QLabel;

    zoneChart->setPixmap(
        makeSegments(
            zonesAffichees,
            nombresZones,
            {
                QColor("#2671c9"),
                QColor("#5799df"),
                QColor("#88b6e8"),
                QColor("#b7d2ef"),
                QColor("#d6e5f5")
            },
            480));

    zoneChart->setAlignment(
        Qt::AlignCenter);

    zoneLayout->addWidget(zoneChart);
    rightCharts->addWidget(zoneCard);

    middle->addLayout(rightCharts, 1);
    pageLayout->addLayout(middle);

    auto *statusCard =
        createCard(
            "Répartition selon le statut",
            "État actuel des incidents");

    auto *statusLayout =
        qobject_cast<QVBoxLayout *>(
            statusCard->layout());

    auto *statusChart = new QLabel;

    statusChart->setPixmap(
        makeSegments(
            statuts,
            parStatut,
            {
                QColor("#20a666"),
                QColor("#e77c00"),
                QColor("#929292")
            },
            1010));

    statusChart->setAlignment(
        Qt::AlignCenter);

    statusLayout->addWidget(statusChart);
    pageLayout->addWidget(statusCard);
    pageLayout->addStretch();

    scroll->setWidget(page);
    root->addWidget(scroll, 1);

    connect(
        returnButton,
        &QPushButton::clicked,
        &dialog,
        &QDialog::accept);

    connect(
        pdfButton,
        &QPushButton::clicked,
        &dialog,
        [&dialog] {
            QString path =
                QFileDialog::getSaveFileName(
                    &dialog,
                    "Enregistrer les statistiques",
                    "statistiques_incidents.pdf",
                    "PDF (*.pdf)");

            if (path.isEmpty())
                return;

            if (!path.endsWith(
                    ".pdf",
                    Qt::CaseInsensitive)) {
                path += ".pdf";
            }

            QPdfWriter pdf(path);

            pdf.setPageSize(
                QPageSize(
                    QPageSize::A4));

            pdf.setPageOrientation(
                QPageLayout::Landscape);

            pdf.setResolution(150);

            QPainter pdfPainter(&pdf);

            if (!pdfPainter.isActive()) {
                QMessageBox::warning(
                    &dialog,
                    "Export PDF",
                    "Impossible de créer le fichier PDF.");

                return;
            }

            QPixmap capture =
                dialog.grab();

            QSize size =
                capture.size();

            size.scale(
                pdf.width(),
                pdf.height(),
                Qt::KeepAspectRatio);

            pdfPainter.drawPixmap(
                QRect(
                    (pdf.width() -
                     size.width()) / 2,

                    (pdf.height() -
                     size.height()) / 2,

                    size.width(),
                    size.height()),
                capture);

            pdfPainter.end();

            QMessageBox::information(
                &dialog,
                "Export PDF",
                "Les statistiques ont été enregistrées.");
        });

    dialog.exec();
}

void exportPdf()
{
    QString path =
        QFileDialog::getSaveFileName(
            this,
            "Enregistrer la liste des incidents",
            "incidents.pdf",
            "PDF (*.pdf)");

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
            "Impossible de créer le fichier PDF.");

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

    for (const auto &incident : incidents) {
        if (incident.archived)
            continue;

        const QStringList values = {
            QString("INC-%1")
        .arg(
            incident.id,
            3,
            10,
            QChar('0')),

            incident.type,

            incident.date.toString(
                "dd/MM/yyyy HH:mm"),

            incident.address,
            incident.gravity,
            QString::number(incident.victims),
            incident.status,
            priority(incident)
    };

    html += "<tr>";

    for (const QString &value : values) {
        html +=
            "<td>" +
            value.toHtmlEscaped() +
            "</td>";
    }

    html += "</tr>";
}

html += "</table>";

{
    QPdfWriter pdf(&file);

    pdf.setPageSize(
        QPageSize(
            QPageSize::A4));

    pdf.setPageOrientation(
        QPageLayout::Landscape);

    pdf.setPageMargins(
        QMarginsF(
            12,
            12,
            12,
            12));

    QTextDocument document;
    document.setHtml(html);
    document.print(&pdf);
}

file.close();

QMessageBox::information(
    this,
    "Export PDF",
    "PDF enregistré :\n" + path);
}
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setStyle("Fusion");
    app.setFont(
        QFont("Segoe UI", 10));

    MainWindow window;
    window.show();

    return app.exec();
}