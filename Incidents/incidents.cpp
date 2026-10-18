#include <QtWidgets>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPageLayout>
#include <QPageSize>
#include <QTextDocument>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>

#include <algorithm>
#include <vector>
#include <cmath>

#include "incidents.h"

namespace incidents {

static constexpr double PI = 3.14159265358979323846;

struct Incident {
    QString id;
    QString type;
    QDateTime date;
    QString address;
    QString gravity;
    int victims = 0;
    QString description;
    QString status;
    bool archived = false;
};

static QString incidentNumber(const QString &id)
{
    return id;
}

static int gravityRank(const QString &gravity)
{
    if (gravity == "Critique") return 4;
    if (gravity == "Élevée") return 3;
    if (gravity == "Moyenne") return 2;
    return 1;
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

class AnimatedTitle : public QLabel {
public:
    explicit AnimatedTitle(
        const QString &title,
        QWidget *parent = nullptr
        ) : QLabel(title, parent)
    {
        QFont f("Segoe UI");
        f.setPixelSize(20);
        f.setBold(true);
        setFont(f);

        setMinimumHeight(36);
        setSizePolicy(
            QSizePolicy::Preferred,
            QSizePolicy::Fixed
            );

        reserveTextSpace();

        auto *timer = new QTimer(this);
        timer->setInterval(55);

        connect(timer, &QTimer::timeout, this, [this] {
            const int total = static_cast<int>(text().size());

            shimmer += 0.018;
            if (shimmer >= 1.0)
                shimmer -= 1.0;

            if (pauseTicks > 0) {
                --pauseTicks;
                update();
                return;
            }

            if (direction > 0) {
                revealed = std::min(revealed + 1, total);

                if (revealed >= total) {
                    direction = -1;
                    pauseTicks = 50;
                }
            } else {
                revealed = std::max(0, revealed - 1);

                if (revealed == 0) {
                    direction = 1;
                    pauseTicks = 10;
                }
            }

            update();
        });

        timer->start();
    }

    QSize sizeHint() const override
    {
        const QFontMetricsF metrics(font());

        return QSize(
            static_cast<int>(
                std::ceil(metrics.horizontalAdvance(text()))
                ) + 30,
            36
            );
    }

protected:
    void changeEvent(QEvent *event) override
    {
        QLabel::changeEvent(event);

        if (event->type() == QEvent::FontChange)
            reserveTextSpace();
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QString visible = text().left(revealed);
        const QFontMetricsF metrics(font());

        const double baseline =
            (height() - metrics.height()) / 2.0
            + metrics.ascent();

        QPainterPath path;
        path.addText(QPointF(8, baseline), font(), visible);

        const double pulse =
            0.5 + 0.5 * std::sin(2.0 * PI * shimmer);

        painter.setBrush(Qt::NoBrush);

        painter.setPen(QPen(
            QColor(
                255, 157, 73,
                static_cast<int>(30 + 30 * pulse)
                ),
            9
            ));
        painter.drawPath(path);

        painter.setPen(QPen(
            QColor(
                213, 51, 65,
                static_cast<int>(45 + 35 * pulse)
                ),
            4
            ));
        painter.drawPath(path);

        const double position =
            -60.0 + (width() + 120.0) * shimmer;

        QLinearGradient light(
            position - 55, 0,
            position + 55, 0
            );

        light.setColorAt(0.00, QColor("#8f1723"));
        light.setColorAt(0.35, QColor("#8f1723"));
        light.setColorAt(0.48, QColor("#da4f38"));
        light.setColorAt(0.52, QColor("#ffbf66"));
        light.setColorAt(0.65, QColor("#8f1723"));
        light.setColorAt(1.00, QColor("#8f1723"));

        painter.setPen(Qt::NoPen);
        painter.setBrush(light);
        painter.drawPath(path);

        if (pauseTicks == 0 || pulse > 0.45) {
            const double cursorX =
                11.0 + metrics.horizontalAdvance(visible);

            painter.setPen(
                QPen(QColor("#ffaf5c"), 2.5)
                );

            painter.drawLine(
                QPointF(
                    cursorX,
                    baseline - metrics.ascent() * 0.85
                    ),
                QPointF(cursorX, baseline + 2)
                );
        }
    }

private:
    int revealed = 0;
    int direction = 1;
    int pauseTicks = 0;
    double shimmer = 0.0;

    void reserveTextSpace()
    {
        const QFontMetricsF metrics(font());

        setMinimumWidth(
            static_cast<int>(
                std::ceil(metrics.horizontalAdvance(text()))
                ) + 30
            );
    }
};

class AnimatedButton : public QPushButton {
public:
    explicit AnimatedButton(
        const QString &text,
        QWidget *parent = nullptr
        ) : QPushButton(text, parent)
    {
        setCursor(Qt::PointingHandCursor);
        setMinimumHeight(34);

        auto *animation = new QVariantAnimation(this);
        animation->setStartValue(0.0);
        animation->setEndValue(1.0);
        animation->setDuration(2600);
        animation->setLoopCount(-1);

        connect(
            animation,
            &QVariantAnimation::valueChanged,
            this,
            [this](const QVariant &value) {
                phase = value.toDouble();
                update();
            }
            );

        animation->start();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QStyleOptionButton option;
        initStyleOption(&option);

        QStylePainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const double pulse =
            0.5 + 0.5 * std::sin(2.0 * PI * phase);

        const double zoom =
            isDown() ? 0.93 : 0.94 + 0.05 * pulse;

        painter.translate(
            width() / 2.0,
            height() / 2.0
            );
        painter.scale(zoom, zoom);
        painter.translate(
            -width() / 2.0,
            -height() / 2.0
            );

        option.rect = rect().adjusted(3, 3, -3, -3);

        const QString label = option.text;
        const QIcon icon = option.icon;

        option.text.clear();
        option.icon = QIcon();

        painter.drawControl(
            QStyle::CE_PushButton,
            option
            );

        const QRectF border =
            QRectF(option.rect).adjusted(1, 1, -1, -1);

        if (isEnabled()) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(
                QColor(
                    255, 174, 75,
                    static_cast<int>(110 + 100 * pulse)
                    ),
                2.2
                ));
            painter.drawRoundedRect(border, 9, 9);

            QPainterPath clip;
            clip.addRoundedRect(border, 9, 9);

            painter.save();
            painter.setClipPath(clip);

            const double x =
                -60.0 + (width() + 120.0) * phase;

            QLinearGradient shine(
                x - 35, 0,
                x + 35, height()
                );

            shine.setColorAt(
                0.00, QColor(255, 231, 185, 0)
                );
            shine.setColorAt(
                0.40, QColor(255, 231, 185, 0)
                );
            shine.setColorAt(
                0.50, QColor(255, 231, 185, 120)
                );
            shine.setColorAt(
                0.60, QColor(255, 231, 185, 0)
                );
            shine.setColorAt(
                1.00, QColor(255, 231, 185, 0)
                );

            painter.fillRect(
                border,
                QBrush(shine)
                );
            painter.restore();
        }

        option.text = label;
        option.icon = icon;

        painter.drawControl(
            QStyle::CE_PushButtonLabel,
            option
            );
    }

private:
    double phase = 0.0;
};

class BrightnessOverlay : public QWidget {
public:
    explicit BrightnessOverlay(QWidget *parent)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setAutoFillBackground(false);

