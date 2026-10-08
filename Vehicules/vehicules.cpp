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
#include <QAbstractItemModel>
#include <QPrinter>
#include <QTextDocument>
#include <QFileDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
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
#include <QDebug>
#include <QLocale>
#include <QDialog>
#include <QScrollArea>
#include <QMap>
#include <QVector>
#include <QPaintEvent>
#include <algorithm>
#include <memory>
#include <functional>
#include <cmath>
#include <QEvent>
#include <QMouseEvent>
#include <QStatusBar>
#include <QIcon>
#include <QMenu>
#include <QAction>
#include <QProgressBar>
#include <QToolTip>
#include <QTextEdit>
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

// couleur des badges (états et niveaux de santé)
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

static QIcon emojiIcon(const QString &emoji, int size = 24)
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QFont f("Segoe UI Emoji");
    f.setPixelSize(int(size * 0.78));
    p.setFont(f);
    p.drawText(pix.rect(), Qt::AlignCenter, emoji);
    return QIcon(pix);
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

static bool ligneVehiculeComplete(QTableWidget *table, int row)
{
    if (!table || row < 0 || row >= table->rowCount()) return false;
    const int cols[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15};
    for (int col : cols)
        if (!table->item(row, col))
            return false;
    return true;
}

static int trouverLigneParId(QTableWidget *table, const QString &id)
{
    if (!table || id.isEmpty()) return -1;
    for (int r = 0; r < table->rowCount(); ++r)
        if (table->item(r, 0) && table->item(r, 0)->text() == id)
            return r;
    return -1;
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
static void afficherStatistiques(QWidget *parent, QTableWidget *table)
{
    QDate today = QDate::currentDate();

    int total = 0, dispo = 0, hs = 0, enAlerte = 0, sommeSante = 0;
    int nbEtat[4]  = {0, 0, 0, 0};   // Disponible, En mission, En maintenance, Hors service
    int nbSante[3] = {0, 0, 0};      // Bon, À surveiller, Critique
    QMap<QString, int> parType;

    for (int r = 0; r < table->rowCount(); r++) {
        if (!table->item(r, 5) || !table->item(r, 3) || !table->item(r, 4)
            || !table->item(r, 7) || !table->item(r, 8) || !table->item(r, 9))
            continue;
        total++;
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

        int sc = scoreSante(e, da, dm, pm);
        sommeSante += sc;
        if (sc >= 70)      nbSante[0]++;
        else if (sc >= 40) nbSante[1]++;
        else               nbSante[2]++;
    }

    double tauxDispo = total > 0 ? 100.0 * dispo / total : 0.0;
    double tauxHs    = total > 0 ? 100.0 * hs / total : 0.0;
    int santeMoy     = total > 0 ? int(sommeSante / double(total) + 0.5) : 0;

    QDialog *dlg = new QDialog(parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle("Statistiques - Véhicules");
    dlg->resize(1120, 760);
    dlg->setStyleSheet(R"QSS(
        QDialog{background:#F5F6F8;}
        QScrollArea{background:#F5F6F8;border:none;}
        QWidget#statInner{background:#F5F6F8;}
        QFrame#statCard{background:#FFFFFF;border:1px solid #E5E7EB;border-radius:12px;}
        QLabel{border:none;background:transparent;color:#111827;}
        QLabel#statTitle{font-size:26px;font-weight:700;color:#111827;}
        QLabel#statSubtitle{font-size:13px;color:#6B7280;}
        QLabel#cardTitle{font-size:15px;font-weight:600;color:#111827;}
        QLabel#kpiValue{font-size:24px;font-weight:700;color:#111827;}
        QLabel#kpiCaption{font-size:12px;color:#6B7280;}
        QPushButton#pdfOutline{background:#FFFFFF;color:#C90000;border:1px solid #C90000;border-radius:8px;padding:8px 18px;font-weight:700;}
        QPushButton#pdfOutline:hover{background:#FDE8E8;}
        QPushButton#closePrimary{background:#C90000;color:#FFFFFF;border:1px solid #C90000;border-radius:8px;padding:8px 18px;font-weight:700;}
        QPushButton#closePrimary:hover{background:#A80000;}
    )QSS");

    QVBoxLayout *root = new QVBoxLayout(dlg);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget *inner = new QWidget;
    inner->setObjectName("statInner");
    QVBoxLayout *iv = new QVBoxLayout(inner);
    iv->setContentsMargins(24, 22, 24, 18);
    iv->setSpacing(16);

    QLabel *title = new QLabel("Statistiques des véhicules");
    title->setObjectName("statTitle");
    QLabel *subtitle = new QLabel("Vue d'ensemble des performances de votre parc automobile  •  " + today.toString(FMT));
    subtitle->setObjectName("statSubtitle");
    iv->addWidget(title);
    iv->addWidget(subtitle);

    auto circle = [](const QString &txt, const QString &fg, const QString &bg) -> QLabel * {
        QLabel *l = new QLabel(txt);
        l->setAlignment(Qt::AlignCenter);
        l->setFixedSize(40, 40);
        l->setStyleSheet(QString("background:%1;color:%2;border-radius:20px;font-family:'Segoe UI Emoji';font-size:24px;font-weight:400;").arg(bg, fg));
        return l;
    };

    auto kpi = [&](const QString &val, const QString &cap, const QString &icon, const QString &fg, const QString &bg) -> QFrame * {
        QFrame *f = new QFrame;
        f->setObjectName("statCard");
        f->setMinimumHeight(96);
        QHBoxLayout *l = new QHBoxLayout(f);
        l->setContentsMargins(16, 12, 16, 12);
        l->setSpacing(12);
        l->addWidget(circle(icon, fg, bg));
        QVBoxLayout *texts = new QVBoxLayout;
        texts->setSpacing(0);
        QLabel *v = new QLabel(val);
        v->setObjectName("kpiValue");
        QLabel *c = new QLabel(cap);
        c->setObjectName("kpiCaption");
        c->setWordWrap(true);
        texts->addWidget(v);
        texts->addWidget(c);
        l->addLayout(texts, 1);
        ombre(f);
        return f;
    };

    auto carte = [](const QString &titre, QWidget *contenu) -> QFrame * {
        QFrame *f = new QFrame;
        f->setObjectName("statCard");
        f->setMinimumHeight(230);
        QVBoxLayout *l = new QVBoxLayout(f);
        l->setContentsMargins(18, 16, 18, 16);
        l->setSpacing(10);
        QLabel *t = new QLabel(titre);
        t->setObjectName("cardTitle");
        l->addWidget(t);
        l->addWidget(contenu, 1);
        ombre(f);
        return f;
    };

    QHBoxLayout *kpis = new QHBoxLayout;
    kpis->setSpacing(14);
    kpis->addWidget(kpi(QString::number(total), "Véhicules", "🚒", "#C90000", "#FDE8E8"), 1);
    kpis->addWidget(kpi(QString::number(tauxDispo, 'f', 1) + " %", "Taux de disponibilité", "✅", "#22A559", "#DCFCE7"), 1);
    kpis->addWidget(kpi(QString::number(tauxHs, 'f', 1) + " %", "Taux hors service", "❌", "#6B7280", "#F3F4F6"), 1);
    kpis->addWidget(kpi(QString::number(santeMoy) + " %", "Santé moyenne du parc", "❤️", "#C90000", "#FDE8E8"), 1);
    kpis->addWidget(kpi(QString::number(enAlerte), "Véhicules en alerte", "🚨", "#C90000", "#FDE8E8"), 1);
    iv->addLayout(kpis);

    DonutWidget *donut = new DonutWidget;
    donut->setData({"Disponible", "En mission", "En maintenance", "Hors service"},
                   {nbEtat[0], nbEtat[1], nbEtat[2], nbEtat[3]},
                   {QColor("#22A559"), QColor("#6B7280"), QColor("#F59E0B"), QColor("#C90000")});

    BarsWidget *types = new BarsWidget;
    QVector<BarsWidget::Row> rowsType;
    QStringList typePalette = {"#C90000", "#6B7280", "#B80000", "#9CA3AF", "#D1D5DB", "#F59E0B"};
    int ci = 0;
    for (auto it = parType.constBegin(); it != parType.constEnd(); ++it, ++ci)
        rowsType.push_back({it.key(), it.value(), QColor(typePalette[ci % typePalette.size()])});
    types->setRows(rowsType);

    QHBoxLayout *row1 = new QHBoxLayout;
    row1->setSpacing(14);
    row1->addWidget(carte("Répartition par état", donut), 1);
    row1->addWidget(carte("Véhicules par type", types), 1);
    iv->addLayout(row1);

    BarsWidget *santeBars = new BarsWidget;
    QVector<BarsWidget::Row> rowsSante;
    rowsSante.push_back({"Bon", nbSante[0], QColor("#22A559")});
    rowsSante.push_back({"À surveiller", nbSante[1], QColor("#F59E0B")});
    rowsSante.push_back({"Critique", nbSante[2], QColor("#C90000")});
    santeBars->setRows(rowsSante);

    QWidget *santeBox = new QWidget;
    QVBoxLayout *sbx = new QVBoxLayout(santeBox);
    sbx->setContentsMargins(0, 0, 0, 0);
    sbx->setSpacing(6);
    sbx->addWidget(santeBars);
    QLabel *santeNote = new QLabel("Score calculé automatiquement : cycle de maintenance, âge et état du véhicule.");
    santeNote->setWordWrap(true);
    santeNote->setStyleSheet("color:#6B7280;font-size:11px;");
    sbx->addWidget(santeNote);

    QHBoxLayout *row2 = new QHBoxLayout;
    row2->setSpacing(14);
    row2->addWidget(carte("Santé du parc (maintenance prédictive)", santeBox), 1);
    iv->addLayout(row2);

    iv->addStretch();
    scroll->setWidget(inner);
    root->addWidget(scroll, 1);

    QFrame *foot = new QFrame;
    foot->setStyleSheet("QFrame{background:#FFFFFF;border-top:1px solid #E5E7EB;}");
    QHBoxLayout *fl = new QHBoxLayout(foot);
    fl->setContentsMargins(24, 12, 24, 12);
    fl->addStretch();

    QPushButton *pdfStats = new QPushButton("Exporter en PDF");
    pdfStats->setObjectName("pdfOutline");
    pdfStats->setMinimumHeight(36);
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
        painter.setPen(QColor("#C90000"));
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
    fermer->setObjectName("closePrimary");
    fermer->setMinimumHeight(36);
    fermer->setMinimumWidth(110);
    QObject::connect(fermer, &QPushButton::clicked, dlg, &QDialog::accept);
    fl->addWidget(fermer);
    root->addWidget(foot);

    donut->animer();
    types->animer();
    santeBars->animer();

    fondu(dlg);
    dlg->exec();
}

vehicules::vehicules(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Smart Fire Station - Gestion des véhicules");
    setWindowIcon(QIcon(logoApp()));
    resize(1380, 820);
    statusBar()->setSizeGripEnabled(false);
    statusBar()->hide();

    setStyleSheet(R"QSS(
        QWidget {font-family:'Segoe UI', Arial, sans-serif;color:#111827;}
        QToolTip{background:#111827;color:white;border:none;padding:6px 10px;border-radius:6px;}
        QMainWindow, QWidget#pageBg{background:#F5F6F8;}
        QScrollArea{background:#F5F6F8;border:none;}
        QFrame#dashCard{background:#FFFFFF;border:1px solid #E5E7EB;border-radius:12px;}
        QLabel#pageTitle{font-size:26px;font-weight:700;color:#111827;}
        QLabel#pageSubtitle{font-size:13px;color:#6B7280;}
        QLabel#cardTitle{font-size:15px;font-weight:600;color:#111827;}
        QLabel#muted{font-size:12px;color:#6B7280;}
        QLabel#kpiValue{font-size:26px;font-weight:700;color:#111827;}
        QLabel#kpiLabel{font-size:12px;color:#6B7280;}
        QPushButton#primaryButton{background:#C90000;color:white;border:none;border-radius:9px;padding:0 18px;font-weight:700;min-height:40px;}
        QPushButton#primaryButton:hover{background:#B80000;}
        QPushButton#secondaryButton{background:#FFFFFF;color:#374151;border:1px solid #E5E7EB;border-radius:8px;padding:0 12px;min-height:36px;}
        QPushButton#secondaryButton:hover{background:#F9FAFB;border-color:#D1D5DB;}
        QPushButton#statsButton{background:#FFFFFF;color:#374151;border:1px solid #E5E7EB;border-radius:8px;padding:0 14px;min-height:34px;font-weight:600;}
        QPushButton#statsButton:hover{background:#F9FAFB;border-color:#D1D5DB;}
        QPushButton#dangerIcon{background:#FDE8E8;color:#B80000;border:1px solid #F8CACA;border-radius:7px;min-width:28px;min-height:28px;max-width:28px;max-height:28px;}
        QPushButton#rowIcon{background:#F9FAFB;color:#374151;border:1px solid #E5E7EB;border-radius:7px;min-width:28px;min-height:28px;max-width:28px;max-height:28px;}
        QPushButton#rowIcon:hover{background:#F3F4F6;}
        QLineEdit,QComboBox,QSpinBox,QDateEdit,QTextEdit{background:white;border:1px solid #D1D5DB;border-radius:8px;padding:7px 10px;min-height:22px;}
        QLineEdit:focus,QComboBox:focus,QSpinBox:focus,QDateEdit:focus,QTextEdit:focus{border:1px solid #C90000;}
        QLineEdit[erreur="true"],QComboBox[erreur="true"]{border:1px solid #C90000;background:#FFF5F5;}
        QComboBox::drop-down,QDateEdit::drop-down{border:none;width:24px;}
        QLineEdit#searchInput{background:#FFFFFF;color:#374151;border:1px solid #E5E7EB;border-radius:8px;padding:8px 12px;min-height:22px;}
        QLineEdit#searchInput:hover{border:1px solid #D1D5DB;}
        QLineEdit#searchInput:focus{border:1px solid #C90000;}
        QComboBox#toolbarCombo{background:#FFFFFF;color:#374151;border:1px solid #E5E7EB;border-radius:8px;padding:8px 12px;min-height:22px;}
        QComboBox#toolbarCombo:hover{border:1px solid #D1D5DB;}
        QComboBox#toolbarCombo:focus{border:1px solid #C90000;}
        QComboBox#toolbarCombo::drop-down{border:none;width:26px;}
        QComboBox#toolbarCombo::down-arrow{image:none;border-left:5px solid transparent;border-right:5px solid transparent;border-top:6px solid #374151;width:0;height:0;margin-right:10px;}
        QComboBox#toolbarCombo QAbstractItemView{background:#FFFFFF;color:#374151;border:1px solid #E5E7EB;selection-background-color:#FDE8E8;selection-color:#B80000;outline:0;}
        QTableWidget{background:white;border:none;alternate-background-color:#FFFFFF;selection-background-color:#FDE8E8;selection-color:#111827;gridline-color:#EEF0F3;}
        QHeaderView::section{background:#F3F4F6;color:#4B5563;font-size:12px;font-weight:600;border:none;border-bottom:1px solid #E5E7EB;padding:8px 6px;}
        QTableWidget::item{border-bottom:1px solid #F3F4F6;padding:4px;}
        QMenu{background:white;border:1px solid #E5E7EB;padding:6px;}
        QMenu::item{padding:7px 28px;color:#111827;}
        QMenu::item:selected{background:#FDE8E8;color:#B80000;}
        QStatusBar{background:#FFFFFF;color:#6B7280;border-top:1px solid #E5E7EB;}
        QScrollBar:vertical{background:transparent;width:10px;margin:2px;}
        QScrollBar::handle:vertical{background:#D1D5DB;border-radius:4px;min-height:30px;}
        QScrollBar::handle:vertical:hover{background:#C90000;}
        QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}
        QScrollBar:horizontal{background:transparent;height:10px;margin:2px;}
        QScrollBar::handle:horizontal{background:#D1D5DB;border-radius:4px;min-width:30px;}
        QScrollBar::handle:horizontal:hover{background:#C90000;}
        QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0;}
    )QSS");

    auto card = []() {
        QFrame *f = new QFrame;
        f->setObjectName("dashCard");
        ombre(f);
        return f;
    };

    auto titleLabel = [](const QString &text) {
        QLabel *l = new QLabel(text);
        l->setObjectName("cardTitle");
        return l;
    };

    auto makeIcon = [](const QString &text, const QString &fg, const QString &bg) {
        QLabel *l = new QLabel(text);
        l->setAlignment(Qt::AlignCenter);
        l->setFixedSize(42, 42);
        l->setStyleSheet(QString("background:%1;color:%2;border-radius:21px;font-family:'Segoe UI Emoji';font-size:24px;font-weight:400;").arg(bg, fg));
        return l;
    };

    QWidget *central = new QWidget;
    central->setObjectName("pageBg");
    setCentralWidget(central);
    QVBoxLayout *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);

    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    QWidget *content = new QWidget;
    content->setObjectName("pageBg");
    QVBoxLayout *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(24, 22, 24, 24);
    contentLayout->setSpacing(14);
    scroll->setWidget(content);
    root->addWidget(scroll);

    // Champs conservés pour réutiliser la logique métier existante depuis les dialogues.
    nom = new QLineEdit(this);
    type = new QComboBox(this);
    type->addItems({"Sélectionner un type", "Incendie", "Ambulance", "Commandement", "Secours médical", "Intervention technique"});
    marque = new QLineEdit(this);
    modele = new QLineEdit(this);
    quantite = new QSpinBox(this);
    quantite->setRange(0, 100);
    quantite->setSuffix(" %");
    quantite->setValue(80);
    seuil = new QSpinBox(this);
    seuil->setRange(0, 100);
    seuil->setSuffix(" %");
    seuil->setValue(25);
    etat = new QComboBox(this);
    etat->addItems({"Disponible", "En mission", "En maintenance", "Hors service"});
    localisation = new QLineEdit(this);
    dateAchat = new QDateEdit(QDate::currentDate(), this);
    dateAchat->setCalendarPopup(true);
    dateAchat->setDisplayFormat(FMT);
    dateMaintenance = new QDateEdit(QDate::currentDate(), this);
    dateMaintenance->setCalendarPopup(true);
    dateMaintenance->setDisplayFormat(FMT);
    prochaineMaintenance = new QDateEdit(QDate::currentDate().addMonths(6), this);
    prochaineMaintenance->setCalendarPopup(true);
    prochaineMaintenance->setDisplayFormat(FMT);
    prochaineMaintenance->setMinimumDate(dateMaintenance->date().addDays(1));
    connect(dateMaintenance, &QDateEdit::dateChanged, this, [=](const QDate &d) {
        prochaineMaintenance->setMinimumDate(d.addDays(1));
    });
    remarques = new QLineEdit(this);
    for (QWidget *w : QVector<QWidget *>{nom, type, marque, modele, quantite, seuil, etat, localisation,
                                         dateAchat, dateMaintenance, prochaineMaintenance, remarques})
        w->hide();

    QHBoxLayout *header = new QHBoxLayout;
    header->setSpacing(12);
    QVBoxLayout *headerText = new QVBoxLayout;
    headerText->setSpacing(2);
    QLabel *title = new QLabel("Gestion des véhicules");
    title->setObjectName("pageTitle");
    QLabel *subtitle = new QLabel("Vue d'ensemble de votre parc de véhicules");
    subtitle->setObjectName("pageSubtitle");
    headerText->addWidget(title);
    headerText->addWidget(subtitle);
    header->addLayout(headerText, 1);
    QPushButton *maintenanceAction = new QPushButton("Maintenances");
    maintenanceAction->setObjectName("secondaryButton");
    QPushButton *addAction = new QPushButton("+ Ajouter un véhicule");
    addAction->setObjectName("primaryButton");
    header->addWidget(maintenanceAction, 0, Qt::AlignTop);
    header->addWidget(addAction, 0, Qt::AlignTop);
    contentLayout->addLayout(header);

    QHBoxLayout *kpis = new QHBoxLayout;
    kpis->setSpacing(14);
    auto kpi = [&](const QString &name, const QString &icon, const QString &fg, const QString &bg, const QString &label) {
        QFrame *f = card();
        f->setMinimumHeight(104);
        QHBoxLayout *l = new QHBoxLayout(f);
        l->setContentsMargins(18, 14, 18, 14);
        l->setSpacing(14);
        l->addWidget(makeIcon(icon, fg, bg));
        QVBoxLayout *texts = new QVBoxLayout;
        texts->setSpacing(0);
        QLabel *v = new QLabel("0");
        v->setObjectName(name);
        v->setProperty("role", "kpi");
        v->setStyleSheet("font-size:26px;font-weight:700;color:#111827;");
        QLabel *cap = new QLabel(label);
        cap->setObjectName("kpiLabel");
        texts->addWidget(v);
        texts->addWidget(cap);
        l->addLayout(texts, 1);
        kpis->addWidget(f, 1);
    };
    kpi("val_total", "🚒", "#C90000", "#FDE8E8", "Véhicules total");
    kpi("val_dispo", "✅", "#22A559", "#DCFCE7", "Disponibles");
    kpi("val_maintenance", "🔧", "#C90000", "#FDE8E8", "En maintenance");
    kpi("val_hs", "❌", "#6B7280", "#F3F4F6", "Hors service");
    contentLayout->addLayout(kpis);

    QFrame *tableCard = card();
    tableCard->setMinimumHeight(360);
    QVBoxLayout *tableLayout = new QVBoxLayout(tableCard);
    tableLayout->setContentsMargins(18, 16, 18, 16);
    tableLayout->setSpacing(12);

    tableLayout->addWidget(titleLabel("Liste des véhicules"));

    QHBoxLayout *tableHead = new QHBoxLayout;
    tableHead->setSpacing(10);
    recherche = new QLineEdit;
    recherche->setObjectName("searchInput");
    recherche->setPlaceholderText("Rechercher un véhicule...");
    recherche->setMinimumWidth(300);
    recherche->setMinimumHeight(40);
    recherche->addAction(emojiIcon("🔎"), QLineEdit::LeadingPosition);
    filtreType = new QComboBox;
    filtreType->setObjectName("toolbarCombo");
    filtreType->addItems({"Tous les types", "Incendie", "Ambulance", "Commandement", "Secours médical", "Intervention technique"});
    filtreType->setMinimumHeight(40);
    triCombo = new QComboBox;
    triCombo->setObjectName("toolbarCombo");
    triCombo->addItems({"Nom", "État", "Date de maintenance", "Carburant (%)"});
    triCombo->setCurrentIndex(0);
    triCombo->setMinimumHeight(40);
    tableHead->addWidget(recherche, 5);
    tableHead->addWidget(filtreType, 2);
    tableHead->addWidget(triCombo, 2);
    tableLayout->addLayout(tableHead);

    QAction *actPdf = new QAction("Exporter en PDF", this);
    QAction *actAlerts = new QAction("Alertes", this);
    QAction *actMaint = new QAction("Maintenance prédictive", this);

    table = new QTableWidget(0, 16);
    table->setHorizontalHeaderLabels({
        "ID", "Matricule", "Type", "Carburant", "Carburant min.", "État", "Localisation",
        "Date achat", "Dernière maintenance", "Prochaine maintenance", "Actions",
        "Marque", "Modèle", "Remarques", "Santé", ""
    });
    table->setAlternatingRowColors(false);
    table->setShowGrid(false);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(44);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setFocusPolicy(Qt::NoFocus);
    table->setMouseTracking(true);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    table->horizontalHeader()->setMinimumSectionSize(36);
    table->horizontalHeader()->setStretchLastSection(false);
    table->setItemDelegateForColumn(5, new EtatDelegate(table));
    table->setItemDelegate(new HoverDelegate(table));
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

    auto moveColumn = [=](int logical, int visual) {
        int current = table->horizontalHeader()->visualIndex(logical);
        if (current >= 0 && current != visual)
            table->horizontalHeader()->moveSection(current, visual);
    };
    moveColumn(15, 0);
    moveColumn(0, 1);
    moveColumn(1, 2);
    moveColumn(12, 3);
    moveColumn(2, 4);
    moveColumn(3, 5);
    moveColumn(5, 6);
    moveColumn(8, 7);
    moveColumn(9, 8);
    moveColumn(10, 9);
    for (int c : {4, 6, 7, 11, 13, 14})
        table->setColumnHidden(c, true);
    table->setColumnWidth(15, 48);
    table->setColumnWidth(0, 76);
    table->setColumnWidth(1, 145);
    table->setColumnWidth(12, 130);
    table->setColumnWidth(2, 126);
    table->setColumnWidth(3, 82);
    table->setColumnWidth(5, 136);
    table->setColumnWidth(8, 132);
    table->setColumnWidth(9, 136);
    table->setColumnWidth(10, 260);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    table->horizontalHeader()->setSectionResizeMode(10, QHeaderView::Interactive);
    table->horizontalHeader()->setSectionResizeMode(12, QHeaderView::Stretch);
    tableLayout->addWidget(table, 1);
    contentLayout->addWidget(tableCard, 1);

    QHBoxLayout *bottom = new QHBoxLayout;
    bottom->setSpacing(14);
    QFrame *donutCard = card();
    donutCard->setMinimumHeight(260);
    QVBoxLayout *dl = new QVBoxLayout(donutCard);
    dl->setContentsMargins(18, 16, 18, 16);
    dl->setSpacing(8);
    QHBoxLayout *donutHead = new QHBoxLayout;
    donutHead->addWidget(titleLabel("Répartition par état"));
    donutHead->addStretch();
    QPushButton *statsButton = new QPushButton("Statistiques");
    statsButton->setObjectName("statsButton");
    statsButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    donutHead->addWidget(statsButton);
    dl->addLayout(donutHead);
    donutEtat = new DonutWidget;
    dl->addWidget(donutEtat, 1);
    bottom->addWidget(donutCard, 1);

    QFrame *alertsCard = card();
    QVBoxLayout *al = new QVBoxLayout(alertsCard);
    al->setContentsMargins(18, 16, 18, 16);
    al->setSpacing(10);
    QHBoxLayout *alertHead = new QHBoxLayout;
    alertHead->addWidget(titleLabel("Alertes récentes"));
    alertHead->addStretch();
    QPushButton *viewAllAlerts = new QPushButton("Voir toutes");
    viewAllAlerts->setObjectName("secondaryButton");
    alertHead->addWidget(viewAllAlerts);
    al->addLayout(alertHead);
    QWidget *alertsInner = new QWidget;
    alertesRecentesLayout = new QVBoxLayout(alertsInner);
    alertesRecentesLayout->setContentsMargins(0, 0, 0, 0);
    alertesRecentesLayout->setSpacing(8);
    al->addWidget(alertsInner, 1);
    bottom->addWidget(alertsCard, 1);
    contentLayout->addLayout(bottom);

    QStringList noms = {"VSAV-01", "FPT-02", "CCF-03", "VLCG-04", "VTU-05", "EPA-06", "VSR-07", "VPC-08"};
    QStringList marques = {"Renault Trucks", "Mercedes-Benz", "Iveco", "Peugeot", "Ford", "MAN", "Scania", "Citroën"};
    QStringList typesDemo = {"Secours médical", "Incendie", "Incendie", "Commandement", "Intervention technique", "Incendie", "Intervention technique", "Commandement"};
    QStringList modeles = {"Master", "Atego", "Daily 4x4", "Expert", "Transit", "TGM", "P320", "Jumpy"};
    QStringList etats = {"Disponible", "En maintenance", "Hors service", "Disponible", "En mission", "Disponible", "En maintenance", "En mission"};
    QStringList lieux = {"Remise principale", "Aire de stationnement", "Zone départ rapide", "Aire de stationnement", "Magasin", "Aire de stationnement", "Remise principale", "Atelier mécanique"};
    int carburants[8] = {85, 60, 15, 90, 25, 75, 45, 55};
    int seuils[8] = {30, 25, 30, 20, 25, 25, 30, 25};
    QDate today = QDate::currentDate();
    for (int i = 0; i < noms.size(); i++) {
        QDate pm = today.addMonths(6);
        if (i == 2) pm = today.addDays(10);
        if (i == 4) pm = today.addDays(-5);
        ajouterLigne(noms[i], typesDemo[i], marques[i], modeles[i], carburants[i], seuils[i], etats[i], lieux[i],
                     today.addYears(-1).toString(FMT), today.addMonths(-6).toString(FMT), pm.toString(FMT), "");
    }

    QLabel *vide = new QLabel(table->viewport());
    vide->setObjectName("labelVide");
    vide->setAlignment(Qt::AlignCenter);
    vide->setStyleSheet("color:#9CA3AF;font-size:14px;");
    vide->hide();
    QVBoxLayout *vl = new QVBoxLayout(table->viewport());
    vl->addWidget(vide, 0, Qt::AlignCenter);

    table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table, &QWidget::customContextMenuRequested, this, [=](const QPoint &pos) {
        QMenu menu(this);
        menu.addAction(actPdf);
        menu.addAction(actAlerts);
        menu.addAction(actMaint);
        menu.exec(table->mapToGlobal(pos));
    });

    connect(addAction, &QPushButton::clicked, this, [=]() { ouvrirFormulaireVehicule(false); });
    connect(maintenanceAction, &QPushButton::clicked, this, &vehicules::afficherMaintenances);
    connect(viewAllAlerts, &QPushButton::clicked, this, [=]() { afficherAlertes(true); });
    connect(recherche, &QLineEdit::textChanged, this, &vehicules::rechercher);
    connect(filtreType, &QComboBox::currentIndexChanged, this, &vehicules::rechercher);
    connect(triCombo, &QComboBox::currentIndexChanged, this, &vehicules::trier);
    connect(statsButton, &QPushButton::clicked, this, [=]() { afficherStatistiques(this, table); });
    connect(actAlerts, &QAction::triggered, this, [=]() { afficherAlertes(true); });
    connect(actMaint, &QAction::triggered, this, [=]() { afficherMaintenancePredictive(); });
    connect(actPdf, &QAction::triggered, this, [=]() {
        QString file = QFileDialog::getSaveFileName(this, "Exporter en PDF", QDir::homePath() + "/Documents/vehicules.pdf", "PDF (*.pdf)");
        if (file.isEmpty()) return;
        QString html = "<h2 style='color:#c40000'>Inventaire des véhicules</h2><table border='1' cellspacing='0' cellpadding='4'><tr>";
        const QVector<int> exportCols = {0, 1, 11, 12, 2, 3, 5, 8, 9};
        for (int c : exportCols) html += "<th>" + table->horizontalHeaderItem(c)->text() + "</th>";
        html += "</tr>";
        for (int r = 0; r < table->rowCount(); r++) {
            html += "<tr>";
            for (int c : exportCols)
                html += "<td>" + (table->item(r, c) ? table->item(r, c)->text().toHtmlEscaped() : QString()) + "</td>";
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
    connect(table->model(), &QAbstractItemModel::rowsRemoved, this, [=]() {
        QTimer::singleShot(0, this, [this]() { majCartes(); });
    });
    connect(table->model(), &QAbstractItemModel::rowsInserted, this, [=]() {
        QTimer::singleShot(0, this, [this]() { majCartes(); });
    });

    colorierLignes();
    apparition(tableCard, 250);
    diapo(tableCard, QPoint(0, 30), 250);
}

void vehicules::ouvrirFormulaireVehicule(bool edition, int ligne)
{
    if (edition && ligne < 0)
        ligne = table->currentRow();
    if (edition && ligne < 0) {
        toast(this, "Sélectionnez un véhicule à modifier", "#ef6c00");
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(edition ? "Modifier le véhicule" : "Nouveau véhicule");
    dlg.resize(760, 620);
    dlg.setStyleSheet(
        "QDialog{background:#F5F6F8;}"
        "QLabel#dialogTitle{font-size:22px;font-weight:700;color:#111827;}"
        "QLabel#fieldLabel{color:#374151;font-weight:600;font-size:12px;}"
        "QFrame#formSection{background:white;border:1px solid #E5E7EB;border-radius:12px;}"
        "QPushButton#dialogPrimary{background:#C90000;color:white;border:none;border-radius:8px;"
        "padding:9px 18px;font-weight:700;}"
        "QPushButton#dialogPrimary:hover{background:#B80000;}"
        "QPushButton#dialogCancel{background:white;color:#374151;border:1px solid #D1D5DB;border-radius:8px;"
        "padding:9px 18px;}"
        "QPushButton#dialogCancel:hover{background:#F9FAFB;}"
        "QLineEdit,QComboBox,QSpinBox,QDateEdit,QTextEdit{background:white;border:1px solid #D1D5DB;"
        "border-radius:8px;padding:7px 10px;min-height:24px;}"
        "QLineEdit:focus,QComboBox:focus,QSpinBox:focus,QDateEdit:focus,QTextEdit:focus{border:1px solid #C90000;}"
        "QLineEdit[erreur=\"true\"],QComboBox[erreur=\"true\"]{border:1px solid #C90000;background:#FFF5F5;}");

    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(14);
    QLabel *title = new QLabel(edition ? "Modifier le véhicule" : "Nouveau véhicule");
    title->setObjectName("dialogTitle");
    root->addWidget(title);

    QFrame *section = new QFrame;
    section->setObjectName("formSection");
    QGridLayout *grid = new QGridLayout(section);
    grid->setContentsMargins(18, 16, 18, 16);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(10);

    auto field = [](const QString &label, QWidget *editor) {
        QWidget *box = new QWidget;
        QVBoxLayout *l = new QVBoxLayout(box);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(4);
        QLabel *lb = new QLabel(label);
        lb->setObjectName("fieldLabel");
        l->addWidget(lb);
        l->addWidget(editor);
        return box;
    };

    QLineEdit *dlgNom = new QLineEdit;
    dlgNom->setPlaceholderText("Ex : VSAV-01");
    QComboBox *dlgType = new QComboBox;
    dlgType->addItems({"Sélectionner un type", "Incendie", "Ambulance", "Commandement",
                       "Secours médical", "Intervention technique"});
    QLineEdit *dlgMarque = new QLineEdit;
    QLineEdit *dlgModele = new QLineEdit;
    QSpinBox *dlgCarburant = new QSpinBox;
    dlgCarburant->setRange(0, 100);
    dlgCarburant->setSuffix(" %");
    dlgCarburant->setValue(80);
    QSpinBox *dlgSeuil = new QSpinBox;
    dlgSeuil->setRange(0, 100);
    dlgSeuil->setSuffix(" %");
    dlgSeuil->setValue(25);
    QComboBox *dlgEtat = new QComboBox;
    dlgEtat->addItems({"Disponible", "En mission", "En maintenance", "Hors service"});
    QLineEdit *dlgLocalisation = new QLineEdit;
    QDateEdit *dlgAchat = new QDateEdit(QDate::currentDate());
    dlgAchat->setCalendarPopup(true);
    dlgAchat->setDisplayFormat(FMT);
    QDateEdit *dlgDerniere = new QDateEdit(QDate::currentDate());
    dlgDerniere->setCalendarPopup(true);
    dlgDerniere->setDisplayFormat(FMT);
    QDateEdit *dlgProchaine = new QDateEdit(QDate::currentDate().addMonths(6));
    dlgProchaine->setCalendarPopup(true);
    dlgProchaine->setDisplayFormat(FMT);
    dlgProchaine->setMinimumDate(dlgDerniere->date().addDays(1));
    QTextEdit *dlgRemarques = new QTextEdit;
    dlgRemarques->setFixedHeight(76);
    connect(dlgDerniere, &QDateEdit::dateChanged, &dlg, [=](const QDate &d) {
        dlgProchaine->setMinimumDate(d.addDays(1));
    });

    if (edition) {
        dlgNom->setText(table->item(ligne, 1)->text());
        dlgType->setCurrentText(table->item(ligne, 2)->text());
        dlgCarburant->setValue(table->item(ligne, 3)->text().toInt());
        dlgSeuil->setValue(table->item(ligne, 4)->text().toInt());
        dlgEtat->setCurrentText(table->item(ligne, 5)->text());
        dlgLocalisation->setText(table->item(ligne, 6)->text());
        QDate d1 = QDate::fromString(table->item(ligne, 7)->text(), FMT);
        QDate d2 = QDate::fromString(table->item(ligne, 8)->text(), FMT);
        QDate d3 = QDate::fromString(table->item(ligne, 9)->text(), FMT);
        if (d1.isValid()) dlgAchat->setDate(d1);
        if (d2.isValid()) dlgDerniere->setDate(d2);
        if (d3.isValid()) dlgProchaine->setDate(d3);
        dlgMarque->setText(table->item(ligne, 11)->text());
        dlgModele->setText(table->item(ligne, 12)->text());
        dlgRemarques->setPlainText(table->item(ligne, 13)->text());
    }

    grid->addWidget(field("Matricule / Nom du véhicule *", dlgNom), 0, 0);
    grid->addWidget(field("Type *", dlgType), 0, 1);
    grid->addWidget(field("Marque", dlgMarque), 1, 0);
    grid->addWidget(field("Modèle", dlgModele), 1, 1);
    grid->addWidget(field("Carburant (%)", dlgCarburant), 2, 0);
    grid->addWidget(field("Carburant minimum (%)", dlgSeuil), 2, 1);
    grid->addWidget(field("État", dlgEtat), 3, 0);
    grid->addWidget(field("Localisation", dlgLocalisation), 3, 1);
    grid->addWidget(field("Date d'achat", dlgAchat), 4, 0);
    grid->addWidget(field("Dernière maintenance", dlgDerniere), 4, 1);
    grid->addWidget(field("Prochaine maintenance", dlgProchaine), 5, 0);
    grid->addWidget(field("Remarques", dlgRemarques), 5, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    root->addWidget(section, 1);

    QHBoxLayout *buttons = new QHBoxLayout;
    buttons->addStretch();
    QPushButton *cancel = new QPushButton("Annuler");
    cancel->setObjectName("dialogCancel");
    QPushButton *save = new QPushButton("Enregistrer");
    save->setObjectName("dialogPrimary");
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    root->addLayout(buttons);

    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(save, &QPushButton::clicked, &dlg, [&]() {
        qInfo() << "[Vehicules] Save button clicked" << (edition ? "edit" : "add");
        bool okNom = !dlgNom->text().trimmed().isEmpty();
        bool okType = dlgType->currentIndex() != 0;
        marquerErreur(dlgNom, !okNom);
        marquerErreur(dlgType, !okType);
        if (!okNom || !okType) {
            toast(this, "Le nom et le type sont obligatoires", "#c62828");
            return;
        }
        if (dlgProchaine->date() <= dlgDerniere->date()) {
            toast(this, "La prochaine maintenance doit être après la dernière", "#c62828");
            return;
        }
        qInfo() << "[Vehicules] Validation completed";

        nom->setText(dlgNom->text().trimmed());
        type->setCurrentText(dlgType->currentText());
        marque->setText(dlgMarque->text());
        modele->setText(dlgModele->text());
        quantite->setValue(dlgCarburant->value());
        seuil->setValue(dlgSeuil->value());
        etat->setCurrentText(dlgEtat->currentText());
        localisation->setText(dlgLocalisation->text());
        dateAchat->setDate(dlgAchat->date());
        dateMaintenance->setDate(dlgDerniere->date());
        prochaineMaintenance->setDate(dlgProchaine->date());
        remarques->setText(dlgRemarques->toPlainText());

        if (edition) {
            table->setCurrentCell(ligne, 0);
            table->selectRow(ligne);
            modifier();
        } else {
            ajouter();
        }
        qInfo() << "[Vehicules] Dashboard update started";
        rechercher();
        majCartes();
        qInfo() << "[Vehicules] Dashboard update completed";
        qInfo() << "[Vehicules] Dialog closing";
        dlg.accept();
    });

    fondu(&dlg);
    dlg.exec();
}

void vehicules::afficherDetailsVehicule(int ligne)
{
    if (ligne < 0 || ligne >= table->rowCount())
        return;

    QDialog dlg(this);
    dlg.setWindowTitle("Détails du véhicule");
    dlg.resize(560, 520);
    dlg.setStyleSheet(
        "QDialog{background:#F5F6F8;}"
        "QFrame{background:white;border:1px solid #E5E7EB;border-radius:12px;}"
        "QLabel#detailsTitle{font-size:22px;font-weight:700;color:#111827;}"
        "QLabel#detailsLine{font-size:13px;color:#374151;padding:3px 0;}"
        "QPushButton{background:#C90000;color:white;border:none;border-radius:8px;padding:8px 18px;font-weight:700;}");

    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(20, 18, 20, 18);
    QLabel *title = new QLabel(table->item(ligne, 0)->text() + " - " + table->item(ligne, 1)->text());
    title->setObjectName("detailsTitle");
    root->addWidget(title);

    QFrame *box = new QFrame;
    QGridLayout *grid = new QGridLayout(box);
    grid->setContentsMargins(18, 16, 18, 16);
    grid->setHorizontalSpacing(20);
    grid->setVerticalSpacing(8);

    auto addLine = [&](int row, const QString &label, const QString &value) {
        QLabel *l = new QLabel("<b>" + label.toHtmlEscaped() + "</b>");
        l->setObjectName("detailsLine");
        QLabel *v = new QLabel(value.toHtmlEscaped());
        v->setObjectName("detailsLine");
        v->setWordWrap(true);
        grid->addWidget(l, row, 0);
        grid->addWidget(v, row, 1);
    };

    addLine(0, "Matricule", table->item(ligne, 1)->text());
    addLine(1, "Type", table->item(ligne, 2)->text());
    addLine(2, "Marque", table->item(ligne, 11)->text());
    addLine(3, "Modèle", table->item(ligne, 12)->text());
    addLine(4, "Carburant", table->item(ligne, 3)->text() + " %");
    addLine(5, "Carburant minimum", table->item(ligne, 4)->text() + " %");
    addLine(6, "État", table->item(ligne, 5)->text());
    addLine(7, "Localisation", table->item(ligne, 6)->text());
    addLine(8, "Date d'achat", table->item(ligne, 7)->text());
    addLine(9, "Dernière maintenance", table->item(ligne, 8)->text());
    addLine(10, "Prochaine maintenance", table->item(ligne, 9)->text());
    addLine(11, "Remarques", table->item(ligne, 13)->text());
    root->addWidget(box, 1);

    QPushButton *close = new QPushButton("Fermer");
    connect(close, &QPushButton::clicked, &dlg, &QDialog::accept);
    root->addWidget(close, 0, Qt::AlignRight);
    fondu(&dlg);
    dlg.exec();
}

void vehicules::afficherMaintenances()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Maintenances");
    dlg.resize(780, 520);
    dlg.setStyleSheet(
        "QDialog{background:#F5F6F8;}"
        "QLabel#dialogTitle{font-size:22px;font-weight:700;color:#111827;}"
        "QLabel#dialogSubtitle{font-size:13px;color:#6B7280;}"
        "QFrame#dialogCard{background:white;border:1px solid #E5E7EB;border-radius:12px;}"
        "QPushButton{background:#C90000;color:white;border:none;border-radius:8px;padding:8px 18px;font-weight:700;}"
        "QPushButton:hover{background:#B80000;}"
        "QTableWidget{background:white;border:none;selection-background-color:#FDE8E8;selection-color:#111827;}"
        "QHeaderView::section{background:#F3F4F6;color:#4B5563;font-size:12px;font-weight:600;"
        "border:none;border-bottom:1px solid #E5E7EB;padding:8px 6px;}"
        "QTableWidget::item{border-bottom:1px solid #F3F4F6;padding:4px;}");

    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);

    QLabel *title = new QLabel("Maintenances");
    title->setObjectName("dialogTitle");
    QLabel *subtitle = new QLabel("Prochaines maintenances triées par date prévue");
    subtitle->setObjectName("dialogSubtitle");
    root->addWidget(title);
    root->addWidget(subtitle);

    QFrame *card = new QFrame;
    card->setObjectName("dialogCard");
    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 14, 16, 14);

    QTableWidget *t = new QTableWidget(0, 4);
    t->setHorizontalHeaderLabels({"Véhicule", "Type", "Date prévue", "Jours restants"});
    t->verticalHeader()->setVisible(false);
    t->setShowGrid(false);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    t->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    t->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    t->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    t->verticalHeader()->setDefaultSectionSize(40);

    struct Maint { QString vehicule; QString type; QDate date; int jours; };
    QVector<Maint> rows;
    QDate today = QDate::currentDate();
    for (int r = 0; r < table->rowCount(); ++r) {
        if (!ligneVehiculeComplete(table, r)) continue;
        QDate date = QDate::fromString(table->item(r, 9)->text(), FMT);
        if (!date.isValid()) continue;
        rows.push_back({table->item(r, 0)->text() + "  " + table->item(r, 1)->text(),
                        table->item(r, 2)->text(), date, int(today.daysTo(date))});
    }
    std::sort(rows.begin(), rows.end(), [](const Maint &a, const Maint &b) { return a.date < b.date; });

    for (const Maint &m : rows) {
        int r = t->rowCount();
        t->insertRow(r);
        t->setItem(r, 0, new QTableWidgetItem(m.vehicule));
        t->setItem(r, 1, new QTableWidgetItem(m.type));
        t->setItem(r, 2, new QTableWidgetItem(m.date.toString(FMT)));
        QString jours = m.jours < 0 ? QString("Retard %1 j").arg(-m.jours) : QString("%1 j").arg(m.jours);
        QTableWidgetItem *it = new QTableWidgetItem(jours);
        QColor bg = m.jours < 0 || m.jours <= 7 ? QColor("#FDE8E8")
                                                : (m.jours <= 30 ? QColor("#FEF3C7") : QColor("#F3F4F6"));
        it->setBackground(bg);
        t->setItem(r, 3, it);
    }

    cardLayout->addWidget(t);
    root->addWidget(card, 1);

    QPushButton *close = new QPushButton("Fermer");
    connect(close, &QPushButton::clicked, &dlg, &QDialog::accept);
    root->addWidget(close, 0, Qt::AlignRight);

    fondu(&dlg);
    dlg.exec();
}

void vehicules::majTableauxDashboard()
{
    if (alertesRecentesLayout) {
        while (QLayoutItem *item = alertesRecentesLayout->takeAt(0)) {
            if (QWidget *w = item->widget()) w->deleteLater();
            delete item;
        }

        struct Alert { QString title; QString detail; QString date; QString color; };
        QVector<Alert> alerts;
        QDate today = QDate::currentDate();
        for (int r = 0; r < table->rowCount(); ++r) {
            if (!ligneVehiculeComplete(table, r)) continue;
            QString ref = table->item(r, 0)->text() + "  " + table->item(r, 1)->text();
            QString state = table->item(r, 5)->text();
            int q = table->item(r, 3)->text().toInt();
            int s = table->item(r, 4)->text().toInt();
            QDate pm = QDate::fromString(table->item(r, 9)->text(), FMT);
            if (q < s)
                alerts.push_back({"Carburant faible", ref + QString(" - %1 % / minimum %2 %").arg(q).arg(s),
                                  "État actuel", "#F59E0B"});
            if (state == "Hors service")
                alerts.push_back({"Véhicule hors service", ref + " - intervention requise", "État actuel", "#C90000"});
            if (pm.isValid()) {
                int d = int(today.daysTo(pm));
                if (d < 0)
                    alerts.push_back({"Maintenance en retard", ref + QString(" - retard de %1 jour(s)").arg(-d),
                                      pm.toString(FMT), "#C90000"});
                else if (d <= 30)
                    alerts.push_back({"Maintenance proche", ref + QString(" - dans %1 jour(s)").arg(d),
                                      pm.toString(FMT), "#F59E0B"});
            }
        }

        int shown = 0;
        for (const Alert &a : alerts) {
            if (shown++ >= 5) break;
            QFrame *row = new QFrame;
            row->setStyleSheet(QString("QFrame{background:white;border:0;border-left:3px solid %1;"
                                       "border-radius:0;padding-left:8px;}").arg(a.color));
            QHBoxLayout *rl = new QHBoxLayout(row);
            rl->setContentsMargins(10, 4, 4, 4);
            QVBoxLayout *texts = new QVBoxLayout;
            texts->setSpacing(1);
            QLabel *t = new QLabel("<b>" + a.title.toHtmlEscaped() + "</b>");
            QLabel *d = new QLabel(a.detail.toHtmlEscaped());
            d->setStyleSheet("color:#6B7280;font-size:12px;");
            texts->addWidget(t);
            texts->addWidget(d);
            rl->addLayout(texts, 1);
            QLabel *date = new QLabel(a.date);
            date->setStyleSheet("color:#9CA3AF;font-size:11px;");
            rl->addWidget(date);
            alertesRecentesLayout->addWidget(row);
        }
        if (shown == 0) {
            QLabel *empty = new QLabel("Aucune alerte récente.");
            empty->setStyleSheet("color:#6B7280;");
            alertesRecentesLayout->addWidget(empty);
        }
        alertesRecentesLayout->addStretch();
    }
}

// =====================================================================
//  ACTIONS CRUD : creation des lignes, ajout, modification et suppression
// =====================================================================
QString vehicules::ajouterLigne(const QString &n, const QString &t, const QString &m,
                                const QString &mod, int q, int s, const QString &e,
                                const QString &loc, const QString &da, const QString &dm,
                                const QString &pm, const QString &rem)
{
    qInfo() << "[Vehicules] Vehicle insertion started" << n;

    const bool sortingEnabled = table->isSortingEnabled();
    const int sortSection = table->horizontalHeader()->sortIndicatorSection();
    const Qt::SortOrder sortOrder = table->horizontalHeader()->sortIndicatorOrder();
    if (sortingEnabled)
        table->setSortingEnabled(false);

    const QString id = QString("VEH-%1").arg(prochainId++, 3, 10, QChar('0'));
    int row = table->rowCount();

    table->insertRow(row);

    table->setItem(row, 0, new QTableWidgetItem(id));
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
    table->setItem(row, 15, new QTableWidgetItem(""));

    QLabel *truck = new QLabel("V");
    truck->setAlignment(Qt::AlignCenter);
    truck->setFixedSize(30, 24);
    truck->setStyleSheet("background:#FDE8E8;color:#C90000;border:1px solid #F8CACA;"
                         "border-radius:6px;font-weight:800;");
    QWidget *truckCell = new QWidget;
    QHBoxLayout *truckLayout = new QHBoxLayout(truckCell);
    truckLayout->setContentsMargins(4, 4, 4, 4);
    truckLayout->addWidget(truck, 0, Qt::AlignCenter);
    table->setCellWidget(row, 15, truckCell);

    QWidget *actions = new QWidget;
    actions->setObjectName("actCell");
    QHBoxLayout *actLayout = new QHBoxLayout(actions);
    actLayout->setContentsMargins(4, 5, 4, 5);
    actLayout->setSpacing(5);

    QPushButton *btnView = new QPushButton("Voir");
    QPushButton *btnEdit = new QPushButton("Modifier");
    QPushButton *btnDel  = new QPushButton("Supprimer");
    btnView->setToolTip("Voir les détails");
    btnEdit->setToolTip("Modifier le véhicule");
    btnDel->setToolTip("Supprimer le véhicule");
    btnView->setMinimumSize(56, 30);
    btnEdit->setMinimumSize(76, 30);
    btnDel->setMinimumSize(88, 30);
    btnView->setStyleSheet(
        "QPushButton{background:#F3F4F6;color:#374151;border:1px solid #D1D5DB;"
        "border-radius:6px;padding:4px 10px;font-weight:600;}"
        "QPushButton:hover{background:#E5E7EB;}");
    btnEdit->setStyleSheet(
        "QPushButton{background:#FFFFFF;color:#C90000;border:1px solid #C90000;"
        "border-radius:6px;padding:4px 10px;font-weight:600;}"
        "QPushButton:hover{background:#FDE8E8;}");
    btnDel->setStyleSheet(
        "QPushButton{background:#C90000;color:#FFFFFF;border:1px solid #C90000;"
        "border-radius:6px;padding:4px 10px;font-weight:600;}"
        "QPushButton:hover{background:#A80000;}");
    btnView->setCursor(Qt::PointingHandCursor);
    btnEdit->setCursor(Qt::PointingHandCursor);
    btnDel->setCursor(Qt::PointingHandCursor);
    actLayout->addWidget(btnView);
    actLayout->addWidget(btnEdit);
    actLayout->addWidget(btnDel);

    connect(btnView, &QPushButton::clicked, this, [=]() {
        int r = trouverLigneParId(table, id);
        if (r >= 0) afficherDetailsVehicule(r);
    });

    connect(btnDel, &QPushButton::clicked, this, [=]() {
        int r = trouverLigneParId(table, id);
        if (r < 0) return;
        if (QMessageBox::question(this, "Suppression", "Supprimer ce véhicule ?") == QMessageBox::Yes) {
            table->removeRow(r);
            toast(this, "Véhicule supprimé", "#c62828");
        }
    });

    connect(btnEdit, &QPushButton::clicked, this, [=]() {
        int r = trouverLigneParId(table, id);
        if (r < 0) return;
        table->selectRow(r);
        ouvrirFormulaireVehicule(true, r);
    });

    table->setCellWidget(row, 10, actions);

    if (sortingEnabled) {
        table->setSortingEnabled(true);
        if (sortSection >= 0)
            table->sortItems(sortSection, sortOrder);
    }

    qInfo() << "[Vehicules] Vehicle insertion completed" << id << "row" << trouverLigneParId(table, id);
    return id;
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

    qInfo() << "[Vehicules] Add vehicle requested";
    const QString id = ajouterLigne(nom->text().trimmed(), type->currentText(), marque->text(), modele->text(),
                                    quantite->value(), seuil->value(), etat->currentText(), localisation->text(),
                                    dateAchat->date().toString(FMT),
                                    dateMaintenance->date().toString(FMT),
                                    prochaineMaintenance->date().toString(FMT),
                                    remarques->text());

    qInfo() << "[Vehicules] Dashboard update started after insertion";
    colorierLignes();
    const int newRow = trouverLigneParId(table, id);
    flashLigne(table, newRow, [this]() { colorierLignes(); });
    qInfo() << "[Vehicules] Dashboard update completed after insertion" << id << "row" << newRow;

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

    struct Champ { int col; QString nouveau; };
    QVector<Champ> champs = {
        {1, nom->text().trimmed()},
        {2, type->currentText()},
        {3, QString::number(quantite->value())},
        {4, QString::number(seuil->value())},
        {5, etat->currentText()},
        {6, localisation->text()},
        {7, dateAchat->date().toString(FMT)},
        {8, dateMaintenance->date().toString(FMT)},
        {9, prochaineMaintenance->date().toString(FMT)},
        {11, marque->text()},
        {12, modele->text()},
        {13, remarques->text()}
    };

    for (const Champ &c : champs) {
        QString ancien = table->item(r, c.col)->text();
        if (ancien != c.nouveau) {
            table->item(r, c.col)->setText(c.nouveau);
        }
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
//  Affichage local des alertes du tableau de bord
// =====================================================================
void vehicules::afficherAlertes(bool afficherSiVide)
{
    QDate today = QDate::currentDate();

    struct AlertItem {
        QString emoji;
        QString titre;
        QString vehicule;
        QString description;
        QString detail;
        QString badge;
        QString couleur;
        bool critique = false;
        bool carburant = false;
    };

    QVector<AlertItem> alertes;

    for (int r = 0; r < table->rowCount(); r++) {
        if (!table->item(r, 1) || !table->item(r, 5) || !table->item(r, 3) || !table->item(r, 4))
            continue;

        QString ref = table->item(r, 0)->text() + " — " + table->item(r, 1)->text();
        QString e = table->item(r, 5)->text();
        QString loc = table->item(r, 6) ? table->item(r, 6)->text() : QString();
        int q = table->item(r, 3)->text().toInt();
        int s = table->item(r, 4)->text().toInt();

        if (e == "Hors service") {
            QString desc = "État : Hors service";
            QString detail = loc.isEmpty() ? QString() : "Localisation : " + loc;
            alertes.push_back({"❌", "Véhicule hors service", ref, desc, detail,
                               "Critique", "#C90000", true, false});
        }
        if (q < s) {
            QString desc = QString("Carburant : %1 % / minimum : %2 %").arg(q).arg(s);
            QString detail = QString("Il manque %1 point(s) pour atteindre le seuil minimum.").arg(s - q);
            alertes.push_back({"⛽", "Carburant faible", ref, desc, detail,
                               "Attention", "#F59E0B", false, true});
        }
        if (table->item(r, 9)) {
            QDate pm = QDate::fromString(table->item(r, 9)->text(), FMT);
            if (pm.isValid()) {
                int jours = int(today.daysTo(pm));
                if (jours < 0) {
                    QString desc = QString("Maintenance en retard de %1 jour(s)").arg(-jours);
                    alertes.push_back({"📅", "Maintenance dépassée", ref, desc,
                                       "Date prévue : " + pm.toString(FMT),
                                       "Critique", "#C90000", true, false});
                } else if (jours <= 30) {
                    QString desc = QString("Maintenance prévue dans %1 jour(s)").arg(jours);
                    alertes.push_back({"⚠️", "Maintenance proche", ref, desc,
                                       "Date prévue : " + pm.toString(FMT),
                                       "Attention", "#F59E0B", false, false});
                }
            }
        }
    }

    int concernes = alertes.size();
    if (concernes == 0) {
        if (afficherSiVide)
            toast(this, "✓  Aucune alerte pour le moment");
        return;
    }

    int critiques = 0, carburant = 0;
    for (const AlertItem &a : alertes) {
        if (a.critique) critiques++;
        if (a.carburant) carburant++;
    }

    if (!afficherSiVide)
        return;

    QDialog *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle("Alertes des véhicules");
    dlg->resize(760, qMin(720, 260 + int(alertes.size()) * 116));
    dlg->setMinimumSize(680, 460);
    dlg->setStyleSheet(R"QSS(
        QDialog{background:#F5F6F8;}
        QScrollArea{background:#F5F6F8;border:none;}
        QWidget#alertsInner{background:#F5F6F8;}
        QFrame#alertCard,QFrame#summaryCard{background:#FFFFFF;border:1px solid #E5E7EB;border-radius:12px;}
        QLabel{border:none;background:transparent;color:#111827;}
        QLabel#alertTitle{font-size:26px;font-weight:700;color:#111827;}
        QLabel#alertSubtitle{font-size:13px;color:#6B7280;}
        QLabel#sectionTitle{font-size:15px;font-weight:700;color:#111827;}
        QLabel#summaryValue{font-size:24px;font-weight:700;color:#111827;}
        QLabel#summaryCaption{font-size:12px;color:#6B7280;}
        QLabel#badge{border-radius:10px;padding:3px 10px;font-size:11px;font-weight:700;}
        QPushButton#closeButton{background:#C90000;color:#FFFFFF;border:1px solid #C90000;border-radius:8px;padding:8px 18px;font-weight:700;}
        QPushButton#closeButton:hover{background:#A80000;}
    )QSS");

    QVBoxLayout *root = new QVBoxLayout(dlg);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget *inner = new QWidget;
    inner->setObjectName("alertsInner");
    QVBoxLayout *iv = new QVBoxLayout(inner);
    iv->setContentsMargins(24, 22, 24, 18);
    iv->setSpacing(14);

    QLabel *titre = new QLabel("Alertes des véhicules");
    titre->setObjectName("alertTitle");
    QLabel *sousTitre = new QLabel("Surveillance de l'état et de la maintenance de votre parc  •  " + today.toString(FMT));
    sousTitre->setObjectName("alertSubtitle");
    iv->addWidget(titre);
    iv->addWidget(sousTitre);

    auto summary = [](const QString &emoji, int value, const QString &caption) -> QFrame * {
        QFrame *f = new QFrame;
        f->setObjectName("summaryCard");
        f->setMinimumHeight(92);
        QHBoxLayout *l = new QHBoxLayout(f);
        l->setContentsMargins(16, 12, 16, 12);
        l->setSpacing(12);
        QLabel *ico = new QLabel(emoji);
        ico->setAlignment(Qt::AlignCenter);
        ico->setFixedSize(42, 42);
        ico->setStyleSheet("background:#FDE8E8;border-radius:21px;font-family:'Segoe UI Emoji';font-size:24px;");
        l->addWidget(ico);
        QVBoxLayout *txt = new QVBoxLayout;
        txt->setSpacing(0);
        QLabel *v = new QLabel(QString::number(value));
        v->setObjectName("summaryValue");
        QLabel *c = new QLabel(caption);
        c->setObjectName("summaryCaption");
        txt->addWidget(v);
        txt->addWidget(c);
        l->addLayout(txt, 1);
        ombre(f);
        return f;
    };

    QHBoxLayout *resume = new QHBoxLayout;
    resume->setSpacing(14);
    resume->addWidget(summary("🚨", concernes, "Véhicules en alerte"), 1);
    resume->addWidget(summary("❌", critiques, "Alertes critiques"), 1);
    resume->addWidget(summary("⛽", carburant, "Alertes carburant"), 1);
    iv->addLayout(resume);

    QLabel *section = new QLabel("Alertes actives");
    section->setObjectName("sectionTitle");
    iv->addWidget(section);

    for (const AlertItem &a : alertes) {
        QFrame *card = new QFrame;
        card->setObjectName("alertCard");
        QVBoxLayout *cl = new QVBoxLayout(card);
        cl->setContentsMargins(16, 12, 16, 12);
        cl->setSpacing(6);

        QHBoxLayout *head = new QHBoxLayout;
        QLabel *title = new QLabel(QString("<span style='font-family:Segoe UI Emoji;font-size:20px'>%1</span>&nbsp; <b>%2</b>")
                                       .arg(a.emoji, a.titre.toHtmlEscaped()));
        QLabel *badge = new QLabel(a.badge);
        badge->setObjectName("badge");
        badge->setStyleSheet(QString("QLabel#badge{background:%1;color:white;border-radius:10px;padding:3px 10px;font-size:11px;font-weight:700;}").arg(a.couleur));
        head->addWidget(title);
        head->addStretch();
        head->addWidget(badge);
        cl->addLayout(head);

        QLabel *veh = new QLabel(a.vehicule.toHtmlEscaped());
        veh->setStyleSheet("font-weight:700;color:#374151;");
        QLabel *desc = new QLabel(a.description.toHtmlEscaped());
        desc->setStyleSheet("color:#111827;");
        cl->addWidget(veh);
        cl->addWidget(desc);
        if (!a.detail.isEmpty()) {
            QLabel *det = new QLabel(a.detail.toHtmlEscaped());
            det->setStyleSheet("color:#6B7280;font-size:12px;");
            cl->addWidget(det);
        }
        ombre(card);
        iv->addWidget(card);
    }

    scroll->setWidget(inner);
    root->addWidget(scroll, 1);

    QFrame *foot = new QFrame;
    foot->setStyleSheet("QFrame{background:#FFFFFF;border-top:1px solid #E5E7EB;}");
    QHBoxLayout *fl = new QHBoxLayout(foot);
    fl->setContentsMargins(24, 12, 24, 12);
    fl->addStretch();

    QPushButton *fermer = new QPushButton("Fermer");
    fermer->setObjectName("closeButton");
    fermer->setMinimumHeight(36);
    fermer->setMinimumWidth(110);
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
    int total = 0, dispo = 0, maintenance = 0, mission = 0, hs = 0, alertes = 0, qteTotale = 0;
    int nbEtat[4] = {0, 0, 0, 0};   // Disponible, En mission, En maintenance, Hors service
    QStringList lignes;
    int minScore = 101;
    QString minNom, minInfo;

    for (int r = 0; r < table->rowCount(); r++) {
        if (!ligneVehiculeComplete(table, r))
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
        else if (e == "En mission")      { mission++; nbEtat[1]++; }
        else if (e == "En maintenance")  { maintenance++; nbEtat[2]++; }
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
    set("val_maintenance", maintenance);
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

    if (DonutWidget *w = dynamic_cast<DonutWidget *>(donutEtat)) {
        w->setData({"Disponible", "En mission", "En maintenance", "Hors service"},
                   {dispo, mission, maintenance, hs},
                   {QColor("#22A559"), QColor("#2563EB"), QColor("#F59E0B"), QColor("#C90000")});
        w->animer();
    }
    majTableauxDashboard();

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
    if (statusBar())
        statusBar()->hide();

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
    Q_UNUSED(role);
    if (QLabel *l = findChild<QLabel *>("lblUser"))
        l->setText("👤  " + nomUtilisateur + "  ▾");
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

