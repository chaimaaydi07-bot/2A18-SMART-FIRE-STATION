#include <QtWidgets>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPageLayout>
#include <QPageSize>
#include <QTextDocument>
#include <algorithm>
#include <vector>
#include <cmath>
#include "incidents.h"

namespace incidents {

static constexpr double PI = 3.14159265358979323846;

// ============================================================
// DONNEES
// ============================================================

struct Incident {
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

static QPixmap loadLogo()
{
    const QStringList paths = {
        ":/images/logo.jpeg",
        ":/logo.jpeg",
        ":/images/logo.png",
        ":/logo.png",
        QCoreApplication::applicationDirPath() + "/logo.jpeg",
        QCoreApplication::applicationDirPath() + "/logo.png",
        "logo.jpeg",
        "logo.png"
    };

    for (const QString &path : paths) {
        QPixmap image(path);
        if (!image.isNull())
            return image;
    }

    return QPixmap();
}

static QString navigationText(const QString &icon, QString name)
{
    if (name == "Gestion des bâtiments à risque")
        name = "Gestion des bâtiments\nà risque";

    if (name == "Gestion des formations")
        name = "Gestion des\nformations";

    return icon + "   " + name;
}

// ============================================================
// LOGO : ZOOM DOUX + CERCLE LUMINEUX
// ============================================================

class AnimatedLogo : public QWidget {
public:
    explicit AnimatedLogo(
        const QPixmap &image,
        QWidget *parent = nullptr
        )
        : QWidget(parent), logo(image)
    {
        setFixedSize(164, 164);

        auto *animation = new QVariantAnimation(this);
        animation->setStartValue(0.0);
        animation->setEndValue(1.0);
        animation->setDuration(7500);
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
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);

        const QPointF center(width() / 2.0, height() / 2.0);
        const QRectF ring(5, 5, width() - 10, height() - 10);
        const double angle = phase * 360.0;

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 144, 85, 35), 10));
        painter.drawEllipse(ring);

        QConicalGradient gradient(center, -angle);
        gradient.setColorAt(0.00, QColor("#ffe4a3"));
        gradient.setColorAt(0.20, QColor("#ff9b55"));
        gradient.setColorAt(0.48, QColor("#b51c2a"));
        gradient.setColorAt(0.76, QColor("#ff6b8c"));
        gradient.setColorAt(1.00, QColor("#ffe4a3"));

        painter.setPen(QPen(QBrush(gradient), 3));
        painter.drawEllipse(ring);

        painter.save();
        painter.translate(center);
        painter.rotate(angle);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#fff0c4"));
        painter.drawEllipse(QPointF(0, -77), 3.5, 3.5);
        painter.restore();

        const QRectF card(
            center.x() - 56,
            center.y() - 56,
            112,
            112
            );

        painter.setBrush(Qt::white);
        painter.setPen(QPen(QColor("#f0c5ca"), 1.5));
        painter.drawRoundedRect(card, 18, 18);

        if (logo.isNull()) {
            QFont fallbackFont("Segoe UI");
            fallbackFont.setPixelSize(12);
            fallbackFont.setBold(true);
            painter.setFont(fallbackFont);
            painter.setPen(QColor("#981a27"));
            painter.drawText(
                card,
                Qt::AlignCenter,
                "SMART FIRE\nSTATION"
                );
            return;
        }

        const double zoom =
            0.985 + 0.030 * std::cos(4.0 * PI * phase);

        const double offsetY = std::sin(2.0 * PI * phase);

        QSizeF imageSize(logo.width(), logo.height());
        imageSize.scale(QSizeF(104, 104), Qt::KeepAspectRatio);
        imageSize *= zoom;

        const QRectF destination(
            center.x() - imageSize.width() / 2.0,
            center.y() - imageSize.height() / 2.0 + offsetY,
            imageSize.width(),
            imageSize.height()
            );

        // L'image ne tourne pas.
        painter.drawPixmap(
            destination,
            logo,
            QRectF(0, 0, logo.width(), logo.height())
            );
    }

private:
    QPixmap logo;
    double phase = 0.0;
};

// ============================================================
// TITRES : ECRITURE ET LUMIERE EN BOUCLE
// ============================================================