        parent->installEventFilter(this);
        setGeometry(parent->rect());

        show();
        raise();
    }

    void setBrightness(int value)
    {
        brightness = qBound(60, value, 100);
        update();
    }

protected:
    bool eventFilter(
        QObject *object,
        QEvent *event
        ) override
    {
        if (
            object == parentWidget() &&
            (
                event->type() == QEvent::Resize ||
                event->type() == QEvent::Show
                )
            ) {
            setGeometry(parentWidget()->rect());
            raise();
        }

        return QWidget::eventFilter(object, event);
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);

        painter.fillRect(
            rect(),
            QColor(0, 0, 0, (100 - brightness) * 2)
            );
    }

private:
    int brightness = 100;
};

static QString applicationStyle()
{
    return QString::fromUtf8(R"CSS(
        QWidget {
            font-family: "Segoe UI";
            color: #302934;
        }

        QWidget#root, QWidget#content, QScrollArea {
            background: #f3f5f7;
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

        QFrame#brightnessCard {
            background: white;
            border: 1px solid #ebc5ca;
            border-radius: 10px;
        }

        QLabel#heading {
            font-size: 27px;
            font-weight: 800;
            color: #211e25;
        }

        QLabel#statisticsTitle {
            font-size: 27px;
            font-weight: 800;
            color: #211e25;
        }

        QLabel#subheading {
            font-size: 12px;
            color: #777984;
        }

        QLabel#fieldTitle {
            font-size: 12px;
            font-weight: 700;
        }

        QLabel#count {
            color: white;
            background: #981a27;
            border-radius: 12px;
            padding: 5px 10px;
        }

        QLabel#alertTitle {
            color: #981a27;
            font-size: 15px;
            font-weight: 800;
        }

        QLabel#chartTitle {
            font-size: 15px;
            font-weight: 800;
        }

        QLineEdit, QComboBox, QDateTimeEdit,
        QSpinBox, QTextEdit {
            background: #fbfbfc;
            color: #302934;
            border: 2px solid #dfe2e7;
            border-radius: 8px;
            padding: 4px 6px;
            min-height: 19px;
            font-size: 12px;
        }

        QLineEdit:focus, QComboBox:focus,
        QDateTimeEdit:focus, QSpinBox:focus,
        QTextEdit:focus {
            background: white;
            border-color: #bd1e2c;
        }

        QComboBox QAbstractItemView {
            background: white;
            selection-background-color: #ffe5e7;
            selection-color: #64131d;
        }

        QPushButton#primary, QPushButton#outline,
        QPushButton#archive, QPushButton#danger {
            border-radius: 9px;
            padding: 5px 10px;
            font-weight: 700;
        }

        QPushButton#primary {
            color: white;
            background: #b51c2a;
            border: 2px solid #8f1420;
        }

        QPushButton#primary:hover {
            background: #cf2635;
        }

        QPushButton#outline {
            color: #a61725;
            background: white;
            border: 2px solid #c94d59;
        }

        QPushButton#outline:hover {
            background: #fff1f2;
        }

        QPushButton#archive {
            color: #981a27;
            background: #fff0f1;
            border: 2px solid #e0a3aa;
        }

        QPushButton#archive:hover {
            background: #f9dce0;
        }

        QPushButton#danger {
            color: white;
            background: #8f1420;
            border: 2px solid #74101a;
        }

        QPushButton#danger:hover {
            background: #c1121f;
        }

        QPushButton#compactAction {
            background: white;
            color: #981a27;
            border: 1px solid #d6a6ae;
            border-radius: 6px;
            padding: 0px 8px;
            font-size: 11px;
            font-weight: 600;
        }

        QPushButton#compactAction:hover {
            background: #fff0f2;
            border-color: #b51c2a;
        }

        QPushButton#compactAction:pressed {
            background: #f5dce1;
        }

        QPushButton:disabled {
            color: #999999;
        }

        QTableWidget {
            background: white;
            alternate-background-color: #fbf7f8;
            border: 1px solid #e4e6ea;
            border-radius: 10px;
            gridline-color: #eceef1;
            selection-background-color: #ffe5e7;
            selection-color: #302934;
            font-size: 11px;
        }

        QHeaderView::section {
            color: white;
            background: #5b1722;
            border: none;
            border-right: 1px solid #762834;
            padding: 8px 4px;
            font-size: 10px;
            font-weight: 700;
        }

        QPushButton#tableAction {
            color: #9e1724;
            background: #fff4f5;
            border: 1px solid #efc9cd;
            border-radius: 6px;
            padding: 4px;
            font-size: 10px;
        }

        QPushButton#tableAction:hover {
            background: #f6d6da;
        }

        QPushButton#tableDelete {
            color: white;
            background: #8f1420;
            border: 1px solid #74101a;
            border-radius: 6px;
            padding: 4px;
            font-size: 10px;
        }

        QPushButton#tableDelete:hover {
            background: #c1121f;
        }

        QSlider::groove:horizontal {
            background: #ead7da;
            height: 5px;
            border-radius: 2px;
        }

        QSlider::sub-page:horizontal {
            background: #b51c2a;
        }

        QSlider::handle:horizontal {
            background: #b51c2a;
            border: 2px solid white;
            width: 13px;
            margin: -6px 0;
            border-radius: 7px;
        }

        QProgressBar {
            background: #f3e4e6;
            border: none;
            border-radius: 7px;
            min-height: 14px;
            max-height: 18px;
        }

        QProgressBar::chunk {
            background: #b51c2a;
            border-radius: 7px;
        }
    )CSS");
}

class MainWindow : public QMainWindow {
public:
    MainWindow()
    {
        setWindowTitle(
            "Smart Fire Station — Gestion des incidents"
            );

        resize(1200, 700);
        setMinimumSize(0, 0);
        setStyleSheet(applicationStyle());

        createInterface();

        loadIncidents();
        refreshTable();
    }

private:
    std::vector<Incident> incidents;

    QString selectedId;
    int brightness = 100;

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

    QVBoxLayout *urgentListLayout = nullptr;
    AnimatedButton *alertButton = nullptr;

    QSlider *brightnessSlider = nullptr;
    QLabel *brightnessLabel = nullptr;
    BrightnessOverlay *brightnessOverlay = nullptr;

    QFrame *createCard()
    {
        auto *card = new QFrame;
        card->setObjectName("card");
        return card;
    }

    AnimatedButton *createButton(
        const QString &text,
        const QString &styleName = "outline"
        )
    {
        auto *button = new AnimatedButton(text);
        button->setObjectName(styleName);
        return button;
    }

    QVBoxLayout *createField(
        const QString &title,
        QWidget *input
        )
    {
        auto *layout = new QVBoxLayout;
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(3);

        auto *label = new QLabel(title);
        label->setObjectName("fieldTitle");

        layout->addWidget(label);
        layout->addWidget(input);

        return layout;
    }

    void createInterface()
    {
        auto *root = new QWidget;
        root->setObjectName("root");
        setCentralWidget(root);

        auto *mainLayout = new QHBoxLayout(root);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);

        mainLayout->addWidget(scroll, 1);

        auto *content = new QWidget;
        content->setObjectName("content");
        scroll->setWidget(content);

        auto *page = new QVBoxLayout(content);
        page->setContentsMargins(14, 10, 14, 10);
        page->setSpacing(10);

