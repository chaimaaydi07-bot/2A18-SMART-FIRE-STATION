#include "vehicules.h"


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
#include <QPrinter>
#include <QTextDocument>
#include <QFileDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>
#include <QTimer>
#include <QColor>
#include <QBrush>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QApplication>
#include <QStyle>
#include <QFontMetrics>
#include <QDateTime>
#include <QLocale>
#include <QDialog>
#include <QScrollArea>
#include <QMap>
#include <QVector>
#include <QPair>
#include <QPaintEvent>
#include <algorithm>
#include <memory>
#include <functional>
#include <cmath>
#include <QSslSocket>
#include <QSettings>
#include <QCheckBox>
#include <QThread>
#include <QPointer>
#include <QMetaObject>
#include <QEvent>
#include <QMouseEvent>
#include <QStatusBar>
#include <QIcon>
#include <QMenu>
#include <QAction>
#include <QProgressBar>
#include <QPalette>
#include <QToolTip>
#include <QLinearGradient>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QPageSize>
#include <QPageLayout>
#include <QMargins>

// =====================================================================
//  Colonnes du tableau
//  0 ID | 1 Nom | 2 Type | 3 Carburant (%) | 4 Seuil | 5 État | 6 Localisation
//  7 Date achat | 8 Dern. maint. | 9 Proch. maint. | 10 Actions
//  11 Marque | 12 Modèle | 13 Remarques  (colonnes cachées)
// =====================================================================
static const QString FMT("dd/MM/yyyy");

// logo de l'application : cherche dans les ressources (les deux chemins possibles)
static QPixmap logoApp()
{
    static QPixmap pm;
    if (pm.isNull()) {
        pm = QPixmap(":/logo.png");
        if (pm.isNull())
            pm = QPixmap(":/images/logo.png");
    }
    return pm;
}


// ----- ligne du tableau survolée par la souris -----
static int g_hoverRow = -1;
static void surlignerHover(QPainter *p, const QRect &r, int row)
{
    if (row != g_hoverRow) return;
    p->fillRect(r, QColor(196, 0, 0, 20));
}

// ----- styles simples pour les boutons colorés -----
static QString styleBoutonCouleur(const QString &couleur, const QString &survol,
                                  const QString &padding = "6px 18px")
{
    return QString(
        "QPushButton{background:%1;color:white;border:none;border-radius:6px;"
        "padding:%3;font-weight:bold;}"
        "QPushButton:hover{background:%2;}"
    ).arg(couleur, survol, padding);
}

static QString styleBoutonCouleurDesactive(const QString &couleur, const QString &survol,
                                           const QString &padding = "6px 18px")
{
    return styleBoutonCouleur(couleur, survol, padding)
        + "QPushButton:disabled{background:#ddd;color:#888;}";
}

// =====================================================================
//  MÉTIER INNOVANT : maintenance prédictive
//  Score de santé d'un véhicule (0 - 100 %), calculé automatiquement :
//   - part du cycle de maintenance déjà écoulée (dernière -> prochaine)
//   - âge de du véhicule
//   - état (en maintenance / hors service)
// =====================================================================
static int scoreSante(const QString &etat, const QDate &achat, const QDate &derniere, const QDate &prochaine)
{
    QDate today = QDate::currentDate();
    double score = 100.0;

    if (derniere.isValid() && prochaine.isValid() && prochaine > derniere) {
        double total  = derniere.daysTo(prochaine);
        double ecoule = derniere.daysTo(today);
        double ratio  = ecoule / total;
        if (ratio < 0) ratio = 0;
        if (ratio <= 1.0) score -= 40.0 * ratio;
        else              score -= 40.0 + qMin(30.0, (ratio - 1.0) * 60.0);
    }
    if (achat.isValid()) {
        double ans = achat.daysTo(today) / 365.0;
        if (ans > 0) score -= qMin(20.0, ans * 3.0);
    }
    if (etat == "En maintenance") score -= 15.0;
    if (etat == "Hors service")   score = qMin(score, 15.0);

    if (score < 0) score = 0;
    if (score > 100) score = 100;
    return int(score + 0.5);
}

static QStringList facteursSante(const QString &etat, const QDate &achat, const QDate &derniere, const QDate &prochaine)
{
    QDate today = QDate::currentDate();
    QStringList f;
    if (etat == "Hors service")   f << "véhicule hors service";
    if (etat == "En maintenance") f << "en cours de maintenance";
    if (prochaine.isValid()) {
        int d = int(today.daysTo(prochaine));
        if (d < 0)
            f << QString("maintenance en retard de %1 jour(s)").arg(-d);
        else if (derniere.isValid() && prochaine > derniere) {
            double ratio = double(derniere.daysTo(today)) / double(derniere.daysTo(prochaine));
            if (ratio >= 0.8)
                f << QString("maintenance dans %1 jour(s)").arg(d);
        }
    }
    if (achat.isValid()) {
        int ans = int(achat.daysTo(today) / 365);
        if (ans >= 3) f << QString("âge : %1 an(s)").arg(ans);
    }
    if (f.isEmpty()) f << "aucun facteur de risque";
    return f;
}

static QString recommandation(int score, const QDate &pm, QDate &suggeree)
{
    QDate today = QDate::currentDate();
    if (score < 40) { suggeree = today;              return "Intervention immédiate"; }
    if (score < 70) { suggeree = today.addDays(15);  return "Planifier sous 15 jours"; }
    suggeree = pm;
    return "Aucune action requise";
}

// couleur des badges (états, actions du journal, niveaux de santé)
static QColor couleurBadge(const QString &t)
{
    if (t == "Hors service" || t == "Suppression" || t == "Critique")  return QColor("#c62828");
    if (t == "En maintenance" || t == "Alerte" || t == "À surveiller") return QColor("#ef6c00");
    if (t == "En mission" || t == "Modification")                      return QColor("#1565c0");
    if (t == "Planification")                                          return QColor("#6a1b9a");
    if (t == "Export")                                                 return QColor("#00838f");
    if (t == "Système")                                                return QColor("#455a64");
    return QColor("#2e7d32");   // Disponible, Ajout, Bon
}

namespace {
// tri numérique
class NumItem : public QTableWidgetItem {
public:
    explicit NumItem(const QString &t) : QTableWidgetItem(t) {}
    bool operator<(const QTableWidgetItem &o) const override {
        return text().toInt() < o.text().toInt();
    }
};
// tri par date
class DateItem : public QTableWidgetItem {
public:
    explicit DateItem(const QString &t) : QTableWidgetItem(t) {}
    bool operator<(const QTableWidgetItem &o) const override {
        return QDate::fromString(text(), FMT) < QDate::fromString(o.text(), FMT);
    }
};
// badge coloré pour la colonne "État"
class EtatDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override {
        QStyleOptionViewItem o(opt);
        initStyleOption(&o, idx);
        QString txt = o.text;
        o.text.clear();
        QStyle *st = o.widget ? o.widget->style() : QApplication::style();
        st->drawControl(QStyle::CE_ItemViewItem, &o, p, o.widget);
        surlignerHover(p, opt.rect, idx.row());

        QColor bg = couleurBadge(txt);

        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        QFontMetrics fm(o.font);
        QRect r(0, 0, fm.horizontalAdvance(txt) + 22, 24);
        r.moveCenter(opt.rect.center());
        p->setPen(Qt::NoPen);
        p->setBrush(bg);
        p->drawRoundedRect(r, 12, 12);
        p->setPen(Qt::white);
        p->setFont(o.font);
        p->drawText(r, Qt::AlignCenter, txt);
        p->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &opt, const QModelIndex &idx) const override {
        QSize sz = QStyledItemDelegate::sizeHint(opt, idx);
        sz.setWidth(sz.width() + 14);
        return sz;
    }
};

// barre de santé (maintenance prédictive)
class SanteDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override {
        QStyleOptionViewItem o(opt);
        initStyleOption(&o, idx);
        int score = o.text.toInt();
        o.text.clear();
        QStyle *st = o.widget ? o.widget->style() : QApplication::style();
        st->drawControl(QStyle::CE_ItemViewItem, &o, p, o.widget);
        surlignerHover(p, opt.rect, idx.row());

        QColor c("#2e7d32");
        if (score < 40)      c = QColor("#c62828");
        else if (score < 70) c = QColor("#ef6c00");

        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        QRectF piste(opt.rect.left() + 8, opt.rect.center().y() - 5, opt.rect.width() - 58, 10);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor("#f1f1f1"));
        p->drawRoundedRect(piste, 5, 5);
        p->setBrush(c);
        p->drawRoundedRect(QRectF(piste.left(), piste.top(), piste.width() * score / 100.0, piste.height()), 5, 5);
        p->setPen(QColor("#222222"));
        QFont f = o.font;
        f.setBold(true);
        p->setFont(f);
        p->drawText(QRectF(piste.right() + 4, opt.rect.top(), 46, opt.rect.height()),
                    Qt::AlignVCenter | Qt::AlignLeft, QString::number(score) + " %");
        p->restore();
    }
};

// filtre d'événements générique (le comportement est donné par une fonction)
class EvtFilter : public QObject {
public:
    EvtFilter(QObject *parent, std::function<bool(QObject *, QEvent *)> f)
        : QObject(parent), fn(std::move(f)) {}
protected:
    bool eventFilter(QObject *o, QEvent *e) override { return fn(o, e); }
private:
    std::function<bool(QObject *, QEvent *)> fn;
};

// délégué par défaut du tableau : dessine normalement puis surligne la ligne survolée
class HoverDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override {
        QStyledItemDelegate::paint(p, opt, idx);
        surlignerHover(p, opt.rect, idx.row());
    }
};

}

// =====================================================================
//  Widgets de graphiques (dessinés avec QPainter, sans module Charts)
// =====================================================================
namespace {

// ---------- base : widget animé ----------
class AnimWidget : public QWidget {
public:
    explicit AnimWidget(QWidget *parent = nullptr) : QWidget(parent) {}
    void animer() {
        progress = 0.0;
        QVariantAnimation *a = new QVariantAnimation(this);
        a->setDuration(900);
        a->setStartValue(0.0);
        a->setEndValue(1.0);
        a->setEasingCurve(QEasingCurve::OutCubic);
        connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            progress = v.toDouble();
            update();
        });
        a->start(QAbstractAnimation::DeleteWhenStopped);
    }
protected:
    qreal progress = 1.0;
};

// ---------- Donut + légende ----------
class DonutWidget : public AnimWidget {
public:
    explicit DonutWidget(QWidget *parent = nullptr) : AnimWidget(parent) {
        setMinimumSize(380, 200);
        setMouseTracking(true);
    }
    void setData(const QVector<QString> &l, const QVector<int> &v, const QVector<QColor> &c) {
        labels = l; values = v; colors = c; update();
    }
protected:
    bool event(QEvent *e) override {
        if (e->type() == QEvent::Leave) {
            if (hover != -1) { hover = -1; update(); }
            QToolTip::hideText();
        }
        return AnimWidget::event(e);
    }

    void mouseMoveEvent(QMouseEvent *e) override {
        int i = segmentAt(e->position());
        if (i != hover) { hover = i; update(); }
        if (i >= 0) {
            int total = 0;
            for (int v : values) total += v;
            double pct = total ? 100.0 * values[i] / total : 0.0;
            QToolTip::showText(e->globalPosition().toPoint(),
                               QString("<b>%1</b><br>%2 véhicule(s)  —  %3 %")
                                   .arg(labels[i]).arg(values[i]).arg(pct, 0, 'f', 1), this);
        } else {
            QToolTip::hideText();
        }
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        int total = 0;
        for (int v : values) total += v;

        int size = qMin(height() - 20, 180);
        QRectF rect(10, (height() - size) / 2.0, size, size);
        qreal th = 28;
        QRectF ring = rect.adjusted(th / 2, th / 2, -th / 2, -th / 2);

        if (total == 0) {
            p.setPen(QPen(QColor("#f1f1f1"), th));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(ring);
        } else {
            int start = 90 * 16;
            for (int i = 0; i < values.size(); i++) {
                if (values[i] <= 0) continue;
                int span = -int(values[i] * 360.0 * 16 / total * progress);
                bool hv = (i == hover);
                p.setPen(QPen(hv ? colors[i].lighter(112) : colors[i], hv ? th + 8 : th,
                              Qt::SolidLine, Qt::FlatCap));
                p.drawArc(ring, start, span);
                start += span;
            }
        }

        // total au centre
        QFont big = font();
        big.setPointSize(22);
        big.setBold(true);
        p.setFont(big);
        p.setPen(QColor("#222222"));
        p.drawText(QRectF(rect.x(), rect.y() + rect.height() / 2 - 26, rect.width(), 34),
                   Qt::AlignCenter, QString::number(total));
        QFont small = font();
        small.setPointSize(9);
        p.setFont(small);
        p.setPen(QColor("#888888"));
        p.drawText(QRectF(rect.x(), rect.y() + rect.height() / 2 + 6, rect.width(), 20),
                   Qt::AlignCenter, "véhicules");

        // légende
        QFont lf = font();
        lf.setPointSize(10);
        p.setFont(lf);
        int x = size + 40;
        int y0 = (height() - int(labels.size()) * 30) / 2;
        for (int i = 0; i < labels.size(); i++) {
            int y = y0 + i * 30;
            p.setPen(Qt::NoPen);
            p.setBrush(colors[i]);
            p.drawEllipse(QRect(x, y + 6, 12, 12));
            double pct = total ? 100.0 * values[i] / total : 0.0;
            p.setPen(QColor("#444444"));
            p.drawText(QRect(x + 22, y, 260, 24), Qt::AlignVCenter | Qt::AlignLeft,
                       QString("%1 : %2  (%3 %)").arg(labels[i]).arg(values[i]).arg(pct, 0, 'f', 0));
        }
    }
private:
    // quelle part du donut se trouve sous la souris ? (-1 si aucune)
    int segmentAt(const QPointF &pos) const {
        int total = 0;
        for (int v : values) total += v;
        if (total <= 0) return -1;

        int size = qMin(height() - 20, 180);
        QPointF c(10 + size / 2.0, height() / 2.0);
        double dx = pos.x() - c.x();
        double dy = c.y() - pos.y();
        double d = std::sqrt(dx * dx + dy * dy);
        if (d < size / 2.0 - 30 || d > size / 2.0 + 4) return -1;

        double deg = std::atan2(dy, dx) * 180.0 / 3.14159265358979;   // sens trigonométrique
        double a = std::fmod(90.0 - deg + 720.0, 360.0);               // sens horaire depuis le haut
        double cum = 0;
        for (int i = 0; i < values.size(); i++) {
            double span = 360.0 * values[i] / total;
            if (values[i] > 0 && a >= cum && a < cum + span) return i;
            cum += span;
        }
        return -1;
    }

    QVector<QString> labels;
    QVector<int> values;
    QVector<QColor> colors;
    int hover = -1;
};

// ---------- Barres horizontales (dégradé + bulle au survol) ----------
class BarsWidget : public AnimWidget {
public:
    struct Row { QString label; int value; QColor color; };

    explicit BarsWidget(QWidget *parent = nullptr) : AnimWidget(parent) {
        setMinimumHeight(60);
        setMouseTracking(true);
    }
    void setRows(const QVector<Row> &r) {
        rows = r;
        setMinimumHeight(qMax(60, int(rows.size()) * 30 + 10));
        update();
    }
protected:
    bool event(QEvent *e) override {
        if (e->type() == QEvent::Leave) {
            if (hover != -1) { hover = -1; update(); }
            QToolTip::hideText();
        }
        return AnimWidget::event(e);
    }