class AnimatedTitle : public QLabel {
public:
    explicit AnimatedTitle(
        const QString &titleText,
        QWidget *parent = nullptr
        )
        : QLabel(titleText, parent)
    {
        QFont titleFont("Segoe UI");
        titleFont.setPixelSize(23);
        titleFont.setBold(true);
        setFont(titleFont);

        setMinimumHeight(48);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        reserveTextSpace();

        auto *timer = new QTimer(this);
        timer->setInterval(55);

        connect(timer, &QTimer::timeout, this, [this] {
            const int total = static_cast<int>(this->text().size());

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
            48
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
            (height() - metrics.height()) / 2.0 + metrics.ascent();

        QPainterPath path;
        path.addText(QPointF(8, baseline), font(), visible);

        const double pulse =
            0.5 + 0.5 * std::sin(2.0 * PI * shimmer);

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(
            QColor(255, 157, 73, static_cast<int>(30 + 30 * pulse)),
            9
            ));
        painter.drawPath(path);

        painter.setPen(QPen(
            QColor(213, 51, 65, static_cast<int>(45 + 35 * pulse)),
            4
            ));
        painter.drawPath(path);

        const double lightPosition =
            -60.0 + (width() + 120.0) * shimmer;

        QLinearGradient light(
            lightPosition - 55, 0,
            lightPosition + 55, 0
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

            painter.setPen(QPen(QColor("#ffaf5c"), 2.5));
            painter.drawLine(
                QPointF(cursorX, baseline - metrics.ascent() * 0.85),
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

// ============================================================
// BOUTONS D'ACTION ANIMES
// ============================================================

class AnimatedButton : public QPushButton {
public:
    explicit AnimatedButton(
        const QString &buttonText,
        QWidget *parent = nullptr
        )
        : QPushButton(buttonText, parent)
    {
        setCursor(Qt::PointingHandCursor);
        setMinimumHeight(46);

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

        painter.translate(width() / 2.0, height() / 2.0);
        painter.scale(zoom, zoom);
        painter.translate(-width() / 2.0, -height() / 2.0);

        option.rect = rect().adjusted(3, 3, -3, -3);

        const QString buttonText = option.text;
        const QIcon buttonIcon = option.icon;

        option.text.clear();
        option.icon = QIcon();

        painter.drawControl(QStyle::CE_PushButton, option);

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

            const double lightX =
                -60.0 + (width() + 120.0) * phase;

            QLinearGradient shine(
                lightX - 35, 0,
                lightX + 35, height()
                );

            shine.setColorAt(0.00, QColor(255, 231, 185, 0));
            shine.setColorAt(0.40, QColor(255, 231, 185, 0));
            shine.setColorAt(0.50, QColor(255, 231, 185, 120));
            shine.setColorAt(0.60, QColor(255, 231, 185, 0));
            shine.setColorAt(1.00, QColor(255, 231, 185, 0));

            painter.fillRect(border, QBrush(shine));
            painter.restore();
        }

        option.text = buttonText;
        option.icon = buttonIcon;
        painter.drawControl(QStyle::CE_PushButtonLabel, option);
    }

private:
    double phase = 0.0;
};

// ============================================================
// LUMINOSITE DE L'INTERFACE
// ============================================================

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
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (object == parentWidget() &&
            (event->type() == QEvent::Resize ||
             event->type() == QEvent::Show)) {
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

// ============================================================
// STYLE
// ============================================================

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

        QFrame#sidebar {
            background: qlineargradient(
                x1:0, y1:0, x2:0, y2:1,
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

        QFrame#brightnessCard {
            background: white;
            border: 1px solid #ebc5ca;
            border-radius: 10px;
        }

        QLabel#heading {
            font-size: 34px;
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
            padding: 7px 12px;
        }

        QLabel#alertTitle {
            color: #981a27;
            font-size: 16px;
            font-weight: 800;
        }

        QLabel#chartTitle {
            font-size: 15px;
            font-weight: 800;
        }

        QPushButton#nav {
            background: transparent;
            color: white;
            border: 1px solid transparent;
            text-align: left;
            border-radius: 10px;
            padding: 12px 13px;
            font-size: 13px;
        }