        auto *header = new QHBoxLayout;
        auto *titles = new QVBoxLayout;
        titles->setSpacing(5);

        auto *heading = new QLabel(
            "Gestion des incidents"
            );
        heading->setObjectName("heading");

        auto *subtitle = new QLabel(
            "Créer, consulter et suivre les incidents en temps réel"
            );
        subtitle->setObjectName("subheading");
        subtitle->setWordWrap(true);

        auto *line = new QFrame;
        line->setFixedSize(82, 4);
        line->setStyleSheet(
            "background:#c51f2e;"
            "border:none;"
            "border-radius:2px;"
            );

        titles->addWidget(heading);
        titles->addWidget(subtitle);
        titles->addWidget(line);

        header->addLayout(titles, 1);

        auto *statisticsButton =
            new QPushButton("Statistiques");

        auto *pdfButton =
            new QPushButton("Exporter en PDF");

        for (
            QPushButton *button :
            {statisticsButton, pdfButton}
            ) {
            button->setObjectName("compactAction");
            button->setCursor(Qt::PointingHandCursor);
            button->setFixedHeight(32);
            button->setSizePolicy(
                QSizePolicy::Fixed,
                QSizePolicy::Fixed
                );
        }

        statisticsButton->setFixedWidth(112);
        pdfButton->setFixedWidth(138);

        auto *headerRight = new QVBoxLayout;
        headerRight->setSpacing(8);

        auto *brightnessCard = new QFrame;
        brightnessCard->setObjectName("brightnessCard");
        brightnessCard->setFixedWidth(150);

        auto *brightnessLayout =
            new QVBoxLayout(brightnessCard);

        brightnessLayout->setContentsMargins(9, 6, 9, 6);
        brightnessLayout->setSpacing(5);

        brightnessLabel =
            new QLabel("☀ Luminosité : 100 %");

        brightnessLabel->setStyleSheet(
            "font-size:11px;font-weight:600;"
            );

        brightnessSlider = new QSlider(Qt::Horizontal);
        brightnessSlider->setRange(60, 100);
        brightnessSlider->setValue(100);
        brightnessSlider->setFixedWidth(85);

        brightnessLayout->addWidget(brightnessLabel);
        brightnessLayout->addWidget(
            brightnessSlider,
            0,
            Qt::AlignHCenter
            );

        headerRight->addWidget(
            brightnessCard,
            0,
            Qt::AlignRight
            );

        auto *topActions = new QHBoxLayout;
        topActions->setSpacing(8);
        topActions->addStretch();
        topActions->addWidget(statisticsButton);
        topActions->addWidget(pdfButton);

        headerRight->addLayout(topActions);
        header->addLayout(headerRight);
        page->addLayout(header);

        auto *columns = new QHBoxLayout;
        columns->setSpacing(10);
        page->addLayout(columns, 1);

        auto *formCard = createCard();
        columns->addWidget(formCard, 43);

        auto *form = new QVBoxLayout(formCard);
        form->setContentsMargins(12, 10, 12, 10);
        form->setSpacing(7);

        form->addWidget(
            new AnimatedTitle("Nouvel incident")
            );

        auto *formDescription = new QLabel(
            "Saisir les informations de l'incident"
            );
        formDescription->setObjectName("subheading");
        form->addWidget(formDescription);

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
        victimsInput->setRange(0, 9999);

        addressInput = new QLineEdit;
        addressInput->setMaxLength(150);
        addressInput->setPlaceholderText(
            "Ex. Tunis, Centre-ville"
            );

        descriptionInput = new QTextEdit;
        descriptionInput->setFixedHeight(60);
        descriptionInput->setPlaceholderText(
            "Décrivez brièvement la situation et les risques observés..."
            );

        statusInput = new QComboBox;
        statusInput->addItems({
            "Signalé",
            "En cours",
            "Terminé"
        });

        auto *grid = new QGridLayout;
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(7);

        grid->addLayout(
            createField("Type d'incident", typeInput),
            0, 0
            );

        grid->addLayout(
            createField("Niveau de gravité", gravityInput),
            0, 1
            );

        grid->addLayout(
            createField("Date et heure", dateInput),
            1, 0
            );

        grid->addLayout(
            createField("Nombre de victimes", victimsInput),
            1, 1
            );

        grid->addLayout(
            createField("Adresse", addressInput),
            2, 0, 1, 2
            );

        grid->addLayout(
            createField("Description", descriptionInput),
            3, 0, 1, 2
            );

        grid->addLayout(
            createField("Statut", statusInput),
            4, 0, 1, 2
            );

        form->addLayout(grid);
        form->addStretch();

        auto *formButtons = new QGridLayout;

        auto *addButton =
            createButton("＋ Ajouter", "primary");

        auto *editButton =
            createButton("✎ Modifier");

        auto *archiveButton =
            createButton("▣ Archiver", "archive");

        auto *deleteButton =
            createButton("✕ Supprimer", "danger");

        for (
            QPushButton *button :
            {addButton, editButton, archiveButton, deleteButton}
            ) {
            button->setFixedHeight(36);
            button->setMinimumWidth(0);
            button->setMaximumWidth(16777215);
            button->setSizePolicy(
                QSizePolicy::Expanding,
                QSizePolicy::Fixed
                );
        }

        formButtons->setHorizontalSpacing(8);
        formButtons->setVerticalSpacing(6);
        formButtons->setColumnStretch(0, 1);
        formButtons->setColumnStretch(1, 1);

        formButtons->addWidget(addButton, 0, 0);
        formButtons->addWidget(editButton, 0, 1);
        formButtons->addWidget(archiveButton, 1, 0);
        formButtons->addWidget(deleteButton, 1, 1);

        form->addLayout(formButtons);

        auto *listCard = createCard();
        columns->addWidget(listCard, 57);

        auto *list = new QVBoxLayout(listCard);
        list->setContentsMargins(12, 10, 12, 10);
        list->setSpacing(7);

        auto *listTitleRow = new QHBoxLayout;

        listTitleRow->addWidget(
            new AnimatedTitle("Liste des incidents")
            );
        listTitleRow->addStretch();

        countLabel = new QLabel;
        countLabel->setObjectName("count");

        listTitleRow->addWidget(countLabel);
        list->addLayout(listTitleRow);

        searchInput = new QLineEdit;
        searchInput->setPlaceholderText(
            "⌕ Rechercher par type, adresse ou ID..."
            );

        gravityFilter = new QComboBox;
        gravityFilter->addItems({
            "Toutes gravités",
            "Faible",
            "Moyenne",
            "Élevée",
            "Critique"
        });

        auto *searchRow = new QHBoxLayout;
        searchRow->setSpacing(6);
        searchRow->addWidget(searchInput, 2);
        searchRow->addWidget(gravityFilter, 1);

        list->addLayout(searchRow);

        sortInput = new QComboBox;
        sortInput->addItems({
            "Date : récente d'abord",
            "Date : ancienne d'abord",
            "Gravité : élevée d'abord",
            "Gravité : faible d'abord"
        });

        auto *sortRow = new QHBoxLayout;
        sortRow->addWidget(new QLabel("Trier par :"));
        sortRow->addWidget(sortInput);
        sortRow->addStretch();

        list->addLayout(sortRow);

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

