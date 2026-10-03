#include <QtWidgets>
#include <QPdfWriter>
#include <QPageLayout>
#include <QPageSize>
#include <QTextDocument>
#include <algorithm>
#include <vector>

struct Incident
{
    int id = 0;
    QString type;
    QDateTime date;
    QString address;
    QString gravity;
    int victims = 0;
    QString description;
    QString status;
    bool archived = false;
};

static QString incidentNumber(int id)
{
    return QString("INC-%1").arg(id, 3, 10, QChar('0'));
}

static QString calculatePriority(const Incident &incident)
{
    if (incident.gravity == "Critique" || incident.victims >= 5)
        return "Urgente";

    if (incident.gravity == "Élevée" || incident.victims >= 2)
        return "Haute";

    if (incident.gravity == "Moyenne" || incident.victims >= 1)
        return "Normale";

    return "Faible";
}

static QPixmap loadLogo()
{
    const QStringList paths = {
        ":/images/logo.png",
        ":/images/logo.jpeg",
        ":/logo.png",
        ":/logo.jpeg",
        "logo.png",
        "logo.jpeg"
    };

    for (const QString &path : paths) {
        QPixmap image(path);

        if (!image.isNull())
            return image;
    }

    return {};
}

static void animateEntrance(QWidget *widget, int delay, QPoint offset)
{
    QTimer::singleShot(delay, widget, [widget, offset] {
        if (!widget || widget->graphicsEffect())
            return;

        QPoint finalPosition = widget->pos();
        widget->move(finalPosition + offset);

        auto *effect = new QGraphicsOpacityEffect(widget);
        effect->setOpacity(0.0);
        widget->setGraphicsEffect(effect);

        auto *group = new QParallelAnimationGroup(widget);

        auto *fade = new QPropertyAnimation(effect, "opacity", group);
        fade->setDuration(500);
        fade->setStartValue(0.0);
        fade->setEndValue(1.0);
        fade->setEasingCurve(QEasingCurve::OutCubic);

        auto *slide = new QPropertyAnimation(widget, "pos", group);
        slide->setDuration(500);
        slide->setStartValue(finalPosition + offset);
        slide->setEndValue(finalPosition);
        slide->setEasingCurve(QEasingCurve::OutCubic);

        QObject::connect(
            group,
            &QParallelAnimationGroup::finished,
            widget,
            [widget] {
                widget->setGraphicsEffect(nullptr);
            });

        group->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

static void animateRefresh(QWidget *widget)
{
    if (!widget || widget->graphicsEffect())
        return;

    auto *effect = new QGraphicsOpacityEffect(widget);
    widget->setGraphicsEffect(effect);

    auto *animation = new QPropertyAnimation(
        effect,
        "opacity",
        widget
        );

    animation->setDuration(280);
    animation->setStartValue(0.25);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);

    QObject::connect(
        animation,
        &QPropertyAnimation::finished,
        widget,
        [widget] {
            widget->setGraphicsEffect(nullptr);
        });

    animation->start(QAbstractAnimation::DeleteWhenStopped);
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
        setMinimumSize(1100, 700);

        createInterface();

        incidents = {
            {
                1,
                "Incendie",
                QDateTime::currentDateTime().addSecs(-7200),
                "Tunis",
                "Critique",
                5,
                "Incendie dans un bâtiment",
                "En cours",
                false
            },
            {
                2,
                "Accident",
                QDateTime::currentDateTime().addDays(-1),
                "Ariana",
                "Élevée",
                2,
                "Accident de la route",
                "Signalé",
                false
            },
            {
                3,
                "Fuite de gaz",
                QDateTime::currentDateTime().addDays(-2),
                "Ben Arous",
                "Moyenne",
                0,
                "Fuite de gaz signalée",
                "Terminé",
                false
            },
            {
                4,
                "Sauvetage",
                QDateTime::currentDateTime().addDays(-3),
                "La Marsa",
                "Élevée",
                1,
                "Assistance à une personne",
                "En cours",
                false
            },
            {
                5,
                "Autre",
                QDateTime::currentDateTime().addDays(-4),
                "Sfax",
                "Faible",
                0,
                "Incident mineur sans victime",
                "Signalé",
                false
            }
        };

        refreshTable();
    }

private:
    std::vector<Incident> incidents;

    int nextId = 6;
    int selectedId = -1;

    QComboBox *typeInput = nullptr;
    QComboBox *gravityInput = nullptr;
    QDateTimeEdit *dateInput = nullptr;
    QSpinBox *victimsInput = nullptr;
    QLineEdit *addressInput = nullptr;
    QTextEdit *descriptionInput = nullptr;
    QComboBox *statusInput = nullptr;

    QLineEdit *searchInput = nullptr;
    QComboBox *gravityFilter = nullptr;
    QComboBox *sortInput = nullptr;

    QTableWidget *table = nullptr;

    QLabel *countLabel = nullptr;
    QLabel *alertText = nullptr;
    QLabel *alertDetails = nullptr;
    QPushButton *alertButton = nullptr;

    QLabel *createFieldTitle(const QString &text)
    {
        auto *label = new QLabel(text);
        label->setObjectName("fieldTitle");

        return label;
    }

    QVBoxLayout *createField(
        const QString &title,
        QWidget *input)
    {
        auto *layout = new QVBoxLayout;

        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);

        layout->addWidget(createFieldTitle(title));
        layout->addWidget(input);

        return layout;
    }

    QFrame *createCard()
    {
        auto *card = new QFrame;
        card->setObjectName("card");

        auto *shadow = new QGraphicsDropShadowEffect(card);

        shadow->setBlurRadius(24);
        shadow->setOffset(0, 5);
        shadow->setColor(QColor(80, 35, 35, 25));

        card->setGraphicsEffect(shadow);

        return card;
    }

    QPushButton *createNavigationButton(
        const QString &icon,
        const QString &text,
        bool active)
    {
        auto *button = new QPushButton(
            icon + "   " + text
            );

        button->setObjectName(
            active ? "navActive" : "nav"
            );

        button->setMinimumHeight(49);
        button->setCursor(Qt::PointingHandCursor);

        if (!active) {
            connect(
                button,
                &QPushButton::clicked,
                this,
                [this, text] {
                    QMessageBox::information(
                        this,
                        text,
                        "Ce module sera intégré prochainement."
                        );
                });
        }

        return button;
    }

    void createInterface()
    {
        setStyleSheet(R"CSS(
            QMainWindow,
            QWidget#root,
            QWidget#content,
            QScrollArea {
                background: #f3f5f7;
                font-family: "Segoe UI";
            }

            QFrame#sidebar {
                background: qlineargradient(
                    x1:0, y1:0,
                    x2:0, y2:1,
                    stop:0 #27050c,
                    stop:0.32 #59101a,
                    stop:0.68 #a71928,
                    stop:1 #d53d39
                );
                border: none;
            }

            QFrame#card {
                background: white;
                border: 1px solid #e6e8ec;
                border-radius: 18px;
            }

            QFrame#priorityCard {
                background: #fff5f4;
                border: 2px solid #f2c7c2;
                border-radius: 14px;
            }

            QLabel#heading {
                color: #211e25;
                font-size: 34px;
                font-weight: 800;
            }

            QLabel#subheading {
                color: #777984;
                font-size: 12px;
            }

            QLabel#cardTitle {
                color: #8f1723;
                font-size: 23px;
                font-weight: 800;
            }

            QLabel#fieldTitle {
                color: #3d3a42;
                font-size: 12px;
                font-weight: 700;
            }

            QLabel#count {
                color: white;
                background: #981a27;
                border-radius: 12px;
                padding: 7px 12px;
                font-weight: 800;
            }

            QLabel#alertTitle {
                color: #a71928;
                font-size: 17px;
                font-weight: 800;
            }

            QLabel#alertText {
                color: #70444b;
                font-size: 12px;
            }

            QPushButton#nav {
                color: #fff8f7;
                background: transparent;
                text-align: left;
                border: 1px solid transparent;
                border-radius: 10px;
                padding: 12px 13px;
                font-size: 13px;
            }

            QPushButton#nav:hover {
                background: rgba(255,255,255,35);
                border-color: #cf7780;
            }

            QPushButton#navActive {
                color: #9d1724;
                background: white;
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
                color: #26232a;
                background: #fbfbfc;
                border: 2px solid #dfe2e7;
                border-radius: 9px;
                padding: 8px;
                min-height: 23px;
                font-size: 12px;
            }

            QLineEdit:focus,
            QTextEdit:focus,
            QComboBox:focus,
            QDateTimeEdit:focus,
            QSpinBox:focus {
                background: white;
                border-color: #bd1e2c;
            }

            QPushButton#primary {
                color: white;
                background: #b51c2a;
                border: 2px solid #8f1420;
                border-radius: 10px;
                padding: 10px 16px;
                font-weight: 800;
            }

            QPushButton#primary:hover {
                background: #cf2635;
            }

            QPushButton#outline {
                color: #a61725;
                background: white;
                border: 2px solid #c94d59;
                border-radius: 10px;
                padding: 10px 16px;
                font-weight: 800;
            }

            QPushButton#outline:hover {
                background: #fff1f2;
            }

            QPushButton#archive {
                color: #a61725;
                background: #fff0f1;
                border: 2px solid #e0a3aa;
                border-radius: 10px;
                padding: 10px 16px;
                font-weight: 800;
            }

            QPushButton#archive:hover {
                background: #f9dce0;
            }

            QPushButton#danger {
                color: white;
                background: #8f1420;
                border: 2px solid #74101a;
                border-radius: 10px;
                padding: 10px 16px;
                font-weight: 800;
            }

            QPushButton#danger:hover {
                background: #c1121f;
            }

            QTableWidget {
                color: #302d34;
                background: white;
                alternate-background-color: #fbf7f8;
                border: 1px solid #e4e6ea;
                border-radius: 10px;
                gridline-color: #eceef1;
                selection-background-color: #ffe5e7;
                selection-color: #2c252a;
                font-size: 12px;
            }

            QHeaderView::section {
                color: white;
                background: #5b1722;
                border: none;
                border-right: 1px solid #762834;
                padding: 12px 5px;
                font-size: 11px;
                font-weight: 800;
            }

            QPushButton#tableAction {
                color: #9e1724;
                background: #fff4f5;
                border: 1px solid #efc9cd;
                border-radius: 7px;
                padding: 5px;
            }

            QPushButton#tableAction:hover {
                background: #f6d6da;
            }

            QPushButton#tableDelete {
                color: white;
                background: #8f1420;
                border: 1px solid #74101a;
                border-radius: 7px;
                padding: 5px;
            }

            QPushButton#tableDelete:hover {
                background: #c1121f;
            }
        )CSS");

        auto *rootWidget = new QWidget;
        rootWidget->setObjectName("root");

        setCentralWidget(rootWidget);

        auto *mainLayout = new QHBoxLayout(rootWidget);

        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        auto *sidebar = new QFrame;

        sidebar->setObjectName("sidebar");
        sidebar->setFixedWidth(230);

        mainLayout->addWidget(sidebar);

        auto *sidebarLayout = new QVBoxLayout(sidebar);

        sidebarLayout->setContentsMargins(
            17, 24, 17, 24
            );

        sidebarLayout->setSpacing(8);

        auto *logoLabel = new QLabel;

        logoLabel->setFixedSize(116, 116);
        logoLabel->setAlignment(Qt::AlignCenter);

        logoLabel->setStyleSheet(
            "background:white;"
            "border:2px solid #f0c5c9;"
            "border-radius:14px;"
            "color:#981a27;"
            "font-weight:800;"
            );

        QPixmap logo = loadLogo();

        if (!logo.isNull()) {
            logoLabel->setPixmap(
                logo.scaled(
                    104,
                    104,
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation
                    )
                );
        } else {
            logoLabel->setText(
                "SMART FIRE\nSTATION"
                );
        }

        sidebarLayout->addWidget(
            logoLabel,
            0,
            Qt::AlignHCenter
            );

        sidebarLayout->addSpacing(18);

        sidebarLayout->addWidget(
            createNavigationButton(
                "⌂",
                "Tableau de bord",
                false
                )
            );

        sidebarLayout->addWidget(
            createNavigationButton(
                "⚠",
                "Incidents",
                true
                )
            );

        sidebarLayout->addWidget(
            createNavigationButton(
                "✚",
                "Interventions",
                false
                )
            );

        sidebarLayout->addWidget(
            createNavigationButton(
                "♟",
                "Personnel",
                false
                )
            );

        sidebarLayout->addWidget(
            createNavigationButton(
                "▰",
                "Véhicules",
                false
                )
            );

        sidebarLayout->addWidget(
            createNavigationButton(
                "⚙",
                "Équipements",
                false
                )
            );

        sidebarLayout->addWidget(
            createNavigationButton(
                "▥",
                "Rapports",
                false
                )
            );

        sidebarLayout->addStretch();

        auto *protectionLabel =
            new QLabel("PROTECTION CIVILE");

        protectionLabel->setAlignment(Qt::AlignCenter);

        protectionLabel->setStyleSheet(
            "color:#ffd9d6;"
            "font-weight:700;"
            );

        sidebarLayout->addWidget(protectionLabel);

        auto *scrollArea = new QScrollArea;

        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);

        mainLayout->addWidget(scrollArea, 1);

        auto *contentWidget = new QWidget;
        contentWidget->setObjectName("content");

        scrollArea->setWidget(contentWidget);

        auto *pageLayout = new QVBoxLayout(contentWidget);

        pageLayout->setContentsMargins(
            28, 26, 28, 26
            );

        pageLayout->setSpacing(20);

        auto *headerLayout = new QHBoxLayout;
        auto *titlesLayout = new QVBoxLayout;

        auto *heading =
            new QLabel("Gestion des incidents");

        heading->setObjectName("heading");

        auto *subtitle = new QLabel(
            "Créer, consulter et suivre les incidents en temps réel"
            );

        subtitle->setObjectName("subheading");

        auto *redLine = new QFrame;

        redLine->setFixedSize(82, 4);

        redLine->setStyleSheet(
            "background:#c51f2e;"
            "border:0;"
            "border-radius:2px;"
            );

        titlesLayout->addWidget(heading);
        titlesLayout->addWidget(subtitle);
        titlesLayout->addSpacing(5);
        titlesLayout->addWidget(redLine);

        headerLayout->addLayout(titlesLayout);
        headerLayout->addStretch();

        auto *roleLabel =
            new QLabel("Agent de coordination");

        roleLabel->setStyleSheet(
            "color:#8e1521;"
            "background:white;"
            "border:2px solid #ebc5ca;"
            "border-radius:12px;"
            "padding:11px 17px;"
            "font-weight:bold;"
            );

        headerLayout->addWidget(
            roleLabel,
            0,
            Qt::AlignTop
            );

        pageLayout->addLayout(headerLayout);

        auto *columnsLayout = new QHBoxLayout;

        columnsLayout->setSpacing(18);
        pageLayout->addLayout(columnsLayout, 1);

        auto *formCard = createCard();
        columnsLayout->addWidget(formCard, 46);

        auto *formLayout = new QVBoxLayout(formCard);

        formLayout->setContentsMargins(
            23, 23, 23, 23
            );

        formLayout->setSpacing(14);

        auto *formTitle =
            new QLabel("▣  Nouvel incident");

        formTitle->setObjectName("cardTitle");
        formLayout->addWidget(formTitle);

        auto *formDescription = new QLabel(
            "Saisir les informations de l'incident"
            );

        formDescription->setObjectName("subheading");
        formLayout->addWidget(formDescription);

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

        descriptionInput->setFixedHeight(90);

        descriptionInput->setPlaceholderText(
            "Décrivez brièvement la situation et les risques observés..."
            );

        statusInput = new QComboBox;

        statusInput->addItems({
            "Signalé",
            "En cours",
            "Terminé"
        });

        auto *formGrid = new QGridLayout;

        formGrid->setHorizontalSpacing(14);
        formGrid->setVerticalSpacing(14);

        formGrid->addLayout(
            createField(
                "Type d'incident",
                typeInput
                ),
            0,
            0
            );

        formGrid->addLayout(
            createField(
                "Niveau de gravité",
                gravityInput
                ),
            0,
            1
            );

        formGrid->addLayout(
            createField(
                "Date et heure",
                dateInput
                ),
            1,
            0
            );

        formGrid->addLayout(
            createField(
                "Nombre de victimes",
                victimsInput
                ),
            1,
            1
            );

        formGrid->addLayout(
            createField(
                "Adresse",
                addressInput
                ),
            2,
            0,
            1,
            2
            );

        formGrid->addLayout(
            createField(
                "Description",
                descriptionInput
                ),
            3,
            0,
            1,
            2
            );

        formGrid->addLayout(
            createField(
                "Statut",
                statusInput
                ),
            4,
            0,
            1,
            2
            );

        formLayout->addLayout(formGrid);
        formLayout->addStretch();

        auto *formButtons = new QGridLayout;

        formButtons->setSpacing(9);

        auto *addButton =
            new QPushButton("＋ Ajouter");

        auto *editButton =
            new QPushButton("✎ Modifier");

        auto *archiveButton =
            new QPushButton("▣ Archiver");

        auto *deleteButton =
            new QPushButton("✕ Supprimer");

        addButton->setObjectName("primary");
        editButton->setObjectName("outline");
        archiveButton->setObjectName("archive");
        deleteButton->setObjectName("danger");

        formButtons->addWidget(
            addButton, 0, 0
            );

        formButtons->addWidget(
            editButton, 0, 1
            );

        formButtons->addWidget(
            archiveButton, 1, 0
            );

        formButtons->addWidget(
            deleteButton, 1, 1
            );

        formLayout->addLayout(formButtons);

        auto *listCard = createCard();
        columnsLayout->addWidget(listCard, 54);

        auto *listLayout = new QVBoxLayout(listCard);

        listLayout->setContentsMargins(
            23, 23, 23, 23
            );

        listLayout->setSpacing(14);

        auto *listHeader = new QHBoxLayout;

        auto *listTitle =
            new QLabel("☷  Liste des incidents");

        listTitle->setObjectName("cardTitle");

        countLabel = new QLabel;
        countLabel->setObjectName("count");

        listHeader->addWidget(listTitle);
        listHeader->addStretch();
        listHeader->addWidget(countLabel);

        listLayout->addLayout(listHeader);

        searchInput = new QLineEdit;

        searchInput->setPlaceholderText(
            "⌕  Rechercher par type, adresse ou ID..."
            );

        gravityFilter = new QComboBox;

        gravityFilter->addItems({
            "Toutes gravités",
            "Faible",
            "Moyenne",
            "Élevée",
            "Critique"
        });

        auto *searchLayout = new QHBoxLayout;

        searchLayout->addWidget(searchInput, 2);
        searchLayout->addWidget(gravityFilter, 1);

        listLayout->addLayout(searchLayout);

        sortInput = new QComboBox;

        sortInput->addItems({
            "Date : récente d'abord",
            "Date : ancienne d'abord",
            "Gravité : élevée d'abord",
            "Gravité : faible d'abord"
        });

        auto *sortLayout = new QHBoxLayout;

        sortLayout->addWidget(
            new QLabel("Trier par :")
            );

        sortLayout->addWidget(sortInput);
        sortLayout->addStretch();

        listLayout->addLayout(sortLayout);

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
            QHeaderView::Stretch
            );

        table->horizontalHeader()->setSectionResizeMode(
            0,
            QHeaderView::ResizeToContents
            );

        table->horizontalHeader()->setSectionResizeMode(
            5,
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

        listLayout->addWidget(table, 1);

        auto *priorityCard = new QFrame;
        priorityCard->setObjectName("priorityCard");

        auto *priorityLayout =
            new QHBoxLayout(priorityCard);

        priorityLayout->setContentsMargins(
            18, 15, 18, 15
            );

        auto *warningIcon = new QLabel("⚠");

        warningIcon->setStyleSheet(
            "color:#c1121f;"
            "font-size:32px;"
            "font-weight:800;"
            );

        auto *priorityTextLayout =
            new QVBoxLayout;

        auto *priorityTitle =
            new QLabel("Alerte de priorité");

        priorityTitle->setObjectName("alertTitle");

        alertText = new QLabel;
        alertText->setObjectName("alertText");
        alertText->setWordWrap(true);

        alertDetails = new QLabel;
        alertDetails->setObjectName("alertText");
        alertDetails->setWordWrap(true);

        priorityTextLayout->addWidget(
            priorityTitle
            );

        priorityTextLayout->addWidget(
            alertText
            );

        priorityTextLayout->addWidget(
            alertDetails
            );

        alertButton =
            new QPushButton("Voir les détails  ›");

        alertButton->setObjectName("primary");

        priorityLayout->addWidget(warningIcon);
        priorityLayout->addLayout(
            priorityTextLayout,
            1
            );
        priorityLayout->addWidget(alertButton);

        listLayout->addWidget(priorityCard);

        auto *bottomButtons = new QHBoxLayout;

        auto *statisticsButton =
            new QPushButton("▥ Statistiques");

        auto *pdfButton =
            new QPushButton("Exporter en PDF");

        statisticsButton->setObjectName("outline");
        pdfButton->setObjectName("outline");

        bottomButtons->addStretch();
        bottomButtons->addWidget(statisticsButton);
        bottomButtons->addWidget(pdfButton);

        listLayout->addLayout(bottomButtons);

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
            deleteButton,
            &QPushButton::clicked,
            this,
            [this] {
                deleteIncident();
            });

        connect(
            searchInput,
            &QLineEdit::textChanged,
            this,
            [this] {
                refreshTable();
            });

        connect(
            gravityFilter,
            qOverload<int>(
                &QComboBox::currentIndexChanged
                ),
            this,
            [this] {
                refreshTable();
            });

        connect(
            sortInput,
            qOverload<int>(
                &QComboBox::currentIndexChanged
                ),
            this,
            [this] {
                refreshTable();
            });

        connect(
            table,
            &QTableWidget::cellClicked,
            this,
            [this](int row, int) {
                selectTableRow(row);
            });

        connect(
            statisticsButton,
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
                exportIncidentsPdf();
            });

        connect(
            alertButton,
            &QPushButton::clicked,
            this,
            [this] {
                showUrgentIncident();
            });

        animateEntrance(
            sidebar,
            70,
            QPoint(-45, 0)
            );

        animateEntrance(
            contentWidget,
            150,
            QPoint(35, 0)
            );
    }

    Incident *selectedIncident()
    {
        for (Incident &incident : incidents) {
            if (incident.id == selectedId
                && !incident.archived) {
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

        dateInput->setDateTime(
            QDateTime::currentDateTime()
            );

        victimsInput->setValue(0);
        addressInput->clear();
        descriptionInput->clear();
        statusInput->setCurrentIndex(0);
    }

    void fillForm(const Incident &incident)
    {
        typeInput->setCurrentText(
            incident.type
            );

        gravityInput->setCurrentText(
            incident.gravity
            );

        dateInput->setDateTime(
            incident.date
            );

        victimsInput->setValue(
            incident.victims
            );

        addressInput->setText(
            incident.address
            );

        descriptionInput->setPlainText(
            incident.description
            );

        statusInput->setCurrentText(
            incident.status
            );
    }

    void selectTableRow(int row)
    {
        if (row < 0 || !table->item(row, 0))
            return;

        selectedId =
            table->item(row, 0)
                ->data(Qt::UserRole)
                .toInt();

        Incident *incident =
            selectedIncident();

        if (incident)
            fillForm(*incident);
    }

    bool validateForm()
    {
        if (!addressInput
                 ->text()
                 .trimmed()
                 .isEmpty()) {
            return true;
        }

        QMessageBox::warning(
            this,
            "Adresse manquante",
            "Saisissez l'adresse de l'incident."
            );

        addressInput->setFocus();

        return false;
    }

    void copyFormToIncident(Incident &incident)
    {
        incident.type =
            typeInput->currentText();

        incident.gravity =
            gravityInput->currentText();

        incident.date =
            dateInput->dateTime();

        incident.victims =
            victimsInput->value();

        incident.address =
            addressInput->text().trimmed();

        incident.description =
            descriptionInput
                ->toPlainText()
                .trimmed();

        incident.status =
            statusInput->currentText();
    }

    void addIncident()
    {
        if (!validateForm())
            return;

        Incident incident;

        incident.id = nextId++;

        copyFormToIncident(incident);
        incidents.push_back(incident);

        clearForm();
        refreshTable();
    }

    void editIncident()
    {
        Incident *incident =
            selectedIncident();

        if (!incident) {
            QMessageBox::information(
                this,
                "Modifier",
                "Sélectionnez d'abord un incident."
                );

            return;
        }

        if (!validateForm())
            return;

        copyFormToIncident(*incident);

        clearForm();
        refreshTable();
    }

    void archiveIncident()
    {
        Incident *incident =
            selectedIncident();

        if (!incident) {
            QMessageBox::information(
                this,
                "Archiver",
                "Sélectionnez d'abord un incident."
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

        incident->archived = true;

        clearForm();
        refreshTable();
    }

    void deleteIncident()
    {
        Incident *incident =
            selectedIncident();

        if (!incident) {
            QMessageBox::information(
                this,
                "Supprimer",
                "Sélectionnez d'abord un incident."
                );

            return;
        }

        int id = incident->id;

        QString message =
            QString(
                "Supprimer définitivement %1 ?\n"
                "Cette action est irréversible."
                ).arg(incidentNumber(id));

        if (QMessageBox::warning(
                this,
                "Suppression définitive",
                message,
                QMessageBox::Yes
                    | QMessageBox::No,
                QMessageBox::No
                ) != QMessageBox::Yes) {
            return;
        }

        incidents.erase(
            std::remove_if(
                incidents.begin(),
                incidents.end(),
                [id](const Incident &item) {
                    return item.id == id;
                }),
            incidents.end()
            );

        clearForm();
        refreshTable();
    }

    int gravityRank(
        const QString &gravity) const
    {
        if (gravity == "Critique")
            return 4;

        if (gravity == "Élevée")
            return 3;

        if (gravity == "Moyenne")
            return 2;

        return 1;
    }

    QColor gravityBackground(
        const QString &gravity) const
    {
        if (gravity == "Critique")
            return QColor("#ffd8dc");

        if (gravity == "Élevée")
            return QColor("#ffe1e4");

        if (gravity == "Moyenne")
            return QColor("#ffe9c3");

        return QColor("#d7f4df");
    }

    QColor gravityForeground(
        const QString &gravity) const
    {
        if (gravity == "Faible")
            return QColor("#176d37");

        if (gravity == "Critique")
            return QColor("#9d1020");

        return QColor("#42383c");
    }

    void refreshTable()
    {
        std::vector<const Incident *> visible;

        QString search =
            searchInput->text().trimmed();

        QString filter =
            gravityFilter->currentText();

        for (const Incident &incident : incidents) {
            if (incident.archived)
                continue;

            if (filter != "Toutes gravités"
                && incident.gravity != filter) {
                continue;
            }

            bool matches =
                search.isEmpty()
                || incident.type.contains(
                    search,
                    Qt::CaseInsensitive
                    )
                || incident.address.contains(
                    search,
                    Qt::CaseInsensitive
                    )
                || incidentNumber(incident.id)
                       .contains(
                           search,
                           Qt::CaseInsensitive
                           );

            if (matches)
                visible.push_back(&incident);
        }

        int sortMode =
            sortInput->currentIndex();

        std::stable_sort(
            visible.begin(),
            visible.end(),
            [this, sortMode](
                const Incident *first,
                const Incident *second
                ) {
                if (sortMode == 0)
                    return first->date > second->date;

                if (sortMode == 1)
                    return first->date < second->date;

                if (sortMode == 2) {
                    return gravityRank(first->gravity)
                    > gravityRank(second->gravity);
                }

                return gravityRank(first->gravity)
                       < gravityRank(second->gravity);
            });

        table->setRowCount(0);

        for (const Incident *incident : visible) {
            int row = table->rowCount();
            table->insertRow(row);

            QStringList values = {
                incidentNumber(incident->id),
                incident->type,
                incident->address,
                incident->gravity,
                incident->status
            };

            for (int column = 0;
                 column < values.size();
                 ++column) {
                auto *item =
                    new QTableWidgetItem(
                        values[column]
                        );

                item->setData(
                    Qt::UserRole,
                    incident->id
                    );

                table->setItem(
                    row,
                    column,
                    item
                    );
            }

            table->item(row, 3)->setBackground(
                gravityBackground(
                    incident->gravity
                    )
                );

            table->item(row, 3)->setForeground(
                gravityForeground(
                    incident->gravity
                    )
                );

            if (incident->gravity == "Faible") {
                QFont font =
                    table->item(row, 3)->font();

                font.setBold(true);

                table->item(row, 3)
                    ->setFont(font);
            }

            auto *actionsWidget = new QWidget;
            auto *actionsLayout =
                new QHBoxLayout(actionsWidget);

            actionsLayout->setContentsMargins(
                3, 3, 3, 3
                );

            actionsLayout->setSpacing(4);

            auto *viewButton =
                new QPushButton("Voir");

            auto *archiveButton =
                new QPushButton("Archiver");

            auto *deleteButton =
                new QPushButton("Supprimer");

            viewButton->setObjectName(
                "tableAction"
                );

            archiveButton->setObjectName(
                "tableAction"
                );

            deleteButton->setObjectName(
                "tableDelete"
                );

            actionsLayout->addWidget(viewButton);
            actionsLayout->addWidget(archiveButton);
            actionsLayout->addWidget(deleteButton);

            table->setCellWidget(
                row,
                5,
                actionsWidget
                );

            int id = incident->id;

            connect(
                viewButton,
                &QPushButton::clicked,
                this,
                [this, id] {
                    selectedId = id;

                    Incident *incident =
                        selectedIncident();

                    if (incident)
                        fillForm(*incident);
                });

            connect(
                archiveButton,
                &QPushButton::clicked,
                this,
                [this, id] {
                    selectedId = id;
                    archiveIncident();
                });

            connect(
                deleteButton,
                &QPushButton::clicked,
                this,
                [this, id] {
                    selectedId = id;
                    deleteIncident();
                });

            table->setRowHeight(row, 48);
        }

        countLabel->setText(
            QString("%1 incidents")
                .arg(
                    static_cast<int>(
                        visible.size()
                        )
                    )
            );

        int urgentCount = 0;
        const Incident *firstUrgent = nullptr;

        for (const Incident &incident : incidents) {
            if (!incident.archived
                && calculatePriority(incident)
                       == "Urgente"
                && incident.status != "Terminé") {
                if (!firstUrgent)
                    firstUrgent = &incident;

                ++urgentCount;
            }
        }

        if (urgentCount > 0) {
            alertText->setText(
                QString(
                    "%1 incident(s) urgent(s) "
                    "nécessitent une intervention immédiate."
                    ).arg(urgentCount)
                );

            alertDetails->setText(
                QString("%1 — %2 — %3")
                    .arg(
                        incidentNumber(
                            firstUrgent->id
                            )
                        )
                    .arg(firstUrgent->type)
                    .arg(firstUrgent->address)
                );
        } else {
            alertText->setText(
                "Aucun incident urgent en attente."
                );

            alertDetails->setText(
                "La liste des incidents est à jour."
                );
        }

        alertButton->setEnabled(
            firstUrgent != nullptr
            );

        animateRefresh(table);
    }

    void showUrgentIncident()
    {
        for (const Incident &incident : incidents) {
            if (!incident.archived
                && calculatePriority(incident)
                       == "Urgente"
                && incident.status != "Terminé") {
                selectedId = incident.id;
                fillForm(incident);

                for (int row = 0;
                     row < table->rowCount();
                     ++row) {
                    if (table->item(row, 0)
                        && table->item(row, 0)
                                   ->data(Qt::UserRole)
                                   .toInt()
                               == incident.id) {
                        table->selectRow(row);

                        table->scrollToItem(
                            table->item(row, 0)
                            );

                        break;
                    }
                }

                return;
            }
        }
    }

    void showStatistics()
    {
        int total = 0;
        int critical = 0;
        int low = 0;
        int active = 0;
        int finished = 0;

        QMap<QString, int> byType;
        QMap<QString, int> byGravity;
        QMap<QString, int> byStatus;
        QMap<QString, int> byZone;

        for (const Incident &incident : incidents) {
            if (incident.archived)
                continue;

            ++total;
            ++byType[incident.type];
            ++byGravity[incident.gravity];
            ++byStatus[incident.status];

            ++byZone[
                incident.address.isEmpty()
                    ? "Non précisée"
                    : incident.address
            ];

            if (incident.gravity == "Critique")
                ++critical;

            if (incident.gravity == "Faible")
                ++low;

            if (incident.status == "En cours")
                ++active;

            if (incident.status == "Terminé")
                ++finished;
        }

        QDialog dialog(this);

        dialog.setWindowTitle(
            "Statistiques — Gestion des incidents"
            );

        dialog.resize(1280, 760);
        dialog.setMinimumSize(1000, 650);

        dialog.setStyleSheet(R"CSS(
            QDialog,
            QWidget#statisticsPage,
            QScrollArea {
                background: #f3f5f7;
                font-family: "Segoe UI";
            }

            QFrame#statisticsSidebar {
                background: qlineargradient(
                    x1:0, y1:0,
                    x2:0, y2:1,
                    stop:0 #27050c,
                    stop:0.32 #59101a,
                    stop:0.68 #a71928,
                    stop:1 #d53d39
                );
                border: none;
            }

            QLabel#statisticsTitle {
                color: #231f27;
                font-size: 27px;
                font-weight: 800;
            }

            QFrame#statisticsCard {
                background: white;
                border: 1px solid #e7e9ed;
                border-radius: 14px;
            }

            QLabel#statisticsCardTitle {
                color: #3b252a;
                font-size: 15px;
                font-weight: 800;
            }

            QPushButton#statisticsButton {
                color: #a61725;
                background: white;
                border: 2px solid #c94d59;
                border-radius: 9px;
                padding: 8px 13px;
                font-weight: 700;
            }

            QPushButton#statisticsNav {
                color: white;
                background: transparent;
                border: none;
                border-radius: 8px;
                text-align: left;
                padding: 10px 14px;
            }

            QPushButton#statisticsNavActive {
                color: #9d1724;
                background: white;
                border: none;
                border-radius: 9px;
                text-align: left;
                padding: 10px 14px;
                font-weight: 800;
            }

            QProgressBar {
                background: #f3e4e6;
                border: none;
                border-radius: 7px;
                min-height: 14px;
                text-align: center;
            }

            QProgressBar::chunk {
                background: #b51c2a;
                border-radius: 7px;
            }
        )CSS");

        auto *rootLayout =
            new QHBoxLayout(&dialog);

        rootLayout->setContentsMargins(
            0, 0, 0, 0
            );

        rootLayout->setSpacing(0);

        auto *statisticsSidebar = new QFrame;

        statisticsSidebar->setObjectName(
            "statisticsSidebar"
            );

        statisticsSidebar->setFixedWidth(225);

        auto *sidebarLayout =
            new QVBoxLayout(statisticsSidebar);

        sidebarLayout->setContentsMargins(
            18, 24, 18, 24
            );

        sidebarLayout->setSpacing(7);

        auto *logoLabel = new QLabel;
        logoLabel->setAlignment(Qt::AlignCenter);

        QPixmap logo = loadLogo();

        if (!logo.isNull()) {
            logoLabel->setPixmap(
                logo.scaled(
                    90,
                    90,
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation
                    )
                );
        }

        sidebarLayout->addWidget(logoLabel);

        auto *brandLabel = new QLabel(
            "SMART FIRE\nSTATION"
            );

        brandLabel->setAlignment(
            Qt::AlignCenter
            );

        brandLabel->setStyleSheet(
            "color:white;"
            "font-size:17px;"
            "font-weight:800;"
            );

        sidebarLayout->addWidget(brandLabel);
        sidebarLayout->addSpacing(18);

        const QList<QPair<QString, QString>>
            navigation = {
                {"⌂", "Tableau de bord"},
                {"⚠", "Incidents"},
                {"✚", "Interventions"},
                {"♟", "Personnel"},
                {"▰", "Véhicules"},
                {"⚙", "Équipements"},
                {"▥", "Rapports"}
            };

        for (const auto &entry : navigation) {
            QString icon = entry.first;
            QString name = entry.second;

            auto *button = new QPushButton(
                icon + "   " + name
                );

            button->setObjectName(
                name == "Incidents"
                    ? "statisticsNavActive"
                    : "statisticsNav"
                );

            sidebarLayout->addWidget(button);

            if (name == "Incidents") {
                connect(
                    button,
                    &QPushButton::clicked,
                    &dialog,
                    &QDialog::accept
                    );
            }
        }

        sidebarLayout->addStretch();

        rootLayout->addWidget(
            statisticsSidebar
            );

        auto *scrollArea = new QScrollArea;

        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(
            QFrame::NoFrame
            );

        auto *statisticsPage = new QWidget;

        statisticsPage->setObjectName(
            "statisticsPage"
            );

        auto *pageLayout =
            new QVBoxLayout(statisticsPage);

        pageLayout->setContentsMargins(
            28, 22, 28, 24
            );

        pageLayout->setSpacing(14);

        auto *headerLayout = new QHBoxLayout;

        auto *titleLabel = new QLabel(
            "Statistiques — Gestion des incidents"
            );

        titleLabel->setObjectName(
            "statisticsTitle"
            );

        auto *backButton =
            new QPushButton("Retour à la liste");

        auto *exportButton =
            new QPushButton("Exporter en PDF");

        backButton->setObjectName(
            "statisticsButton"
            );

        exportButton->setObjectName(
            "statisticsButton"
            );

        headerLayout->addWidget(titleLabel);
        headerLayout->addStretch();
        headerLayout->addWidget(backButton);
        headerLayout->addWidget(exportButton);

        pageLayout->addLayout(headerLayout);

        auto createCounter = [](
                                 const QString &number,
                                 const QString &text,
                                 const QString &color
                                 ) {
            auto *card = new QFrame;
            card->setObjectName(
                "statisticsCard"
                );

            card->setMinimumHeight(85);

            auto *layout =
                new QVBoxLayout(card);

            auto *numberLabel =
                new QLabel(number);

            numberLabel->setStyleSheet(
                QString(
                    "font-size:26px;"
                    "font-weight:800;"
                    "color:%1;"
                    ).arg(color)
                );

            auto *textLabel = new QLabel(text);

            textLabel->setStyleSheet(
                "color:#777984;"
                );

            layout->addWidget(numberLabel);
            layout->addWidget(textLabel);

            return card;
        };

        auto *counterLayout = new QHBoxLayout;

        counterLayout->addWidget(
            createCounter(
                QString::number(total),
                "Total des incidents",
                "#5b1722"
                )
            );

        counterLayout->addWidget(
            createCounter(
                QString::number(critical),
                "Critiques",
                "#c1121f"
                )
            );

        counterLayout->addWidget(
            createCounter(
                QString::number(low),
                "Faibles",
                "#2e9d57"
                )
            );

        counterLayout->addWidget(
            createCounter(
                QString::number(active),
                "En cours",
                "#b51c2a"
                )
            );

        counterLayout->addWidget(
            createCounter(
                QString::number(finished),
                "Terminés",
                "#74121e"
                )
            );

        pageLayout->addLayout(counterLayout);

        auto createChart = [](
                               const QString &title
                               ) {
            auto *card = new QFrame;

            card->setObjectName(
                "statisticsCard"
                );

            auto *layout =
                new QVBoxLayout(card);

            layout->setContentsMargins(
                18, 15, 18, 15
                );

            layout->setSpacing(8);

            auto *titleLabel =
                new QLabel(title);

            titleLabel->setObjectName(
                "statisticsCardTitle"
                );

            layout->addWidget(titleLabel);

            return card;
        };

        auto addBars = [](
                           QFrame *card,
                           const QMap<QString, int> &values
                           ) {
            auto *layout =
                qobject_cast<QVBoxLayout *>(
                    card->layout()
                    );

            int maximum = 1;

            for (int value : values)
                maximum = std::max(
                    maximum,
                    value
                    );

            int delay = 140;

            for (auto iterator = values.cbegin();
                 iterator != values.cend();
                 ++iterator) {
                auto *rowLayout =
                    new QHBoxLayout;

                auto *label = new QLabel(
                    QString("%1 (%2)")
                        .arg(iterator.key())
                        .arg(iterator.value())
                    );

                label->setMinimumWidth(135);

                auto *bar = new QProgressBar;

                bar->setRange(0, maximum);
                bar->setValue(0);
                bar->setTextVisible(false);

                if (iterator.key() == "Faible") {
                    label->setStyleSheet(
                        "color:#176d37;"
                        "font-weight:700;"
                        );

                    bar->setStyleSheet(
                        "QProgressBar {"
                        "background:#dff3e5;"
                        "border:none;"
                        "border-radius:7px;"
                        "min-height:14px;"
                        "}"
                        "QProgressBar::chunk {"
                        "background:#2e9d57;"
                        "border-radius:7px;"
                        "}"
                        );
                }

                rowLayout->addWidget(label);
                rowLayout->addWidget(bar, 1);

                layout->addLayout(rowLayout);

                int finalValue =
                    iterator.value();

                QTimer::singleShot(
                    delay,
                    bar,
                    [bar, finalValue] {
                        auto *animation =
                            new QPropertyAnimation(
                                bar,
                                "value",
                                bar
                                );

                        animation->setDuration(650);
                        animation->setStartValue(0);
                        animation->setEndValue(
                            finalValue
                            );

                        animation->setEasingCurve(
                            QEasingCurve::OutCubic
                            );

                        animation->start(
                            QAbstractAnimation::
                            DeleteWhenStopped
                            );
                    });

                delay += 90;
            }
        };

        auto *topCharts = new QHBoxLayout;

        auto *typeChart =
            createChart("Incidents par type");

        auto *gravityChart =
            createChart("Incidents par gravité");

        addBars(typeChart, byType);
        addBars(gravityChart, byGravity);

        topCharts->addWidget(typeChart, 1);
        topCharts->addWidget(gravityChart, 1);

        pageLayout->addLayout(topCharts);

        auto *bottomCharts =
            new QHBoxLayout;

        auto *zoneChart =
            createChart("Incidents par zone");

        auto *statusChart =
            createChart("Incidents par statut");

        addBars(zoneChart, byZone);
        addBars(statusChart, byStatus);

        bottomCharts->addWidget(zoneChart, 1);
        bottomCharts->addWidget(statusChart, 1);

        pageLayout->addLayout(bottomCharts);
        pageLayout->addStretch();

        scrollArea->setWidget(statisticsPage);
        rootLayout->addWidget(scrollArea, 1);

        connect(
            backButton,
            &QPushButton::clicked,
            &dialog,
            &QDialog::accept
            );

        connect(
            exportButton,
            &QPushButton::clicked,
            &dialog,
            [&dialog, statisticsPage] {
                QString path =
                    QFileDialog::getSaveFileName(
                        &dialog,
                        "Enregistrer les statistiques",
                        "statistiques_incidents.pdf",
                        "PDF (*.pdf)"
                        );

                if (path.isEmpty())
                    return;

                if (!path.endsWith(
                        ".pdf",
                        Qt::CaseInsensitive)) {
                    path += ".pdf";
                }

                QPdfWriter pdf(path);

                pdf.setPageSize(
                    QPageSize(QPageSize::A4)
                    );

                pdf.setPageOrientation(
                    QPageLayout::Landscape
                    );

                pdf.setResolution(150);

                QPainter painter(&pdf);

                if (!painter.isActive()) {
                    QMessageBox::warning(
                        &dialog,
                        "Export PDF",
                        "Impossible de créer le PDF."
                        );

                    return;
                }

                QPixmap capture =
                    statisticsPage->grab();

                QSize size = capture.size();

                size.scale(
                    pdf.width(),
                    pdf.height(),
                    Qt::KeepAspectRatio
                    );

                painter.drawPixmap(
                    QRect(
                        (pdf.width()
                         - size.width()) / 2,
                        (pdf.height()
                         - size.height()) / 2,
                        size.width(),
                        size.height()
                        ),
                    capture
                    );

                painter.end();

                QMessageBox::information(
                    &dialog,
                    "Export PDF",
                    "Les statistiques ont été enregistrées."
                    );
            });

        dialog.setWindowOpacity(0.0);

        QTimer::singleShot(
            30,
            &dialog,
            [&dialog] {
                auto *animation =
                    new QPropertyAnimation(
                        &dialog,
                        "windowOpacity",
                        &dialog
                        );

                animation->setDuration(420);
                animation->setStartValue(0.0);
                animation->setEndValue(1.0);

                animation->setEasingCurve(
                    QEasingCurve::OutCubic
                    );

                animation->start(
                    QAbstractAnimation::
                    DeleteWhenStopped
                    );
            });

        animateEntrance(
            statisticsSidebar,
            60,
            QPoint(-40, 0)
            );

        animateEntrance(
            statisticsPage,
            130,
            QPoint(35, 0)
            );

        dialog.exec();
    }

    void exportIncidentsPdf()
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

        QString html =
            "<h1 style='color:#b51c2a'>"
            "SMART FIRE STATION"
            "</h1>"
            "<h2>Liste des incidents</h2>"
            "<table width='100%' "
            "border='1' cellspacing='0' "
            "cellpadding='6'>"
            "<tr style='background:#5b1722;"
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

        for (const Incident &incident : incidents) {
            if (incident.archived)
                continue;

            QString color =
                incident.gravity == "Faible"
                    ? "#d7f4df"
                    : "white";

            html += QString(
                        "<tr style='background:%1'>"
                        ).arg(color);

            QStringList values = {
                incidentNumber(incident.id),
                incident.type,
                incident.date.toString(
                    "dd/MM/yyyy HH:mm"
                    ),
                incident.address,
                incident.gravity,
                QString::number(
                    incident.victims
                    ),
                incident.status,
                calculatePriority(incident)
            };

            for (const QString &value : values) {
                html +=
                    "<td>"
                    + value.toHtmlEscaped()
                    + "</td>";
            }

            html += "</tr>";
        }

        html += "</table>";

        QPdfWriter pdf(path);

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

        QMessageBox::information(
            this,
            "Export PDF",
            "PDF enregistré :\n" + path
            );
    }
};

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);

    application.setStyle("Fusion");
    application.setFont(
        QFont("Segoe UI", 10)
        );

    MainWindow window;
    window.show();

    return application.exec();
}