        QPushButton#nav:hover {
            background: rgba(255,255,255,35);
            border: 1px solid #cf7780;
        }

        QPushButton#navActive {
            background: white;
            color: #981a27;
            border: 2px solid #f1bec4;
            border-radius: 11px;
            text-align: left;
            padding: 12px 13px;
            font-size: 14px;
            font-weight: 800;
        }

        QLineEdit, QComboBox, QDateTimeEdit, QSpinBox, QTextEdit {
            background: #fbfbfc;
            color: #302934;
            border: 2px solid #dfe2e7;
            border-radius: 9px;
            padding: 8px;
            min-height: 23px;
            font-size: 12px;
        }

        QLineEdit:focus, QComboBox:focus, QDateTimeEdit:focus,
        QSpinBox:focus, QTextEdit:focus {
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
            border-radius: 10px;
            padding: 10px 16px;
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
            font-size: 12px;
        }

        QHeaderView::section {
            color: white;
            background: #5b1722;
            border: none;
            border-right: 1px solid #762834;
            padding: 12px 5px;
            font-size: 11px;
            font-weight: 700;
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

// ============================================================
// FENETRE PRINCIPALE
// ============================================================

class MainWindow : public QMainWindow {
public:
    MainWindow()
    {
        setWindowTitle("Smart Fire Station — Gestion des incidents");
        resize(1460, 850);
        setMinimumSize(1100, 700);
        setStyleSheet(applicationStyle());

        createInterface();

        const QDateTime now = QDateTime::currentDateTime();

        incidents = {
            {
                1, "Incendie", now.addSecs(-7200), "Tunis",
                "Critique", 5, "Incendie dans un bâtiment",
                "En cours", false
            },
            {
                2, "Accident", now.addDays(-1), "Ariana",
                "Élevée", 2, "Accident de la route",
                "Signalé", false
            },
            {
                3, "Fuite de gaz", now.addDays(-2), "Ben Arous",
                "Moyenne", 0, "Fuite de gaz signalée",
                "Terminé", false
            },
            {
                4, "Sauvetage", now.addDays(-3), "La Marsa",
                "Élevée", 1, "Assistance à une personne",
                "En cours", false
            },
            {
                5, "Autre", now.addDays(-4), "Sfax",
                "Faible", 0, "Incident mineur sans victime",
                "Signalé", false
            }
        };

        refreshTable();
    }

private:
    std::vector<Incident> incidents;

    int nextId = 6;
    int selectedId = -1;
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
    QLabel *alertDetails = nullptr;
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
        layout->setSpacing(6);

        auto *label = new QLabel(title);
        label->setObjectName("fieldTitle");

        layout->addWidget(label);
        layout->addWidget(input);
        return layout;
    }

    // Menu lateral : boutons normaux, sans animation.
    QFrame *createSidebar(QWidget *owner, bool statistics)
    {
        auto *sidebar = new QFrame;
        sidebar->setObjectName("sidebar");
        sidebar->setFixedWidth(230);

        auto *layout = new QVBoxLayout(sidebar);
        layout->setContentsMargins(17, 20, 17, 20);
        layout->setSpacing(6);

        layout->addWidget(
            new AnimatedLogo(loadLogo()),
            0,
            Qt::AlignHCenter
            );

        auto *brand = new QLabel("SMART FIRE STATION");
        brand->setAlignment(Qt::AlignCenter);
        brand->setStyleSheet(
            "color:white;font-size:12px;font-weight:800;"
            );

        layout->addWidget(brand);
        layout->addSpacing(8);

        const QList<QPair<QString, QString>> entries = {
            {"⚠", "Incidents"},
            {"▣", "Gestion des bâtiments à risque"},
            {"♟", "Personnel"},
            {"▰", "Véhicules"},
            {"⚙", "Équipements"},
            {"▥", "Rapports"},
            {"✦", "Gestion des formations"}
        };

        for (const auto &entry : entries) {
            const QString name = entry.second;

            // QPushButton classique : pas de zoom ni de reflet.
            auto *button = new QPushButton(
                navigationText(entry.first, name)
                );

            button->setObjectName(
                name == "Incidents" ? "navActive" : "nav"
                );
            button->setCursor(Qt::PointingHandCursor);
            button->setMinimumHeight(
                name.startsWith("Gestion des") ? 66 : 49
                );

            layout->addWidget(button);

            if (name == "Incidents") {
                if (statistics) {
                    connect(
                        button,
                        &QPushButton::clicked,
                        owner,
                        [owner] {
                            if (auto *dialog =
                                qobject_cast<QDialog *>(owner)) {
                                dialog->accept();
                            }
                        }
                        );
                }
            } else {
                connect(
                    button,
                    &QPushButton::clicked,
                    owner,
                    [owner, name] {
                        QMessageBox::information(
                            owner,
                            name,
                            "Ce module sera intégré prochainement."
                            );
                    }
                    );
            }
        }

        layout->addStretch();

        auto *footer = new QLabel("PROTECTION CIVILE");
        footer->setAlignment(Qt::AlignCenter);
        footer->setStyleSheet(
            "color:#ffd9d6;font-size:11px;font-weight:700;"
            );

        layout->addWidget(footer);
        return sidebar;
    }

    void createInterface()
    {
        auto *root = new QWidget;
        root->setObjectName("root");
        setCentralWidget(root);

        auto *mainLayout = new QHBoxLayout(root);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // mainLayout->addWidget(createSidebar(this, false));

        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        mainLayout->addWidget(scroll, 1);

        auto *content = new QWidget;
        content->setObjectName("content");
        scroll->setWidget(content);

        auto *page = new QVBoxLayout(content);
        page->setContentsMargins(28, 26, 28, 26);
        page->setSpacing(20);

        auto *header = new QHBoxLayout;
        auto *titles = new QVBoxLayout;

        auto *heading = new QLabel("Gestion des incidents");
        heading->setObjectName("heading");

        auto *subtitle = new QLabel(
            "Créer, consulter et suivre les incidents en temps réel"
            );
        subtitle->setObjectName("subheading");
        subtitle->setWordWrap(true);

        auto *line = new QFrame;
        line->setFixedSize(82, 4);
        line->setStyleSheet(
            "background:#c51f2e;border:none;border-radius:2px;"
            );

        titles->addWidget(heading);
        titles->addWidget(subtitle);
        titles->addSpacing(5);
        titles->addWidget(line);

        header->addLayout(titles, 1);

        // Luminosite : petit cadre et ligne courte.
        auto *brightnessCard = new QFrame;
        brightnessCard->setObjectName("brightnessCard");
        brightnessCard->setFixedWidth(150);

        auto *brightnessLayout = new QVBoxLayout(brightnessCard);
        brightnessLayout->setContentsMargins(10, 8, 10, 8);
        brightnessLayout->setSpacing(6);

        brightnessLabel = new QLabel("☀ Luminosité : 100 %");
        brightnessLabel->setStyleSheet(
            "font-size:11px;font-weight:600;"
            );

        brightnessSlider = new QSlider(Qt::Horizontal);
        brightnessSlider->setRange(60, 100);
        brightnessSlider->setValue(100);
        brightnessSlider->setFixedWidth(85);

        brightnessLayout->addWidget(brightnessLabel);
        brightnessLayout->addWidget(
            brightnessSlider, 0, Qt::AlignHCenter
            );

        header->addWidget(brightnessCard, 0, Qt::AlignTop);
        page->addLayout(header);

        auto *columns = new QHBoxLayout;
        columns->setSpacing(18);
        page->addLayout(columns, 1);

        // Formulaire.
        auto *formCard = createCard();
        columns->addWidget(formCard, 46);

        auto *form = new QVBoxLayout(formCard);
        form->setContentsMargins(23, 23, 23, 23);
        form->setSpacing(14);

        form->addWidget(new AnimatedTitle("Nouvel incident"));

        auto *formDescription = new QLabel(
            "Saisir les informations de l'incident"
            );
        formDescription->setObjectName("subheading");
        form->addWidget(formDescription);

        typeInput = new QComboBox;
        typeInput->addItems({
            "Incendie", "Accident", "Fuite de gaz",
            "Sauvetage", "Autre"
        });

        gravityInput = new QComboBox;
        gravityInput->addItems({
            "Faible", "Moyenne", "Élevée", "Critique"
        });

        dateInput = new QDateTimeEdit(QDateTime::currentDateTime());
        dateInput->setDisplayFormat("dd/MM/yyyy HH:mm");
        dateInput->setCalendarPopup(true);

        victimsInput = new QSpinBox;
        victimsInput->setRange(0, 999);

        addressInput = new QLineEdit;
        addressInput->setPlaceholderText("Ex. Tunis, Centre-ville");

        descriptionInput = new QTextEdit;
        descriptionInput->setFixedHeight(90);
        descriptionInput->setPlaceholderText(
            "Décrivez brièvement la situation et les risques observés..."
            );

        statusInput = new QComboBox;
        statusInput->addItems({"Signalé", "En cours", "Terminé"});

        auto *grid = new QGridLayout;
        grid->setHorizontalSpacing(14);
        grid->setVerticalSpacing(14);

        grid->addLayout(
            createField("Type d'incident", typeInput), 0, 0
            );
        grid->addLayout(
            createField("Niveau de gravité", gravityInput), 0, 1
            );
        grid->addLayout(
            createField("Date et heure", dateInput), 1, 0
            );
        grid->addLayout(
            createField("Nombre de victimes", victimsInput), 1, 1
            );
        grid->addLayout(
            createField("Adresse", addressInput), 2, 0, 1, 2
            );
        grid->addLayout(
            createField("Description", descriptionInput), 3, 0, 1, 2
            );
        grid->addLayout(
            createField("Statut", statusInput), 4, 0, 1, 2
            );

        form->addLayout(grid);
        form->addStretch();

        auto *formButtons = new QGridLayout;
        formButtons->setSpacing(9);

        auto *addButton = createButton("＋ Ajouter", "primary");
        auto *editButton = createButton("✎ Modifier");
        auto *archiveButton = createButton("▣ Archiver", "archive");
        auto *deleteButton = createButton("✕ Supprimer", "danger");

        formButtons->addWidget(addButton, 0, 0);
        formButtons->addWidget(editButton, 0, 1);
        formButtons->addWidget(archiveButton, 1, 0);
        formButtons->addWidget(deleteButton, 1, 1);

        form->addLayout(formButtons);

        // Liste.
        auto *listCard = createCard();
        columns->addWidget(listCard, 54);

        auto *list = new QVBoxLayout(listCard);
        list->setContentsMargins(23, 23, 23, 23);
        list->setSpacing(14);

        list->addWidget(new AnimatedTitle("Liste des incidents"));

        auto *countRow = new QHBoxLayout;
        countRow->addStretch();

        countLabel = new QLabel;
        countLabel->setObjectName("count");
        countRow->addWidget(countLabel);

        list->addLayout(countRow);

        searchInput = new QLineEdit;
        searchInput->setPlaceholderText(
            "⌕ Rechercher par type, adresse ou ID..."
            );

        gravityFilter = new QComboBox;
        gravityFilter->addItems({
            "Toutes gravités", "Faible",
            "Moyenne", "Élevée", "Critique"
        });

        auto *searchRow = new QHBoxLayout;
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
            "ID", "TYPE", "ADRESSE", "GRAVITÉ", "STATUT", "ACTION"
        });

        table->horizontalHeader()->setSectionResizeMode(
            QHeaderView::Stretch
            );
        table->horizontalHeader()->setSectionResizeMode(
            0, QHeaderView::ResizeToContents
            );
        table->horizontalHeader()->setSectionResizeMode(
            5, QHeaderView::ResizeToContents
            );

        table->verticalHeader()->hide();
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setAlternatingRowColors(true);
        table->setMinimumHeight(280);

        list->addWidget(table, 1);

        auto *priorityCard = new QFrame;
        priorityCard->setObjectName("priorityCard");

        auto *priorityLayout = new QHBoxLayout(priorityCard);
        priorityLayout->setContentsMargins(18, 15, 18, 15);

        auto *warning = new QLabel("⚠");
        warning->setStyleSheet(
            "color:#c1121f;font-size:32px;font-weight:800;"
            );

        auto *priorityTexts = new QVBoxLayout;

        auto *priorityTitle = new QLabel("Alerte de priorité");
        priorityTitle->setObjectName("alertTitle");

        alertText = new QLabel;
        alertText->setWordWrap(true);

        alertDetails = new QLabel;
        alertDetails->setWordWrap(true);

        priorityTexts->addWidget(priorityTitle);
        priorityTexts->addWidget(alertText);
        priorityTexts->addWidget(alertDetails);

        alertButton = createButton("Voir les détails ›", "primary");

        priorityLayout->addWidget(warning);
        priorityLayout->addLayout(priorityTexts, 1);
        priorityLayout->addWidget(alertButton);

        list->addWidget(priorityCard);

        auto *bottom = new QHBoxLayout;
        auto *statisticsButton = createButton("▥ Statistiques");
        auto *pdfButton = createButton("Exporter en PDF");

        bottom->addStretch();
        bottom->addWidget(statisticsButton);
        bottom->addWidget(pdfButton);

        list->addLayout(bottom);

        connect(addButton, &QPushButton::clicked,
                this, [this] { addIncident(); });

        connect(editButton, &QPushButton::clicked,
                this, [this] { editIncident(); });

        connect(archiveButton, &QPushButton::clicked,
                this, [this] { archiveIncident(); });

        connect(deleteButton, &QPushButton::clicked,
                this, [this] { deleteIncident(); });

        connect(statisticsButton, &QPushButton::clicked,
                this, [this] { showStatistics(); });

        connect(pdfButton, &QPushButton::clicked,
                this, [this] { exportIncidentsPdf(); });

        connect(alertButton, &QPushButton::clicked,
                this, [this] { showUrgentIncident(); });

        connect(searchInput, &QLineEdit::textChanged,
                this, [this] { refreshTable(); });

        connect(
            gravityFilter,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this] { refreshTable(); }
            );

        connect(
            sortInput,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this] { refreshTable(); }
            );

        connect(
            table,
            &QTableWidget::cellClicked,
            this,
            [this](int row, int) { selectTableRow(row); }
            );

        brightnessOverlay = new BrightnessOverlay(root);

        connect(
            brightnessSlider,
            &QSlider::valueChanged,
            this,
            [this](int value) {
                brightness = value;
                brightnessOverlay->setBrightness(value);
                brightnessLabel->setText(
                    QString("☀ Luminosité : %1 %").arg(value)
                    );
            }
            );
    }

    // ========================================================
    // FORMULAIRE ET ACTIONS
    // ========================================================

    Incident *selectedIncident()
    {
        for (Incident &incident : incidents) {
            if (incident.id == selectedId && !incident.archived)
                return &incident;
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

    void fillForm(const Incident &incident)
    {
        selectedId = incident.id;

        typeInput->setCurrentText(incident.type);
        gravityInput->setCurrentText(incident.gravity);
        dateInput->setDateTime(incident.date);
        victimsInput->setValue(incident.victims);
        addressInput->setText(incident.address);
        descriptionInput->setPlainText(incident.description);
        statusInput->setCurrentText(incident.status);
    }

    void selectTableRow(int row)
    {
        const QTableWidgetItem *item = table->item(row, 0);
        if (!item)
            return;

        selectedId = item->data(Qt::UserRole).toInt();

        if (Incident *incident = selectedIncident())
            fillForm(*incident);
    }

    bool validateForm()
    {
        if (addressInput->text().trimmed().isEmpty()) {
            QMessageBox::warning(
                this,
                "Adresse manquante",
                "Veuillez saisir l'adresse de l'incident."
                );
            addressInput->setFocus();
            return false;
        }
        return true;
    }

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
        incident.id = nextId++;
        copyFormToIncident(incident);

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

        copyFormToIncident(*incident);
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
            QString("Archiver %1 ?").arg(incidentNumber(incident->id)),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
            );

        if (answer != QMessageBox::Yes)
            return;

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

        const int id = incident->id;

        const auto answer = QMessageBox::warning(
            this,
            "Confirmer la suppression",
            QString(
                "Supprimer définitivement %1 ?\n"
                "Cette action est irréversible."
                ).arg(incidentNumber(id)),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
            );

        if (answer != QMessageBox::Yes)
            return;

        incidents.erase(
            std::remove_if(
                incidents.begin(),
                incidents.end(),
                [id](const Incident &entry) {
                    return entry.id == id;
                }
                ),
            incidents.end()
            );

        clearForm();
        refreshTable();
    }

    // ========================================================
    // TABLEAU : BOUTONS SANS ANIMATION
    // ========================================================

    void refreshTable()
    {
        std::vector<const Incident *> visible;
        const QString search = searchInput->text().trimmed();

        for (const Incident &incident : incidents) {
            if (incident.archived)
                continue;

            if (gravityFilter->currentIndex() > 0 &&
                incident.gravity != gravityFilter->currentText()) {
                continue;
            }

            if (!search.isEmpty() &&
                !incident.type.contains(search, Qt::CaseInsensitive) &&
                !incident.address.contains(search, Qt::CaseInsensitive) &&
                !incidentNumber(incident.id).contains(
                    search, Qt::CaseInsensitive
                    )) {
                continue;
            }

            visible.push_back(&incident);
        }

        const int sorting = sortInput->currentIndex();

        std::stable_sort(
            visible.begin(),
            visible.end(),
            [sorting](const Incident *a, const Incident *b) {
                if (sorting == 1)
                    return a->date < b->date;

                if (sorting == 2)
                    return gravityRank(a->gravity) >
                           gravityRank(b->gravity);

                if (sorting == 3)
                    return gravityRank(a->gravity) <
                           gravityRank(b->gravity);

                return a->date > b->date;
            }
            );

        table->setRowCount(static_cast<int>(visible.size()));

        for (int row = 0;
             row < static_cast<int>(visible.size());
             ++row) {
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
                auto *item = new QTableWidgetItem(values.at(column));
                item->setData(Qt::UserRole, incident.id);
                table->setItem(row, column, item);

                if (column == 3) {
                    QColor background("#ffe9c3");
                    QColor foreground("#42383c");

                    if (incident.gravity == "Critique") {
                        background = QColor("#ffd8dc");
                        foreground = QColor("#9d1020");
                    } else if (incident.gravity == "Élevée") {
                        background = QColor("#ffe1e4");
                    } else if (incident.gravity == "Faible") {
                        background = QColor("#d7f4df");
                        foreground = QColor("#176d37");

                        QFont itemFont = item->font();
                        itemFont.setBold(true);
                        item->setFont(itemFont);
                    }

                    item->setBackground(QBrush(background));
                    item->setForeground(QBrush(foreground));
                }
            }

            auto *actions = new QWidget;
            auto *actionLayout = new QHBoxLayout(actions);
            actionLayout->setContentsMargins(3, 3, 3, 3);
            actionLayout->setSpacing(4);

            auto *viewButton = new QPushButton("Voir");
            auto *archiveButton = new QPushButton("Archiver");
            auto *deleteButton = new QPushButton("Supprimer");

            viewButton->setObjectName("tableAction");
            archiveButton->setObjectName("tableAction");
            deleteButton->setObjectName("tableDelete");

            actionLayout->addWidget(viewButton);
            actionLayout->addWidget(archiveButton);
            actionLayout->addWidget(deleteButton);

            table->setCellWidget(row, 5, actions);
            table->setRowHeight(row, 48);

            const int id = incident.id;

            connect(
                viewButton,
                &QPushButton::clicked,
                this,
                [this, id] {
                    selectedId = id;
                    if (Incident *entry = selectedIncident())
                        fillForm(*entry);
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
            QString("%1 incident(s)").arg(
                static_cast<int>(visible.size())
                )
            );

        int urgentCount = 0;
        QString firstUrgent;

        for (const Incident &incident : incidents) {
            if (!incident.archived &&
                incident.status != "Terminé" &&
                calculatePriority(incident) == "Urgente") {
                ++urgentCount;

                if (firstUrgent.isEmpty()) {
                    firstUrgent = QString("%1 — %2 — %3")
                                      .arg(incidentNumber(incident.id))
                                      .arg(incident.type)
                                      .arg(incident.address);
                }
            }
        }

        alertText->setText(
            urgentCount > 0
                ? QString(
                      "%1 incident(s) urgent(s) nécessitent "
                      "une intervention immédiate."
                      ).arg(urgentCount)
                : "Aucun incident urgent en attente."
            );

        alertDetails->setText(firstUrgent);
        alertButton->setEnabled(urgentCount > 0);
    }

    void showUrgentIncident()
    {
        for (const Incident &incident : incidents) {
            if (incident.archived ||
                incident.status == "Terminé" ||
                calculatePriority(incident) != "Urgente") {
                continue;
            }

            fillForm(incident);

            for (int row = 0; row < table->rowCount(); ++row) {
                const auto *item = table->item(row, 0);

                if (item &&
                    item->data(Qt::UserRole).toInt() == incident.id) {
                    table->selectRow(row);
                    table->scrollToItem(item);
                    break;
                }
            }

            return;
        }
    }

    // ========================================================
    // STATISTIQUES
    // ========================================================

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

            if (incident.gravity == "Critique") ++critical;
            if (incident.gravity == "Faible") ++low;
            if (incident.status == "En cours") ++active;
            if (incident.status == "Terminé") ++finished;
        }

        if (!byGravity.contains("Faible"))
            byGravity.insert("Faible", 0);

        QDialog dialog(this);
        dialog.setWindowTitle("Statistiques — Gestion des incidents");
        dialog.resize(1280, 760);
        dialog.setMinimumSize(1000, 650);
        dialog.setStyleSheet(applicationStyle());

        auto *root = new QHBoxLayout(&dialog);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        // Integration : la fenetre des statistiques s'affiche exactement a la place
        // de la page Incidents, a droite du menu de l'application (sans cadre).
        // root->addWidget(createSidebar(&dialog, true));
        dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        dialog.setMinimumSize(0, 0);
        dialog.setGeometry(QRect(mapToGlobal(QPoint(0, 0)), size()));

        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);

        auto *statisticsPage = new QWidget;
        statisticsPage->setObjectName("content");

        auto *page = new QVBoxLayout(statisticsPage);
        page->setContentsMargins(28, 22, 28, 24);
        page->setSpacing(14);

        scroll->setWidget(statisticsPage);
        root->addWidget(scroll, 1);

        auto *heading = new QLabel(
            "Statistiques — Gestion des incidents"
            );
        heading->setObjectName("statisticsTitle");
        heading->setWordWrap(true);
        page->addWidget(heading);

        auto *buttons = new QHBoxLayout;

        auto *backButton = createButton("Retour à la liste");
        auto *exportButton = createButton("Exporter en PDF");

        buttons->addStretch();
        buttons->addWidget(backButton);
        buttons->addWidget(exportButton);

        page->addLayout(buttons);

        auto createCounter = [](
                                 int value,
                                 const QString &labelText,
                                 const QString &color
                                 ) {
            auto *card = new QFrame;
            card->setObjectName("card");
            card->setMinimumHeight(85);

            auto *layout = new QVBoxLayout(card);
            layout->setContentsMargins(16, 12, 16, 12);

            auto *number = new QLabel(QString::number(value));
            number->setStyleSheet(
                QString(
                    "font-size:26px;font-weight:800;color:%1;"
                    ).arg(color)
                );

            auto *label = new QLabel(labelText);
            label->setWordWrap(true);
            label->setObjectName("subheading");

            layout->addWidget(number);
            layout->addWidget(label);

            return card;
        };

        auto *counters = new QHBoxLayout;
        counters->setSpacing(12);

        counters->addWidget(
            createCounter(total, "Total des incidents", "#5b1722")
            );
        counters->addWidget(
            createCounter(critical, "Critiques", "#c1121f")
            );
        counters->addWidget(
            createCounter(low, "Faibles", "#2e9d57")
            );
        counters->addWidget(
            createCounter(active, "En cours", "#b51c2a")
            );
        counters->addWidget(
            createCounter(finished, "Terminés", "#74121e")
            );

        page->addLayout(counters);

        auto createChart = [](
                               const QString &title,
                               const QMap<QString, int> &values
                               ) {
            auto *card = new QFrame;
            card->setObjectName("card");

            auto *layout = new QVBoxLayout(card);
            layout->setContentsMargins(18, 15, 18, 15);
            layout->setSpacing(10);

            auto *titleLabel = new QLabel(title);
            titleLabel->setObjectName("chartTitle");
            layout->addWidget(titleLabel);

            int maximum = 1;

            for (int value : values)
                maximum = std::max(maximum, value);

            if (values.isEmpty())
                layout->addWidget(new QLabel("Aucun incident."));

            int delay = 140;

            for (auto it = values.cbegin(); it != values.cend(); ++it) {
                auto *row = new QHBoxLayout;

                auto *label = new QLabel(
                    QString("%1 (%2)").arg(it.key()).arg(it.value())
                    );
                label->setMinimumWidth(135);
                label->setWordWrap(true);

                auto *bar = new QProgressBar;
                bar->setRange(0, maximum);
                bar->setValue(0);
                bar->setTextVisible(false);

                if (it.key() == "Faible") {
                    label->setStyleSheet(
                        "color:#176d37;font-weight:700;"
                        );

                    bar->setStyleSheet(
                        "QProgressBar {"
                        "background:#dff3e5;border:none;"
                        "border-radius:7px;min-height:14px;"
                        "max-height:18px;"
                        "}"
                        "QProgressBar::chunk {"
                        "background:#2e9d57;border-radius:7px;"
                        "}"
                        );
                }

                row->addWidget(label);
                row->addWidget(bar, 1);
                layout->addLayout(row);

                const int finalValue = it.value();

                QTimer::singleShot(delay, bar, [bar, finalValue] {
                    auto *animation =
                        new QPropertyAnimation(bar, "value", bar);

                    animation->setDuration(650);
                    animation->setStartValue(0);
                    animation->setEndValue(finalValue);
                    animation->setEasingCurve(QEasingCurve::OutCubic);

                    animation->start(
                        QAbstractAnimation::DeleteWhenStopped
                        );
                });

                delay += 90;
            }

            layout->addStretch();
            return card;
        };

        auto *charts = new QGridLayout;
        charts->setHorizontalSpacing(14);
        charts->setVerticalSpacing(14);

        charts->addWidget(
            createChart("Incidents par type", byType), 0, 0
            );
        charts->addWidget(
            createChart("Incidents par gravité", byGravity), 0, 1
            );
        charts->addWidget(
            createChart("Incidents par zone", byZone), 1, 0
            );
        charts->addWidget(
            createChart("Incidents par statut", byStatus), 1, 1
            );

        page->addLayout(charts, 1);
        page->addStretch();

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
                const QString path = QFileDialog::getSaveFileName(
                    &dialog,
                    "Exporter les statistiques",
                    "statistiques.pdf",
                    "PDF (*.pdf)"
                    );

                if (path.isEmpty())
                    return;

                bool exported = false;

                {
                    QPdfWriter pdf(path);
                    pdf.setResolution(150);
                    pdf.setPageSize(QPageSize(QPageSize::A4));
                    pdf.setPageOrientation(QPageLayout::Landscape);
                    pdf.setPageMargins(QMarginsF(12, 12, 12, 12));

                    QPainter painter(&pdf);

                    if (painter.isActive()) {
                        const QPixmap capture = statisticsPage->grab();

                        const QSize target = capture.size().scaled(
                            QSize(pdf.width(), pdf.height()),
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

        auto *overlay = new BrightnessOverlay(&dialog);
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

    // ========================================================
    // EXPORT PDF DES INCIDENTS
    // ========================================================

    void exportIncidentsPdf()
    {
        const QString path = QFileDialog::getSaveFileName(
            this,
            "Exporter les incidents",
            "incidents.pdf",
            "PDF (*.pdf)"
            );

        if (path.isEmpty())
            return;

        QString html = QString::fromUtf8(R"HTML(
            <html>
            <head>
            <style>
                body {
                    font-family: Arial;
                    color: #302934;
                }
                h1 {
                    color: #981a27;
                }
                table {
                    width: 100%;
                    border-collapse: collapse;
                }
                th {
                    color: white;
                    background: #5b1722;
                    padding: 8px;
                }
                td {
                    border: 1px solid #dedede;
                    padding: 7px;
                }
            </style>
            </head>
            <body>
            <h1>Gestion des incidents</h1>
            <table cellspacing="0" cellpadding="6">
            <tr>
                <th>ID</th>
                <th>Type</th>
                <th>Date</th>
                <th>Adresse</th>
                <th>Gravité</th>
                <th>Victimes</th>
                <th>Statut</th>
                <th>Priorité</th>
            </tr>
        )HTML");

        for (const Incident &incident : incidents) {
            if (incident.archived)
                continue;

            html += incident.gravity == "Faible"
                        ? "<tr style='background-color:#d7f4df;'>"
                        : "<tr>";

            const QStringList cells = {
                incidentNumber(incident.id),
                incident.type,
                incident.date.toString("dd/MM/yyyy HH:mm"),
                incident.address,
                incident.gravity,
                QString::number(incident.victims),
                incident.status,
                calculatePriority(incident)
            };

            for (const QString &cell : cells)
                html += "<td>" + cell.toHtmlEscaped() + "</td>";

            html += "</tr>";
        }

        html += "</table></body></html>";

        {
            QPdfWriter pdf(path);
            pdf.setResolution(150);
            pdf.setPageSize(QPageSize(QPageSize::A4));
            pdf.setPageOrientation(QPageLayout::Landscape);
            pdf.setPageMargins(QMarginsF(12, 12, 12, 12));

            QTextDocument document;
            document.setHtml(html);
            document.print(&pdf);
        }

        const bool exported =
            QFileInfo(path).exists() && QFileInfo(path).size() > 0;

        if (exported) {
            QMessageBox::information(
                this,
                "Export terminé",
                "PDF enregistré :\n" + path
                );
        } else {
            QMessageBox::warning(
                this,
                "Export impossible",
                "Impossible de créer le PDF."
                );
        }
    }
};

// ============================================================
// ============================================================

} // namespace incidents

// Integration : cree la page du module Incidents
QMainWindow *creerModuleIncidents()
{
    auto *fenetre = new incidents::MainWindow;
    fenetre->setWindowFlags(Qt::Widget);
    fenetre->setMinimumSize(0, 0);
    return fenetre;
}