        table->verticalHeader()->hide();
        table->setEditTriggers(
            QAbstractItemView::NoEditTriggers
            );
        table->setSelectionBehavior(
            QAbstractItemView::SelectRows
            );
        table->setSelectionMode(
            QAbstractItemView::SingleSelection
            );
        table->setAlternatingRowColors(true);
        table->setMinimumHeight(160);

        list->addWidget(table, 1);

        auto *priorityCard = new QFrame;
        priorityCard->setObjectName("priorityCard");

        auto *priorityLayout =
            new QHBoxLayout(priorityCard);

        priorityLayout->setContentsMargins(10, 8, 10, 8);

        auto *warning = new QLabel("⚠");
        warning->setStyleSheet(
            "color:#c1121f;"
            "font-size:27px;"
            "font-weight:800;"
            );

        auto *priorityTexts = new QVBoxLayout;
        priorityTexts->setSpacing(4);

        auto *priorityTitle =
            new QLabel("Alerte de priorité");

        priorityTitle->setObjectName("alertTitle");

        alertText = new QLabel;
        alertText->setWordWrap(true);
        alertText->setStyleSheet("font-size:11px;");

        priorityTexts->addWidget(priorityTitle);
        priorityTexts->addWidget(alertText);

        urgentListLayout = new QVBoxLayout;
        urgentListLayout->setContentsMargins(0, 0, 0, 0);
        urgentListLayout->setSpacing(3);

        priorityTexts->addLayout(urgentListLayout);

        alertButton = createButton(
            "Voir la première ›",
            "primary"
            );

        priorityLayout->addWidget(warning);
        priorityLayout->addLayout(priorityTexts, 1);
        priorityLayout->addWidget(alertButton);

        list->addWidget(priorityCard);

        connect(
            addButton, &QPushButton::clicked,
            this, [this] { addIncident(); }
            );

        connect(
            editButton, &QPushButton::clicked,
            this, [this] { editIncident(); }
            );

        connect(
            archiveButton, &QPushButton::clicked,
            this, [this] { archiveIncident(); }
            );

        connect(
            deleteButton, &QPushButton::clicked,
            this, [this] { deleteIncident(); }
            );

        connect(
            statisticsButton, &QPushButton::clicked,
            this, [this] { showStatistics(); }
            );

        connect(
            pdfButton, &QPushButton::clicked,
            this, [this] { exportIncidentsPdf(); }
            );

        connect(
            alertButton, &QPushButton::clicked,
            this, [this] { showUrgentIncident(); }
            );

        connect(
            searchInput, &QLineEdit::textChanged,
            this, [this] { refreshTable(); }
            );

        connect(
            gravityFilter,
            qOverload<int>(
                &QComboBox::currentIndexChanged
                ),
            this,
            [this] { refreshTable(); }
            );

        connect(
            sortInput,
            qOverload<int>(
                &QComboBox::currentIndexChanged
                ),
            this,
            [this] { refreshTable(); }
            );

        connect(
            table,
            &QTableWidget::cellClicked,
            this,
            [this](int row, int) {
                selectTableRow(row);
            }
            );

        brightnessOverlay =
            new BrightnessOverlay(root);

        connect(
            brightnessSlider,
            &QSlider::valueChanged,
            this,
            [this](int value) {
                brightness = value;
                brightnessOverlay->setBrightness(value);

                brightnessLabel->setText(
                    QString("☀ Luminosité : %1 %")
                        .arg(value)
                    );
            }
            );
    }

    Incident *selectedIncident()
    {
        for (Incident &incident : incidents) {
            if (
                incident.id == selectedId &&
                !incident.archived
                ) {
                return &incident;
            }
        }

        return nullptr;
    }

    void clearForm()
    {
        selectedId.clear();
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
        selectedId = incident.id;

        if (typeInput->findText(incident.type) < 0)
            typeInput->addItem(incident.type);

        typeInput->setCurrentText(incident.type);
        gravityInput->setCurrentText(incident.gravity);
        dateInput->setDateTime(incident.date);
        victimsInput->setValue(incident.victims);
        addressInput->setText(incident.address);

        descriptionInput->setPlainText(
            incident.description
            );

        statusInput->setCurrentText(incident.status);
    }

    void selectTableRow(int row)
    {
        const auto *item = table->item(row, 0);

        if (!item)
            return;

        selectedId = item->data(Qt::UserRole).toString();

        if (Incident *incident = selectedIncident())
            fillForm(*incident);
    }

    bool validateForm()
    {
        const QString adresse =
            addressInput->text().trimmed();

        if (adresse.isEmpty()) {
            QMessageBox::warning(
                this,
                "Adresse manquante",
                "Veuillez saisir l'adresse de l'incident."
                );

            addressInput->setFocus();
            return false;
        }

        if (adresse.toUtf8().size() > 150) {
            QMessageBox::warning(
                this,
                "Adresse trop longue",
                "L'adresse dépasse 150 octets. "
                "Raccourcissez-la, notamment si elle contient des accents."
                );

            addressInput->setFocus();
            return false;
        }

        const QString description =
            descriptionInput->toPlainText().trimmed();

        if (description.toUtf8().size() > 500) {
            QMessageBox::warning(
                this,
                "Description trop longue",
                "La description dépasse 500 octets. "
                "Veuillez la raccourcir."
                );

            descriptionInput->setFocus();
            return false;
        }

        if (
            typeInput->currentText().toUtf8().size() > 40 ||
            !dateInput->dateTime().isValid()
            ) {
            QMessageBox::warning(
                this,
                "Données invalides",
                "Vérifiez le type et la date de l'incident."
                );

            return false;
        }

        return true;
    }

    QSqlDatabase database() const
    {
        // Connexion par défaut ouverte par le Singleton.
        return QSqlDatabase::database(
            QSqlDatabase::defaultConnection,
            false
            );
    }

    void showDatabaseError(
        const QString &action,
        const QString &detail
        )
    {
        QMessageBox::critical(
            this,
            "Erreur Oracle",
            action + "\n\n" + detail +
                "\n\nL'opération n'a pas été confirmée. "
                "Le formulaire est conservé."
            );
    }

    bool loadIncidents()
    {
        QSqlDatabase db = database();

        if (!db.isValid() || !db.isOpen()) {
            showDatabaseError(
                "Chargement impossible",
                "La connexion Oracle est fermée. "
                "Relancez FireStation et connectez-vous à smartfire."
                );

            return false;
        }

        QSqlQuery query(db);
        query.setForwardOnly(true);

        if (!query.exec(
                "SELECT id_incident, type, "
                "TO_CHAR(date_heure, 'YYYY-MM-DD HH24:MI:SS'), "
                "adresse, niveau_gravite, nb_victimes, description, statut "
                "FROM INCIDENT "
                "ORDER BY date_heure DESC, id_incident"
                )) {
            showDatabaseError(
                "Chargement impossible",
                query.lastError().text()
                );

            return false;
        }

        std::vector<Incident> loaded;

        while (query.next()) {
            Incident incident;

            incident.id = query.value(0).toString();
            incident.type = query.value(1).toString();

            incident.date = QDateTime::fromString(
                query.value(2).toString(),
                "yyyy-MM-dd HH:mm:ss"
                );

            incident.address = query.value(3).toString();
            incident.gravity = query.value(4).toString();
            incident.victims = query.value(5).toInt();
            incident.description = query.value(6).toString();
            incident.status = query.value(7).toString();

            incident.archived =
                incident.status == "Archivé";

            loaded.push_back(incident);
        }

        if (query.lastError().isValid()) {
            showDatabaseError(
                "Lecture interrompue",
                query.lastError().text()
                );

            return false;
        }

        incidents = std::move(loaded);
        return true;
    }

    bool executeWrite(
        const QString &sql,
        const QVariantList &values,
        const QString &action,
        bool mustExist = false
        )
    {
        QSqlDatabase db = database();

        if (!db.isValid() || !db.isOpen()) {
            showDatabaseError(
                action,
                "La connexion Oracle est fermée."
                );

            return false;
        }

        if (!db.transaction()) {
            showDatabaseError(
                action,
                db.lastError().text()
                );

            return false;
        }

        QSqlQuery query(db);

        if (!query.prepare(sql)) {
            const QString error =
                query.lastError().text();

            query.finish();
            db.rollback();

            showDatabaseError(action, error);
            return false;
        }

        for (const QVariant &value : values)
            query.addBindValue(value);

        if (!query.exec()) {
            const QString error =
                query.lastError().text();

            query.finish();
            db.rollback();

            showDatabaseError(action, error);
            return false;
        }

        const qint64 affected =
            query.numRowsAffected();

        query.finish();

        if (mustExist && affected == 0) {
            db.rollback();

            showDatabaseError(
                action,
                "Cet incident a été supprimé ou archivé ailleurs. "
                "Revenez sur le module pour actualiser la liste."
                );

            return false;
        }

        if (!db.commit()) {
            const QString error =
                db.lastError().text();

            db.rollback();

            showDatabaseError(action, error);
            return false;
        }

        return true;
    }