    void mouseMoveEvent(QMouseEvent *e) override {
        int y = int(e->position().y()) - 5;
        int i = (y >= 0 && (y % 30) < 24) ? y / 30 : -1;
        if (i >= rows.size()) i = -1;
        if (i != hover) { hover = i; update(); }
        if (i >= 0) {
            int tot = 0;
            for (const Row &r : rows) tot += r.value;
            double pct = tot ? 100.0 * rows[i].value / tot : 0.0;
            QToolTip::showText(e->globalPosition().toPoint(),
                               QString("<b>%1</b><br>%2  —  %3 %")
                                   .arg(rows[i].label).arg(rows[i].value).arg(pct, 0, 'f', 1), this);
        } else {
            QToolTip::hideText();
        }
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        if (rows.isEmpty()) {
            p.setPen(QColor("#888888"));
            p.drawText(rect(), Qt::AlignCenter, "Aucune donnée");
            return;
        }
        int mx = 1;
        for (const Row &r : rows) mx = qMax(mx, r.value);
        const int labelW = 130, valueW = 40;
        int barW = qMax(20, width() - labelW - valueW - 10);
        QFontMetrics fm(font());
        for (int i = 0; i < rows.size(); i++) {
            bool hv = (i == hover);
            int y = i * 30 + 5;
            p.setPen(hv ? QColor("#222222") : QColor("#444444"));
            p.drawText(QRect(0, y, labelW - 8, 22), Qt::AlignVCenter | Qt::AlignLeft,
                       fm.elidedText(rows[i].label, Qt::ElideRight, labelW - 8));
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#f1f1f1"));
            p.drawRoundedRect(QRectF(labelW, y + (hv ? 3 : 4), barW, hv ? 16 : 14), 7, 7);
            double w = barW * double(rows[i].value) / mx * progress;
            if (w > 0) {
                double lw = qMax(14.0, w);
                QColor c = hv ? rows[i].color.lighter(112) : rows[i].color;
                QLinearGradient g(QPointF(labelW, 0), QPointF(labelW + lw, 0));
                g.setColorAt(0.0, c.lighter(140));
                g.setColorAt(1.0, c);
                p.setBrush(g);
                p.drawRoundedRect(QRectF(labelW, y + (hv ? 3 : 4), lw, hv ? 16 : 14), 7, 7);
            }
            p.setPen(QColor("#222222"));
            p.drawText(QRect(labelW + barW + 8, y, valueW, 22), Qt::AlignVCenter | Qt::AlignLeft,
                       QString::number(rows[i].value));
        }
    }
private:
    QVector<Row> rows;
    int hover = -1;
};

// ---------- Stock : Carburant (%) vs seuil (dégradé + bulle au survol) ----------
class StockWidget : public AnimWidget {
public:
    struct Row { QString label; int qte; int seuil; };

    explicit StockWidget(QWidget *parent = nullptr) : AnimWidget(parent) {
        setMinimumHeight(60);
        setMouseTracking(true);
    }
    void setRows(const QVector<Row> &r) {
        rows = r;
        setMinimumHeight(qMax(60, int(rows.size()) * 30 + 10));
        update();
    }
protected:
    bool event(QEvent *e) override {
        if (e->type() == QEvent::Leave) {
            if (hover != -1) { hover = -1; update(); }
            QToolTip::hideText();
        }
        return AnimWidget::event(e);
    }

    void mouseMoveEvent(QMouseEvent *e) override {
        int y = int(e->position().y()) - 5;
        int i = (y >= 0 && (y % 30) < 24) ? y / 30 : -1;
        if (i >= rows.size()) i = -1;
        if (i != hover) { hover = i; update(); }
        if (i >= 0) {
            const Row &r = rows[i];
            QString etat = r.qte < r.seuil
                               ? QString("<span style='color:#ef6c00'>il manque %1</span>").arg(r.seuil - r.qte)
                               : QString("<span style='color:#2e7d32'>niveau carburant correct</span>");
            QToolTip::showText(e->globalPosition().toPoint(),
                               QString("<b>%1</b><br>Carburant (%) : %2  •  seuil : %3<br>%4")
                                   .arg(r.label).arg(r.qte).arg(r.seuil).arg(etat), this);
        } else {
            QToolTip::hideText();
        }
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        if (rows.isEmpty()) {
            p.setPen(QColor("#888888"));
            p.drawText(rect(), Qt::AlignCenter, "Aucune donnée");
            return;
        }
        int mx = 1;
        for (const Row &r : rows) mx = qMax(mx, qMax(r.qte, r.seuil));
        const int labelW = 150, textW = 80;
        int barW = qMax(20, width() - labelW - textW - 10);
        QFontMetrics fm(font());
        for (int i = 0; i < rows.size(); i++) {
            const Row &r = rows[i];
            bool bas = r.qte < r.seuil;
            bool hv = (i == hover);
            int y = i * 30 + 5;

            p.setPen(hv ? QColor("#222222") : QColor("#444444"));
            p.drawText(QRect(0, y, labelW - 8, 22), Qt::AlignVCenter | Qt::AlignLeft,
                       fm.elidedText(r.label, Qt::ElideRight, labelW - 8));

            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#f1f1f1"));
            p.drawRoundedRect(QRectF(labelW, y + (hv ? 3 : 4), barW, hv ? 16 : 14), 7, 7);

            double w = barW * double(r.qte) / mx * progress;
            if (w > 0) {
                double lw = qMax(14.0, w);
                QColor c = bas ? QColor("#ef6c00") : QColor("#2e7d32");
                if (hv) c = c.lighter(112);
                QLinearGradient g(QPointF(labelW, 0), QPointF(labelW + lw, 0));
                g.setColorAt(0.0, c.lighter(140));
                g.setColorAt(1.0, c);
                p.setBrush(g);
                p.drawRoundedRect(QRectF(labelW, y + (hv ? 3 : 4), lw, hv ? 16 : 14), 7, 7);
            }

            // trait du seuil minimum
            double sx = labelW + barW * double(r.seuil) / mx;
            p.setPen(QPen(QColor("#c62828"), 2));
            p.drawLine(QPointF(sx, y + 1), QPointF(sx, y + 21));

            p.setPen(bas ? QColor("#c62828") : QColor("#222222"));
            p.drawText(QRect(labelW + barW + 8, y, textW, 22), Qt::AlignVCenter | Qt::AlignLeft,
                       QString("%1 / %2").arg(r.qte).arg(r.seuil));
        }
    }
private:
    QVector<Row> rows;
    int hover = -1;
};

} // namespace

// =====================================================================
//  Petits helpers visuels
// =====================================================================
// notification flottante qui apparaît puis disparaît toute seule
static void toast(QWidget *parent, const QString &msg, const QString &couleur = "#2e7d32")
{
    QLabel *t = new QLabel(msg, parent);
    t->setStyleSheet(QString(
                         "background:%1;color:white;border-radius:18px;"
                         "padding:10px 22px;font-weight:bold;font-size:13px;").arg(couleur));
    t->setAlignment(Qt::AlignCenter);
    t->adjustSize();
    static int actifs = 0;           // nombre de notifications affichées (elles s'empilent)
    int rang = actifs++;
    QObject::connect(t, &QObject::destroyed, [](QObject *) { actifs--; });
    QPoint fin((parent->width() - t->width()) / 2,
               parent->height() - t->height() - 40 - rang * (t->height() + 10));
    t->move(fin + QPoint(0, 28));

    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(t);
    t->setGraphicsEffect(eff);
    t->show();
    t->raise();

    QPropertyAnimation *in = new QPropertyAnimation(eff, "opacity", t);
    in->setDuration(250);
    in->setStartValue(0.0);
    in->setEndValue(1.0);
    in->start();

    QPropertyAnimation *glisse = new QPropertyAnimation(t, "pos", t);
    glisse->setDuration(380);
    glisse->setStartValue(fin + QPoint(0, 28));
    glisse->setEndValue(fin);
    glisse->setEasingCurve(QEasingCurve::OutBack);
    glisse->start(QAbstractAnimation::DeleteWhenStopped);

    QTimer::singleShot(2400, t, [t, eff]() {
        QPropertyAnimation *out = new QPropertyAnimation(eff, "opacity", t);
        out->setDuration(450);
        out->setStartValue(1.0);
        out->setEndValue(0.0);
        QObject::connect(out, &QPropertyAnimation::finished, t, &QLabel::deleteLater);
        out->start();
    });
}

// ombre douce sous une carte
static void ombre(QWidget *w)
{
    QGraphicsDropShadowEffect *e = new QGraphicsDropShadowEffect(w);
    e->setBlurRadius(18);
    e->setOffset(0, 2);
    e->setColor(QColor(0, 0, 0, 35));
    w->setGraphicsEffect(e);
}

// lueur douce qui apparaît (en fondu) quand la souris survole un bouton
static void survolBouton(QPushButton *b, const QColor &lueur)
{
    QGraphicsDropShadowEffect *e = new QGraphicsDropShadowEffect(b);
    e->setColor(lueur);
    e->setOffset(0, 0);
    e->setBlurRadius(0);
    b->setGraphicsEffect(e);

    QVariantAnimation *a = new QVariantAnimation(b);
    a->setDuration(200);
    a->setStartValue(0.0);
    a->setEndValue(1.0);
    a->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(a, &QVariantAnimation::valueChanged, e, [e](const QVariant &v) {
        double t = v.toDouble();
        e->setBlurRadius(18.0 * t);
        e->setOffset(0, 4.0 * t);
    });
    b->installEventFilter(new EvtFilter(b, [a](QObject *, QEvent *ev) {
        if (ev->type() == QEvent::Enter) {
            a->setDirection(QAbstractAnimation::Forward);
            if (a->state() != QAbstractAnimation::Running) a->start();
        } else if (ev->type() == QEvent::Leave) {
            a->setDirection(QAbstractAnimation::Backward);
            if (a->state() != QAbstractAnimation::Running) a->start();
        }
        return false;
    }));
}