protected:
    void showEvent(QShowEvent *event) override
    {
        QMainWindow::showEvent(event);

        if (loadIncidents()) {
            clearForm();
            refreshTable();
        }
    }

private:
    void copyFormToIncident(Incident &incident)
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
        if (!validateForm())
            return;

        Incident incident;

        // Identifiant de 15 caractères.
        incident.id = "INC-" + QString::fromLatin1(
                                   QUuid::createUuid()
                                       .toRfc4122()
                                       .toHex()
                                       .left(11)
                                   ).toUpper();

        copyFormToIncident(incident);

        if (!executeWrite(
                "INSERT INTO INCIDENT "
                "(id_incident, type, date_heure, adresse, "
                "niveau_gravite, nb_victimes, description, statut) "
                "VALUES "
                "(?, ?, TO_DATE(?, 'YYYY-MM-DD HH24:MI:SS'), "
                "?, ?, ?, ?, ?)",
                {
                    incident.id,
                    incident.type,
                    incident.date.toString("yyyy-MM-dd HH:mm:ss"),
                    incident.address,
                    incident.gravity,
                    incident.victims,
                    incident.description,
                    incident.status
                },
                "Ajout impossible"
                )) {
            return;
        }

        incidents.push_back(incident);

        clearForm();
        refreshTable();
    }

    void editIncident()
    {
        Incident *incident = selectedIncident();

        if (!incident) {
            QMessageBox::information(
                this,
                "Modifier",
                "Sélectionnez un incident dans le tableau."
                );

            return;
        }

        if (!validateForm())
            return;

        Incident updated = *incident;
        copyFormToIncident(updated);

        if (!executeWrite(
                "UPDATE INCIDENT SET "
                "type = ?, "
                "date_heure = TO_DATE(?, 'YYYY-MM-DD HH24:MI:SS'), "
                "adresse = ?, "
                "niveau_gravite = ?, "
                "nb_victimes = ?, "
                "description = ?, "
                "statut = ? "
                "WHERE id_incident = ? "
                "AND statut <> 'Archivé'",
                {
                    updated.type,
                    updated.date.toString("yyyy-MM-dd HH:mm:ss"),
                    updated.address,
                    updated.gravity,
                    updated.victims,
                    updated.description,
                    updated.status,
                    updated.id
                },
                "Modification impossible",
                true
                )) {
            return;
        }

        *incident = updated;

        clearForm();
        refreshTable();
    }

    void archiveIncident()
    {
        Incident *incident = selectedIncident();

        if (!incident) {
            QMessageBox::information(
                this,
                "Archiver",
                "Sélectionnez un incident dans le tableau."
                );

            return;
        }

        const auto answer = QMessageBox::question(
            this,
            "Confirmer l'archivage",
            QString("Archiver %1 ?")
                .arg(incidentNumber(incident->id)),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
            );

        if (answer != QMessageBox::Yes)
            return;

        if (!executeWrite(
                "UPDATE INCIDENT "
                "SET statut = 'Archivé' "
                "WHERE id_incident = ? "
                "AND statut <> 'Archivé'",
                {incident->id},
                "Archivage impossible",
                true
                )) {
            return;
        }

        incident->status = "Archivé";
        incident->archived = true;

        clearForm();
        refreshTable();
    }

    void deleteIncident()
    {
        Incident *incident = selectedIncident();

        if (!incident) {
            QMessageBox::information(
                this,
                "Supprimer",
                "Sélectionnez un incident dans le tableau."
                );

            return;
        }

        const QString id = incident->id;

        const auto answer = QMessageBox::warning(
            this,
            "Confirmer la suppression",
            QString(
                "Supprimer définitivement %1 ?\n"
                "Ses associations aux agents et véhicules "
                "seront aussi supprimées."
                ).arg(incidentNumber(id)),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
            );

        if (answer != QMessageBox::Yes)
            return;

        if (!executeWrite(
                "DELETE FROM INCIDENT WHERE id_incident = ?",
                {id},
                "Suppression impossible",
                true
                )) {
            return;
        }

        incidents.erase(
            std::remove_if(
                incidents.begin(),
                incidents.end(),
                [&id](const Incident &entry) {
                    return entry.id == id;
                }
                ),
            incidents.end()
            );

        clearForm();
        refreshTable();
    }

    std::vector<const Incident *> filteredIncidents() const
    {
        std::vector<const Incident *> visible;

        const QString search =
            searchInput->text().trimmed();

        for (const Incident &incident : incidents) {
            if (incident.archived)
                continue;

            if (
                gravityFilter->currentIndex() > 0 &&
                incident.gravity != gravityFilter->currentText()
                ) {
                continue;
            }

            if (
                !search.isEmpty() &&
                !incident.type.contains(
                    search, Qt::CaseInsensitive
                    ) &&
                !incident.address.contains(
                    search, Qt::CaseInsensitive
                    ) &&
                !incidentNumber(incident.id).contains(
                    search, Qt::CaseInsensitive
                    )
                ) {
                continue;
            }

            visible.push_back(&incident);
        }

        const int sorting =
            sortInput->currentIndex();

        std::stable_sort(
            visible.begin(),
            visible.end(),
            [sorting](
                const Incident *a,
                const Incident *b
                ) {
                if (sorting == 1)
                    return a->date < b->date;

                if (sorting == 2) {
                    return gravityRank(a->gravity)
                    > gravityRank(b->gravity);
                }

                if (sorting == 3) {
                    return gravityRank(a->gravity)
                    < gravityRank(b->gravity);
                }

                return a->date > b->date;
            }
            );

        return visible;
    }

    void displayIncident(const QString &id)
    {
        selectedId = id;

        if (Incident *entry = selectedIncident())
            fillForm(*entry);

        for (
            int row = 0;
            row < table->rowCount();
            ++row
            ) {
            const auto *item = table->item(row, 0);

            if (
                item &&
                item->data(Qt::UserRole).toString() == id
                ) {
                table->selectRow(row);
                table->scrollToItem(item);
                break;
            }
        }
    }

    void refreshTable()
    {
        const auto visible = filteredIncidents();

        table->setRowCount(
            static_cast<int>(visible.size())
            );

        for (
            int row = 0;
            row < static_cast<int>(visible.size());
            ++row
            ) {
            const Incident &incident =
                *visible[static_cast<std::size_t>(row)];

            const QStringList values = {
                incidentNumber(incident.id),
                incident.type,
                incident.address,
                incident.gravity,
                incident.status
            };

            for (int column = 0; column < 5; ++column) {
                auto *item =
                    new QTableWidgetItem(values.at(column));

                item->setData(
                    Qt::UserRole,
                    incident.id
                    );

                table->setItem(row, column, item);

                if (column == 3) {
                    QColor background("#ffe9c3");
                    QColor foreground("#42383c");

                    if (incident.gravity == "Critique") {
                        background = QColor("#ffd8dc");
                        foreground = QColor("#9d1020");
                    } else if (
                        incident.gravity == "Élevée"
                        ) {
                        background = QColor("#ffe1e4");
                    } else if (
                        incident.gravity == "Faible"
                        ) {
                        background = QColor("#d7f4df");
                        foreground = QColor("#176d37");

                        QFont font = item->font();
                        font.setBold(true);
                        item->setFont(font);
                    }

                    item->setBackground(
                        QBrush(background)
                        );
                    item->setForeground(
                        QBrush(foreground)
                        );
                }
            }

            auto *actions = new QWidget;
            auto *layout = new QHBoxLayout(actions);

            layout->setContentsMargins(2, 2, 2, 2);
            layout->setSpacing(3);

            auto *viewButton =
                new QPushButton("Voir");

            auto *editButton =
                new QPushButton("Modifier");

            auto *archiveButton =
                new QPushButton("Archiver");

            auto *deleteButton =
                new QPushButton("Supprimer");

            viewButton->setObjectName("tableAction");
            editButton->setObjectName("tableAction");
            archiveButton->setObjectName("tableAction");
            deleteButton->setObjectName("tableDelete");

            layout->addWidget(viewButton);
            layout->addWidget(editButton);
            layout->addWidget(archiveButton);
            layout->addWidget(deleteButton);

            table->setCellWidget(row, 5, actions);
            table->setRowHeight(row, 36);

            const QString id = incident.id;

            connect(
                viewButton,
                &QPushButton::clicked,
                this,
                [this, id] {
                    displayIncident(id);
                }
                );

            connect(
                editButton,
                &QPushButton::clicked,
                this,
                [this, id] {
                    displayIncident(id);
                    addressInput->setFocus();
                }
                );

            connect(
                archiveButton,
                &QPushButton::clicked,
                this,
                [this, id] {
                    selectedId = id;
                    archiveIncident();
                }
                );

            connect(
                deleteButton,
                &QPushButton::clicked,
                this,
                [this, id] {
                    selectedId = id;
                    deleteIncident();
                }
                );
        }

        countLabel->setText(
            QString("%1 incident(s)")
                .arg(static_cast<int>(visible.size()))
            );

        std::vector<const Incident *> urgent;

        for (const Incident &incident : incidents) {
            if (
                !incident.archived &&
                incident.status != "Terminé" &&
                calculatePriority(incident) == "Urgente"
                ) {
                urgent.push_back(&incident);
            }
        }

        while (
            QLayoutItem *item =
            urgentListLayout->takeAt(0)
            ) {
            if (item->widget())
                item->widget()->deleteLater();

            delete item;
        }

        for (const Incident *incident : urgent) {
            auto *rowWidget = new QWidget;
            auto *layout =
                new QHBoxLayout(rowWidget);

            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(5);

            auto *details = new QLabel(
                QString(
                    "%1 — %2 — %3 — %4 victime(s)"
                    )
                    .arg(incidentNumber(incident->id))
                    .arg(incident->type)
                    .arg(incident->address)
                    .arg(incident->victims)
                );

            details->setWordWrap(true);
            details->setStyleSheet("font-size:11px;");

            auto *button = new QPushButton("Voir");
            button->setObjectName("tableAction");

            layout->addWidget(details, 1);
            layout->addWidget(button);

            urgentListLayout->addWidget(rowWidget);

            const QString id = incident->id;

            connect(
                button,
                &QPushButton::clicked,
                this,
                [this, id] {
                    displayIncident(id);
                }
                );
        }

        const int count =
            static_cast<int>(urgent.size());

        alertText->setText(
            count > 0
                ? QString(
                      "%1 incident(s) urgent(s) nécessitent "
                      "une intervention immédiate."
                      ).arg(count)
                : "Aucun incident urgent en attente."
            );

        alertButton->setEnabled(count > 0);
        alertButton->setVisible(count > 1);
    }

    void showUrgentIncident()
    {
        for (const Incident &incident : incidents) {
            if (
                incident.archived ||
                incident.status == "Terminé" ||
                calculatePriority(incident) != "Urgente"
                ) {
                continue;
            }

            displayIncident(incident.id);
            return;
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
            ++byZone[incident.address];

            if (incident.gravity == "Critique")
                ++critical;

            if (incident.gravity == "Faible")
                ++low;

            if (incident.status == "En cours")
                ++active;

            if (incident.status == "Terminé")
                ++finished;
        }

        if (!byGravity.contains("Faible"))
            byGravity.insert("Faible", 0);

        QDialog dialog(this);

        dialog.setWindowTitle(
            "Statistiques — Gestion des incidents"
            );

        dialog.setStyleSheet(applicationStyle());

        dialog.setWindowFlags(
            Qt::Dialog | Qt::FramelessWindowHint
            );

        dialog.setMinimumSize(0, 0);

        dialog.setGeometry(
            QRect(
                mapToGlobal(QPoint(0, 0)),
                size()
                )
            );

        auto *root = new QHBoxLayout(&dialog);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);

        auto *statisticsPage = new QWidget;
        statisticsPage->setObjectName("content");

        auto *page =
            new QVBoxLayout(statisticsPage);

        page->setContentsMargins(16, 12, 16, 12);
        page->setSpacing(10);

        scroll->setWidget(statisticsPage);
        root->addWidget(scroll, 1);

        auto *heading = new QLabel(
            "Statistiques — Gestion des incidents"
            );
        heading->setObjectName("statisticsTitle");
        heading->setWordWrap(true);

        page->addWidget(heading);

        auto *buttons = new QHBoxLayout;

        auto *backButton =
            createButton("Retour à la liste");

        auto *exportButton =
            createButton("Exporter en PDF");

        buttons->addStretch();
        buttons->addWidget(backButton);
        buttons->addWidget(exportButton);

        page->addLayout(buttons);

        auto createCounter = [](
                                 int value,
                                 const QString &text,
                                 const QString &color
                                 ) {
            auto *card = new QFrame;
            card->setObjectName("card");
            card->setMinimumHeight(75);

            auto *layout = new QVBoxLayout(card);
            layout->setContentsMargins(12, 9, 12, 9);

            auto *number =
                new QLabel(QString::number(value));

            number->setStyleSheet(
                QString(
                    "font-size:26px;"
                    "font-weight:800;"
                    "color:%1;"
                    ).arg(color)
                );

            auto *label = new QLabel(text);
            label->setWordWrap(true);
            label->setObjectName("subheading");

            layout->addWidget(number);
            layout->addWidget(label);

            return card;
        };

        auto *counters = new QHBoxLayout;
        counters->setSpacing(8);

        counters->addWidget(
            createCounter(
                total,
                "Total des incidents",
                "#5b1722"
                )
            );

        counters->addWidget(
            createCounter(
                critical,
                "Critiques",
                "#c1121f"
                )
            );

        counters->addWidget(
            createCounter(
                low,
                "Faibles",
                "#2e9d57"
                )
            );

        counters->addWidget(
            createCounter(
                active,
                "En cours",
                "#b51c2a"
                )
            );

        counters->addWidget(
            createCounter(
                finished,
                "Terminés",
                "#74121e"
                )
            );

        page->addLayout(counters);

        auto createChart = [](
                               const QString &title,
                               const QMap<QString, int> &values
                               ) {
            auto *card = new QFrame;
            card->setObjectName("card");

            auto *layout = new QVBoxLayout(card);
            layout->setContentsMargins(14, 12, 14, 12);
            layout->setSpacing(8);

            auto *titleLabel = new QLabel(title);
            titleLabel->setObjectName("chartTitle");

            layout->addWidget(titleLabel);

            int maximum = 1;

            for (int value : values)
                maximum = std::max(maximum, value);

            if (values.isEmpty()) {
                layout->addWidget(
                    new QLabel("Aucun incident.")
                    );
            }

            int delay = 140;

            for (
                auto it = values.cbegin();
                it != values.cend();
                ++it
                ) {
                auto *row = new QHBoxLayout;

                auto *label = new QLabel(
                    QString("%1 (%2)")
                        .arg(it.key())
                        .arg(it.value())
                    );

                label->setMinimumWidth(115);
                label->setWordWrap(true);

                auto *bar = new QProgressBar;
                bar->setRange(0, maximum);
                bar->setValue(0);
                bar->setTextVisible(false);
                bar->setProperty(
                    "finalValue",
                    it.value()
                    );

                if (it.key() == "Faible") {
                    label->setStyleSheet(
                        "color:#176d37;font-weight:700;"
                        );

                    bar->setStyleSheet(
                        "QProgressBar {"
                        "background:#dff3e5;"
                        "border:none;"
                        "border-radius:7px;"
                        "min-height:14px;"
                        "max-height:18px;"
                        "}"
                        "QProgressBar::chunk {"
                        "background:#2e9d57;"
                        "border-radius:7px;"
                        "}"
                        );
                }

                row->addWidget(label);
                row->addWidget(bar, 1);
                layout->addLayout(row);

                const int finalValue = it.value();

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
                        animation->setEndValue(finalValue);

                        animation->setEasingCurve(
                            QEasingCurve::OutCubic
                            );

                        animation->start(
                            QAbstractAnimation::DeleteWhenStopped
                            );
                    }
                    );

                delay += 90;
            }

            layout->addStretch();
            return card;
        };

        auto *charts = new QGridLayout;
        charts->setHorizontalSpacing(10);
        charts->setVerticalSpacing(10);

        charts->addWidget(
            createChart(
                "Incidents par type",
                byType
                ),
            0, 0
            );

        charts->addWidget(
            createChart(
                "Incidents par gravité",
                byGravity
                ),
            0, 1
            );

        charts->addWidget(
            createChart(
                "Incidents par zone",
                byZone
                ),
            1, 0
            );

        charts->addWidget(
            createChart(
                "Incidents par statut",
                byStatus
                ),
            1, 1
            );

        page->addLayout(charts, 1);

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
                        "Exporter les statistiques",
                        "statistiques.pdf",
                        "PDF (*.pdf)"
                        );

                if (path.isEmpty())
                    return;

                if (!path.endsWith(
                        ".pdf",
                        Qt::CaseInsensitive
                        )) {
                    path += ".pdf";
                }

                const auto bars =
                    statisticsPage
                        ->findChildren<QProgressBar *>();

                for (QProgressBar *bar : bars) {
                    const auto animations =
                        bar->findChildren<
                            QPropertyAnimation *
                            >();

                    for (
                        QPropertyAnimation *animation :
                        animations
                        ) {
                        animation->stop();
                    }

                    bar->setValue(
                        bar->property("finalValue").toInt()
                        );
                }

                bool exported = false;

                {
                    QPdfWriter pdf(path);
                    pdf.setResolution(150);

                    pdf.setPageSize(
                        QPageSize(QPageSize::A4)
                        );

                    pdf.setPageOrientation(
                        QPageLayout::Landscape
                        );

                    pdf.setPageMargins(
                        QMarginsF(12, 12, 12, 12)
                        );

                    QPainter painter(&pdf);

                    if (painter.isActive()) {
                        const QPixmap capture =
                            statisticsPage->grab();

                        const QSize target =
                            capture.size().scaled(
                                QSize(
                                    pdf.width(),
                                    pdf.height()
                                    ),
                                Qt::KeepAspectRatio
                                );

                        painter.drawPixmap(
                            QRect(
                                (pdf.width() - target.width()) / 2,
                                (pdf.height() - target.height()) / 2,
                                target.width(),
                                target.height()
                                ),
                            capture
                            );

                        exported = true;
                    }
                }

                if (exported) {
                    QMessageBox::information(
                        &dialog,
                        "Export terminé",
                        "PDF enregistré :\n" + path
                        );
                } else {
                    QMessageBox::warning(
                        &dialog,
                        "Export impossible",
                        "Impossible de créer le PDF."
                        );
                }
            }
            );

        auto *overlay =
            new BrightnessOverlay(&dialog);

        overlay->setBrightness(brightness);

        connect(
            brightnessSlider,
            &QSlider::valueChanged,
            overlay,
            [overlay](int value) {
                overlay->setBrightness(value);
            }
            );

        dialog.exec();
    }

    void exportIncidentsPdf()
    {
        QString path =
            QFileDialog::getSaveFileName(
                this,
                "Exporter le rapport de suivi",
                "rapport_suivi_incidents.pdf",
                "PDF (*.pdf)"
                );

        if (path.isEmpty())
            return;

        if (!path.endsWith(
                ".pdf",
                Qt::CaseInsensitive
                )) {
            path += ".pdf";
        }

        auto report = filteredIncidents();

        auto priorityRank = [](
                                const Incident *incident
                                ) {
            const QString priority =
                calculatePriority(*incident);

            if (priority == "Urgente") return 4;
            if (priority == "Haute") return 3;
            if (priority == "Normale") return 2;

            return 1;
        };

        std::stable_sort(
            report.begin(),
            report.end(),
            [priorityRank](
                const Incident *a,
                const Incident *b
                ) {
                const int ar = priorityRank(a);
                const int br = priorityRank(b);

                if (ar != br)
                    return ar > br;

                return a->date > b->date;
            }
            );

        int signaled = 0;
        int inProgress = 0;
        int finished = 0;
        int victims = 0;
        int urgent = 0;

        QMap<QString, int> byType;
        QMap<QString, int> byZone;

        for (const Incident *incident : report) {
            if (incident->status == "Signalé")
                ++signaled;
            else if (incident->status == "En cours")
                ++inProgress;
            else if (incident->status == "Terminé")
                ++finished;

            victims += incident->victims;

            ++byType[incident->type];
            ++byZone[incident->address];

            if (
                incident->status != "Terminé" &&
                calculatePriority(*incident) == "Urgente"
                ) {
                ++urgent;
            }
        }

        QString scope =
            "Incidents non archivés (filtres de la liste)";

        if (!searchInput->text().trimmed().isEmpty()) {
            scope += " | Recherche : " +
                     searchInput
                         ->text()
                         .trimmed()
                         .toHtmlEscaped();
        }

        if (gravityFilter->currentIndex() > 0) {
            scope += " | Gravité : " +
                     gravityFilter
                         ->currentText()
                         .toHtmlEscaped();
        }

        QString html = QString::fromUtf8(R"HTML(
<html>
<head>
<meta charset="utf-8">
<style>
body {
    font-family: Arial;
    color: #302934;
    font-size: 10pt;
}
h1 {
    color: #981a27;
}
h2 {
    color: #5b1722;
    margin-top: 18px;
}
.meta {
    color: #6d6470;
}
table {
    width: 100%;
    border-collapse: collapse;
}
th {
    color: white;
    background: #5b1722;
    padding: 7px;
}
td {
    border: 1px solid #dedede;
    padding: 6px;
}
.summary td {
    text-align: center;
    background: #fbf6f7;
}
.summary strong {
    color: #981a27;
    font-size: 15pt;
}
.urgent {
    background: #fff0f1;
    color: #981a27;
}
.high {
    background: #fff7e4;
}
</style>
</head>
<body>
<h1>Rapport de suivi des incidents</h1>
)HTML");

        html +=
            "<p class='meta'>Généré le " +
            QDateTime::currentDateTime().toString(
                "dd/MM/yyyy à HH:mm"
                ) +
            "<br>Contexte : " +
            scope +
            "</p>";

        html +=
            "<p><b>Suivi opérationnel :</b> "
            "état daté des incidents, "
            "urgences actives et victimes déclarées "
            "pour faciliter le suivi et le partage "
            "des informations.</p>";

        html += QString(
                    "<table class='summary'><tr>"
                    "<td><strong>%1</strong><br>Total sélectionné</td>"
                    "<td><strong>%2</strong><br>Urgences actives</td>"
                    "<td><strong>%3</strong><br>Signalés</td>"
                    "<td><strong>%4</strong><br>En cours</td>"
                    "<td><strong>%5</strong><br>Terminés</td>"
                    "<td><strong>%6</strong><br>Victimes déclarées</td>"
                    "</tr></table>"
                    )
                    .arg(static_cast<int>(report.size()))
                    .arg(urgent)
                    .arg(signaled)
                    .arg(inProgress)
                    .arg(finished)
                    .arg(victims);

        auto appendRow = [&html](
                             const QStringList &cells,
                             const QString &rowClass = QString()
                             ) {
            html += "<tr" + rowClass + ">";

            for (const QString &cell : cells) {
                html +=
                    "<td>" +
                    cell.toHtmlEscaped() +
                    "</td>";
            }

            html += "</tr>";
        };

        html +=
            "<h2>Incidents nécessitant "
            "une intervention immédiate</h2>";

        if (urgent == 0) {
            html +=
                "<p>Aucun incident urgent actif "
                "dans la sélection actuelle.</p>";
        } else {
            html +=
                "<table><tr>"
                "<th>ID</th>"
                "<th>Type</th>"
                "<th>Adresse</th>"
                "<th>Victimes</th>"
                "<th>Statut</th>"
                "</tr>";

            for (const Incident *incident : report) {
                if (
                    incident->status == "Terminé" ||
                    calculatePriority(*incident) != "Urgente"
                    ) {
                    continue;
                }

                appendRow(
                    {
                        incidentNumber(incident->id),
                        incident->type,
                        incident->address,
                        QString::number(incident->victims),
                        incident->status
                    },
                    " class='urgent'"
                    );
            }

            html += "</table>";
        }

        html +=
            "<h2>Détail des incidents classés par priorité</h2>"
            "<table><tr>"
            "<th>ID</th>"
            "<th>Type</th>"
            "<th>Date</th>"
            "<th>Adresse</th>"
            "<th>Gravité</th>"
            "<th>Victimes</th>"
            "<th>Statut</th>"
            "<th>Priorité</th>"
            "</tr>";

        if (report.empty()) {
            html +=
                "<tr><td colspan='8'>"
                "Aucun incident ne correspond "
                "aux filtres sélectionnés."
                "</td></tr>";
        }

        for (const Incident *incident : report) {
            const QString priority =
                calculatePriority(*incident);

            const QString rowClass =
                priority == "Urgente"
                    ? " class='urgent'"
                    : priority == "Haute"
                          ? " class='high'"
                          : "";

            appendRow(
                {
                    incidentNumber(incident->id),
                    incident->type,
                    incident->date.toString(
                        "dd/MM/yyyy HH:mm"
                        ),
                    incident->address,
                    incident->gravity,
                    QString::number(incident->victims),
                    incident->status,
                    priority
                },
                rowClass
                );
        }

        html += "</table>";

        html +=
            "<h2>Répartition par type</h2>"
            "<table>"
            "<tr><th>Type</th><th>Nombre</th></tr>";

        for (
            auto it = byType.cbegin();
            it != byType.cend();
            ++it
            ) {
            appendRow({
                it.key(),
                QString::number(it.value())
            });
        }

        if (byType.isEmpty()) {
            html +=
                "<tr><td colspan='2'>"
                "Aucune donnée"
                "</td></tr>";
        }

        html += "</table>";

        html +=
            "<h2>Répartition par zone</h2>"
            "<table>"
            "<tr><th>Adresse / zone</th><th>Nombre</th></tr>";

        for (
            auto it = byZone.cbegin();
            it != byZone.cend();
            ++it
            ) {
            appendRow({
                it.key(),
                QString::number(it.value())
            });
        }

        if (byZone.isEmpty()) {
            html +=
                "<tr><td colspan='2'>"
                "Aucune donnée"
                "</td></tr>";
        }

        html += "</table></body></html>";

        {
            QPdfWriter pdf(path);

            pdf.setTitle(
                "Rapport de suivi des incidents"
                );
            pdf.setCreator("Smart Fire Station");
            pdf.setResolution(150);

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

        if (
            QFileInfo(path).exists() &&
            QFileInfo(path).size() > 0
            ) {
            QMessageBox::information(
                this,
                "Export terminé",
                "Rapport PDF enregistré :\n" + path
                );
        } else {
            QMessageBox::warning(
                this,
                "Export impossible",
                "Impossible de créer le rapport PDF."
                );
        }
    }
};

} // namespace incidents

QMainWindow *creerModuleIncidents()
{
    auto *fenetre = new incidents::MainWindow;

    fenetre->setWindowFlags(Qt::Widget);
    fenetre->setMinimumSize(0, 0);

    return fenetre;
}