// bordure rouge sur un champ invalide
static void marquerErreur(QWidget *w, bool err)
{
    w->setProperty("erreur", err);
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();

    if (err) {   // petite secousse du champ
        if (!w->property("pos0").isValid())
            w->setProperty("pos0", w->pos());
        QPoint p0 = w->property("pos0").toPoint();
        QPropertyAnimation *a = new QPropertyAnimation(w, "pos", w);
        a->setDuration(320);
        a->setKeyValueAt(0.0, p0);
        a->setKeyValueAt(0.2, p0 + QPoint(-8, 0));
        a->setKeyValueAt(0.4, p0 + QPoint(8, 0));
        a->setKeyValueAt(0.6, p0 + QPoint(-5, 0));
        a->setKeyValueAt(0.8, p0 + QPoint(5, 0));
        a->setKeyValueAt(1.0, p0);
        a->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

// apparition en fondu d'un widget (avec délai), l'effet est retiré à la fin
static void apparition(QWidget *w, int delai)
{
    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(w);
    eff->setOpacity(0.0);
    w->setGraphicsEffect(eff);
    QTimer::singleShot(delai, w, [w, eff]() {
        QPropertyAnimation *a = new QPropertyAnimation(eff, "opacity", w);
        a->setDuration(480);
        a->setStartValue(0.0);
        a->setEndValue(1.0);
        a->setEasingCurve(QEasingCurve::OutCubic);
        QObject::connect(a, &QPropertyAnimation::finished, w, [w]() {
            QTimer::singleShot(0, w, [w]() { w->setGraphicsEffect(nullptr); });
        });
        a->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

// =====================================================================
//  MÉTIER 1 : alerte par EMAIL (SMTP)
//  Un seul canal de notification : l'email envoyé au responsable.
// =====================================================================
struct ConfigEmail {
    QString serveur = "smtp.gmail.com";
    int     port = 465;               // 465 = SSL direct, autre (587, 2525) = STARTTLS
    QString expediteur;
    QString mdp;                      // mot de passe d'application
    QString destinataire;
    bool    automatique = true;
};

static ConfigEmail chargerConfigEmail()
{
    QSettings st("SmartFireStation", "Vehicules");
    ConfigEmail c;
    c.serveur      = st.value("email/serveur", c.serveur).toString();
    c.port         = st.value("email/port", c.port).toInt();
    c.expediteur   = st.value("email/expediteur").toString();
    c.mdp          = st.value("email/mdp").toString();
    c.destinataire = st.value("email/destinataire").toString();
    c.automatique  = st.value("email/automatique", true).toBool();
    return c;
}

static void sauverConfigEmail(const ConfigEmail &c)
{
    QSettings st("SmartFireStation", "Vehicules");
    st.setValue("email/serveur", c.serveur);
    st.setValue("email/port", c.port);
    st.setValue("email/expediteur", c.expediteur);
    st.setValue("email/mdp", c.mdp);
    st.setValue("email/destinataire", c.destinataire);
    st.setValue("email/automatique", c.automatique);
}

static bool configEmailComplete(const ConfigEmail &c)
{
    return !c.serveur.trimmed().isEmpty() && c.port > 0
           && c.expediteur.contains('@') && !c.mdp.isEmpty() && c.destinataire.contains('@');
}

static QString corpsEmail(const QStringList &lignes)
{
    return "Bonjour,\n\n"
           "Smart Fire Station a détecté une alerte automatique sur les véhicules de la caserne :\n\n"
           + lignes.join("\n")
           + "\n\nDate : " + QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm")
           + "\n\nMerci de prendre les mesures nécessaires.\n\n"
             "— Smart Fire Station (message automatique)";
}

// envoi SMTP synchrone (à exécuter dans un thread) : renvoie true si le serveur accepte le message
static bool envoyerSmtp(const ConfigEmail &c, const QString &sujet, const QString &corps, QString &erreur)
{
    QSslSocket sock;

    // lit une réponse SMTP complète (les lignes "250-..." continuent jusqu'à "250 ...")
    auto lire = [&](int &code, QString &texte) -> bool {
        texte.clear();
        code = 0;
        while (true) {
            while (!sock.canReadLine())
                if (!sock.waitForReadyRead(15000)) return false;
            QString ligne = QString::fromUtf8(sock.readLine()).trimmed();
            texte += ligne + " ";
            if (ligne.size() >= 3) code = ligne.left(3).toInt();
            if (ligne.size() < 4 || ligne.at(3) != QLatin1Char('-')) return true;
        }
    };

    auto cmd = [&](const QString &commande, int attendu) -> bool {
        sock.write(commande.toUtf8() + "\r\n");
        if (!sock.waitForBytesWritten(15000)) {
            erreur = "Envoi impossible : " + sock.errorString();
            return false;
        }
        int code = 0;
        QString texte;
        if (!lire(code, texte)) {
            erreur = "Pas de réponse du serveur";
            return false;
        }
        if (code != attendu) {
            erreur = "Serveur : " + texte.trimmed();
            return false;
        }
        return true;
    };

    const QString hote = c.serveur.trimmed();
    if (c.port == 465) {
        sock.connectToHostEncrypted(hote, quint16(c.port));
        if (!sock.waitForEncrypted(15000)) {
            erreur = "Connexion sécurisée impossible : " + sock.errorString();
            return false;
        }
    } else {
        sock.connectToHost(hote, quint16(c.port));
        if (!sock.waitForConnected(15000)) {
            erreur = "Connexion impossible : " + sock.errorString();
            return false;
        }
    }

    int code = 0;
    QString texte;
    if (!lire(code, texte) || code != 220) {
        erreur = "Réponse inattendue du serveur : " + texte.trimmed();
        return false;
    }
    if (!cmd("EHLO smartfirestation", 250)) return false;

    if (c.port != 465) {
        if (!cmd("STARTTLS", 220)) return false;
        sock.startClientEncryption();
        if (!sock.waitForEncrypted(15000)) {
            erreur = "STARTTLS impossible : " + sock.errorString();
            return false;
        }
        if (!cmd("EHLO smartfirestation", 250)) return false;
    }

    QString mdp = c.mdp;
    mdp.remove(' ');
    if (!cmd("AUTH LOGIN", 334)) return false;
    if (!cmd(QString::fromLatin1(c.expediteur.trimmed().toUtf8().toBase64()), 334)) return false;
    if (!cmd(QString::fromLatin1(mdp.toUtf8().toBase64()), 235)) {
        erreur = "Authentification refusée : vérifiez l'email et le mot de passe d'application";
        return false;
    }

    if (!cmd("MAIL FROM:<" + c.expediteur.trimmed() + ">", 250)) return false;
    if (!cmd("RCPT TO:<" + c.destinataire.trimmed() + ">", 250)) return false;
    if (!cmd("DATA", 354)) return false;

    QByteArray b64 = corps.toUtf8().toBase64();
    QByteArray corpsMime;
    for (int i = 0; i < b64.size(); i += 76)
        corpsMime += b64.mid(i, 76) + "\r\n";

    QByteArray msg;
    msg += "From: Smart Fire Station <" + c.expediteur.trimmed().toUtf8() + ">\r\n";
    msg += "To: " + c.destinataire.trimmed().toUtf8() + "\r\n";
    msg += "Subject: =?UTF-8?B?" + sujet.toUtf8().toBase64() + "?=\r\n";
    msg += "MIME-Version: 1.0\r\n";
    msg += "Content-Type: text/plain; charset=UTF-8\r\n";
    msg += "Content-Transfer-Encoding: base64\r\n\r\n";
    msg += corpsMime;
    msg += "\r\n.\r\n";
    sock.write(msg);
    if (!sock.waitForBytesWritten(15000)) {
        erreur = "Envoi du message impossible : " + sock.errorString();
        return false;
    }
    if (!lire(code, texte) || code != 250) {
        erreur = "Message refusé : " + texte.trimmed();
        return false;
    }

    cmd("QUIT", 221);
    sock.disconnectFromHost();
    return true;
}

// envoi dans un thread séparé (l'interface ne se bloque pas)
// "fin" est rappelée dans le thread de l'interface avec (succès, message d'erreur)
static void envoyerAsync(QObject *contexte, const ConfigEmail &c, const QString &sujet, const QString &corps,
                         std::function<void(bool, const QString &)> fin)
{
    QPointer<QObject> ctx(contexte);
    QThread *th = QThread::create([=]() {
        QString err;
        bool ok = envoyerSmtp(c, sujet, corps, err);
        if (!ctx) return;
        QMetaObject::invokeMethod(ctx.data(), [=]() { fin(ok, err); }, Qt::QueuedConnection);
    });
    QObject::connect(th, &QThread::finished, th, &QObject::deleteLater);
    th->start();
}

// affiche un message au centre du tableau quand il est vide ou que la recherche ne donne rien
static void majEtatVide(QTableWidget *table)
{
    QLabel *l = table->findChild<QLabel *>("labelVide");
    if (!l) return;
    int visibles = 0;
    for (int r = 0; r < table->rowCount(); r++)
        if (!table->isRowHidden(r)) visibles++;
    if (table->rowCount() == 0)
        l->setText("📭\nAucun véhicule\nAjoutez-en un avec le formulaire");
    else
        l->setText("🔍\nAucun résultat\nEssayez une autre recherche");
    l->setVisible(visibles == 0);
}

// glissement d'un widget depuis un décalage jusqu'à sa position finale
static void diapo(QWidget *w, QPoint decalage, int delai)
{
    QTimer::singleShot(delai, w, [w, decalage]() {
        QPoint fin = w->pos();
        QPropertyAnimation *a = new QPropertyAnimation(w, "pos", w);
        a->setDuration(650);
        a->setStartValue(fin + decalage);
        a->setEndValue(fin);
        a->setEasingCurve(QEasingCurve::OutCubic);
        a->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

// apparition en fondu d'une fenêtre
static void fondu(QWidget *w)
{
    w->setWindowOpacity(0.0);
    QPropertyAnimation *a = new QPropertyAnimation(w, "windowOpacity", w);
    a->setDuration(300);
    a->setStartValue(0.0);
    a->setEndValue(1.0);
    a->setEasingCurve(QEasingCurve::OutCubic);
    a->start(QAbstractAnimation::DeleteWhenStopped);
}

// la ligne ajoutée / modifiée "flashe" en vert puis reprend sa couleur normale
static void flashLigne(QTableWidget *table, int row, std::function<void()> fin)
{
    if (row < 0 || row >= table->rowCount()) return;
    if (table->item(row, 1)) table->scrollToItem(table->item(row, 1));

    QVariantAnimation *a = new QVariantAnimation(table);
    a->setDuration(1500);
    a->setStartValue(1.0);
    a->setEndValue(0.0);
    a->setEasingCurve(QEasingCurve::InQuad);
    QObject::connect(a, &QVariantAnimation::valueChanged, table, [table, row](const QVariant &v) {
        if (row >= table->rowCount()) return;
        QColor c(46, 204, 113);
        c.setAlphaF(0.08 + 0.55 * v.toDouble());
        for (int col = 0; col < table->columnCount(); col++)
            if (table->item(row, col))
                table->item(row, col)->setBackground(QBrush(c));
    });
    QObject::connect(a, &QVariantAnimation::finished, table, [fin]() { fin(); });
    a->start(QAbstractAnimation::DeleteWhenStopped);
}

// =====================================================================
//  Page "Statistiques"
// =====================================================================
static void afficherStatistiques(QWidget *parent, QTableWidget *table, const QVector<QStringList> &journal)
{
    QDate today = QDate::currentDate();

    int total = 0, dispo = 0, hs = 0, enAlerte = 0, sommeSante = 0;
    int nbEtat[4]  = {0, 0, 0, 0};   // Disponible, En mission, En maintenance, Hors service
    int nbSante[3] = {0, 0, 0};      // Bon, À surveiller, Critique
    QMap<QString, int> parType;
    QVector<StockWidget::Row> stock;
    struct Prio { int score; QString nom; QString facteurs; };
    QVector<Prio> prio;

    for (int r = 0; r < table->rowCount(); r++) {
        if (!table->item(r, 5) || !table->item(r, 3) || !table->item(r, 4)
            || !table->item(r, 7) || !table->item(r, 8) || !table->item(r, 9))
            continue;
        total++;
        QString id  = table->item(r, 0)->text();
        QString nom = table->item(r, 1)->text();
        QString typ = table->item(r, 2)->text();
        int q = table->item(r, 3)->text().toInt();
        int s = table->item(r, 4)->text().toInt();
        QString e = table->item(r, 5)->text();
        QDate da = QDate::fromString(table->item(r, 7)->text(), FMT);
        QDate dm = QDate::fromString(table->item(r, 8)->text(), FMT);
        QDate pm = QDate::fromString(table->item(r, 9)->text(), FMT);

        parType[typ]++;
        if (e == "Disponible")           { nbEtat[0]++; dispo++; }
        else if (e == "En mission")      nbEtat[1]++;
        else if (e == "En maintenance")  nbEtat[2]++;
        else if (e == "Hors service")    { nbEtat[3]++; hs++; }
        if (e == "Hors service" || q < s) enAlerte++;
        stock.push_back({nom, q, s});

        int sc = scoreSante(e, da, dm, pm);
        sommeSante += sc;
        if (sc >= 70)      nbSante[0]++;
        else if (sc >= 40) nbSante[1]++;
        else               nbSante[2]++;
        prio.push_back({sc, id + "  " + nom, facteursSante(e, da, dm, pm).join("  •  ")});
    }

    double tauxDispo = total > 0 ? 100.0 * dispo / total : 0.0;
    double tauxHs    = total > 0 ? 100.0 * hs / total : 0.0;
    int santeMoy     = total > 0 ? int(sommeSante / double(total) + 0.5) : 0;

    std::sort(stock.begin(), stock.end(), [](const StockWidget::Row &a, const StockWidget::Row &b) {
        return (a.qte - a.seuil) < (b.qte - b.seuil);
    });
    std::sort(prio.begin(), prio.end(), [](const Prio &a, const Prio &b) { return a.score < b.score; });

    QMap<QString, int> parAction;
    for (const QStringList &e : journal)
        if (e.size() > 1 && e[1] != "Système")
            parAction[e[1]]++;

    // ---------- fenêtre ----------
    QDialog *dlg = new QDialog(parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle("Statistiques - Véhicules");
    dlg->resize(1050, 720);
    dlg->setStyleSheet(
        "QDialog{background:#f4f4f6;}"
        "QFrame#statCard{background:white;border:1px solid #e6e6e6;border-radius:10px;}"
        "QLabel{border:none;background:transparent;}"
        "QLabel#cardTitle{color:#a90000;font-weight:bold;font-size:14px;}");

    QVBoxLayout *root = new QVBoxLayout(dlg);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QFrame *banner = new QFrame;
    banner->setStyleSheet("QFrame{background:#c40000;}");
    banner->setFixedHeight(70);
    QVBoxLayout *bl = new QVBoxLayout(banner);
    bl->setContentsMargins(24, 8, 24, 8);
    bl->setSpacing(0);
    QLabel *bt = new QLabel("📊  Statistiques des véhicules");
    bt->setStyleSheet("color:white;font-size:20px;font-weight:bold;");
    QLabel *bs = new QLabel("Situation au " + today.toString(FMT));
    bs->setStyleSheet("color:#ffd6d6;font-size:12px;");
    bl->addWidget(bt);
    bl->addWidget(bs);
    root->addWidget(banner);

    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea{background:#f4f4f6;border:none;}");
    QWidget *inner = new QWidget;
    inner->setObjectName("statInner");
    inner->setStyleSheet("QWidget#statInner{background:#f4f4f6;}");
    QVBoxLayout *iv = new QVBoxLayout(inner);
    iv->setContentsMargins(22, 18, 22, 18);
    iv->setSpacing(16);

    auto kpi = [](const QString &val, const QString &cap, const QString &col) -> QFrame * {
        static int n = 0;
        QString name = QString("kpi%1").arg(n++);
        QFrame *f = new QFrame;
        f->setObjectName(name);
        f->setFixedHeight(84);
        f->setStyleSheet(QString(
                             "QFrame#%1{background:white;border:1px solid #e6e6e6;"
                             "border-left:5px solid %2;border-radius:10px;border-top-left-radius:2px;border-bottom-left-radius:2px;}"
                             "QLabel{border:none;background:transparent;}").arg(name, col));
        QVBoxLayout *l = new QVBoxLayout(f);
        l->setContentsMargins(14, 8, 14, 8);
        l->setSpacing(0);
        QLabel *v = new QLabel(val);
        v->setStyleSheet(QString("font-size:26px;font-weight:bold;color:%1;").arg(col));
        QLabel *c = new QLabel(cap);
        c->setStyleSheet("color:#777;font-size:12px;");
        l->addWidget(v);
        l->addWidget(c);
        ombre(f);
        return f;
    };

    auto carte = [](const QString &titre, QWidget *contenu) -> QFrame * {
        QFrame *f = new QFrame;
        f->setObjectName("statCard");
        QVBoxLayout *l = new QVBoxLayout(f);
        l->setContentsMargins(16, 12, 16, 14);
        l->setSpacing(10);
        QLabel *t = new QLabel(titre);
        t->setObjectName("cardTitle");
        l->addWidget(t);
        l->addWidget(contenu, 1);
        ombre(f);
        return f;
    };

    // --- ligne 1 : indicateurs clés ---
    QHBoxLayout *kpis = new QHBoxLayout;
    kpis->setSpacing(14);
    kpis->addWidget(kpi(QString::number(total), "🚗  Véhicules", "#c40000"));
    kpis->addWidget(kpi(QString::number(tauxDispo, 'f', 1) + " %", "✅  Taux de disponibilité", "#2e7d32"));
    kpis->addWidget(kpi(QString::number(tauxHs, 'f', 1) + " %", "⛔  Taux hors service", "#c62828"));
    kpis->addWidget(kpi(QString::number(santeMoy) + " %", "💚  Santé moyenne du parc", "#00897b"));
    kpis->addWidget(kpi(QString::number(enAlerte), "🚨  Véhicules en alerte", "#ef6c00"));
    iv->addLayout(kpis);

    // --- ligne 2 : état + type ---
    DonutWidget *donut = new DonutWidget;
    donut->setData({"Disponible", "En mission", "En maintenance", "Hors service"},
                   {nbEtat[0], nbEtat[1], nbEtat[2], nbEtat[3]},
                   {QColor("#2e7d32"), QColor("#1565c0"), QColor("#ef6c00"), QColor("#c62828")});

    BarsWidget *types = new BarsWidget;
    QVector<BarsWidget::Row> rowsType;
    QStringList palette = {"#c40000", "#ef6c00", "#1565c0", "#2e7d32", "#6a1b9a", "#00838f"};
    int ci = 0;
    for (auto it = parType.constBegin(); it != parType.constEnd(); ++it, ++ci)
        rowsType.push_back({it.key(), it.value(), QColor(palette[ci % palette.size()])});
    types->setRows(rowsType);

    QHBoxLayout *row2 = new QHBoxLayout;
    row2->setSpacing(14);
    row2->addWidget(carte("Répartition par état", donut), 1);
    row2->addWidget(carte("Véhicules par type", types), 1);
    iv->addLayout(row2);

    // --- ligne 3 : santé du parc + activité du journal ---
    BarsWidget *santeBars = new BarsWidget;
    QVector<BarsWidget::Row> rowsSante;
    rowsSante.push_back({"Bon (≥ 70 %)", nbSante[0], QColor("#2e7d32")});
    rowsSante.push_back({"À surveiller", nbSante[1], QColor("#ef6c00")});
    rowsSante.push_back({"Critique (< 40 %)", nbSante[2], QColor("#c62828")});
    santeBars->setRows(rowsSante);

    QWidget *santeBox = new QWidget;
    QVBoxLayout *sbx = new QVBoxLayout(santeBox);
    sbx->setContentsMargins(0, 0, 0, 0);
    sbx->setSpacing(6);
    sbx->addWidget(santeBars);
    QLabel *santeNote = new QLabel("Score calculé automatiquement : cycle de maintenance, âge et état de du véhicule.");
    santeNote->setWordWrap(true);
    santeNote->setStyleSheet("color:#777;font-size:11px;");
    sbx->addWidget(santeNote);

    BarsWidget *actBars = new BarsWidget;
    QVector<BarsWidget::Row> rowsAct;
    for (const QString &a : QStringList{"Ajout", "Modification", "Suppression",
                                        "Alerte", "Planification", "Export"})
        rowsAct.push_back({a, parAction.value(a, 0), couleurBadge(a)});
    actBars->setRows(rowsAct);

    QHBoxLayout *row3 = new QHBoxLayout;
    row3->setSpacing(14);
    row3->addWidget(carte("Santé du parc (maintenance prédictive)", santeBox), 1);
    row3->addWidget(carte("Activité du journal (opérations)", actBars), 1);
    iv->addLayout(row3);

    // --- ligne 4 : stock + priorités ---
    StockWidget *stockW = new StockWidget;
    stockW->setRows(stock);

    QWidget *stockBox = new QWidget;
    QVBoxLayout *sbl = new QVBoxLayout(stockBox);
    sbl->setContentsMargins(0, 0, 0, 0);
    sbl->setSpacing(6);
    sbl->addWidget(stockW);
    QLabel *legStock = new QLabel(
        "<span style='color:#2e7d32'>■</span> niveau carburant correct &nbsp;&nbsp; "
        "<span style='color:#ef6c00'>■</span> carburant faible &nbsp;&nbsp; "
        "<span style='color:#c62828'>|</span> seuil minimum");
    legStock->setStyleSheet("color:#777;font-size:11px;");
    sbl->addWidget(legStock);

    QWidget *prioBox = new QWidget;
    QVBoxLayout *pbl = new QVBoxLayout(prioBox);
    pbl->setContentsMargins(0, 0, 0, 0);
    pbl->setSpacing(8);
    int shown = 0;
    for (const Prio &p : prio) {
        if (p.score >= 70 || shown >= 5) break;
        QString col = p.score < 40 ? "#c62828" : "#ef6c00";
        QLabel *l = new QLabel(QString(
                                   "<span style='color:%1;font-size:14px'>●</span>&nbsp; <b>%2</b> &nbsp;"
                                   "<span style='color:%1'><b>%3 %</b></span><br>"
                                   "<span style='color:#777;font-size:11px'>&nbsp;&nbsp;&nbsp;&nbsp;%4</span>")
                                   .arg(col, p.nom.toHtmlEscaped()).arg(p.score).arg(p.facteurs.toHtmlEscaped()));
        pbl->addWidget(l);
        shown++;
    }
    if (shown == 0)
        pbl->addWidget(new QLabel("✓  Tous les véhicules sont en bon état."));
    pbl->addStretch();

    QHBoxLayout *row4 = new QHBoxLayout;
    row4->setSpacing(14);
    row4->addWidget(carte("Carburant : niveau actuel vs minimum", stockBox), 3);
    row4->addWidget(carte("À maintenir en priorité", prioBox), 2);
    iv->addLayout(row4);

    iv->addStretch();
    scroll->setWidget(inner);
    root->addWidget(scroll, 1);

    // pied de page
    QFrame *foot = new QFrame;
    foot->setStyleSheet("QFrame{background:white;border-top:1px solid #e6e6e6;}");
    QHBoxLayout *fl = new QHBoxLayout(foot);
    fl->setContentsMargins(22, 10, 22, 10);
    fl->addStretch();

    QPushButton *pdfStats = new QPushButton("📄  Exporter en PDF");
    pdfStats->setFixedHeight(34);
    pdfStats->setStyleSheet(styleBoutonCouleur("#1565c0", "#0d47a1"));
    QObject::connect(pdfStats, &QPushButton::clicked, dlg, [=]() {
        QString file = QFileDialog::getSaveFileName(
            dlg, "Exporter les statistiques",
            QDir::homePath() + "/Documents/statistiques_vehicules.pdf", "PDF (*.pdf)");
        if (file.isEmpty()) return;

        QPixmap pix = inner->grab();
        QPrinter printer(QPrinter::ScreenResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(file);
        printer.setPageSize(QPageSize(QPageSize::A4));
        printer.setPageMargins(QMarginsF(12, 12, 12, 12), QPageLayout::Millimeter);

        QPainter painter(&printer);
        QRectF page = printer.pageRect(QPrinter::DevicePixel);
        painter.setPen(QColor("#c40000"));
        QFont f = painter.font();
        f.setPointSize(16);
        f.setBold(true);
        painter.setFont(f);
        painter.drawText(QRectF(0, 0, page.width(), 30), Qt::AlignLeft | Qt::AlignVCenter,
                         "Statistiques des véhicules - " + today.toString(FMT));
        double sc = page.width() / pix.width();
        painter.translate(0, 40);
        painter.scale(sc, sc);
        painter.drawPixmap(0, 0, pix);
        painter.end();

        QMessageBox::information(dlg, "PDF", "Export réussi !");
    });
    fl->addWidget(pdfStats);

    QPushButton *fermer = new QPushButton("Fermer");
    fermer->setFixedHeight(34);
    fermer->setMinimumWidth(110);
    fermer->setStyleSheet(
        "QPushButton{background:#c40000;color:white;border:none;border-radius:6px;"
        "padding:6px 18px;font-weight:bold;}"
        "QPushButton:hover{background:#8f0000;}");
    QObject::connect(fermer, &QPushButton::clicked, dlg, &QDialog::accept);
    fl->addWidget(fermer);
    root->addWidget(foot);

    donut->animer();
    types->animer();
    santeBars->animer();
    actBars->animer();
    stockW->animer();

    fondu(dlg);
    dlg->exec();
}

vehicules::vehicules(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Smart Fire Station");
    setWindowIcon(QIcon(logoApp()));   // icône de la fenêtre et de la barre des tâches
    resize(1300, 720);

    setStyleSheet(
        // info-bulles
        "QToolTip{background:#2b2b2b;color:white;border:none;padding:6px 10px;border-radius:6px;}"
        // barres de défilement fines et arrondies
        "QScrollBar:vertical{background:transparent;width:10px;margin:2px;}"
        "QScrollBar::handle:vertical{background:#d9bcbc;border-radius:4px;min-height:30px;}"
        "QScrollBar::handle:vertical:hover{background:#c40000;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
        "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:transparent;}"
        "QScrollBar:horizontal{background:transparent;height:10px;margin:2px;}"
        "QScrollBar::handle:horizontal{background:#d9bcbc;border-radius:4px;min-width:30px;}"
        "QScrollBar::handle:horizontal:hover{background:#c40000;}"
        "QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0;}"
        "QScrollBar::add-page:horizontal,QScrollBar::sub-page:horizontal{background:transparent;}"
        // listes déroulantes
        "QComboBox QAbstractItemView{background:white;border:1px solid #ddd;outline:0;"
        "selection-background-color:#f3c9c9;selection-color:black;}"
        // boîtes de message
        "QMessageBox{background:white;}"
        "QMessageBox QPushButton{background:#c40000;color:white;border:none;border-radius:6px;"
        "padding:6px 18px;min-width:70px;font-weight:bold;}"
        "QMessageBox QPushButton:hover{background:#8f0000;}"
        // calendrier des dates
        "QCalendarWidget QWidget#qt_calendar_navigationbar{background:#c40000;}"
        "QCalendarWidget QToolButton{color:white;background:transparent;font-weight:bold;"
        "border:none;padding:4px 8px;}"
        "QCalendarWidget QToolButton:hover{background:#8f0000;border-radius:4px;}"
        "QCalendarWidget QAbstractItemView:enabled{selection-background-color:#c40000;"
        "selection-color:white;}"
        // menus contextuels
        "QMenu{background:white;border:1px solid #ddd;padding:6px;}"
        "QMenu::item{padding:6px 26px;}"
        "QMenu::item:selected{background:#f3c9c9;color:black;}"
        "QMenu::item:disabled{color:#a90000;font-weight:bold;}"
        "QMenu::separator{height:1px;background:#eee;margin:4px 8px;}"
        // barre d'état
        "QStatusBar{background:white;color:#777;border-top:1px solid #e6e6e6;}"
        "QStatusBar::item{border:none;}");
    statusBar()->setSizeGripEnabled(false);

    QWidget *central = new QWidget;
    setCentralWidget(central);

    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // ===================== BARRE DU HAUT =====================
    QFrame *top = new QFrame;
    top->setFixedHeight(65);
    top->setObjectName("topBar");
    top->setStyleSheet(
        "QFrame#topBar{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #d40000,stop:1 #8f0000);"
        "border-bottom:3px solid #6e0000;}");

    QHBoxLayout *topLayout = new QHBoxLayout(top);

    topLayout->addStretch();

    QLabel *horloge = new QLabel;
    horloge->setStyleSheet("color:white;font-size:12px;");
    auto majHorloge = [horloge]() {
        horloge->setText(QLocale(QLocale::French).toString(
            QDateTime::currentDateTime(), "dddd dd MMMM yyyy  •  HH:mm"));
    };
    majHorloge();
    QTimer *timerHorloge = new QTimer(this);
    connect(timerHorloge, &QTimer::timeout, this, majHorloge);
    timerHorloge->start(30000);
    topLayout->addWidget(horloge);
    topLayout->addSpacing(18);

    QPushButton *cloche = new QPushButton("🔔  0");
    cloche->setObjectName("btnCloche");
    cloche->setCursor(Qt::PointingHandCursor);
    cloche->setStyleSheet(
        "QPushButton{background:rgba(255,255,255,0.18);color:white;border:none;"
        "border-radius:15px;padding:5px 14px;font-weight:bold;}"
        "QPushButton:hover{background:rgba(255,255,255,0.32);}");
    connect(cloche, &QPushButton::clicked, this, [=]() { afficherAlertes(true); });

    QGraphicsOpacityEffect *effCloche = new QGraphicsOpacityEffect(cloche);
    cloche->setGraphicsEffect(effCloche);
    QPropertyAnimation *pulse = new QPropertyAnimation(effCloche, "opacity", cloche);
    pulse->setObjectName("pulseCloche");
    pulse->setDuration(1100);
    pulse->setKeyValueAt(0.0, 1.0);
    pulse->setKeyValueAt(0.5, 0.4);
    pulse->setKeyValueAt(1.0, 1.0);
    pulse->setLoopCount(-1);

    QPushButton *btnEmail = new QPushButton("✉  Email");
    btnEmail->setCursor(Qt::PointingHandCursor);
    btnEmail->setToolTip("Paramètres de l'email d'alerte");
    btnEmail->setStyleSheet(
        "QPushButton{background:rgba(255,255,255,0.18);color:white;border:none;"
        "border-radius:15px;padding:5px 14px;font-weight:bold;}"
        "QPushButton:hover{background:rgba(255,255,255,0.32);}");
    connect(btnEmail, &QPushButton::clicked, this, [=]() { afficherParametresEmail(); });
    topLayout->addWidget(btnEmail);
    topLayout->addSpacing(10);
    topLayout->addWidget(cloche);
    topLayout->addSpacing(18);

    QLabel *admin = new QLabel("Admin  ▾");
    admin->setObjectName("lblUser");
    admin->setStyleSheet("color:white;font-size:12px;");
    topLayout->addWidget(admin);

    mainLayout->addWidget(top);

    // ===================== CORPS =====================
    QHBoxLayout *body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    // ===================== CONTENU =====================
    QWidget *content = new QWidget;
    content->setObjectName("pageBg");
    content->setStyleSheet("QWidget#pageBg{background:#f5f5f7;}");
    QVBoxLayout *contentLayout = new QVBoxLayout(content);

    QLabel *title = new QLabel("⚒  Gestion des véhicules");
    title->setStyleSheet("color:#b40000;font-size:20px;font-weight:bold;");
    contentLayout->addWidget(title);

    QLabel *description = new QLabel(
        "Consulter, ajouter, modifier et suivre les véhicules de la caserne.");
    description->setStyleSheet("color:#777;");
    contentLayout->addWidget(description);

    QSplitter *splitter = new QSplitter(Qt::Horizontal);

    // ===================== FORMULAIRE =====================
    QFrame *form = new QFrame;
    form->setObjectName("formCard");
    form->setStyleSheet(
        "QFrame#formCard{background:white;border:1px solid #ddd;border-radius:8px;}"
        "QLabel{border:none;background:transparent;}"
        "QLabel#fieldLabel{color:#444;font-weight:600;font-size:12px;}"
        "QLabel#sectionLabel{color:#a90000;font-weight:bold;font-size:11px;"
        "border-bottom:1px solid #f0c9c9;padding-bottom:2px;margin-top:0px;}"
        "QLineEdit,QComboBox,QSpinBox,QDateEdit{"
        "border:1px solid #ccc;border-radius:6px;padding:4px 8px;"
        "background:white;min-height:20px;}"
        "QLineEdit:focus,QComboBox:focus,QSpinBox:focus,QDateEdit:focus{"
        "border:1px solid #c40000;}"
        "QLineEdit[erreur=\"true\"],QComboBox[erreur=\"true\"]{"
        "border:1px solid #c62828;background:#fff5f5;}"
        "QComboBox::drop-down,QDateEdit::drop-down{border:none;width:24px;}"
        "QSpinBox::up-button,QSpinBox::down-button{border:none;width:18px;}");

    QVBoxLayout *formLayout = new QVBoxLayout(form);
    formLayout->setContentsMargins(16, 14, 16, 14);
    formLayout->setSpacing(8);

    QLabel *formTitle = new QLabel("Nouveau / Modifier véhicule");
    formTitle->setStyleSheet("color:#b40000;font-size:15px;font-weight:bold;border:none;");
    formLayout->addWidget(formTitle);

    nom = new QLineEdit;
    nom->setPlaceholderText("Ex : VSAV-01");

    type = new QComboBox;
    type->addItems({"Sélectionner un type", "Incendie", "Ambulance",
                    "Commandement", "Secours médical", "Intervention technique"});

    marque = new QLineEdit;
    marque->setPlaceholderText("Marque");

    modele = new QLineEdit;
    modele->setPlaceholderText("Modèle");

    quantite = new QSpinBox;
    quantite->setRange(0, 100);
    quantite->setSuffix(" %");
    quantite->setValue(80);

    seuil = new QSpinBox;
    seuil->setRange(0, 100);
    seuil->setSuffix(" %");
    seuil->setValue(25);
    seuil->setToolTip("Alerte si le carburant devient inférieur au minimum défini.");

    etat = new QComboBox;
    etat->addItems({"Disponible", "En mission", "En maintenance", "Hors service"});

    localisation = new QLineEdit;
    localisation->setPlaceholderText("Ex : Remise principale, Zone départ rapide");

    dateAchat = new QDateEdit(QDate::currentDate());
    dateAchat->setCalendarPopup(true);
    dateAchat->setDisplayFormat(FMT);

    dateMaintenance = new QDateEdit(QDate::currentDate());
    dateMaintenance->setCalendarPopup(true);
    dateMaintenance->setDisplayFormat(FMT);

    prochaineMaintenance = new QDateEdit(QDate::currentDate().addMonths(6));
    prochaineMaintenance->setCalendarPopup(true);
    prochaineMaintenance->setDisplayFormat(FMT);
    prochaineMaintenance->setMinimumDate(dateMaintenance->date().addDays(1));

    // "Prochaine maintenance" lazem tkoun ba3d "Dernière maintenance"
    connect(dateMaintenance, &QDateEdit::dateChanged, this, [=](const QDate &d) {
        prochaineMaintenance->setMinimumDate(d.addDays(1));
    });

    remarques = new QLineEdit;
    remarques->setPlaceholderText("Remarques (optionnel)");

    auto champ = [](const QString &texte, QWidget *w) -> QWidget * {
        QWidget *c = new QWidget;
        QVBoxLayout *l = new QVBoxLayout(c);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(3);
        QLabel *lb = new QLabel(texte);
        lb->setObjectName("fieldLabel");
        l->addWidget(lb);
        l->addWidget(w);
        return c;
    };

    QGridLayout *grid = new QGridLayout;
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);

    auto section = [](const QString &t) -> QLabel * {
        QLabel *l = new QLabel(t);
        l->setObjectName("sectionLabel");
        return l;
    };

    grid->addWidget(section("INFORMATIONS"), 0, 0, 1, 2);
    grid->addWidget(champ("Matricule / Nom du véhicule *", nom), 1, 0);
    grid->addWidget(champ("Type *", type),               1, 1);
    grid->addWidget(champ("Marque", marque),             2, 0);
    grid->addWidget(champ("Modèle", modele),             2, 1);

    grid->addWidget(section("DISPONIBILITÉ & ÉTAT"), 3, 0, 1, 2);
    grid->addWidget(champ("Carburant (%)", quantite),         4, 0);
    grid->addWidget(champ("Carburant min. (%)", seuil), 4, 1);
    grid->addWidget(champ("État", etat),                 5, 0);
    grid->addWidget(champ("Localisation", localisation), 5, 1);

    grid->addWidget(section("MAINTENANCE & REMARQUES"), 6, 0, 1, 2);
    grid->addWidget(champ("Date d'achat", dateAchat),    7, 0);
    grid->addWidget(champ("Dernière maintenance", dateMaintenance), 7, 1);
    grid->addWidget(champ("Prochaine maintenance", prochaineMaintenance), 8, 0);
    grid->addWidget(champ("Remarques", remarques),       8, 1);

    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    formLayout->addLayout(grid);

    connect(nom, &QLineEdit::textChanged, this, [=]() { marquerErreur(nom, false); });
    connect(type, &QComboBox::currentIndexChanged, this, [=]() { marquerErreur(type, false); });

    // boutons
    QPushButton *add     = new QPushButton("+  Ajouter");
    QPushButton *edit    = new QPushButton("✏  Modifier");

    QString btnPlein =
        "QPushButton{background:#c40000;color:white;border:none;border-radius:6px;"
        "padding:6px 14px;font-weight:bold;}"
        "QPushButton:hover{background:#8f0000;}";
    QString btnContour =
        "QPushButton{background:white;color:#c40000;border:1px solid #c40000;"
        "border-radius:6px;padding:6px 14px;font-weight:bold;}"
        "QPushButton:hover{background:#f5dcdc;}";

    add->setStyleSheet(btnPlein);
    edit->setStyleSheet(btnContour);

    for (QPushButton *b : {add, edit})
        b->setFixedHeight(34);

    QHBoxLayout *buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    buttons->addWidget(add);
    buttons->addWidget(edit);
    formLayout->addLayout(buttons);
    formLayout->addStretch();

    // carte "priorité de maintenance" : remplit le bas du formulaire
    {
        QFrame *prio = new QFrame;
        prio->setObjectName("prioCard");
        prio->setStyleSheet(
            "QFrame#prioCard{background:#fff5f5;border:1px solid #f0c9c9;"
            "border-left:4px solid #c40000;border-radius:8px;"
            "border-top-left-radius:2px;border-bottom-left-radius:2px;}"
            "QLabel{border:none;background:transparent;}");
        QVBoxLayout *pl = new QVBoxLayout(prio);
        pl->setContentsMargins(14, 10, 14, 12);
        pl->setSpacing(6);

        QLabel *pt = new QLabel("Priorité de maintenance");
        pt->setStyleSheet("color:#a90000;font-weight:bold;font-size:12px;");
        QLabel *pn = new QLabel("—");
        pn->setObjectName("prioNom");
        pn->setTextFormat(Qt::RichText);
        pn->setWordWrap(true);
        pn->setStyleSheet("color:#333;font-size:12px;");
        QProgressBar *pb = new QProgressBar;
        pb->setObjectName("prioBar");
        pb->setRange(0, 100);
        pb->setTextVisible(false);
        pb->setFixedHeight(8);
        QPushButton *pv = new QPushButton("Voir la maintenance prédictive");
        pv->setCursor(Qt::PointingHandCursor);
        pv->setStyleSheet(
            "QPushButton{background:white;color:#c40000;border:1px solid #c40000;border-radius:6px;"
            "padding:5px 12px;font-weight:bold;font-size:11px;}"
            "QPushButton:hover{background:#c40000;color:white;}");
        connect(pv, &QPushButton::clicked, this, [=]() { afficherMaintenancePredictive(); });

        pl->addWidget(pt);
        pl->addWidget(pn);
        pl->addWidget(pb);
        pl->addWidget(pv, 0, Qt::AlignLeft);
        formLayout->addWidget(prio);
    }

    splitter->addWidget(form);

    // ===================== TABLEAU =====================
    QFrame *tableFrame = new QFrame;
    tableFrame->setObjectName("tableCard");
    tableFrame->setStyleSheet(
        "QFrame#tableCard{background:white;border:1px solid #ddd;border-radius:8px;}"
        "QLabel{border:none;}"
        "QLineEdit{border:1px solid #ccc;border-radius:8px;padding:7px 12px;background:white;}"
        "QLineEdit:focus{border:1px solid #c40000;}"
        "QComboBox{border:1px solid #ccc;border-radius:8px;padding:5px 10px;background:white;}"
        "QComboBox:focus{border:1px solid #c40000;}"
        "QComboBox::drop-down{border:none;width:22px;}");

    QVBoxLayout *tableLayout = new QVBoxLayout(tableFrame);

    QLabel *listTitle = new QLabel("☷  Liste des véhicules");
    listTitle->setStyleSheet("color:#b40000;font-size:14px;font-weight:bold;");
    QPushButton *btnForm = new QPushButton("◀  Masquer le formulaire");
    btnForm->setObjectName("btnForm");
    btnForm->setCursor(Qt::PointingHandCursor);
    btnForm->setStyleSheet(
        "QPushButton{background:white;color:#c40000;border:1px solid #c40000;border-radius:6px;"
        "padding:4px 12px;font-weight:bold;font-size:11px;}"
        "QPushButton:hover{background:#c40000;color:white;}");
    QHBoxLayout *entete = new QHBoxLayout;
    entete->addWidget(listTitle);
    entete->addStretch();
    entete->addWidget(btnForm);
    tableLayout->addLayout(entete);

    // cartes statistiques
    auto carte = [](const QString &id, const QString &caption, const QString &couleur) -> QFrame * {
        QFrame *f = new QFrame;
        f->setObjectName("card_" + id);
        f->setFixedHeight(64);
        f->setStyleSheet(QString(
                             "QFrame#card_%1{background:white;border:1px solid #eee;"
                             "border-left:4px solid %2;border-radius:8px;border-top-left-radius:2px;border-bottom-left-radius:2px;}"
                             "QLabel{border:none;background:transparent;}").arg(id, couleur));
        QVBoxLayout *l = new QVBoxLayout(f);
        l->setContentsMargins(12, 6, 12, 6);
        l->setSpacing(0);
        QLabel *v = new QLabel("0");
        v->setObjectName("val_" + id);
        v->setStyleSheet(QString("font-size:20px;font-weight:bold;color:%1;").arg(couleur));
        QLabel *c = new QLabel(caption);
        c->setStyleSheet("color:#777;font-size:11px;");
        l->addWidget(v);
        l->addWidget(c);
        return f;
    };
    QHBoxLayout *cardsLayout = new QHBoxLayout;
    cardsLayout->setSpacing(10);
    cardsLayout->addWidget(carte("total", "🚗  Véhicules", "#c40000"));
    cardsLayout->addWidget(carte("dispo", "✅  Disponibles", "#2e7d32"));
    cardsLayout->addWidget(carte("hs",    "⛔  Hors service", "#c62828"));
    cardsLayout->addWidget(carte("alert", "🔔  Alertes", "#ef6c00"));
    tableLayout->addLayout(cardsLayout);

    QLabel *legende = new QLabel(
        "🔴 Alerte état : hors service     "
        "🟠 Alerte Carburant (%) : carburant faible");
    legende->setStyleSheet("color:#666;font-size:11px;");
    tableLayout->addWidget(legende);

    QPushButton *btnPdf     = new QPushButton("⬇ PDF");
    QPushButton *btnStat    = new QPushButton("📊 Statistiques");
    QPushButton *btnAlert   = new QPushButton("🔔 Alertes");
    QPushButton *btnJournal = new QPushButton("📜 Journal");
    QPushButton *btnMaint   = new QPushButton("🛠 Maintenance prédictive");

    btnPdf->setStyleSheet(styleBoutonCouleur("#1565c0", "#0d47a1", "8px 12px"));
    btnStat->setStyleSheet(styleBoutonCouleur("#2e7d32", "#1b5e20", "8px 12px"));
    btnAlert->setStyleSheet(styleBoutonCouleur("#c62828", "#8e1b1b", "8px 12px"));
    btnJournal->setStyleSheet(styleBoutonCouleur("#6a1b9a", "#4a148c", "8px 12px"));
    btnMaint->setStyleSheet(styleBoutonCouleur("#00897b", "#00695c", "8px 12px"));

    triCombo = new QComboBox;
    triCombo->addItems({"Nom", "État", "Date de maintenance", "Carburant (%)"});
    triCombo->setCurrentIndex(-1);
    triCombo->setPlaceholderText("Choisir un critère");

    QHBoxLayout *toolsLayout = new QHBoxLayout;
    toolsLayout->addWidget(btnPdf);
    toolsLayout->addWidget(btnStat);
    toolsLayout->addWidget(btnAlert);
    toolsLayout->addWidget(btnJournal);
    toolsLayout->addWidget(btnMaint);
    toolsLayout->addStretch();
    toolsLayout->addWidget(new QLabel("Trier par :"));
    toolsLayout->addWidget(triCombo);
    tableLayout->addLayout(toolsLayout);

    recherche = new QLineEdit;
    recherche->setPlaceholderText("🔍  Rechercher un véhicule...");

    filtreType = new QComboBox;
    filtreType->addItems({"Tous les types", "Incendie", "Ambulance",
                          "Commandement", "Secours médical", "Intervention technique"});

    QHBoxLayout *searchLayout = new QHBoxLayout;
    searchLayout->addWidget(recherche, 1);
    searchLayout->addWidget(new QLabel("Type :"));
    searchLayout->addWidget(filtreType);
    tableLayout->addLayout(searchLayout);

    table = new QTableWidget(0, 15);
    table->setHorizontalHeaderLabels({
        "ID", "Nom", "Type", "Carburant", "Carburant min.", "État", "Localisation",
        "Date achat", "Dern. maint.", "Proch. maint.", "Actions",
        "Marque", "Modèle", "Remarques", "Santé"
    });

    for (int c = 0; c < 10; c++)
        table->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(10, QHeaderView::Fixed);
    table->setColumnWidth(10, 112);
    table->horizontalHeader()->setMinimumSectionSize(40);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    table->setColumnHidden(7, true);    // "Date achat" : visible dans le formulaire, masquée dans la liste
    table->setColumnHidden(11, true);
    table->setColumnHidden(12, true);
    table->setColumnHidden(13, true);

    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setFocusPolicy(Qt::NoFocus);

    // clic droit sur l'en-tête : choisir les colonnes à afficher
    table->horizontalHeader()->setToolTip("Clic droit : afficher / masquer des colonnes");
    table->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table->horizontalHeader(), &QWidget::customContextMenuRequested, this, [=](const QPoint &pos) {
        QMenu menuCols(this);
        QAction *titreMenu = menuCols.addAction("Colonnes affichées");
        titreMenu->setEnabled(false);
        menuCols.addSeparator();
        const QVector<int> cols = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 14};
        for (int c : cols) {
            QAction *a = menuCols.addAction(table->horizontalHeaderItem(c)->text());
            a->setCheckable(true);
            a->setChecked(!table->isColumnHidden(c));
            connect(a, &QAction::toggled, this, [=](bool on) { table->setColumnHidden(c, !on); });
        }
        menuCols.exec(table->horizontalHeader()->mapToGlobal(pos));
    });
    table->setMouseTracking(true);
    table->verticalHeader()->setDefaultSectionSize(44);
    table->setItemDelegateForColumn(5, new EtatDelegate(table));
    table->setItemDelegateForColumn(14, new SanteDelegate(table));
    table->setItemDelegate(new HoverDelegate(table));       // colonnes "normales" : survol de la ligne
    table->viewport()->installEventFilter(new EvtFilter(table, [this](QObject *, QEvent *e) {
        if (e->type() == QEvent::MouseMove) {
            QMouseEvent *me = static_cast<QMouseEvent *>(e);
            int r = table->rowAt(int(me->position().y()));
            if (r != g_hoverRow) { g_hoverRow = r; table->viewport()->update(); }
        } else if (e->type() == QEvent::Leave) {
            if (g_hoverRow != -1) { g_hoverRow = -1; table->viewport()->update(); }
        }
        return false;
    }));
    table->horizontalHeader()->setSectionResizeMode(14, QHeaderView::Fixed);
    table->setColumnWidth(14, 104);
    table->horizontalHeader()->moveSection(14, 10);   // "Santé" juste avant "Actions"
    table->setStyleSheet(
        "QTableWidget{border:none;background:white;alternate-background-color:#fafafa;"
        "selection-background-color:#f3c9c9;selection-color:black;}"
        "QHeaderView::section{background:#fbeaea;color:#a90000;font-weight:bold;"
        "border:none;border-bottom:2px solid #c40000;padding:8px 6px;}");

    // ===================== DONNEES DE DEMONSTRATION =====================
    QStringList noms = {
        "VSAV-01", "FPT-02", "CCF-03", "VLCG-04",
        "VTU-05", "EPA-06", "VSR-07", "VPC-08"
    };
    QStringList marques = {
        "Renault Trucks", "Mercedes-Benz", "Iveco", "Peugeot",
        "Ford", "MAN", "Scania", "Citroën"
    };
    QStringList etats = {
        "Disponible", "En maintenance", "Hors service", "Disponible",
        "En mission", "Disponible", "En maintenance", "En mission"
    };
    QStringList lieux = {
        "Remise principale", "Aire de stationnement", "Zone départ rapide", "Aire de stationnement",
        "Magasin", "Aire de stationnement", "Remise principale", "Atelier mécanique"
    };
    int carburants[8] = {85, 60, 15, 90, 25, 75, 45, 55};
    int seuils[8] = {30, 25, 30, 20, 25, 25, 30, 25};

    QDate today = QDate::currentDate();
    for (int i = 0; i < noms.size(); i++) {
        QDate pm = today.addMonths(6);
        if (i == 2) pm = today.addDays(10);   // maintenance proche
        if (i == 4) pm = today.addDays(-5);   // maintenance dépassée
        ajouterLigne(noms[i], "Incendie", marques[i], "Intervention",
                     carburants[i], seuils[i], etats[i], lieux[i],
                     today.addYears(-1).toString(FMT),
                     today.addMonths(-6).toString(FMT),
                     pm.toString(FMT), "");
    }

    // message affiché quand le tableau est vide / sans résultat
    {
        QLabel *vide = new QLabel(table->viewport());
        vide->setObjectName("labelVide");
        vide->setAlignment(Qt::AlignCenter);
        vide->setStyleSheet("color:#b0b0b0;font-size:14px;");
        vide->hide();
        QVBoxLayout *vl = new QVBoxLayout(table->viewport());
        vl->addWidget(vide, 0, Qt::AlignCenter);
    }

    tableLayout->addWidget(table);

    // ===================== EXPORT PDF =====================
    connect(btnPdf, &QPushButton::clicked, this, [=]() {
        QString file = QFileDialog::getSaveFileName(
            this, "Exporter en PDF",
            QDir::homePath() + "/Documents/vehicules.pdf", "PDF (*.pdf)");
        if (file.isEmpty()) return;

        QString html = "<h2 style='color:#c40000'>Inventaire des véhicules</h2>"
                       "<table border='1' cellspacing='0' cellpadding='4'><tr>";
        for (int c = 0; c < 10; c++)
            html += "<th>" + table->horizontalHeaderItem(c)->text() + "</th>";
        html += "</tr>";
        for (int r = 0; r < table->rowCount(); r++) {
            html += "<tr>";
            for (int c = 0; c < 10; c++)
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
        journaliser("Export", "Liste des véhicules",
                    QString("Export PDF de l'inventaire (%1 véhicules)").arg(table->rowCount()));
    });

    // ===================== STATISTIQUES =====================
    connect(btnStat, &QPushButton::clicked, this, [=]() { afficherStatistiques(this, table, journal); });

    connect(btnAlert, &QPushButton::clicked, this, [=]() { afficherAlertes(true); });
    connect(btnJournal, &QPushButton::clicked, this, [=]() { afficherJournal(); });
    connect(btnMaint, &QPushButton::clicked, this, [=]() { afficherMaintenancePredictive(); });

    splitter->addWidget(tableFrame);
    splitter->setSizes({340, 960});

    // le formulaire se replie / se déplie avec une animation
    connect(btnForm, &QPushButton::clicked, this, [=]() {
        bool cible = !btnForm->property("replie").toBool();     // true = on replie
        btnForm->setProperty("replie", cible);
        if (cible)
            btnForm->setProperty("largeur", form->width());
        int plein = btnForm->property("largeur").isValid() ? btnForm->property("largeur").toInt() : 340;
        if (!cible) form->show();

        QVariantAnimation *a = new QVariantAnimation(form);
        a->setDuration(380);
        a->setStartValue(cible ? form->width() : 0);
        a->setEndValue(cible ? 0 : plein);
        a->setEasingCurve(QEasingCurve::InOutCubic);
        connect(a, &QVariantAnimation::valueChanged, form, [=](const QVariant &v) {
            int w = v.toInt();
            form->setMaximumWidth(w);
            splitter->setSizes({w, qMax(1, splitter->width() - w)});
        });
        connect(a, &QVariantAnimation::finished, form, [=]() {
            if (cible) form->hide();
            else       form->setMaximumWidth(QWIDGETSIZE_MAX);
        });
        btnForm->setText(cible ? "▶  Afficher le formulaire" : "◀  Masquer le formulaire");
        a->start(QAbstractAnimation::DeleteWhenStopped);
    });

    contentLayout->addWidget(splitter, 1);
    contentLayout->setAlignment(title, Qt::AlignTop);
    contentLayout->setAlignment(description, Qt::AlignTop);
    body->addWidget(content, 1);
    mainLayout->addLayout(body);

    // ===================== CONNEXIONS =====================
    connect(add,     &QPushButton::clicked, this, &vehicules::ajouter);
    connect(edit,    &QPushButton::clicked, this, &vehicules::modifier);
    connect(recherche, &QLineEdit::textChanged, this, &vehicules::rechercher);
    connect(filtreType, &QComboBox::currentIndexChanged, this, &vehicules::rechercher);
    connect(triCombo, &QComboBox::currentIndexChanged, this, &vehicules::trier);

    connect(table->model(), &QAbstractItemModel::rowsRemoved, this, [=]() { majCartes(); });

    colorierLignes();

    journaliser("Système", "—",
                QString("Application démarrée  •  %1 véhicules chargés").arg(table->rowCount()));

    // lueur au survol des boutons
    for (QPushButton *b : {btnPdf, btnStat, btnAlert, btnJournal, btnMaint})
        survolBouton(b, QColor(245, 166, 35, 200));
    survolBouton(add, QColor(196, 0, 0, 170));
    survolBouton(edit, QColor(196, 0, 0, 170));
    survolBouton(btnEmail, QColor(255, 255, 255, 150));


    // ----- animations d'entrée de l'interface visible -----
    apparition(form, 450);
    apparition(tableFrame, 650);

    // glissement des panneaux
    diapo(form, QPoint(-80, 0), 450);
    diapo(tableFrame, QPoint(100, 0), 650);

    {   // le titre s'écrit lettre par lettre
        const QString plein = title->text();
        title->setMinimumHeight(title->sizeHint().height());
        title->setText(QString());
        QVariantAnimation *typo = new QVariantAnimation(title);
        typo->setDuration(900);
        typo->setStartValue(0);
        typo->setEndValue(int(plein.size()));
        connect(typo, &QVariantAnimation::valueChanged, title, [title, plein](const QVariant &v) {
            title->setText(plein.left(v.toInt()));
        });
        connect(typo, &QVariantAnimation::finished, title, [title, plein]() { title->setText(plein); });
        QTimer::singleShot(350, typo, [typo]() { typo->start(QAbstractAnimation::DeleteWhenStopped); });
    }

    // la fenêtre apparaît en fondu
    setWindowOpacity(0.0);
    QTimer::singleShot(0, this, [=]() {
        QPropertyAnimation *fade = new QPropertyAnimation(this, "windowOpacity", this);
        fade->setDuration(650);
        fade->setStartValue(0.0);
        fade->setEndValue(1.0);
        fade->setEasingCurve(QEasingCurve::OutCubic);
        fade->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

// =====================================================================
//  ACTIONS CRUD : creation des lignes, ajout, modification et suppression
// =====================================================================
void vehicules::ajouterLigne(const QString &n, const QString &t, const QString &m,
                         const QString &mod, int q, int s, const QString &e,
                         const QString &loc, const QString &da, const QString &dm,
                         const QString &pm, const QString &rem)
{
    int row = table->rowCount();
    table->insertRow(row);

    table->setItem(row, 0, new QTableWidgetItem(QString("VEH-%1").arg(prochainId++, 3, 10, QChar('0'))));
    table->setItem(row, 1, new QTableWidgetItem(n));
    table->setItem(row, 2, new QTableWidgetItem(t));
    table->setItem(row, 3, new NumItem(QString::number(q)));
    table->setItem(row, 4, new NumItem(QString::number(s)));
    table->setItem(row, 5, new QTableWidgetItem(e));
    table->setItem(row, 6, new QTableWidgetItem(loc));
    table->setItem(row, 7, new DateItem(da));
    table->setItem(row, 8, new DateItem(dm));
    table->setItem(row, 9, new DateItem(pm));
    table->setItem(row, 11, new QTableWidgetItem(m));
    table->setItem(row, 12, new QTableWidgetItem(mod));
    table->setItem(row, 13, new QTableWidgetItem(rem));
    table->setItem(row, 14, new NumItem("0"));

    QWidget *actions = new QWidget;
    actions->setObjectName("actCell");
    QHBoxLayout *actLayout = new QHBoxLayout(actions);
    actLayout->setContentsMargins(2, 2, 2, 2);
    actLayout->setSpacing(4);

    QPushButton *btnEdit = new QPushButton("Modifier");
    QPushButton *btnDel  = new QPushButton("Supprimer");
    btnEdit->setStyleSheet("QPushButton{background:white;color:#c40000;border:1px solid #c40000;border-radius:4px;padding:2px 4px;font-size:11px;}"
                           "QPushButton:hover{background:#c40000;color:white;}");
    btnDel->setStyleSheet("QPushButton{background:#c40000;color:white;border:none;border-radius:4px;padding:2px 4px;font-size:11px;}"
                          "QPushButton:hover{background:#8f0000;}");
    btnEdit->setMinimumWidth(48);
    btnDel->setMinimumWidth(48);
    actLayout->addWidget(btnEdit);
    actLayout->addWidget(btnDel);

    connect(btnDel, &QPushButton::clicked, this, [=]() {
        int r = table->indexAt(actions->mapTo(table->viewport(), QPoint(0, 0))).row();
        if (r < 0) return;
        if (QMessageBox::question(this, "Suppression", "Supprimer ce véhicule ?") == QMessageBox::Yes) {
            QString ref = table->item(r, 0)->text() + "  " + table->item(r, 1)->text();
            QString det = QString("Type : %1  •  Carburant (%) : %2  •  État : %3")
                              .arg(table->item(r, 2)->text(), table->item(r, 3)->text(), table->item(r, 5)->text());
            table->removeRow(r);
            journaliser("Suppression", ref, det);
            toast(this, "🗑  Véhicule supprimé", "#c62828");
        }
    });

    connect(btnEdit, &QPushButton::clicked, this, [=]() {
        int r = table->indexAt(actions->mapTo(table->viewport(), QPoint(0, 0))).row();
        if (r < 0) return;
        if (QPushButton *t = findChild<QPushButton *>("btnForm"))
            if (t->property("replie").toBool())
                t->click();                       // réaffiche le formulaire replié
        chargerFormulaire(r);
        table->selectRow(r);
    });

    table->setCellWidget(row, 10, actions);
}

// =====================================================================
void vehicules::chargerFormulaire(int r)
{
    nom->setText(table->item(r, 1)->text());
    type->setCurrentText(table->item(r, 2)->text());
    quantite->setValue(table->item(r, 3)->text().toInt());
    seuil->setValue(table->item(r, 4)->text().toInt());
    etat->setCurrentText(table->item(r, 5)->text());
    localisation->setText(table->item(r, 6)->text());

    QDate d1 = QDate::fromString(table->item(r, 7)->text(), FMT);
    QDate d2 = QDate::fromString(table->item(r, 8)->text(), FMT);
    QDate d3 = QDate::fromString(table->item(r, 9)->text(), FMT);
    if (d1.isValid()) dateAchat->setDate(d1);
    if (d2.isValid()) dateMaintenance->setDate(d2);
    if (d3.isValid()) prochaineMaintenance->setDate(d3);

    marque->setText(table->item(r, 11)->text());
    modele->setText(table->item(r, 12)->text());
    remarques->setText(table->item(r, 13)->text());
}

// =====================================================================
void vehicules::ajouter()
{
    bool okNom  = !nom->text().trimmed().isEmpty();
    bool okType = type->currentIndex() != 0;
    marquerErreur(nom, !okNom);
    marquerErreur(type, !okType);
    if (!okNom || !okType) {
        toast(this, "⚠  Le nom et le type sont obligatoires", "#c62828");
        return;
    }
    if (prochaineMaintenance->date() <= dateMaintenance->date()) {
        toast(this, "⚠  La prochaine maintenance doit être après la dernière", "#c62828");
        return;
    }

    ajouterLigne(nom->text().trimmed(), type->currentText(), marque->text(), modele->text(),
                 quantite->value(), seuil->value(), etat->currentText(), localisation->text(),
                 dateAchat->date().toString(FMT),
                 dateMaintenance->date().toString(FMT),
                 prochaineMaintenance->date().toString(FMT),
                 remarques->text());

    {
        int nr = table->rowCount() - 1;
        QString ref = table->item(nr, 0)->text() + "  " + table->item(nr, 1)->text();
        journaliser("Ajout", ref,
                    QString("Type : %1  •  Carburant (%) : %2  •  État : %3  •  Localisation : %4")
                        .arg(type->currentText()).arg(quantite->value())
                        .arg(etat->currentText(), localisation->text()));
        QStringList motifs;
        if (etat->currentText() == "Hors service")
            motifs << "Alerte état : véhicule hors service";
        if (quantite->value() < seuil->value())
            motifs << QString("Alerte Carburant (%) : carburant faible (%1 / %2)")
                          .arg(quantite->value()).arg(seuil->value());
        if (!motifs.isEmpty())
            envoyerAlerteEmail("[Smart Fire Station] Alerte véhicule : " + ref,
                               corpsEmail(QStringList{"• " + ref + " — " + motifs.join(" ; ")}), ref);
    }

    flashLigne(table, table->rowCount() - 1, [this]() { colorierLignes(); });

    if (quantite->value() < seuil->value())
        toast(this, QString("✓  Véhicule ajouté   •   ⚠ carburant faible (%1 / %2)")
                        .arg(quantite->value()).arg(seuil->value()), "#ef6c00");
    else
        toast(this, "✓  Véhicule ajouté");

    nom->clear();
    marque->clear();
    modele->clear();
    remarques->clear();
    localisation->clear();
    quantite->setValue(1);
    seuil->setValue(2);
    type->setCurrentIndex(0);
}

// =====================================================================
void vehicules::modifier()
{
    int r = table->currentRow();
    if (r < 0) {
        toast(this, "⚠  Sélectionnez d'abord une ligne (bouton Modif.)", "#ef6c00");
        return;
    }
    bool okNom  = !nom->text().trimmed().isEmpty();
    bool okType = type->currentIndex() != 0;
    marquerErreur(nom, !okNom);
    marquerErreur(type, !okType);
    if (!okNom || !okType) {
        toast(this, "⚠  Le nom et le type sont obligatoires", "#c62828");
        return;
    }
    if (prochaineMaintenance->date() <= dateMaintenance->date()) {
        toast(this, "⚠  La prochaine maintenance doit être après la dernière", "#c62828");
        return;
    }

    struct Champ { QString label; int col; QString nouveau; };
    QVector<Champ> champs = {
        {"Nom", 1, nom->text().trimmed()},
        {"Type", 2, type->currentText()},
        {"Carburant (%)", 3, QString::number(quantite->value())},
        {"Carburant min.", 4, QString::number(seuil->value())},
        {"État", 5, etat->currentText()},
        {"Localisation", 6, localisation->text()},
        {"Date d'achat", 7, dateAchat->date().toString(FMT)},
        {"Dernière maintenance", 8, dateMaintenance->date().toString(FMT)},
        {"Prochaine maintenance", 9, prochaineMaintenance->date().toString(FMT)},
        {"Marque", 11, marque->text()},
        {"Modèle", 12, modele->text()},
        {"Remarques", 13, remarques->text()}
    };

    QStringList changements;
    bool alerteConcernee = false;
    for (const Champ &c : champs) {
        QString ancien = table->item(r, c.col)->text();
        if (ancien != c.nouveau) {
            changements << QString("%1 : %2 → %3").arg(c.label, ancien, c.nouveau);
            if (c.col == 3 || c.col == 4 || c.col == 5) alerteConcernee = true;
            table->item(r, c.col)->setText(c.nouveau);
        }
    }

    QString ref = table->item(r, 0)->text() + "  " + table->item(r, 1)->text();
    if (!changements.isEmpty())
        journaliser("Modification", ref, changements.join("  •  "));
    if (alerteConcernee) {
        QStringList motifs;
        if (etat->currentText() == "Hors service")
            motifs << "Alerte état : véhicule hors service";
        if (quantite->value() < seuil->value())
            motifs << QString("Alerte Carburant (%) : carburant faible (%1 / %2)")
                          .arg(quantite->value()).arg(seuil->value());
        if (!motifs.isEmpty())
            envoyerAlerteEmail("[Smart Fire Station] Alerte véhicule : " + ref,
                               corpsEmail(QStringList{"• " + ref + " — " + motifs.join(" ; ")}), ref);
    }

    colorierLignes();

    flashLigne(table, r, [this]() { colorierLignes(); });

    if (quantite->value() < seuil->value())
        toast(this, QString("✓  Véhicule modifié   •   ⚠ carburant faible (%1 / %2)")
                        .arg(quantite->value()).arg(seuil->value()), "#ef6c00");
    else
        toast(this, "✓  Véhicule modifié");
}

// =====================================================================
void vehicules::rechercher()
{
    QString texte = recherche->text();
    QString typeFiltre = filtreType->currentIndex() > 0 ? filtreType->currentText() : QString();

    for (int row = 0; row < table->rowCount(); row++) {
        bool found = false;
        for (int col = 0; col < table->columnCount(); col++) {
            if (table->item(row, col) &&
                table->item(row, col)->text().contains(texte, Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }
        bool typeOk = typeFiltre.isEmpty() || table->item(row, 2)->text() == typeFiltre;
        table->setRowHidden(row, !(found && typeOk));
    }
    majEtatVide(table);
}

// =====================================================================
void vehicules::trier()
{
    int colonne;
    switch (triCombo->currentIndex()) {
    case 0: colonne = 1; break;   // Nom
    case 1: colonne = 5; break;   // État
    case 2: colonne = 9; break;   // Prochaine maintenance
    case 3: colonne = 3; break;   // Carburant (%)
    default: return;
    }

    table->sortItems(colonne, Qt::AscendingOrder);
    colorierLignes();
    rechercher();
}

// =====================================================================
//  Couleur des lignes selon l'alerte (UNE SEULE alerte : état ou Carburant (%))
//    rouge  : alerte état  -> véhicule hors service
//    orange : alerte Carburant (%) -> Carburant (%) < seuil minimum
//  + calcul du score de santé de chaque véhicule (maintenance prédictive)
// =====================================================================
void vehicules::colorierLignes()
{
    for (int r = 0; r < table->rowCount(); r++) {
        if (!table->item(r, 5) || !table->item(r, 3) || !table->item(r, 4)
            || !table->item(r, 7) || !table->item(r, 8) || !table->item(r, 9))
            continue;

        QString e = table->item(r, 5)->text();
        int q = table->item(r, 3)->text().toInt();
        int s = table->item(r, 4)->text().toInt();

        QBrush brush(Qt::NoBrush);
        if (e == "Hors service")
            brush = QBrush(QColor("#f8d7da"));
        else if (q < s)
            brush = QBrush(QColor("#ffe0b2"));

        // score de santé (colonne 14)
        if (table->item(r, 14)) {
            QDate da = QDate::fromString(table->item(r, 7)->text(), FMT);
            QDate dm = QDate::fromString(table->item(r, 8)->text(), FMT);
            QDate pm = QDate::fromString(table->item(r, 9)->text(), FMT);
            table->item(r, 14)->setText(QString::number(scoreSante(e, da, dm, pm)));
        }

        for (int c = 0; c < table->columnCount(); c++)
            if (table->item(r, c))
                table->item(r, c)->setBackground(brush);

        // la cellule "Actions" prend la même couleur que la ligne
        if (QWidget *w = table->cellWidget(r, 10))
            w->setStyleSheet(brush.style() == Qt::NoBrush
                                 ? QString("QWidget#actCell{background:transparent;}")
                                 : QString("QWidget#actCell{background:%1;}").arg(brush.color().name()));
    }
    majCartes();
}

// =====================================================================
//  MÉTIER 1 : alerte automatique unique (Carburant (%) OU état)
//  Notification par EMAIL (voir envoyerAlerteEmail)
// =====================================================================
void vehicules::afficherAlertes(bool afficherSiVide)
{
    QDate today = QDate::currentDate();
    using Liste = QVector<QPair<QString, QString>>;
    Liste hs, stock;
    int concernes = 0;

    for (int r = 0; r < table->rowCount(); r++) {
        if (!table->item(r, 1) || !table->item(r, 5) || !table->item(r, 3) || !table->item(r, 4))
            continue;

        QString n   = table->item(r, 0)->text() + "  " + table->item(r, 1)->text();
        QString e   = table->item(r, 5)->text();
        QString loc = table->item(r, 6) ? table->item(r, 6)->text() : QString();
        int q = table->item(r, 3)->text().toInt();
        int s = table->item(r, 4)->text().toInt();

        bool alerte = false;
        if (e == "Hors service") {
            hs.push_back(qMakePair(n, QString("État : Hors service") +
                                          (loc.isEmpty() ? QString() : "  •  Localisation : " + loc)));
            alerte = true;
        }
        if (q < s) {
            stock.push_back(qMakePair(n, QString("Carburant %1 % / minimum %2 %  •  il manque %3")
                                             .arg(q).arg(s).arg(s - q)));
            alerte = true;
        }
        if (alerte) concernes++;
    }

    if (concernes == 0) {
        if (afficherSiVide)
            toast(this, "✓  Aucune alerte pour le moment");
        return;
    }

    // contenu de l'email d'alerte
    QStringList lignesMail;
    for (const auto &x : hs)    lignesMail << "• [ÉTAT] " + x.first + " — " + x.second;
    for (const auto &x : stock) lignesMail << "• [CARBURANT] " + x.first + " — " + x.second;
    const QString sujetMail = QString("[Smart Fire Station] %1 véhicule(s) en alerte").arg(concernes);
    const QString corpsMail = corpsEmail(lignesMail);
    const QString refMail   = QString("%1 véhicule(s)").arg(concernes);

    if (!afficherSiVide) {      // alerte automatique : envoi de l'email, sans fenêtre
        envoyerAlerteEmail(sujetMail, corpsMail, refMail);
        return;
    }

    // ---------- fenêtre ----------
    QDialog *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle("Alerte véhicules");
    dlg->resize(580, 640);
    dlg->setStyleSheet("QDialog{background:#f4f4f6;} QLabel{border:none;background:transparent;}");

    QVBoxLayout *root = new QVBoxLayout(dlg);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QFrame *banner = new QFrame;
    banner->setStyleSheet("QFrame{background:#c40000;}");
    banner->setFixedHeight(76);
    QVBoxLayout *bl = new QVBoxLayout(banner);
    bl->setContentsMargins(24, 10, 24, 10);
    bl->setSpacing(0);
    QLabel *bt = new QLabel("🚨  Alerte véhicules");
    bt->setStyleSheet("color:white;font-size:20px;font-weight:bold;");
    QLabel *bs = new QLabel(QString("%1 véhicule(s) en alerte  •  %2").arg(concernes).arg(today.toString(FMT)));
    bs->setStyleSheet("color:#ffd6d6;font-size:12px;");
    bl->addWidget(bt);
    bl->addWidget(bs);
    root->addWidget(banner);

    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea{background:#f4f4f6;border:none;}");
    QWidget *inner = new QWidget;
    inner->setObjectName("alInner");
    inner->setStyleSheet("QWidget#alInner{background:#f4f4f6;}");
    QVBoxLayout *iv = new QVBoxLayout(inner);
    iv->setContentsMargins(20, 16, 20, 16);
    iv->setSpacing(14);

    auto section = [&](const QString &titre, const QString &couleur, const Liste &items) {
        if (items.isEmpty()) return;
        static int n = 0;
        QString name = QString("alCard%1").arg(n++);
        QFrame *f = new QFrame;
        f->setObjectName(name);
        f->setStyleSheet(QString(
                             "QFrame#%1{background:white;border:1px solid #e6e6e6;"
                             "border-left:5px solid %2;border-radius:10px;border-top-left-radius:2px;border-bottom-left-radius:2px;}"
                             "QLabel{border:none;background:transparent;}").arg(name, couleur));
        QVBoxLayout *l = new QVBoxLayout(f);
        l->setContentsMargins(16, 12, 16, 12);
        l->setSpacing(10);

        QHBoxLayout *head = new QHBoxLayout;
        QLabel *t = new QLabel(titre);
        t->setStyleSheet(QString("color:%1;font-size:14px;font-weight:bold;").arg(couleur));
        QLabel *badge = new QLabel(QString::number(items.size()));
        badge->setAlignment(Qt::AlignCenter);
        badge->setStyleSheet(QString(
                                 "background:%1;color:white;border-radius:10px;padding:2px 10px;font-weight:bold;").arg(couleur));
        head->addWidget(t);
        head->addStretch();
        head->addWidget(badge);
        l->addLayout(head);

        for (const auto &it : items) {
            QLabel *x = new QLabel(QString(
                                       "<b>%1</b><br><span style='color:#777;font-size:11px'>%2</span>")
                                       .arg(it.first.toHtmlEscaped(), it.second.toHtmlEscaped()));
            l->addWidget(x);
        }
        ombre(f);
        iv->addWidget(f);
    };

    section("🔴  Alerte état : hors service", "#c62828", hs);
    section("🟠  Alerte Carburant (%) : carburant faible", "#ef6c00", stock);

    iv->addStretch();
    scroll->setWidget(inner);
    root->addWidget(scroll, 1);

    QFrame *foot = new QFrame;
    foot->setStyleSheet("QFrame{background:white;border-top:1px solid #e6e6e6;}");
    QHBoxLayout *fl = new QHBoxLayout(foot);
    fl->setContentsMargins(20, 10, 20, 10);

    QLabel *canaux = new QLabel("✉  Canal de notification : email");
    canaux->setStyleSheet("color:#888;font-size:11px;");
    fl->addWidget(canaux);
    fl->addStretch();

    QPushButton *btnMail = new QPushButton("✉  Envoyer par email");
    btnMail->setFixedHeight(34);
    btnMail->setStyleSheet(styleBoutonCouleur("#2e7d32", "#1b5e20"));
    connect(btnMail, &QPushButton::clicked, dlg, [=]() {
        envoyerAlerteEmail(sujetMail, corpsMail, refMail, true);
    });
    fl->addWidget(btnMail);

    QPushButton *fermer = new QPushButton("Fermer");
    fermer->setFixedHeight(34);
    fermer->setMinimumWidth(110);
    fermer->setStyleSheet(
        "QPushButton{background:#c40000;color:white;border:none;border-radius:6px;"
        "padding:6px 18px;font-weight:bold;}"
        "QPushButton:hover{background:#8f0000;}");
    connect(fermer, &QPushButton::clicked, dlg, &QDialog::accept);
    fl->addWidget(fermer);
    root->addWidget(foot);

    fondu(dlg);
    dlg->exec();
}

// =====================================================================
//  Cartes statistiques + tableau de bord de l'accueil
// =====================================================================
void vehicules::majCartes()
{
    int total = 0, dispo = 0, hs = 0, alertes = 0, qteTotale = 0;
    int nbEtat[4] = {0, 0, 0, 0};   // Disponible, En mission, En maintenance, Hors service
    QStringList lignes;
    int minScore = 101;
    QString minNom, minInfo;

    for (int r = 0; r < table->rowCount(); r++) {
        if (!table->item(r, 5) || !table->item(r, 3) || !table->item(r, 4))
            continue;
        total++;
        QString nom = table->item(r, 0)->text() + "  " + table->item(r, 1)->text();
        QString e = table->item(r, 5)->text();
        int q = table->item(r, 3)->text().toInt();
        int s = table->item(r, 4)->text().toInt();
        qteTotale += q;

        if (table->item(r, 14) && table->item(r, 7) && table->item(r, 8) && table->item(r, 9)) {
            int sc = table->item(r, 14)->text().toInt();
            if (sc < minScore) {
                minScore = sc;
                minNom = nom;
                minInfo = facteursSante(e,
                                        QDate::fromString(table->item(r, 7)->text(), FMT),
                                        QDate::fromString(table->item(r, 8)->text(), FMT),
                                        QDate::fromString(table->item(r, 9)->text(), FMT)).join("  •  ");
            }
        }

        if (e == "Disponible")           { dispo++; nbEtat[0]++; }
        else if (e == "En mission")      nbEtat[1]++;
        else if (e == "En maintenance")  nbEtat[2]++;
        else if (e == "Hors service")    nbEtat[3]++;

        bool horsService = (e == "Hors service");
        bool bas = q < s;
        if (horsService) hs++;
        if (horsService || bas) alertes++;

        auto ligne = [&](const QString &icone, const QString &txt) {
            if (lignes.size() < 6)
                lignes << QString("<p style='margin:3px 0'>%1&nbsp; <b>%2</b> "
                                  "<span style='color:#777'>— %3</span></p>")
                              .arg(icone, nom.toHtmlEscaped(), txt);
        };
        if (horsService) ligne("🔴", "hors service");
        if (bas)         ligne("🟠", QString("carburant faible (%1 / %2)").arg(q).arg(s));
    }

    // les chiffres "roulent" de l'ancienne valeur vers la nouvelle
    auto set = [this](const QString &name, int v) {
        QLabel *l = findChild<QLabel *>(name);
        if (!l) return;
        int from = l->text().toInt();
        if (from == v) { l->setText(QString::number(v)); return; }
        for (QVariantAnimation *old : l->findChildren<QVariantAnimation *>()) {
            old->stop();
            old->deleteLater();
        }
        QVariantAnimation *a = new QVariantAnimation(l);
        a->setDuration(600);
        a->setStartValue(from);
        a->setEndValue(v);
        a->setEasingCurve(QEasingCurve::OutCubic);
        connect(a, &QVariantAnimation::valueChanged, l, [l](const QVariant &x) {
            l->setText(QString::number(x.toInt()));
        });
        connect(a, &QVariantAnimation::finished, l, [l, v]() { l->setText(QString::number(v)); });
        a->start(QAbstractAnimation::DeleteWhenStopped);
    };
    // cartes de la page véhicules
    set("val_total", total);
    set("val_dispo", dispo);
    set("val_hs", hs);
    set("val_alert", alertes);
    // cartes de la page Accueil
    set("val_a_total", total);
    set("val_a_dispo", dispo);
    set("val_a_hs", hs);
    set("val_a_alert", alertes);
    set("val_a_qte", qteTotale);

    if (QPushButton *b = findChild<QPushButton *>("btnCloche"))
        b->setText(QString("🔔  %1").arg(alertes));

    // carte "priorité de maintenance"
    if (QFrame *card = findChild<QFrame *>("prioCard")) {
        card->setVisible(minScore <= 100);
        if (QLabel *l = findChild<QLabel *>("prioNom"))
            l->setText(QString("<b>%1</b> — santé <b>%2 %</b><br>"
                               "<span style='color:#777;font-size:11px'>%3</span>")
                           .arg(minNom.toHtmlEscaped()).arg(minScore).arg(minInfo.toHtmlEscaped()));
        if (QProgressBar *b = findChild<QProgressBar *>("prioBar")) {
            b->setValue(minScore <= 100 ? minScore : 0);
            QString col = minScore < 40 ? "#c62828" : (minScore < 70 ? "#ef6c00" : "#2e7d32");
            b->setStyleSheet(QString("QProgressBar{background:#eee") + QString(";border:none;border-radius:4px;}"
                                                                                                             "QProgressBar::chunk{background:%1;border-radius:4px;}").arg(col));
        }
    }

    majEtatVide(table);
    statusBar()->showMessage(QString("📦  %1 véhicule(s)     •     🚨  %2 alerte(s)     •     Smart Fire Station © 2026")
                                 .arg(total).arg(alertes));

    // la cloche pulse tant qu'il y a des alertes
    if (QPropertyAnimation *p = findChild<QPropertyAnimation *>("pulseCloche")) {
        if (alertes > 0 && p->state() != QAbstractAnimation::Running)
            p->start();
        else if (alertes == 0 && p->state() == QAbstractAnimation::Running) {
            p->stop();
            p->setCurrentTime(0);
        }
    }

}

// =====================================================================
void vehicules::setUtilisateur(const QString &nomUtilisateur, const QString &role)
{
    utilisateurCourant = nomUtilisateur;
    if (QLabel *l = findChild<QLabel *>("lblUser"))
        l->setText("👤  " + nomUtilisateur + "  ▾");
}

// =====================================================================
//  JOURNAL D'ACTIVITÉ (remplace la présentation "CRUD")
//  Chaque opération (ajout, modification, suppression,
//  alerte, planification, export) est enregistrée avec date et heure.
// =====================================================================
void vehicules::journaliser(const QString &action, const QString &equipement, const QString &details)
{
    journal.push_back(QStringList{
        QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm:ss"),
        action, equipement, details, utilisateurCourant});
}

void vehicules::afficherJournal()
{
    QDialog *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle("Journal d'activité");
    dlg->resize(1000, 640);
    dlg->setStyleSheet(
        "QDialog{background:#f4f4f6;}"
        "QLabel{border:none;background:transparent;}"
        "QLineEdit,QComboBox{border:1px solid #ccc;border-radius:6px;padding:5px 8px;"
        "background:white;min-height:22px;}"
        "QLineEdit:focus{border:1px solid #c40000;}"
        "QComboBox::drop-down{border:none;width:24px;}");

    QVBoxLayout *root = new QVBoxLayout(dlg);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QFrame *banner = new QFrame;
    banner->setStyleSheet("QFrame{background:#c40000;}");
    banner->setFixedHeight(76);
    QVBoxLayout *bl = new QVBoxLayout(banner);
    bl->setContentsMargins(24, 10, 24, 10);
    bl->setSpacing(0);
    QLabel *bt = new QLabel("📜  Journal d'activité");
    bt->setStyleSheet("color:white;font-size:20px;font-weight:bold;");
    QLabel *bs = new QLabel("Historique de toutes les opérations sur les véhicules");
    bs->setStyleSheet("color:#ffd6d6;font-size:12px;");
    bl->addWidget(bt);
    bl->addWidget(bs);
    root->addWidget(banner);

    QWidget *corps = new QWidget;
    QVBoxLayout *cv = new QVBoxLayout(corps);
    cv->setContentsMargins(20, 16, 20, 12);
    cv->setSpacing(10);

    QLineEdit *rech = new QLineEdit;
    rech->setPlaceholderText("Rechercher dans le journal...");
    QComboBox *filtre = new QComboBox;
    filtre->addItems({"Toutes les actions", "Ajout", "Modification", "Suppression",
                      "Alerte", "Planification", "Export", "Système"});
    QLabel *compteur = new QLabel;
    compteur->setStyleSheet("color:#777;font-size:12px;");

    QHBoxLayout *outils = new QHBoxLayout;
    outils->addWidget(rech, 1);
    outils->addWidget(filtre);
    outils->addWidget(compteur);
    cv->addLayout(outils);

    QTableWidget *t = new QTableWidget(0, 5);
    t->setHorizontalHeaderLabels({"Date / heure", "Action", "Véhicule", "Détails", "Utilisateur"});
    t->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    t->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    t->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    t->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    t->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    t->setItemDelegateForColumn(1, new EtatDelegate(t));
    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(40);
    t->setAlternatingRowColors(true);
    t->setShowGrid(false);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setFocusPolicy(Qt::NoFocus);
    t->setStyleSheet(
        "QTableWidget{border:1px solid #e6e6e6;border-radius:8px;background:white;"
        "alternate-background-color:#fafafa;selection-background-color:#f3c9c9;selection-color:black;}"
        "QHeaderView::section{background:#fbeaea;color:#a90000;font-weight:bold;"
        "border:none;border-bottom:2px solid #c40000;padding:8px 6px;}");
    cv->addWidget(t, 1);
    root->addWidget(corps, 1);

    // remplissage (le plus récent en premier)
    auto remplir = [=]() {
        t->setRowCount(0);
        QString f = filtre->currentIndex() > 0 ? filtre->currentText() : QString();
        QString q = rech->text();
        int n = 0;
        for (int i = int(journal.size()) - 1; i >= 0; --i) {
            const QStringList &e = journal[i];
            if (e.size() < 5) continue;
            if (!f.isEmpty() && e[1] != f) continue;
            if (!q.isEmpty() && !e.join(" ").contains(q, Qt::CaseInsensitive)) continue;
            int r = t->rowCount();
            t->insertRow(r);
            for (int c = 0; c < 5; c++)
                t->setItem(r, c, new QTableWidgetItem(e[c]));
            n++;
        }
        compteur->setText(QString("%1 opération(s)").arg(n));
    };
    connect(rech, &QLineEdit::textChanged, dlg, remplir);
    connect(filtre, &QComboBox::currentIndexChanged, dlg, remplir);
    remplir();

    // pied de page
    QFrame *foot = new QFrame;
    foot->setStyleSheet("QFrame{background:white;border-top:1px solid #e6e6e6;}");
    QHBoxLayout *fl = new QHBoxLayout(foot);
    fl->setContentsMargins(20, 10, 20, 10);
    fl->addStretch();

    QPushButton *pdf = new QPushButton("📄  Exporter en PDF");
    pdf->setFixedHeight(34);
    pdf->setStyleSheet(styleBoutonCouleur("#1565c0", "#0d47a1"));
    connect(pdf, &QPushButton::clicked, dlg, [=]() {
        QString file = QFileDialog::getSaveFileName(
            dlg, "Exporter le journal",
            QDir::homePath() + "/Documents/journal_vehicules.pdf", "PDF (*.pdf)");
        if (file.isEmpty()) return;

        QString html = "<h2 style='color:#c40000'>Journal d'activité - véhicules</h2>"
                       "<table border='1' cellspacing='0' cellpadding='4'><tr>";
        const QStringList ent = {"Date / heure", "Action", "Véhicule", "Détails", "Utilisateur"};
        for (const QString &h : ent)
            html += "<th>" + h + "</th>";
        html += "</tr>";
        for (int i = int(journal.size()) - 1; i >= 0; --i) {
            html += "<tr>";
            for (const QString &c : journal[i])
                html += "<td>" + c.toHtmlEscaped() + "</td>";
            html += "</tr>";
        }
        html += "</table>";

        QTextDocument doc;
        doc.setHtml(html);
        QPrinter printer(QPrinter::PrinterResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(file);
        doc.print(&printer);
        QMessageBox::information(dlg, "PDF", "Export réussi !");
        journaliser("Export", "Journal d'activité", "Export PDF du journal");
    });
    fl->addWidget(pdf);

    QPushButton *fermer = new QPushButton("Fermer");
    fermer->setFixedHeight(34);
    fermer->setMinimumWidth(110);
    fermer->setStyleSheet(
        "QPushButton{background:#c40000;color:white;border:none;border-radius:6px;"
        "padding:6px 18px;font-weight:bold;}"
        "QPushButton:hover{background:#8f0000;}");
    connect(fermer, &QPushButton::clicked, dlg, &QDialog::accept);
    fl->addWidget(fermer);
    root->addWidget(foot);

    fondu(dlg);
    dlg->exec();
}

// =====================================================================
//  MÉTIER 2 : maintenance prédictive
//  Score de santé par véhicule + recommandation + planification auto
// =====================================================================
void vehicules::afficherMaintenancePredictive()
{
    QDate today = QDate::currentDate();

    struct Ligne { int row; int score; QString nom; QString facteurs; QString reco; QDate suggeree; };
    QVector<Ligne> lignes;

    for (int r = 0; r < table->rowCount(); r++) {
        if (!table->item(r, 5) || !table->item(r, 7) || !table->item(r, 8) || !table->item(r, 9))
            continue;
        QString e = table->item(r, 5)->text();
        QDate da = QDate::fromString(table->item(r, 7)->text(), FMT);
        QDate dm = QDate::fromString(table->item(r, 8)->text(), FMT);
        QDate pm = QDate::fromString(table->item(r, 9)->text(), FMT);

        int sc = scoreSante(e, da, dm, pm);
        QDate sug;
        QString reco = recommandation(sc, pm, sug);
        lignes.push_back({r, sc,
                          table->item(r, 0)->text() + "  " + table->item(r, 1)->text(),
                          facteursSante(e, da, dm, pm).join("  •  "),
                          reco, sug});
    }
    std::sort(lignes.begin(), lignes.end(), [](const Ligne &a, const Ligne &b) { return a.score < b.score; });

    QDialog *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle("Maintenance prédictive");
    dlg->resize(1020, 660);
    dlg->setStyleSheet("QDialog{background:#f4f4f6;} QLabel{border:none;background:transparent;}");

    QVBoxLayout *root = new QVBoxLayout(dlg);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QFrame *banner = new QFrame;
    banner->setStyleSheet("QFrame{background:#c40000;}");
    banner->setFixedHeight(76);
    QVBoxLayout *bl = new QVBoxLayout(banner);
    bl->setContentsMargins(24, 10, 24, 10);
    bl->setSpacing(0);
    QLabel *bt = new QLabel("🛠  Maintenance prédictive");
    bt->setStyleSheet("color:white;font-size:20px;font-weight:bold;");
    QLabel *bs = new QLabel("Score de santé calculé automatiquement  •  " + today.toString(FMT));
    bs->setStyleSheet("color:#ffd6d6;font-size:12px;");
    bl->addWidget(bt);
    bl->addWidget(bs);
    root->addWidget(banner);

    QWidget *corps = new QWidget;
    QVBoxLayout *cv = new QVBoxLayout(corps);
    cv->setContentsMargins(20, 16, 20, 12);
    cv->setSpacing(12);

    QFrame *info = new QFrame;
    info->setObjectName("infoBox");
    info->setStyleSheet("QFrame#infoBox{background:#fff8e1;border:1px solid #ffe082;border-radius:10px;}"
                        "QLabel{border:none;background:transparent;}");
    QVBoxLayout *il = new QVBoxLayout(info);
    il->setContentsMargins(16, 10, 16, 10);
    QLabel *infoTxt = new QLabel(
        "<b>Comment est calculé le score de santé ?</b><br>"
        "Le score (0 à 100 %) diminue avec la part du cycle de maintenance déjà écoulée, "
        "l'âge de du véhicule et son état (en maintenance / hors service).<br>"
        "<span style='color:#2e7d32'>● ≥ 70 % : bon</span> &nbsp;&nbsp; "
        "<span style='color:#ef6c00'>● 40 à 69 % : à surveiller</span> &nbsp;&nbsp; "
        "<span style='color:#c62828'>● &lt; 40 % : critique</span>");
    infoTxt->setWordWrap(true);
    infoTxt->setStyleSheet("color:#5d4b00;font-size:12px;");
    il->addWidget(infoTxt);
    cv->addWidget(info);

    QTableWidget *t = new QTableWidget(int(lignes.size()), 6);
    t->setHorizontalHeaderLabels({"Véhicule", "Santé", "Niveau", "Facteurs de risque",
                                  "Recommandation", "Date suggérée"});
    for (int i = 0; i < lignes.size(); i++) {
        const Ligne &l = lignes[i];
        QString niveau = l.score >= 70 ? "Bon" : (l.score >= 40 ? "À surveiller" : "Critique");
        t->setItem(i, 0, new QTableWidgetItem(l.nom));
        t->setItem(i, 1, new QTableWidgetItem(QString::number(l.score)));
        t->setItem(i, 2, new QTableWidgetItem(niveau));
        t->setItem(i, 3, new QTableWidgetItem(l.facteurs));
        t->setItem(i, 4, new QTableWidgetItem(l.reco));
        t->setItem(i, 5, new QTableWidgetItem(l.suggeree.isValid() ? l.suggeree.toString(FMT) : QString("—")));
    }
    t->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    t->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    t->setColumnWidth(1, 140);
    t->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    t->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    t->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    t->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    t->setItemDelegateForColumn(1, new SanteDelegate(t));
    t->setItemDelegateForColumn(2, new EtatDelegate(t));
    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(44);
    t->setAlternatingRowColors(true);
    t->setShowGrid(false);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setFocusPolicy(Qt::NoFocus);
    t->setStyleSheet(
        "QTableWidget{border:1px solid #e6e6e6;border-radius:8px;background:white;"
        "alternate-background-color:#fafafa;selection-background-color:#f3c9c9;selection-color:black;}"
        "QHeaderView::section{background:#fbeaea;color:#a90000;font-weight:bold;"
        "border:none;border-bottom:2px solid #c40000;padding:8px 6px;}");
    cv->addWidget(t, 1);
    root->addWidget(corps, 1);

    QFrame *foot = new QFrame;
    foot->setStyleSheet("QFrame{background:white;border-top:1px solid #e6e6e6;}");
    QHBoxLayout *fl = new QHBoxLayout(foot);
    fl->setContentsMargins(20, 10, 20, 10);
    QLabel *aide = new QLabel("La planification avance la prochaine maintenance des véhicules dont le score est inférieur à 70 %.");
    aide->setStyleSheet("color:#888;font-size:11px;");
    fl->addWidget(aide);
    fl->addStretch();

    QPushButton *btnPlan = new QPushButton("📅  Planifier automatiquement");
    btnPlan->setFixedHeight(34);
    btnPlan->setStyleSheet(
        "QPushButton{background:#6a1b9a;color:white;border:none;border-radius:6px;"
        "padding:6px 18px;font-weight:bold;}"
        "QPushButton:hover{background:#4a148c;}");
    fl->addWidget(btnPlan);

    QPushButton *fermer = new QPushButton("Fermer");
    fermer->setFixedHeight(34);
    fermer->setMinimumWidth(110);
    fermer->setStyleSheet(
        "QPushButton{background:#c40000;color:white;border:none;border-radius:6px;"
        "padding:6px 18px;font-weight:bold;}"
        "QPushButton:hover{background:#8f0000;}");
    connect(fermer, &QPushButton::clicked, dlg, &QDialog::accept);
    fl->addWidget(fermer);
    root->addWidget(foot);

    connect(btnPlan, &QPushButton::clicked, dlg, [=]() {
        int n = 0;
        for (const Ligne &l : lignes) {
            if (l.score >= 70) continue;
            QTableWidgetItem *it = table->item(l.row, 9);
            QDate ancienne = QDate::fromString(it->text(), FMT);
            QDate dm = QDate::fromString(table->item(l.row, 8)->text(), FMT);
            QDate nouvelle = l.suggeree;
            if (dm.isValid() && nouvelle <= dm) nouvelle = dm.addDays(1);
            if (!ancienne.isValid() || nouvelle < ancienne) {
                it->setText(nouvelle.toString(FMT));
                journaliser("Planification", l.nom,
                            QString("Prochaine maintenance : %1 → %2  •  score de santé %3 %")
                                .arg(ancienne.toString(FMT), nouvelle.toString(FMT)).arg(l.score));
                n++;
            }
        }
        colorierLignes();
        dlg->accept();
        if (n > 0)
            toast(this, QString("📅  %1 maintenance(s) planifiée(s) automatiquement").arg(n), "#6a1b9a");
        else
            toast(this, "✓  Aucune planification nécessaire");
    });

    fondu(dlg);
    dlg->exec();
}

// =====================================================================
//  Envoi d'une alerte par email (automatique ou manuel)
// =====================================================================
void vehicules::envoyerAlerteEmail(const QString &sujet, const QString &corps, const QString &ref, bool manuel)
{
    ConfigEmail c = chargerConfigEmail();

    if (!configEmailComplete(c)) {
        toast(this, "⚠  Alerte détectée : configurez l'email (bouton ✉ Email en haut)", "#ef6c00");
        journaliser("Alerte", ref, "Alerte détectée  •  email non configuré");
        return;
    }
    if (!manuel && !c.automatique) {
        journaliser("Alerte", ref, "Alerte détectée  •  envoi automatique désactivé");
        return;
    }

    envoyerAsync(this, c, sujet, corps, [this, ref, c](bool ok, const QString &err) {
        if (ok) {
            journaliser("Alerte", ref, "Email d'alerte envoyé à " + c.destinataire);
            toast(this, "✉  Email d'alerte envoyé à " + c.destinataire, "#1565c0");
        } else {
            journaliser("Alerte", ref, "Échec de l'envoi de l'email : " + err);
            toast(this, "⚠  Échec de l'envoi de l'email : " + err.left(70), "#c62828");
        }
    });
}

// =====================================================================
//  Fenêtre : paramètres de l'email d'alerte
// =====================================================================
void vehicules::afficherParametresEmail()
{
    ConfigEmail c = chargerConfigEmail();

    QDialog *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle("Paramètres email d'alerte");
    dlg->resize(580, 640);
    dlg->setStyleSheet(
        "QDialog{background:#f4f4f6;}"
        "QLabel{border:none;background:transparent;}"
        "QLabel#fieldLabel{color:#444;font-weight:600;font-size:12px;}"
        "QLineEdit,QSpinBox{border:1px solid #ccc;border-radius:6px;padding:6px 8px;"
        "background:white;min-height:22px;}"
        "QLineEdit:focus,QSpinBox:focus{border:1px solid #c40000;}"
        "QCheckBox{color:#444;}");

    QVBoxLayout *root = new QVBoxLayout(dlg);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QFrame *banner = new QFrame;
    banner->setStyleSheet("QFrame{background:#c40000;}");
    banner->setFixedHeight(76);
    QVBoxLayout *bl = new QVBoxLayout(banner);
    bl->setContentsMargins(24, 10, 24, 10);
    bl->setSpacing(0);
    QLabel *bt = new QLabel("✉  Alerte par email");
    bt->setStyleSheet("color:white;font-size:20px;font-weight:bold;");
    QLabel *bs = new QLabel("Les alertes (état / Carburant (%)) sont envoyées par email au responsable");
    bs->setStyleSheet("color:#ffd6d6;font-size:12px;");
    bl->addWidget(bt);
    bl->addWidget(bs);
    root->addWidget(banner);

    QWidget *corps = new QWidget;
    QVBoxLayout *cv = new QVBoxLayout(corps);
    cv->setContentsMargins(22, 16, 22, 12);
    cv->setSpacing(10);

    QFrame *info = new QFrame;
    info->setObjectName("infoBox");
    info->setStyleSheet("QFrame#infoBox{background:#fff8e1;border:1px solid #ffe082;border-radius:10px;}"
                        "QLabel{border:none;background:transparent;}");
    QVBoxLayout *il = new QVBoxLayout(info);
    il->setContentsMargins(14, 10, 14, 10);
    QLabel *infoTxt = new QLabel(
        "<b>Avec Gmail :</b> activez la validation en 2 étapes, puis créez un "
        "<b>« mot de passe d'application »</b> (compte Google → Sécurité). "
        "Utilisez ce mot de passe ici, <b>jamais</b> votre vrai mot de passe.<br>"
        "Serveur <b>smtp.gmail.com</b>, port <b>465</b>.");
    infoTxt->setWordWrap(true);
    infoTxt->setStyleSheet("color:#5d4b00;font-size:12px;");
    il->addWidget(infoTxt);
    cv->addWidget(info);

    auto champ = [](const QString &texte, QWidget *w) -> QWidget * {
        QWidget *x = new QWidget;
        QVBoxLayout *l = new QVBoxLayout(x);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(3);
        QLabel *lb = new QLabel(texte);
        lb->setObjectName("fieldLabel");
        l->addWidget(lb);
        l->addWidget(w);
        return x;
    };

    QLineEdit *serveur = new QLineEdit(c.serveur);
    QSpinBox *port = new QSpinBox;
    port->setRange(1, 65535);
    port->setValue(c.port);
    QLineEdit *expediteur = new QLineEdit(c.expediteur);
    expediteur->setPlaceholderText("exemple@gmail.com");
    QLineEdit *mdp = new QLineEdit(c.mdp);
    mdp->setEchoMode(QLineEdit::Password);
    mdp->setPlaceholderText("mot de passe d'application");
    QLineEdit *dest = new QLineEdit(c.destinataire);
    dest->setPlaceholderText("chef.caserne@exemple.com");
    QCheckBox *chkAuto = new QCheckBox("Envoyer automatiquement les alertes par email");
    chkAuto->setChecked(c.automatique);

    QHBoxLayout *ligneSrv = new QHBoxLayout;
    ligneSrv->setSpacing(12);
    ligneSrv->addWidget(champ("Serveur SMTP", serveur), 2);
    ligneSrv->addWidget(champ("Port", port), 1);
    cv->addLayout(ligneSrv);
    cv->addWidget(champ("Email expéditeur", expediteur));
    cv->addWidget(champ("Mot de passe d'application", mdp));
    cv->addWidget(champ("Email destinataire (responsable / chef de caserne)", dest));
    cv->addWidget(chkAuto);

    QLabel *statut = new QLabel;
    statut->setWordWrap(true);
    statut->setMinimumHeight(34);
    cv->addWidget(statut);
    cv->addStretch();
    root->addWidget(corps, 1);

    QFrame *foot = new QFrame;
    foot->setStyleSheet("QFrame{background:white;border-top:1px solid #e6e6e6;}");
    QHBoxLayout *fl = new QHBoxLayout(foot);
    fl->setContentsMargins(20, 10, 20, 10);

    QPushButton *btnTest = new QPushButton("✉  Envoyer un email de test");
    btnTest->setFixedHeight(34);
    btnTest->setStyleSheet(styleBoutonCouleurDesactive("#2e7d32", "#1b5e20", "6px 16px"));
    fl->addWidget(btnTest);
    fl->addStretch();

    QPushButton *btnAnnuler = new QPushButton("Annuler");
    btnAnnuler->setFixedHeight(34);
    btnAnnuler->setStyleSheet(
        "QPushButton{background:#eee;color:#333;border:none;border-radius:6px;"
        "padding:6px 16px;font-weight:bold;}QPushButton:hover{background:#ddd;}");
    QPushButton *btnSave = new QPushButton("Enregistrer");
    btnSave->setFixedHeight(34);
    btnSave->setMinimumWidth(110);
    btnSave->setStyleSheet(
        "QPushButton{background:#c40000;color:white;border:none;border-radius:6px;"
        "padding:6px 18px;font-weight:bold;}QPushButton:hover{background:#8f0000;}");
    fl->addWidget(btnAnnuler);
    fl->addWidget(btnSave);
    root->addWidget(foot);

    auto lireConfig = [=]() {
        ConfigEmail x;
        x.serveur = serveur->text().trimmed();
        x.port = port->value();
        x.expediteur = expediteur->text().trimmed();
        x.mdp = mdp->text();
        x.destinataire = dest->text().trimmed();
        x.automatique = chkAuto->isChecked();
        return x;
    };

    connect(btnTest, &QPushButton::clicked, dlg, [=]() {
        ConfigEmail x = lireConfig();
        if (!configEmailComplete(x)) {
            statut->setStyleSheet("color:#c62828;font-weight:bold;");
            statut->setText("⚠  Remplissez tous les champs (adresses email valides).");
            return;
        }
        statut->setStyleSheet("color:#1565c0;font-weight:bold;");
        statut->setText("⏳  Envoi de l'email de test en cours...");
        btnTest->setEnabled(false);
        envoyerAsync(dlg, x, "[Smart Fire Station] Email de test",
                     "Bonjour,\n\nCeci est un email de test envoyé par Smart Fire Station.\n"
                     "Si vous le recevez, l'envoi des alertes par email fonctionne.\n\n"
                     "— Smart Fire Station",
                     [=](bool ok, const QString &err) {
                         btnTest->setEnabled(true);
                         if (ok) {
                             statut->setStyleSheet("color:#2e7d32;font-weight:bold;");
                             statut->setText("✓  Email de test envoyé à " + x.destinataire);
                         } else {
                             statut->setStyleSheet("color:#c62828;font-weight:bold;");
                             statut->setText("⚠  Échec : " + err);
                         }
                     });
    });

    connect(btnSave, &QPushButton::clicked, dlg, [=]() {
        sauverConfigEmail(lireConfig());
        dlg->accept();
        toast(this, "✓  Paramètres email enregistrés");
    });
    connect(btnAnnuler, &QPushButton::clicked, dlg, &QDialog::reject);

    fondu(dlg);
    dlg->exec();
}


