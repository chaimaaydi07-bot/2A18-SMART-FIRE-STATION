#include "smart.h"
#include "equipement.h"          // couche Modèle : toutes les requêtes SQL
#include "equipement_extra.h"    // couche Modèle : seuil, prochaine maintenance, remarques, localisation
#include <QSqlQueryModel>
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
#include <QConicalGradient>
#include <QPainterPath>
#include <QRegion>
#include <QEvent>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QStatusBar>
#include <QIcon>
#include <QMenu>
#include <QAction>
#include <QProgressBar>
#include <QRegularExpression>
#include <QHash>
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
#include <QGroupBox>
#include <QKeyEvent>
#include <QDesktopServices>
#include <QUrl>


// =====================================================================
//  Colonnes du tableau
//  0 ID | 1 Nom | 2 Type | 3 Quantité | 4 Seuil | 5 État | 6 Localisation
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

// ----- thème (clair / sombre) -----
static bool g_sombre = false;
static QColor cTexte()     { return QColor(g_sombre ? "#e4e4ea" : "#444444"); }
static QColor cTexteFort() { return QColor(g_sombre ? "#f2f2f6" : "#222222"); }
static QColor cTexteDoux() { return QColor(g_sombre ? "#a3a3b0" : "#888888"); }
static QColor cPiste()     { return QColor(g_sombre ? "#3b3b47" : "#f1f1f1"); }

// ----- ligne du tableau survolée par la souris -----
static int g_hoverRow = -1;
static void surlignerHover(QPainter *p, const QRect &r, int row)
{
    if (row != g_hoverRow) return;
    p->fillRect(r, g_sombre ? QColor(255, 255, 255, 22) : QColor(196, 0, 0, 20));
}

// =====================================================================
//  MÉTIER INNOVANT : maintenance prédictive
//  Score de santé d'un équipement (0 - 100 %), calculé automatiquement :
//   - part du cycle de maintenance déjà écoulée (dernière -> prochaine)
//   - âge de l'équipement
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
    if (etat == "Hors service")   f << "équipement hors service";
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

// Couleurs des états : case entière colorée, comme les disponibilités du module Personnel
static QColor fondEtat(const QString &t)
{
    if (t == "Hors service" || t == "Suppression" || t == "Critique")  return QColor("#ffd8dc");
    if (t == "En maintenance" || t == "Alerte" || t == "À surveiller") return QColor("#fdebd0");
    if (t == "En mission" || t == "Modification")                      return QColor("#e3eefa");
    if (t == "Planification")                                          return QColor("#ece3f5");
    if (t == "Export")                                                 return QColor("#dff2f3");
    if (t == "Système")                                                return QColor("#eceff1");
    return QColor("#d7f4df");   // Disponible, Ajout, Bon
}
static QColor texteEtat(const QString &t)
{
    if (t == "Hors service" || t == "Suppression" || t == "Critique")  return QColor("#9d1020");
    if (t == "En maintenance" || t == "Alerte" || t == "À surveiller") return QColor("#9d5500");
    if (t == "En mission" || t == "Modification")                      return QColor("#26598f");
    if (t == "Planification")                                          return QColor("#5b2a86");
    if (t == "Export")                                                 return QColor("#0f6670");
    if (t == "Système")                                                return QColor("#455a64");
    return QColor("#176d37");
}

// Coins arrondis d'un tableau (même rendu que le module Personnel)
namespace {
class CoinsArrondisEquip : public QObject {
public:
    CoinsArrondisEquip(QWidget *w, int rayon) : QObject(w), m_rayon(rayon) { w->installEventFilter(this); }
protected:
    bool eventFilter(QObject *o, QEvent *e) override {
        if (e->type() == QEvent::Resize) {
            auto *w = static_cast<QWidget *>(o);
            QPainterPath chemin;
            chemin.addRoundedRect(QRectF(w->rect()), m_rayon, m_rayon);
            w->setMask(QRegion(chemin.toFillPolygon().toPolygon()));
        }
        return false;
    }
private:
    int m_rayon;
};
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

        // case entière colorée + texte en gras (même rendu que le module Personnel)
        p->save();
        p->fillRect(opt.rect, fondEtat(txt));
        QFont f = o.font;
        f.setBold(true);
        p->setFont(f);
        p->setPen(texteEtat(txt));
        p->drawText(opt.rect, Qt::AlignCenter, txt);
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

        QColor c("#1e8e5a");                       // couleurs de la charte graphique
        if (score < 40)      c = QColor("#c62828");
        else if (score < 70) c = QColor("#c46a00");

        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        QRectF piste(opt.rect.left() + 8, opt.rect.center().y() - 5, opt.rect.width() - 58, 10);
        p->setPen(Qt::NoPen);
        p->setBrush(cPiste());
        p->drawRoundedRect(piste, 5, 5);
        p->setBrush(c);
        p->drawRoundedRect(QRectF(piste.left(), piste.top(), piste.width() * score / 100.0, piste.height()), 5, 5);
        p->setPen(cTexteFort());
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

// couleurs d'un type d'equipement (pastille + apercu du formulaire)
inline void couleursType(const QString &t, QColor &fond, QColor &texte)
{
    if (t == "Protection")         { fond = QColor("#ffe3e5"); texte = QColor("#c62828"); }
    else if (t == "Secours")       { fond = QColor("#fdebd0"); texte = QColor("#b45f06"); }
    else if (t == "Communication") { fond = QColor("#e3eefa"); texte = QColor("#1565c0"); }
    else if (t == "Médical")       { fond = QColor("#d7f4df"); texte = QColor("#1e7a3c"); }
    else if (t == "Outillage")     { fond = QColor("#ece3f5"); texte = QColor("#6a1b9a"); }
    else                           { fond = QColor("#eceff1"); texte = QColor("#455a64"); }
}

// colonne "Type" : pastille coloree
class TypeChipDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override {
        QStyleOptionViewItem o(opt);
        initStyleOption(&o, idx);
        const QString txt = o.text;
        o.text.clear();
        QStyle *st = o.widget ? o.widget->style() : QApplication::style();
        st->drawControl(QStyle::CE_ItemViewItem, &o, p, o.widget);
        surlignerHover(p, opt.rect, idx.row());
        if (txt.isEmpty()) return;

        QColor fond, texte;
        couleursType(txt, fond, texte);
        QFont f = o.font;
        f.setPixelSize(11);
        f.setBold(true);
        const QFontMetrics fm(f);
        const double w = qMin<double>(fm.horizontalAdvance(txt) + 28, opt.rect.width() - 14);
        const QRectF pill(opt.rect.left() + 8, opt.rect.center().y() - 12, w, 24);
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setPen(Qt::NoPen);
        p->setBrush(fond);
        p->drawRoundedRect(pill, 12, 12);
        p->setBrush(texte);
        p->drawEllipse(QPointF(pill.left() + 11, pill.center().y()), 3.4, 3.4);
        p->setFont(f);
        p->setPen(texte);
        p->drawText(pill.adjusted(20, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft,
                    fm.elidedText(txt, Qt::ElideRight, int(w - 26)));
        p->restore();
    }
};

// colonne "Stock" : quantite + jauge par rapport au seuil minimum
class StockCelluleDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override {
        QStyleOptionViewItem o(opt);
        initStyleOption(&o, idx);
        const int q = o.text.toInt();
        o.text.clear();
        QStyle *st = o.widget ? o.widget->style() : QApplication::style();
        st->drawControl(QStyle::CE_ItemViewItem, &o, p, o.widget);
        surlignerHover(p, opt.rect, idx.row());

        const int s = idx.sibling(idx.row(), 4).data(Qt::DisplayRole).toString().toInt();
        const bool bas = q < s;
        const double cy = opt.rect.center().y();

        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        QFont f = o.font;
        f.setBold(true);
        f.setPixelSize(14);
        p->setFont(f);
        p->setPen(bas ? QColor("#c26a00") : cTexteFort());
        p->drawText(QRectF(opt.rect.left() + 10, opt.rect.top(), 40, opt.rect.height()),
                    Qt::AlignVCenter | Qt::AlignLeft, QString::number(q));

        const double bx = opt.rect.left() + 52;
        const double bw = qMax(20, opt.rect.width() - 66);
        p->setPen(Qt::NoPen);
        p->setBrush(cPiste());
        p->drawRoundedRect(QRectF(bx, cy - 4, bw, 8), 4, 4);

        const double maxv = qMax(2.0 * s, double(qMax(q, 1)));
        const double ratio = qBound(0.0, q / maxv, 1.0);
        const QColor c = bas ? QColor("#ef6c00") : QColor("#2e9e5a");
        QLinearGradient g(bx, 0, bx + bw, 0);
        g.setColorAt(0, c.lighter(135));
        g.setColorAt(1, c);
        p->setBrush(g);
        if (ratio > 0.0)
            p->drawRoundedRect(QRectF(bx, cy - 4, qMax(8.0, bw * ratio), 8), 4, 4);

        if (s > 0) {                                  // repere du seuil minimum
            const double sx = bx + bw * qBound(0.0, s / maxv, 1.0);
            p->setPen(QPen(cTexteFort(), 1.6));
            p->drawLine(QPointF(sx, cy - 8), QPointF(sx, cy + 8));
        }
        p->restore();
    }
};

// colonne "Localisation" : pastille avec epingle
class LocPillDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override {
        QStyleOptionViewItem o(opt);
        initStyleOption(&o, idx);
        const QString txt = o.text;
        o.text.clear();
        QStyle *st = o.widget ? o.widget->style() : QApplication::style();
        st->drawControl(QStyle::CE_ItemViewItem, &o, p, o.widget);
        surlignerHover(p, opt.rect, idx.row());

        QFont f = o.font;
        f.setPixelSize(12);
        f.setBold(!txt.isEmpty());
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setFont(f);
        if (txt.isEmpty()) {
            p->setPen(QColor("#b9b9c4"));
            p->drawText(opt.rect.adjusted(12, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, "—");
        } else {
            const QFontMetrics fm(f);
            const QString el = fm.elidedText(txt, Qt::ElideRight, opt.rect.width() - 60);
            const double w = fm.horizontalAdvance(el) + 34;
            const QRectF pill(opt.rect.left() + 8, opt.rect.center().y() - 12, w, 24);
            p->setPen(Qt::NoPen);
            p->setBrush(QColor("#eef1f6"));
            p->drawRoundedRect(pill, 12, 12);
            p->setBrush(QColor("#c40000"));
            p->drawEllipse(QPointF(pill.left() + 13, pill.center().y()), 3.6, 3.6);
            p->setPen(QColor("#3b4a63"));
            p->drawText(pill.adjusted(24, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, el);
        }
        p->restore();
    }
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

// logo animé : arc de sirène qui tourne, ondes radar, respiration,
// zoom au survol et onde plus forte au clic
class LogoAnimeEquip : public QWidget {
public:
    explicit LogoAnimeEquip(const QPixmap &logo, QWidget *parent = nullptr)
        : QWidget(parent), pix(logo)
    {
        setFixedSize(140, 134);
        setCursor(Qt::PointingHandCursor);
        setStyleSheet("background:transparent;");

        QVariantAnimation *intro = new QVariantAnimation(this);
        intro->setDuration(1000);
        intro->setStartValue(0.0);
        intro->setEndValue(1.0);
        intro->setEasingCurve(QEasingCurve::OutBack);
        connect(intro, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            zoomIntro = v.toDouble();
            update();
        });
        intro->start(QAbstractAnimation::DeleteWhenStopped);

        QVariantAnimation *tour = new QVariantAnimation(this);
        tour->setDuration(2800);
        tour->setStartValue(0.0);
        tour->setEndValue(360.0);
        tour->setLoopCount(-1);
        connect(tour, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            angle = v.toDouble();
            update();
        });
        tour->start();

        QVariantAnimation *onde = new QVariantAnimation(this);
        onde->setDuration(2400);
        onde->setStartValue(0.0);
        onde->setEndValue(1.0);
        onde->setLoopCount(-1);
        connect(onde, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            phase = v.toDouble();
        });
        onde->start();

        survol = new QVariantAnimation(this);
        survol->setDuration(260);
        survol->setEasingCurve(QEasingCurve::OutCubic);
        connect(survol, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            zoomSurvol = v.toDouble();
            update();
        });

        clic = new QVariantAnimation(this);
        clic->setDuration(800);
        clic->setStartValue(0.0);
        clic->setEndValue(1.0);
        clic->setEasingCurve(QEasingCurve::OutCubic);
        connect(clic, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            phaseClic = v.toDouble();
            update();
        });
    }

protected:
    bool event(QEvent *e) override {
        if (e->type() == QEvent::Enter)      animerSurvol(1.10);
        else if (e->type() == QEvent::Leave) animerSurvol(1.0);
        return QWidget::event(e);
    }

    void mousePressEvent(QMouseEvent *) override {
        clic->stop();
        clic->start();
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);

        const QPointF c(width() / 2.0, height() / 2.0);
        const double base = 86.0;

        // ondes radar
        for (int k = 0; k < 2; k++) {
            double t = std::fmod(phase + k * 0.5, 1.0);
            double r = base * 0.5 + 4 + t * 20;
            p.setPen(QPen(QColor(255, 214, 102, int(150 * (1.0 - t))), 2));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, r, r);
        }

        // onde plus forte au clic
        if (phaseClic > 0.0 && phaseClic < 1.0) {
            double r = base * 0.5 + 4 + phaseClic * 24;
            p.setPen(QPen(QColor(255, 255, 255, int(230 * (1.0 - phaseClic))), 3));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, r, r);
        }

        // arc de sirène qui tourne
        QConicalGradient cg(c, -angle);
        cg.setColorAt(0.0, QColor(255, 214, 102, 255));
        cg.setColorAt(0.30, QColor(255, 214, 102, 0));
        cg.setColorAt(1.0, QColor(255, 214, 102, 0));
        p.setPen(QPen(QBrush(cg), 4, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        double rs = base * 0.5 + 6;
        p.drawEllipse(c, rs, rs);

        // logo : zoom d'entrée + respiration + zoom au survol
        double s = zoomIntro * zoomSurvol * (1.0 + 0.02 * std::sin(angle * 3.14159265358979 / 90.0));
        double size = base * s;
        QRectF r(c.x() - size / 2, c.y() - size / 2, size, size);
        QPainterPath path;
        path.addRoundedRect(r, 14 * s, 14 * s);
        p.save();
        p.setClipPath(path);
        p.drawPixmap(r, pix, QRectF(pix.rect()));
        p.restore();
    }

private:
    void animerSurvol(double cible) {
        survol->stop();
        survol->setStartValue(zoomSurvol);
        survol->setEndValue(cible);
        survol->start();
    }

    QPixmap pix;
    QVariantAnimation *survol = nullptr;
    QVariantAnimation *clic = nullptr;
    double zoomIntro = 0.0;
    double zoomSurvol = 1.0;
    double angle = 0.0;
    double phase = 0.0;
    double phaseClic = 0.0;
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
                               QString("<b>%1</b><br>%2 équipement(s)  —  %3 %")
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
            p.setPen(QPen(cPiste(), th));
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
        p.setPen(cTexteFort());
        p.drawText(QRectF(rect.x(), rect.y() + rect.height() / 2 - 26, rect.width(), 34),
                   Qt::AlignCenter, QString::number(total));
        QFont small = font();
        small.setPointSize(9);
        p.setFont(small);
        p.setPen(cTexteDoux());
        p.drawText(QRectF(rect.x(), rect.y() + rect.height() / 2 + 6, rect.width(), 20),
                   Qt::AlignCenter, "équipements");

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
            p.setPen(cTexte());
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
            p.setPen(cTexteDoux());
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
            p.setPen(hv ? cTexteFort() : cTexte());
            p.drawText(QRect(0, y, labelW - 8, 22), Qt::AlignVCenter | Qt::AlignLeft,
                       fm.elidedText(rows[i].label, Qt::ElideRight, labelW - 8));
            p.setPen(Qt::NoPen);
            p.setBrush(cPiste());
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
            p.setPen(cTexteFort());
            p.drawText(QRect(labelW + barW + 8, y, valueW, 22), Qt::AlignVCenter | Qt::AlignLeft,
                       QString::number(rows[i].value));
        }
    }
private:
    QVector<Row> rows;
    int hover = -1;
};

// ---------- Stock : quantité vs seuil (dégradé + bulle au survol) ----------
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
                               : QString("<span style='color:#2e7d32'>stock suffisant</span>");
            QToolTip::showText(e->globalPosition().toPoint(),
                               QString("<b>%1</b><br>Quantité : %2  •  seuil : %3<br>%4")
                                   .arg(r.label).arg(r.qte).arg(r.seuil).arg(etat), this);
        } else {
            QToolTip::hideText();
        }
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        if (rows.isEmpty()) {
            p.setPen(cTexteDoux());
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

            p.setPen(hv ? cTexteFort() : cTexte());
            p.drawText(QRect(0, y, labelW - 8, 22), Qt::AlignVCenter | Qt::AlignLeft,
                       fm.elidedText(r.label, Qt::ElideRight, labelW - 8));

            p.setPen(Qt::NoPen);
            p.setBrush(cPiste());
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

            p.setPen(bas ? QColor("#c62828") : cTexteFort());
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
    QSettings st("SmartFireStation", "Equipements");
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
    QSettings st("SmartFireStation", "Equipements");
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
           "Smart Fire Station a détecté une alerte automatique sur les équipements de la caserne :\n\n"
           + lignes.join("\n")
           + "\n\nDate : " + QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm")
           + "\n\nMerci de prendre les mesures nécessaires.\n\n"
             "— Smart Fire Station (message automatique)";
}


// ---------- email HTML (compatible avec Gmail / Outlook : tableaux + styles en ligne) ----------
static QString htmlEmailAlerte(const QStringList &lignes)
{
    QString cartes;
    for (const QString &brut : lignes) {
        QString l = brut.trimmed();
        if (l.startsWith("• ")) l = l.mid(2);
        QString genre = "ALERTE", coul = "#c40000", fond = "#fdeced";
        if (l.startsWith("[ÉTAT]")) {
            genre = "ÉTAT";
            coul = "#c62828";
            fond = "#fdeced";
            l = l.mid(6).trimmed();
        } else if (l.startsWith("[QUANTITÉ]")) {
            genre = "STOCK";
            coul = "#ef6c00";
            fond = "#fdf0e0";
            l = l.mid(10).trimmed();
        }
        QString titre = l, detail;
        const int i = l.indexOf(" — ");
        if (i > 0) { titre = l.left(i); detail = l.mid(i + 3); }

        cartes += "<table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" style=\"margin:10px 0;background:" + fond
                  + ";border-left:5px solid " + coul + ";border-radius:8px;\"><tr><td style=\"padding:14px 18px;\">"
                                                       "<span style=\"background:" + coul + ";color:#ffffff;font-size:10px;font-weight:bold;letter-spacing:1px;"
                           "padding:3px 10px;border-radius:10px;\">" + genre.toHtmlEscaped() + "</span>"
                                            "<div style=\"font-size:16px;font-weight:bold;color:#302934;margin-top:9px;\">" + titre.toHtmlEscaped() + "</div>"
                                            "<div style=\"font-size:13px;color:#666666;margin-top:4px;\">" + detail.toHtmlEscaped() + "</div>"
                                             "</td></tr></table>";
    }
    return "<html><body style=\"margin:0;padding:0;background:#f4f4f6;font-family:'Segoe UI',Arial,sans-serif;\">"
           "<table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" style=\"background:#f4f4f6;padding:28px 0;\"><tr><td align=\"center\">"
           "<table width=\"620\" cellpadding=\"0\" cellspacing=\"0\" style=\"background:#ffffff;border-radius:14px;border:1px solid #e6e0e0;\">"
           "<tr><td style=\"background:#c40000;padding:28px 32px;border-radius:14px 14px 0 0;\">"
           "<div style=\"color:#ffd6d6;font-size:12px;letter-spacing:3px;font-weight:bold;\">SMART FIRE STATION</div>"
           "<div style=\"color:#ffffff;font-size:26px;font-weight:bold;margin-top:8px;\">Alerte équipements</div>"
           "<div style=\"color:#ffd6d6;font-size:14px;margin-top:6px;\">" + QString::number(lignes.size())
           + " équipement(s) nécessitent votre attention</div></td></tr>"
             "<tr><td style=\"padding:26px 32px 8px 32px;color:#302934;font-size:14px;line-height:1.5;\">Bonjour,<br><br>"
             "Smart Fire Station a détecté une alerte automatique sur les équipements de la caserne :</td></tr>"
             "<tr><td style=\"padding:6px 32px;\">" + cartes + "</td></tr>"
                      "<tr><td style=\"padding:12px 32px 28px 32px;color:#555555;font-size:14px;\">Merci de prendre les mesures nécessaires.</td></tr>"
                      "<tr><td style=\"background:#faf5f5;padding:18px 32px;color:#999999;font-size:11px;border-top:1px solid #efe4e4;"
                      "border-radius:0 0 14px 14px;\">Message automatique &bull; " + QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm")
           + " &bull; Smart Fire Station</td></tr></table></td></tr></table></body></html>";
}

static QString htmlEmailTest()
{
    return "<html><body style=\"margin:0;padding:0;background:#f4f4f6;font-family:'Segoe UI',Arial,sans-serif;\">"
           "<table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" style=\"background:#f4f4f6;padding:28px 0;\"><tr><td align=\"center\">"
           "<table width=\"560\" cellpadding=\"0\" cellspacing=\"0\" style=\"background:#ffffff;border-radius:14px;border:1px solid #e6e0e0;\">"
           "<tr><td style=\"background:#c40000;padding:26px 32px;border-radius:14px 14px 0 0;\">"
           "<div style=\"color:#ffd6d6;font-size:12px;letter-spacing:3px;font-weight:bold;\">SMART FIRE STATION</div>"
           "<div style=\"color:#ffffff;font-size:24px;font-weight:bold;margin-top:8px;\">Email de test</div></td></tr>"
           "<tr><td style=\"padding:28px 32px;color:#302934;font-size:14px;line-height:1.6;\">"
           "<div style=\"background:#e3f5ea;color:#1e7a3c;border-radius:10px;padding:14px 18px;font-weight:bold;font-size:15px;\">"
           "&#10003; L'envoi des alertes par email fonctionne.</div><br>"
           "Bonjour,<br><br>Ceci est un email de test envoyé par Smart Fire Station. Vous recevrez désormais ici les alertes "
           "d'état (équipement hors service) et de quantité (stock insuffisant).</td></tr>"
           "<tr><td style=\"background:#faf5f5;padding:16px 32px;color:#999999;font-size:11px;border-top:1px solid #efe4e4;"
           "border-radius:0 0 14px 14px;\">Smart Fire Station &bull; message automatique</td></tr></table></td></tr></table></body></html>";
}

// envoi SMTP synchrone (à exécuter dans un thread) : renvoie true si le serveur accepte le message
static bool envoyerSmtp(const ConfigEmail &c, const QString &sujet, const QString &corps, const QString &html, QString &erreur)
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

    auto enBase64 = [](const QString &texte) {
        const QByteArray b64 = texte.toUtf8().toBase64();
        QByteArray lignes;
        for (int i = 0; i < b64.size(); i += 76)
            lignes += b64.mid(i, 76) + "\r\n";
        return lignes;
    };

    QByteArray msg;
    msg += "From: Smart Fire Station <" + c.expediteur.trimmed().toUtf8() + ">\r\n";
    msg += "To: " + c.destinataire.trimmed().toUtf8() + "\r\n";
    msg += "Subject: =?UTF-8?B?" + sujet.toUtf8().toBase64() + "?=\r\n";
    msg += "Date: " + QLocale::c().toString(QDateTime::currentDateTimeUtc(), "ddd, dd MMM yyyy HH:mm:ss").toUtf8() + " +0000\r\n";
    msg += "Message-ID: <sfs." + QByteArray::number(QDateTime::currentMSecsSinceEpoch()) + "@"
           + c.expediteur.section('@', 1).trimmed().toUtf8() + ">\r\n";
    msg += "MIME-Version: 1.0\r\n";
    if (html.isEmpty()) {
        msg += "Content-Type: text/plain; charset=UTF-8\r\n";
        msg += "Content-Transfer-Encoding: base64\r\n\r\n";
        msg += enBase64(corps);
    } else {
        // message en deux versions : texte simple + HTML (le client mail choisit la meilleure)
        const QByteArray frontiere = "=_sfs_" + QByteArray::number(QDateTime::currentMSecsSinceEpoch());
        msg += "Content-Type: multipart/alternative; boundary=\"" + frontiere + "\"\r\n\r\n";
        msg += "--" + frontiere + "\r\n";
        msg += "Content-Type: text/plain; charset=UTF-8\r\n";
        msg += "Content-Transfer-Encoding: base64\r\n\r\n";
        msg += enBase64(corps);
        msg += "--" + frontiere + "\r\n";
        msg += "Content-Type: text/html; charset=UTF-8\r\n";
        msg += "Content-Transfer-Encoding: base64\r\n\r\n";
        msg += enBase64(html);
        msg += "--" + frontiere + "--\r\n";
    }
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

    const QString reponseServeur = texte.trimmed();      // ex. "250 2.0.0 OK 1731... - gsmtp"
    cmd("QUIT", 221);
    sock.disconnectFromHost();
    erreur = reponseServeur;
    return true;
}

// envoi dans un thread séparé (l'interface ne se bloque pas)
// "fin" est rappelée dans le thread de l'interface avec (succès, message d'erreur)
static void envoyerAsync(QObject *contexte, const ConfigEmail &c, const QString &sujet, const QString &corps,
                         const QString &html, std::function<void(bool, const QString &)> fin)
{
    QPointer<QObject> ctx(contexte);
    QThread *th = QThread::create([=]() {
        QString err;
        bool ok = envoyerSmtp(c, sujet, corps, html, err);
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
        l->setText("📭\nAucun équipement\nAjoutez-en un avec le formulaire");
    else
        l->setText("🔍\nAucun résultat\nEssayez une autre recherche");
    l->setVisible(visibles == 0);
}

// =====================================================================
//  MODE SOMBRE : conversion des feuilles de style claires -> sombres
// =====================================================================
static QString remapCss(const QString &css)
{
    static const QHash<QString, QString> carte = {
        {"#f4f4f6", "#16161b"}, {"#f5f5f7", "#16161b"}, {"#fafafa", "#292932"},
        {"#f1f1f1", "#34343f"}, {"#eee", "#34343f"},    {"#eeeeee", "#34343f"},
        {"#ddd", "#3b3b47"},    {"#dddddd", "#3b3b47"}, {"#e6e6e6", "#3b3b47"},
        {"#ccc", "#4a4a56"},    {"#d9bcbc", "#6a4a4e"}, {"#b0b0b0", "#6c6c78"},
        {"#fbeaea", "#33242a"}, {"#fff5f5", "#2e2326"}, {"#fff8e1", "#322d1c"},
        {"#ffe082", "#6b5a1f"}, {"#f3c9c9", "#5c2b30"}, {"#f5dcdc", "#442a2e"},
        {"#f0c9c9", "#5c3b40"}, {"#5d4b00", "#ffd966"},
        {"#444", "#d8d8e0"},    {"#444444", "#d8d8e0"}, {"#333", "#e8e8ee"},
        {"#333333", "#e8e8ee"}, {"#222", "#f2f2f6"},    {"#222222", "#f2f2f6"},
        {"#555", "#c4c4ce"},    {"#666", "#b4b4c0"},    {"#777", "#a3a3b0"},
        {"#777777", "#a3a3b0"}, {"#888", "#9a9aa8"},    {"#999", "#8c8c9a"}
    };

    // couleurs exactes (#xxx / #xxxxxx)
    QRegularExpression re("#[0-9a-fA-F]{3,6}\\b");
    QString out;
    int pos = 0;
    QRegularExpressionMatchIterator it = re.globalMatch(css);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        out += css.mid(pos, int(m.capturedStart()) - pos);
        QString tok = m.captured().toLower();
        out += carte.contains(tok) ? carte.value(tok) : m.captured();
        pos = int(m.capturedEnd());
    }
    out += css.mid(pos);

    // textes rouges foncés -> rouge clair (lisible sur fond sombre)
    out.replace(QRegularExpression("(?<![\\w-])color\\s*:\\s*#(c40000|b40000|a90000)\\b"), "color:#ff6b6b");

    out.replace("background:white", "background:#23232b");
    out.replace("background-color:white", "background-color:#23232b");
    out.replace("selection-color:black", "selection-color:white");
    return out;
}



// applique (ou retire) le thème sombre sur un widget et tous ses enfants
static void themeSurArbre(QWidget *racine, bool sombre)
{
    QList<QWidget *> tous = racine->findChildren<QWidget *>();
    tous.prepend(racine);
    for (QWidget *w : tous) {
        if (w->objectName() == "actCell" || w->objectName() == "prioBar")
            continue;                       // ces styles sont recalculés dynamiquement
        QVariant orig = w->property("cssClair");
        if (!orig.isValid()) {
            QString cur = w->styleSheet();
            if (cur.isEmpty()) continue;
            w->setProperty("cssClair", cur);
            orig = cur;
        }
        QString css = orig.toString();
        w->setStyleSheet(sombre ? remapCss(css) : css);
    }
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
    if (g_sombre) themeSurArbre(w, true);
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
    dlg->setWindowTitle("Statistiques - Équipements");
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
    QLabel *bt = new QLabel("📊  Statistiques des équipements");
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
    kpis->addWidget(kpi(QString::number(total), "📦  Équipements", "#c40000"));
    kpis->addWidget(kpi(QString::number(tauxDispo, 'f', 1) + " %", "✅  Taux de disponibilité", "#2e7d32"));
    kpis->addWidget(kpi(QString::number(tauxHs, 'f', 1) + " %", "⛔  Taux hors service", "#c62828"));
    kpis->addWidget(kpi(QString::number(santeMoy) + " %", "💚  Santé moyenne du parc", "#00897b"));
    kpis->addWidget(kpi(QString::number(enAlerte), "🚨  Équipements en alerte", "#ef6c00"));
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
    row2->addWidget(carte("Équipements par type", types), 1);
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
    QLabel *santeNote = new QLabel("Score calculé automatiquement : cycle de maintenance, âge et état de l'équipement.");
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
        "<span style='color:#2e7d32'>■</span> stock suffisant &nbsp;&nbsp; "
        "<span style='color:#ef6c00'>■</span> stock insuffisant &nbsp;&nbsp; "
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
        pbl->addWidget(new QLabel("✓  Tous les équipements sont en bon état."));
    pbl->addStretch();

    QHBoxLayout *row4 = new QHBoxLayout;
    row4->setSpacing(14);
    row4->addWidget(carte("Stock : quantité vs seuil minimum", stockBox), 3);
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
    pdfStats->setStyleSheet(
        "QPushButton{background:#f5a623;color:black;border:none;border-radius:6px;"
        "padding:6px 18px;font-weight:bold;}"
        "QPushButton:hover{background:#e0941a;}");
    QObject::connect(pdfStats, &QPushButton::clicked, dlg, [=]() {
        QString file = QFileDialog::getSaveFileName(
            dlg, "Exporter les statistiques",
            QDir::homePath() + "/Documents/statistiques_equipements.pdf", "PDF (*.pdf)");
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
                         "Statistiques des équipements - " + today.toString(FMT));
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

// =====================================================================
//  Petits utilitaires de la vue liés à l'identifiant (colonne 0 du tableau).
//  Le tableau étant le reflet de la table EQUIPEMENT, ils ne font aucune
//  requête SQL : ils lisent seulement ce qui est déjà affiché.
// =====================================================================

// Numéro de ligne portant cet identifiant, -1 s'il n'est pas dans le tableau.
static int ligneDeId(QTableWidget *table, const QString &id)
{
    for (int r = 0; r < table->rowCount(); r++)
        if (table->item(r, 0) && table->item(r, 0)->text() == id)
            return r;
    return -1;
}

// Identifiant suivant, au format EQ-001, sans collision avec ceux déjà
// présents dans la base (l'identifiant est la clé primaire de EQUIPEMENT).
static QString prochainIdentifiant(QTableWidget *table)
{
    int maxNum = 0;
    for (int r = 0; r < table->rowCount(); r++) {
        if (!table->item(r, 0)) continue;
        const QString id = table->item(r, 0)->text();
        if (!id.startsWith("EQ-")) continue;
        bool ok = false;
        const int n = id.mid(3).toInt(&ok);
        if (ok && n > maxNum) maxNum = n;
    }
    return QString("EQ-%1").arg(maxNum + 1, 3, 10, QChar('0'));
}

// Controle de saisie : supprime les caracteres interdits et affiche un petit message rouge
static void controleSaisie(QLineEdit *champ, const QRegularExpression &interdit, const QString &message)
{
    QLabel *msg = new QLabel(message);
    msg->setWordWrap(true);
    msg->setStyleSheet("color:#c62828;font-size:10px;font-weight:bold;");
    msg->hide();
    if (champ->parentWidget() && champ->parentWidget()->layout())
        champ->parentWidget()->layout()->addWidget(msg);

    QTimer *minuteur = new QTimer(champ);
    minuteur->setSingleShot(true);
    minuteur->setInterval(2500);
    QObject::connect(minuteur, &QTimer::timeout, champ, [champ, msg]() {
        msg->hide();
        marquerErreur(champ, false);
    });

    QObject::connect(champ, &QLineEdit::textEdited, champ, [=](const QString &txt) {
        QString propre = txt;
        propre.remove(interdit);
        if (propre != txt) {
            const int pos = champ->cursorPosition() - (txt.size() - propre.size());
            champ->setText(propre);
            champ->setCursorPosition(qMax(0, pos));
            msg->show();
            marquerErreur(champ, true);
            minuteur->start();
        }
    });
}

// Controle de saisie pour les champs numeriques : message rouge si on tape une lettre ou un symbole
static void controleSpin(QSpinBox *spin, const QString &message)
{
    QLabel *msg = new QLabel(message);
    msg->setWordWrap(true);
    msg->setStyleSheet("color:#c62828;font-size:10px;font-weight:bold;");
    msg->hide();
    if (spin->parentWidget() && spin->parentWidget()->layout())
        spin->parentWidget()->layout()->addWidget(msg);

    QTimer *minuteur = new QTimer(spin);
    minuteur->setSingleShot(true);
    minuteur->setInterval(2500);
    QObject::connect(minuteur, &QTimer::timeout, spin, [spin, msg]() {
        msg->hide();
        marquerErreur(spin, false);
    });

    // le meme filtre est installe sur le spinbox ET sur son champ de saisie interne
    auto *filtre = new EvtFilter(spin, [spin, msg, minuteur](QObject *, QEvent *e) {
        if (e->type() == QEvent::KeyPress) {
            QKeyEvent *k = static_cast<QKeyEvent *>(e);
            const QString t = k->text();
            const bool commande = k->modifiers() & (Qt::ControlModifier | Qt::AltModifier);
            if (!commande && !t.isEmpty() && t.at(0).isPrint() && !t.at(0).isDigit()) {
                msg->show();
                marquerErreur(spin, true);
                minuteur->start();
            }
        }
        return false;
    });
    spin->installEventFilter(filtre);
    const QList<QLineEdit *> saisies = spin->findChildren<QLineEdit *>();
    for (QLineEdit *s : saisies)
        s->installEventFilter(filtre);
}

smart::smart(QWidget *parent)
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

    QPushButton *btnMenu = new QPushButton("☰");
    btnMenu->setCursor(Qt::PointingHandCursor);
    btnMenu->setToolTip("Réduire / agrandir le menu");
    btnMenu->setStyleSheet(
        "QPushButton{background:rgba(255,255,255,0.18);color:white;border:none;"
        "border-radius:15px;padding:5px 12px;font-weight:bold;font-size:15px;}"
        "QPushButton:hover{background:rgba(255,255,255,0.32);}");
    topLayout->addWidget(btnMenu);
    topLayout->addSpacing(8);

    QLabel *logo = new QLabel("♨  SMART FIRE STATION");
    logo->setStyleSheet("color:white;font-size:16px;font-weight:bold;");
    topLayout->addWidget(logo);
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

    QPushButton *btnTheme = new QPushButton("🌙");
    btnTheme->setObjectName("btnTheme");
    btnTheme->setCursor(Qt::PointingHandCursor);
    btnTheme->setToolTip("Mode sombre / mode clair");
    btnTheme->setStyleSheet(
        "QPushButton{background:rgba(255,255,255,0.18);color:white;border:none;"
        "border-radius:15px;padding:5px 14px;font-weight:bold;}"
        "QPushButton:hover{background:rgba(255,255,255,0.32);}");
    connect(btnTheme, &QPushButton::clicked, this, [=]() { appliquerTheme(!g_sombre); });
    topLayout->addWidget(btnTheme);
    topLayout->addSpacing(10);

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
    // Integration : menu, titre et theme sombre geres par l'application
    btnMenu->hide();
    btnTheme->hide();
    logo->hide();
    top->hide();   // même disposition que le module Personnel : pas de barre du haut
    statusBar()->hide();   // la barre d'état de l'application commune suffit

    // ===================== CORPS =====================
    QHBoxLayout *body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    // ===================== MENU GAUCHE =====================
    QFrame *menu = new QFrame;
    menu->setFixedWidth(170);
    menu->setObjectName("menuBar");
    menu->setStyleSheet(
        "QFrame#menuBar{background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #b80000,stop:1 #760000);}");

    QVBoxLayout *menuLayout = new QVBoxLayout(menu);

    LogoAnimeEquip *logoImg = new LogoAnimeEquip(logoApp());
    menuLayout->addWidget(logoImg, 0, Qt::AlignHCenter);
    menuLayout->addSpacing(6);

    QStringList menuItems = {
        "⌂   Accueil",
        "♨   Incidents",
        "▣   Véhicules",
        "⚒   Équipements",
        "♟   Personnel",
        "↗   Interventions",
        "▥   Rapports"
    };

    const QString styleNormal =
        "QPushButton{color:white;background:transparent;border:0;"
        "text-align:left;padding-left:15px;font-weight:bold;}"
        "QPushButton:hover{background:#c40000;}";
    const QString styleActif =
        "QPushButton{color:white;background:#c40000;border:0;border-left:4px solid white;"
        "text-align:left;padding-left:11px;font-weight:bold;}";

    QVector<QPushButton *> boutonsMenu;
    for (int i = 0; i < menuItems.size(); i++) {
        QPushButton *button = new QPushButton(menuItems[i]);
        button->setFixedHeight(42);
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(styleNormal);
        menuLayout->addWidget(button);
        boutonsMenu.push_back(button);
    }
    menuLayout->addStretch();

    QPushButton *deconnexion = new QPushButton("⏻   Déconnexion");
    deconnexion->setFixedHeight(42);
    deconnexion->setCursor(Qt::PointingHandCursor);
    deconnexion->setStyleSheet(styleNormal);
    connect(deconnexion, &QPushButton::clicked, this, [=]() {
        if (QMessageBox::question(this, "Déconnexion",
                                  "Voulez-vous vraiment quitter l'application ?") == QMessageBox::Yes)
            close();
    });
    menuLayout->addWidget(deconnexion);

    // Integration : le menu de gauche est fourni par l'application
    // body->addWidget(menu);
    menu->hide();

    // ===================== MENU REPLIABLE (bouton ☰) =====================
    connect(btnMenu, &QPushButton::clicked, this, [=]() {
        bool cible = !menu->property("replie").toBool();
        menu->setProperty("replie", cible);

        QVariantAnimation *a = new QVariantAnimation(menu);
        a->setDuration(320);
        a->setStartValue(menu->width());
        a->setEndValue(cible ? 64 : 170);
        a->setEasingCurve(QEasingCurve::InOutCubic);
        connect(a, &QVariantAnimation::valueChanged, menu, [menu](const QVariant &v) {
            menu->setFixedWidth(v.toInt());
        });

        if (cible) {        // on garde seulement les icônes
            for (int i = 0; i < boutonsMenu.size(); i++)
                boutonsMenu[i]->setText(menuItems[i].left(1));
            deconnexion->setText("⏻");
            logoImg->hide();
            if (QFrame *ind = menu->findChild<QFrame *>("menuIndic"))
                ind->hide();
        } else {            // on remet les textes à la fin de l'animation
            connect(a, &QVariantAnimation::finished, menu, [=]() {
                for (int i = 0; i < boutonsMenu.size(); i++)
                    boutonsMenu[i]->setText(menuItems[i]);
                deconnexion->setText("⏻   Déconnexion");
                logoImg->show();
            });
        }
        a->start(QAbstractAnimation::DeleteWhenStopped);
    });

    // ===================== CONTENU =====================
    QWidget *content = new QWidget;
    content->setObjectName("pageBg");
    content->setStyleSheet("QWidget#pageBg{background:#f5f5f7;}");
    QVBoxLayout *contentLayout = new QVBoxLayout(content);

    contentLayout->setContentsMargins(22, 18, 22, 14);
    contentLayout->setSpacing(14);
    QHBoxLayout *enteteHaut = new QHBoxLayout;
    QVBoxLayout *blocTitre = new QVBoxLayout;
    blocTitre->setSpacing(2);
    QLabel *title = new QLabel("Gestion des équipements");
    title->setStyleSheet("color:#222;font-size:26px;font-weight:bold;");
    blocTitre->addWidget(title);

    QLabel *description = new QLabel(
        "Inventaire du matériel de la caserne : état, stock et maintenance");
    description->setStyleSheet("color:#777;");
    blocTitre->addWidget(description);

    enteteHaut->addLayout(blocTitre, 1);
    contentLayout->addLayout(enteteHaut);

    QSplitter *splitter = new QSplitter(Qt::Horizontal);

    // ===================== FORMULAIRE =====================
    QFrame *form = new QFrame;
    form->setObjectName("formCard");
    form->setStyleSheet(
        "QFrame#formCard{background:white;border:1px solid #e4e0da;border-radius:10px;}"
        "QLabel{border:none;background:transparent;}"
        "QLabel#fieldLabel{color:#302934;font-weight:700;font-size:12px;}"
        "QLabel#sectionLabel{color:#981a27;font-weight:bold;font-size:11px;"
        "border-bottom:1px solid #f2c7c2;padding-bottom:2px;margin-top:0px;}"
        "QLineEdit,QComboBox,QSpinBox,QDateEdit{"
        "border:2px solid #dfe2e7;border-radius:9px;padding:4px 8px;"
        "background:#fbfbfc;color:#302934;min-height:20px;}"
        "QLineEdit:focus,QComboBox:focus,QSpinBox:focus,QDateEdit:focus{"
        "border:2px solid #bd1e2c;background:white;}"
        "QLineEdit[erreur=\"true\"],QComboBox[erreur=\"true\"],QSpinBox[erreur=\"true\"]{"
        "border:1px solid #c62828;background:#fff5f5;}"
        "QComboBox::drop-down,QDateEdit::drop-down{border:none;width:24px;}"
        "QSpinBox::up-button,QSpinBox::down-button{border:none;width:18px;}");

    QVBoxLayout *formLayout = new QVBoxLayout(form);
    formLayout->setContentsMargins(16, 14, 16, 14);
    formLayout->setSpacing(8);

    QLabel *formTitle = new QLabel("＋ Nouvel équipement");
    formTitle->setStyleSheet("color:#981a27;font-size:20px;font-weight:800;border:none;");
    formLayout->addWidget(formTitle);
    QLabel *formSous = new QLabel("Saisir les informations de l'équipement");
    formSous->setStyleSheet("color:#777984;font-size:12px;border:none;");
    formLayout->addWidget(formSous);

    nom = new QLineEdit;
    nom->setPlaceholderText("Ex : Extincteur CO2");

    type = new QComboBox;
    type->addItems({"Sélectionner un type", "Protection", "Secours",
                    "Communication", "Médical", "Outillage"});

    marque = new QLineEdit;
    marque->setPlaceholderText("Marque");

    modele = new QLineEdit;
    modele->setPlaceholderText("Modèle");

    quantite = new QSpinBox;
    quantite->setRange(0, 9999);
    quantite->setValue(1);

    seuil = new QSpinBox;
    seuil->setRange(0, 9999);
    seuil->setValue(2);
    seuil->setToolTip("Alerte si la quantité devient inférieure à ce seuil");

    etat = new QComboBox;
    etat->addItems({"Disponible", "En mission", "En maintenance", "Hors service"});

    localisation = new QLineEdit;
    localisation->setPlaceholderText("Ex : Garage, Véhicule 1");
    localisation->setReadOnly(true);
    localisation->setCursor(Qt::PointingHandCursor);
    localisation->installEventFilter(this);

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

    grid->addWidget(section("📝  INFORMATIONS"), 0, 0, 1, 2);
    grid->addWidget(champ("Nom de l'équipement *", nom), 1, 0);
    grid->addWidget(champ("Type *", type),               1, 1);
    grid->addWidget(champ("Marque", marque),             2, 0);
    grid->addWidget(champ("Modèle", modele),             2, 1);

    grid->addWidget(section("📦  STOCK & ÉTAT"), 3, 0, 1, 2);
    grid->addWidget(champ("Quantité", quantite),         4, 0);
    grid->addWidget(champ("Seuil minimum (stock)", seuil), 4, 1);
    grid->addWidget(champ("État", etat),                 5, 0);
    grid->addWidget(champ("Localisation", localisation), 5, 1);

    grid->addWidget(section("🛠  MAINTENANCE & REMARQUES"), 6, 0, 1, 2);
    grid->addWidget(champ("Date d'achat", dateAchat),    7, 0);
    grid->addWidget(champ("Dernière maintenance", dateMaintenance), 7, 1);
    grid->addWidget(champ("Prochaine maintenance", prochaineMaintenance), 8, 0);
    grid->addWidget(champ("Remarques", remarques),       8, 1);

    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    formLayout->addLayout(grid);

    connect(nom, &QLineEdit::textChanged, this, [=]() { marquerErreur(nom, false); });
    connect(type, &QComboBox::currentIndexChanged, this, [=]() { marquerErreur(type, false); });

    // Marque : pas de chiffres ni de symboles
    controleSaisie(marque, QRegularExpression("[^\\p{L}\\s'\\-\\.]"),
                   "Les chiffres ne sont pas autorisés");

    // Quantité et seuil : chiffres seulement
    controleSpin(quantite, "Les lettres ne sont pas autorisées");
    controleSpin(seuil, "Les lettres ne sont pas autorisées");

    // boutons
    QPushButton *add     = new QPushButton("➕  Ajouter");
    QPushButton *edit    = new QPushButton("✏  Modifier");

    QString btnPlein =
        "QPushButton{background:#b51c2a;color:white;border:2px solid #8f1420;border-radius:10px;"
        "padding:6px 14px;font-weight:bold;}"
        "QPushButton:hover{background:#cf2635;}";
    QString btnContour =
        "QPushButton{background:white;color:#a61725;border:2px solid #c94d59;"
        "border-radius:10px;padding:6px 14px;font-weight:bold;}"
        "QPushButton:hover{background:#fff1f2;}";

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

    // carte "priorité de maintenance" : affichée sous le tableau (zone d'alertes)
    QFrame *prioCarte = nullptr;
    {
        QFrame *prio = new QFrame;
        prio->setObjectName("prioCard");
        prio->setStyleSheet(
            "QFrame#prioCard{background:#fff5f4;border:2px solid #f2c7c2;border-radius:14px;}"
            "QLabel{border:none;background:transparent;}");
        QHBoxLayout *ph = new QHBoxLayout(prio);
        ph->setContentsMargins(14, 10, 14, 10);
        ph->setSpacing(12);
        QLabel *icone = new QLabel("⚠");
        icone->setStyleSheet("color:#f2a10c;font-size:26px;");
        icone->setFixedWidth(34);
        icone->setAlignment(Qt::AlignCenter);
        ph->addWidget(icone);
        QVBoxLayout *pl = new QVBoxLayout;
        pl->setSpacing(4);
        ph->addLayout(pl, 1);

        QLabel *pt = new QLabel("Priorité de maintenance");
        pt->setStyleSheet("color:#981a27;font-weight:800;font-size:14px;");
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
        QPushButton *pv = new QPushButton("Voir la maintenance prédictive ›");
        pv->setCursor(Qt::PointingHandCursor);
        pv->setStyleSheet(
            "QPushButton{background:#b51c2a;color:white;border:2px solid #8f1420;border-radius:10px;"
            "padding:6px 14px;font-weight:bold;font-size:12px;}"
            "QPushButton:hover{background:#cf2635;}");
        connect(pv, &QPushButton::clicked, this, [=]() { afficherMaintenancePredictive(); });

        pl->addWidget(pt);
        pl->addWidget(pn);
        pl->addWidget(pb);
        pv->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        pb->setMaximumWidth(QWIDGETSIZE_MAX);
        ph->addWidget(pv, 0, Qt::AlignVCenter);
        prioCarte = prio;   // placée sous le tableau, comme les alertes du Personnel
    }

    splitter->addWidget(form);

    // ===================== TABLEAU =====================
    QFrame *tableFrame = new QFrame;
    tableFrame->setObjectName("tableCard");
    tableFrame->setStyleSheet(
        "QFrame#tableCard{background:white;border:1px solid #e4e0da;border-radius:10px;}"
        "QLabel{border:none;}"
        "QLineEdit{border:2px solid #dfe2e7;border-radius:9px;padding:6px 12px;background:#fbfbfc;}"
        "QLineEdit:focus{border:2px solid #bd1e2c;background:white;}"
        "QComboBox{border:2px solid #dfe2e7;border-radius:9px;padding:4px 10px;background:#fbfbfc;}"
        "QComboBox:focus{border:2px solid #bd1e2c;}"
        "QComboBox::drop-down{border:none;width:22px;}");

    QVBoxLayout *tableLayout = new QVBoxLayout(tableFrame);

    QLabel *listTitle = new QLabel("☷  Liste des équipements");
    listTitle->setStyleSheet("color:#222;font-size:16px;font-weight:bold;");
    QPushButton *btnForm = new QPushButton("◀  Masquer le formulaire");
    btnForm->setObjectName("btnForm");
    btnForm->setCursor(Qt::PointingHandCursor);
    btnForm->setStyleSheet(
        "QPushButton{background:white;color:#b3211c;border:1px solid #e8b4b1;border-radius:6px;"
        "padding:6px 12px;font-weight:bold;font-size:12px;}"
        "QPushButton:hover{background:#fbe4e2;}");
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
    cardsLayout->addWidget(carte("total", "📦  Équipements", "#c40000"));
    cardsLayout->addWidget(carte("dispo", "✅  Disponibles", "#2e7d32"));
    cardsLayout->addWidget(carte("hs",    "⛔  Hors service", "#c62828"));
    cardsLayout->addWidget(carte("alert", "🔔  Alertes", "#ef6c00"));
    tableLayout->addLayout(cardsLayout);
    // comme le module Personnel : les chiffres sont dans la page Statistiques
    for (int i = 0; i < cardsLayout->count(); ++i)
        if (QWidget *w = cardsLayout->itemAt(i)->widget()) w->hide();
    btnForm->hide();

    QLabel *legende = new QLabel(
        "🔴 Alerte état : hors service     "
        "🟠 Alerte quantité : stock insuffisant");
    legende->setStyleSheet("color:#666;font-size:11px;");
    tableLayout->addWidget(legende);
    legende->hide();

    QPushButton *btnPdf     = new QPushButton("⬇ PDF");
    QPushButton *btnStat    = new QPushButton("📊 Statistiques");
    QPushButton *btnAlert   = new QPushButton("🔔 Alertes");
    QPushButton *btnJournal = new QPushButton("📜 Journal");
    QPushButton *btnMaint   = new QPushButton("🛠 Maintenance prédictive");

    QString orangeStyle =
        "QPushButton { background:white; color:#b3211c; border:1px solid #e8b4b1;"
        "border-radius:6px; padding:8px 14px; font-weight:bold; }"
        "QPushButton:hover { background:#fbe4e2; }";
    btnPdf->setStyleSheet(orangeStyle);
    btnStat->setStyleSheet(orangeStyle);
    btnAlert->setStyleSheet(orangeStyle);
    btnJournal->setStyleSheet(orangeStyle);
    btnMaint->setStyleSheet(orangeStyle);

    triCombo = new QComboBox;
    triCombo->addItems({"Nom", "État", "Date de maintenance", "Quantité"});
    triCombo->setCurrentIndex(-1);
    triCombo->setPlaceholderText("Choisir un critère");

    // boutons d'action en haut à droite, à côté du titre (comme le Personnel)
    QPushButton *btnEmailHaut = new QPushButton("✉ E-mail");
    btnEmailHaut->setStyleSheet(orangeStyle);
    btnEmailHaut->setToolTip("Paramètres d'envoi des alertes par e-mail");
    for (QPushButton *b : {btnStat, btnMaint, btnAlert, btnJournal, btnPdf, btnEmailHaut}) {
        b->setCursor(Qt::PointingHandCursor);
        enteteHaut->addWidget(b, 0, Qt::AlignVCenter);
    }
    btnPdf->setText("Inventaire (PDF)");
    btnStat->setText("Statistiques");
    btnAlert->setText("Alertes");
    btnJournal->setText("Journal");
    btnMaint->setText("Maintenance prédictive");
    connect(btnEmailHaut, &QPushButton::clicked, this, [=]() { afficherParametresEmail(); });

    recherche = new QLineEdit;
    recherche->setPlaceholderText("🔍  Rechercher un équipement...");

    filtreType = new QComboBox;
    filtreType->addItems({"Tous les types", "Protection", "Secours",
                          "Communication", "Médical", "Outillage"});

    recherche->setPlaceholderText("Rechercher par nom, marque ou localisation…");
    QHBoxLayout *searchLayout = new QHBoxLayout;
    searchLayout->addWidget(recherche, 1);
    searchLayout->addWidget(filtreType);
    searchLayout->addSpacing(8);
    searchLayout->addWidget(new QLabel("Trier par"));
    searchLayout->addWidget(triCombo);
    tableLayout->addLayout(searchLayout);

    table = new QTableWidget(0, 15);
    table->setHorizontalHeaderLabels({
        "ID", "Nom", "Type", "Stock", "Seuil", "État", "Localisation",
        "Date achat", "Dern. maint.", "Proch. maint.", "Actions",
        "Marque", "Modèle", "Remarques", "Santé"
    });

    for (int c = 0; c < 10; c++)
        table->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(10, QHeaderView::Fixed);
    table->setColumnWidth(10, 96);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    table->setColumnWidth(2, 150);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    table->setColumnWidth(3, 170);
    table->horizontalHeader()->setMinimumSectionSize(40);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    table->setColumnHidden(7, true);    // "Date achat" : visible dans le formulaire, masquée dans la liste
    table->setColumnHidden(11, true);
    table->setColumnHidden(12, true);
    table->setColumnHidden(13, true);
    table->setColumnHidden(0, true);
    table->setColumnHidden(4, true);    // le seuil est visible dans la jauge de la colonne "Stock"

    table->setAlternatingRowColors(true);
    table->setShowGrid(true);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setHighlightSections(false);
    new CoinsArrondisEquip(table, 10);
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
    table->verticalHeader()->setDefaultSectionSize(52);
    table->setItemDelegateForColumn(2, new TypeChipDelegate(table));
    table->setItemDelegateForColumn(3, new StockCelluleDelegate(table));
    table->setItemDelegateForColumn(5, new EtatDelegate(table));
    table->setItemDelegateForColumn(6, new LocPillDelegate(table));
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
        "QTableWidget{border:1px solid #e4e6ea;border-radius:10px;background:white;"
        "alternate-background-color:#fbf7f8;gridline-color:#eceef1;"
        "selection-background-color:#ffe5e7;selection-color:#302934;}"
        "QHeaderView{background:#5b1722;border:none;}"
        "QHeaderView::section{background:#5b1722;color:white;font-weight:700;font-size:11px;"
        "border:none;border-right:1px solid #762834;padding:10px 5px;}");

    // Les équipements viennent maintenant d'Oracle, plus des données écrites
    // en dur : chargerDepuisBase() appelle Equipement::afficher().
    chargerDepuisBase();

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
            QDir::homePath() + "/Documents/equipements.pdf", "PDF (*.pdf)");
        if (file.isEmpty()) return;

        QString html = "<h2 style='color:#c40000'>Inventaire des équipements</h2>"
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
        journaliser("Export", "Liste des équipements",
                    QString("Export PDF de l'inventaire (%1 équipements)").arg(table->rowCount()));
    });

    // ===================== STATISTIQUES =====================
    connect(btnStat, &QPushButton::clicked, this, [=]() { afficherStatistiques(this, table, journal); });

    connect(btnAlert, &QPushButton::clicked, this, [=]() { afficherAlertes(true); });
    connect(btnJournal, &QPushButton::clicked, this, [=]() { afficherJournal(); });
    connect(btnMaint, &QPushButton::clicked, this, [=]() { afficherMaintenancePredictive(); });

    QWidget *colonneDroite = new QWidget;
    QVBoxLayout *cd = new QVBoxLayout(colonneDroite);
    cd->setContentsMargins(0, 0, 0, 0);
    cd->setSpacing(10);
    cd->addWidget(tableFrame, 1);
    if (prioCarte) cd->addWidget(prioCarte);
    {   // carte d'alerte stock / état (même style que les alertes du Personnel)
        QFrame *al = new QFrame;
        al->setObjectName("alerteCarte");
        al->setStyleSheet("QFrame#alerteCarte{background:#fff5f4;border:2px solid #f2c7c2;border-radius:14px;}"
                          "QLabel{border:none;background:transparent;}");
        QHBoxLayout *ah = new QHBoxLayout(al);
        ah->setContentsMargins(14, 10, 14, 10);
        ah->setSpacing(12);
        QLabel *ic = new QLabel("⚠");
        ic->setStyleSheet("color:#f2a10c;font-size:26px;");
        ic->setFixedWidth(34);
        ic->setAlignment(Qt::AlignCenter);
        QLabel *txt = new QLabel;
        txt->setObjectName("alerteResume");
        txt->setTextFormat(Qt::RichText);
        txt->setWordWrap(true);
        QPushButton *voir = new QPushButton("Voir les alertes ›");
        voir->setCursor(Qt::PointingHandCursor);
        voir->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        voir->setStyleSheet("QPushButton{background:#b51c2a;color:white;border:2px solid #8f1420;border-radius:10px;"
                            "padding:6px 14px;font-weight:bold;font-size:12px;}"
                            "QPushButton:hover{background:#cf2635;}");
        connect(voir, &QPushButton::clicked, this, [=]() { afficherAlertes(true); });
        ah->addWidget(ic);
        ah->addWidget(txt, 1);
        ah->addWidget(voir, 0, Qt::AlignVCenter);
        cd->addWidget(al);
    }
    form->setFixedWidth(350);
    splitter->addWidget(colonneDroite);
    splitter->setHandleWidth(14);
    splitter->setSizes({350, 950});

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
    // ===================== PAGE ACCUEIL =====================
    QWidget *pageAccueil = new QWidget;
    QVBoxLayout *av = new QVBoxLayout(pageAccueil);
    av->setContentsMargins(20, 16, 20, 16);
    av->setSpacing(16);

    QFrame *welcome = new QFrame;
    welcome->setObjectName("welcome");
    welcome->setFixedHeight(96);
    welcome->setStyleSheet(
        "QFrame#welcome{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #c40000,stop:1 #7a0000);"
        "border-radius:12px;}"
        "QLabel{background:transparent;border:none;}");
    QVBoxLayout *wl = new QVBoxLayout(welcome);
    wl->setContentsMargins(26, 12, 26, 12);
    wl->setSpacing(2);
    QLabel *lblBienvenue = new QLabel("Bonjour 👋");
    lblBienvenue->setObjectName("lblBienvenue");
    lblBienvenue->setStyleSheet("color:white;font-size:22px;font-weight:bold;");
    QLabel *lblRole = new QLabel("Bienvenue sur Smart Fire Station");
    lblRole->setObjectName("lblRole");
    lblRole->setStyleSheet("color:#ffd6d6;font-size:13px;");
    wl->addWidget(lblBienvenue);
    wl->addWidget(lblRole);
    ombre(welcome);
    av->addWidget(welcome);

    QHBoxLayout *accKpis = new QHBoxLayout;
    accKpis->setSpacing(12);
    accKpis->addWidget(carte("a_total", "📦  Équipements", "#c40000"));
    accKpis->addWidget(carte("a_dispo", "✅  Disponibles", "#2e7d32"));
    accKpis->addWidget(carte("a_hs",    "⛔  Hors service", "#c62828"));
    accKpis->addWidget(carte("a_alert", "🔔  Alertes", "#ef6c00"));
    accKpis->addWidget(carte("a_qte",   "🔢  Quantité totale", "#1565c0"));
    av->addLayout(accKpis);

    auto bloc = [](const QString &titre, QWidget *contenu) -> QFrame * {
        QFrame *f = new QFrame;
        f->setObjectName("accCard");
        f->setStyleSheet(
            "QFrame#accCard{background:white;border:1px solid #e6e6e6;border-radius:10px;}"
            "QLabel{border:none;background:transparent;}");
        QVBoxLayout *l = new QVBoxLayout(f);
        l->setContentsMargins(16, 12, 16, 14);
        l->setSpacing(10);
        QLabel *t = new QLabel(titre);
        t->setStyleSheet("color:#a90000;font-weight:bold;font-size:14px;");
        l->addWidget(t);
        l->addWidget(contenu, 1);
        ombre(f);
        return f;
    };

    DonutWidget *accDonut = new DonutWidget;
    accDonut->setObjectName("accDonut");

    QLabel *accAlertes = new QLabel("✓  Aucune alerte pour le moment");
    accAlertes->setObjectName("accAlertes");
    accAlertes->setTextFormat(Qt::RichText);
    accAlertes->setWordWrap(true);
    accAlertes->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    accAlertes->setStyleSheet("color:#333;font-size:13px;");

    QPushButton *voirAlertes = new QPushButton("Voir toutes les alertes");
    voirAlertes->setCursor(Qt::PointingHandCursor);
    voirAlertes->setStyleSheet(
        "QPushButton{background:white;color:#c40000;border:1px solid #c40000;border-radius:6px;"
        "padding:6px 14px;font-weight:bold;}"
        "QPushButton:hover{background:#f5dcdc;}");
    connect(voirAlertes, &QPushButton::clicked, this, [=]() { afficherAlertes(true); });

    QWidget *alBox = new QWidget;
    QVBoxLayout *alv = new QVBoxLayout(alBox);
    alv->setContentsMargins(0, 0, 0, 0);
    alv->addWidget(accAlertes, 1);
    alv->addWidget(voirAlertes, 0, Qt::AlignLeft);

    QHBoxLayout *accRow = new QHBoxLayout;
    accRow->setSpacing(14);
    accRow->addWidget(bloc("Répartition par état", accDonut), 1);
    accRow->addWidget(bloc("Alertes récentes", alBox), 1);
    av->addLayout(accRow, 1);

    QPushButton *rapide1 = new QPushButton("⚒  Gérer les équipements");
    QPushButton *rapide2 = new QPushButton("📊  Statistiques");
    QPushButton *rapide3 = new QPushButton("🔔  Alertes");
    const QString styleRapide =
        "QPushButton{background:white;color:#c40000;border:1px solid #c40000;border-radius:8px;"
        "padding:10px 22px;font-weight:bold;font-size:13px;}"
        "QPushButton:hover{background:#c40000;color:white;}";
    for (QPushButton *b : {rapide1, rapide2, rapide3}) {
        b->setStyleSheet(styleRapide);
        b->setFixedHeight(44);
        b->setCursor(Qt::PointingHandCursor);
    }
    QWidget *rapideBox = new QWidget;
    QHBoxLayout *rv = new QHBoxLayout(rapideBox);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->setSpacing(12);
    rv->addWidget(rapide1);
    rv->addWidget(rapide2);
    rv->addWidget(rapide3);
    rv->addStretch();
    av->addWidget(bloc("Accès rapide", rapideBox));

    // ===================== PAGE "EN COURS" (autres modules) =====================
    QWidget *pagePlaceholder = new QWidget;
    QVBoxLayout *pv = new QVBoxLayout(pagePlaceholder);
    QLabel *phIcone = new QLabel("🚧");
    phIcone->setAlignment(Qt::AlignCenter);
    phIcone->setStyleSheet("font-size:56px;");
    QLabel *phTitre = new QLabel("Module en cours de développement");
    phTitre->setAlignment(Qt::AlignCenter);
    phTitre->setStyleSheet("color:#a90000;font-size:20px;font-weight:bold;");
    QLabel *phTexte = new QLabel("Cette page sera disponible prochainement.");
    phTexte->setAlignment(Qt::AlignCenter);
    phTexte->setStyleSheet("color:#777;font-size:13px;");
    pv->addStretch();
    pv->addWidget(phIcone);
    pv->addWidget(phTitre);
    pv->addWidget(phTexte);
    pv->addStretch();

    // ===================== NAVIGATION =====================
    QStackedWidget *pages = new QStackedWidget;
    pages->addWidget(pageAccueil);       // 0
    pages->addWidget(content);           // 1 : Équipements
    pages->addWidget(pagePlaceholder);   // 2
    body->addWidget(pages, 1);

    auto alerteVue = std::make_shared<bool>(false);
    auto allerA = [=](int i) {
        for (int j = 0; j < boutonsMenu.size(); j++)
            boutonsMenu[j]->setStyleSheet(j == i ? styleActif : styleNormal);
        if (i == 0) {
            pages->setCurrentIndex(0);
            accDonut->animer();
        } else if (i == 3) {
            pages->setCurrentIndex(1);
            if (!*alerteVue) {          // alerte automatique à la 1ère ouverture
                *alerteVue = true;
                QTimer::singleShot(2400, this, [=]() { afficherAlertes(false); });
            }
        } else {
            phTitre->setText(menuItems[i].mid(1).trimmed() + "  —  module en cours de développement");
            pages->setCurrentIndex(2);
        }
    };
    // seul le module "Équipements" est actif : les autres boutons du menu ne font rien
    connect(boutonsMenu[3], &QPushButton::clicked, this, [=]() { allerA(3); });
    connect(rapide1, &QPushButton::clicked, this, [=]() { allerA(3); });
    connect(rapide2, &QPushButton::clicked, this, [=]() { afficherStatistiques(this, table, journal); });
    connect(rapide3, &QPushButton::clicked, this, [=]() { afficherAlertes(true); });
    allerA(3);   // l'application s'ouvre directement sur la page Équipements
    mainLayout->addLayout(body);

    // ===================== CONNEXIONS =====================
    connect(add,     &QPushButton::clicked, this, &smart::ajouter);
    connect(edit,    &QPushButton::clicked, this, &smart::modifier);
    connect(recherche, &QLineEdit::textChanged, this, &smart::rechercher);
    connect(filtreType, &QComboBox::currentIndexChanged, this, &smart::rechercher);
    connect(triCombo, &QComboBox::currentIndexChanged, this, &smart::trier);

    connect(table->model(), &QAbstractItemModel::rowsRemoved, this, [=]() { majCartes(); });

    colorierLignes();

    journaliser("Système", "—",
                QString("Application démarrée  •  %1 équipements chargés").arg(table->rowCount()));

    // lueur au survol des boutons
    for (QPushButton *b : {btnPdf, btnStat, btnAlert, btnJournal, btnMaint})
        survolBouton(b, QColor(245, 166, 35, 200));
    survolBouton(add, QColor(196, 0, 0, 170));
    survolBouton(edit, QColor(196, 0, 0, 170));
    survolBouton(btnMenu, QColor(255, 255, 255, 150));
    survolBouton(btnTheme, QColor(255, 255, 255, 150));
    survolBouton(btnEmail, QColor(255, 255, 255, 150));

    // thème mémorisé (clair / sombre)
    {
        QSettings st("SmartFireStation", "Equipements");

        bool sombreSauve = st.value("theme/sombre", false).toBool();
        if (sombreSauve)
            appliquerTheme(sombreSauve);
    }

    // ----- animations d'entrée de l'interface -----
    for (int i = 0; i < boutonsMenu.size(); i++)
        apparition(boutonsMenu[i], 200 + i * 80);
    apparition(deconnexion, 200 + int(boutonsMenu.size()) * 80);
    apparition(form, 450);
    apparition(tableFrame, 650);

    // glissement des panneaux
    for (int i = 0; i < boutonsMenu.size(); i++)
        diapo(boutonsMenu[i], QPoint(-60, 0), 200 + i * 80);
    diapo(deconnexion, QPoint(-60, 0), 200 + int(boutonsMenu.size()) * 80);
    diapo(form, QPoint(-80, 0), 450);
    diapo(tableFrame, QPoint(100, 0), 650);

    // la barre blanche du menu glisse jusqu'à "Équipements"
    {
        QFrame *indic = new QFrame(menu);
        indic->setObjectName("menuIndic");
        indic->setFixedSize(4, 42);
        indic->setStyleSheet("background:white;border-radius:2px;");
        indic->hide();
        QTimer::singleShot(1000, this, [=]() {
            QPoint depart(0, boutonsMenu[0]->y());
            QPoint arrivee(0, boutonsMenu[3]->y());
            indic->move(depart);
            indic->show();
            indic->raise();
            QPropertyAnimation *an = new QPropertyAnimation(indic, "pos", indic);
            an->setDuration(900);
            an->setStartValue(depart);
            an->setEndValue(arrivee);
            an->setEasingCurve(QEasingCurve::OutBack);
            an->start(QAbstractAnimation::DeleteWhenStopped);
        });
    }

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
    QTimer::singleShot(300, this, [=]() { accDonut->animer(); });
}

// =====================================================================
//  Étape 1 du guide : AFFICHER
//  Vide le tableau, relit la table EQUIPEMENT par Equipement::afficher()
//  et recrée une ligne par enregistrement. Aucune requête SQL ici : la vue
//  appelle seulement la méthode du Modèle.
//
//  Oracle renvoie les dates au format 'YYYY-MM-DD' (TO_CHAR) ; le tableau
//  les affiche au format FMT ("dd/MM/yyyy"), d'où la conversion.
// =====================================================================
void smart::chargerDepuisBase()
{
    table->setRowCount(0);                       // 1. vider le tableau

    Equipement eq;
    QSqlQueryModel *m = eq.afficher();           // 2. lire la base

    if (!eq.derniereErreur().isEmpty()) {
        QMessageBox::critical(this, "Base de données",
                              "Lecture des équipements impossible :\n" + eq.derniereErreur());
        delete m;
        return;
    }

    // seuil, prochaine maintenance, remarques et localisation viennent aussi d'Oracle
    QString erreurExtra;
    const QMap<QString, EquipementExtra::Infos> extras = EquipementExtra::toutes(&erreurExtra);
    static bool avertiExtra = false;
    if (!erreurExtra.isEmpty() && !avertiExtra) {
        avertiExtra = true;
        QMessageBox::warning(this, "Base de données",
                             "Les colonnes SEUIL, DATE_PROCHAINE, REMARQUES et LOCALISATION sont absentes "
                             "de la table EQUIPEMENT.\n\nExécutez le script SQL (ALTER TABLE) dans Oracle SQL Developer.\n\n"
                                 + erreurExtra);
    }

    for (int r = 0; r < m->rowCount(); r++) {    // 3. une ligne par enregistrement
        auto val = [&](int c) { return m->data(m->index(r, c)).toString(); };
        const EquipementExtra::Infos ex = extras.value(val(0));

        // "YYYY-MM-DD" (Oracle)  ->  "dd/MM/yyyy" (tableau)
        auto dateTableau = [&](int c) {
            QDate d = QDate::fromString(val(c), "yyyy-MM-dd");
            return d.isValid() ? d.toString(FMT) : QString();
        };

        ajouterLigne(val(0),                 // id_equipement
                     val(1),                 // nom
                     val(2),                 // type
                     val(3),                 // marque
                     val(4),                 // modele
                     val(5).toInt(),         // quantite
                     ex.seuil,               // seuil
                     val(6),                 // etat
                     ex.localisation,        // localisation
                     dateTableau(7),         // date_achat
                     dateTableau(8),         // date_entretien
                     ex.prochaine.isValid() ? ex.prochaine.toString(FMT) : QString(),   // prochaine maintenance
                     ex.remarques);          // remarques
    }

    delete m;                                    // 4. supprimer le modèle
}

// =====================================================================
void smart::ajouterLigne(const QString &id, const QString &n, const QString &t, const QString &m,
                         const QString &mod, int q, int s, const QString &e,
                         const QString &loc, const QString &da, const QString &dm,
                         const QString &pm, const QString &rem)
{
    int row = table->rowCount();
    table->insertRow(row);

    // L'identifiant vient d'Oracle (colonne id_equipement), il n'est plus
    // fabriqué par la vue : le tableau est le reflet de la table EQUIPEMENT.
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

    QWidget *actions = new QWidget;
    actions->setObjectName("actCell");
    QHBoxLayout *actLayout = new QHBoxLayout(actions);
    actLayout->setContentsMargins(2, 2, 2, 2);
    actLayout->setSpacing(4);

    QPushButton *btnEdit = new QPushButton("✎");
    QPushButton *btnDel  = new QPushButton("🗑");
    btnEdit->setToolTip("Modifier");
    btnDel->setToolTip("Supprimer");
    btnEdit->setCursor(Qt::PointingHandCursor);
    btnDel->setCursor(Qt::PointingHandCursor);
    btnEdit->setStyleSheet("QPushButton{background:#fff4f5;color:#9e1724;border:1px solid #efc9cd;border-radius:9px;padding:0px;font-size:15px;}"
                           "QPushButton:hover{background:#f6d6da;}");
    btnDel->setStyleSheet("QPushButton{background:#8f1420;color:white;border:1px solid #74101a;border-radius:9px;padding:0px;font-size:15px;}"
                          "QPushButton:hover{background:#c1121f;}");
    btnEdit->setFixedSize(36, 32);
    btnDel->setFixedSize(36, 32);
    actLayout->addWidget(btnEdit);
    actLayout->addWidget(btnDel);

    connect(btnDel, &QPushButton::clicked, this, [=]() {
        int r = table->indexAt(actions->mapTo(table->viewport(), QPoint(0, 0))).row();
        if (r < 0) return;
        if (QMessageBox::question(this, "Suppression", "Supprimer cet équipement ?") == QMessageBox::Yes) {
            QString idEq = table->item(r, 0)->text();           // colonne de l'id
            QString ref = idEq + "  " + table->item(r, 1)->text();
            QString det = QString("Type : %1  •  Quantité : %2  •  État : %3")
                              .arg(table->item(r, 2)->text(), table->item(r, 3)->text(), table->item(r, 5)->text());

            // Étape 4 du guide : la suppression se fait dans Oracle, par le Modèle.
            Equipement eq;
            if (!eq.supprimer(idEq)) {
                QMessageBox::critical(this, "Erreur",
                                      "Suppression non effectuée\n\n" + eq.derniereErreur());
                return;
            }
            journaliser("Suppression", ref, det);
            toast(this, "🗑  Équipement supprimé", "#c62828");
            // On ne fait plus removeRow : le tableau est rechargé depuis la base.
            // Le rechargement détruit les boutons de la ligne, donc celui qui est
            // en train d'être cliqué : on attend la fin du clic (singleShot 0).
            QTimer::singleShot(0, this, [this]() {
                chargerDepuisBase();
                colorierLignes();
            });
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
    if (g_sombre)
        themeSurArbre(actions, true);
}

// =====================================================================
void smart::chargerFormulaire(int r)
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
void smart::ajouter()
{
    bool okNom  = !nom->text().trimmed().isEmpty();
    bool okType = type->currentIndex() != 0;
    marquerErreur(nom, !okNom);
    marquerErreur(type, !okType);
    if (!okNom || !okType) {
        toast(this, "⚠  Le nom et le type sont obligatoires", "#c62828");
        return;
    }
    // Contrôle de saisie (guide, étape 6) : la base refuse l'inverse
    // (contrainte ck_equipement_dates).
    if (dateMaintenance->date() < dateAchat->date()) {
        marquerErreur(dateMaintenance, true);
        toast(this, "⚠  La date d'entretien doit être après la date d'achat", "#c62828");
        return;
    }
    marquerErreur(dateMaintenance, false);
    if (prochaineMaintenance->date() <= dateMaintenance->date()) {
        toast(this, "⚠  La prochaine maintenance doit être après la dernière", "#c62828");
        return;
    }

    // L'identifiant est obligatoire : il est fabriqué à partir des
    // identifiants déjà présents dans la table (EQ-001, EQ-002, ...).
    const QString idEq = prochainIdentifiant(table);

    // Étape 2 du guide : l'ajout se fait dans Oracle, par le Modèle.
    Equipement eq(idEq, nom->text().trimmed(), type->currentText(), marque->text(),
                  modele->text(), quantite->value(), etat->currentText(),
                  dateAchat->date(), dateMaintenance->date(), QString());

    if (!eq.ajouter()) {
        QMessageBox::critical(this, "Ajout",
                              "Ajout non effectué\n\n" + eq.derniereErreur());
        return;
    }
    {   // seuil, prochaine maintenance, remarques, localisation : enregistres dans Oracle
        EquipementExtra::Infos ex;
        ex.seuil = seuil->value();
        ex.prochaine = prochaineMaintenance->date();
        ex.remarques = remarques->text().trimmed();
        ex.localisation = localisation->text().trimmed();
        QString errEx;
        if (!EquipementExtra::enregistrer(idEq, ex, &errEx))
            QMessageBox::warning(this, "Base de données",
                                 "Seuil, prochaine maintenance, remarques et localisation non enregistrés :\n" + errEx);
    }
    // On n'appelle plus ajouterLigne à la main : c'est chargerDepuisBase()
    // qui actualise le tableau depuis Oracle.
    chargerDepuisBase();

    {
        QString ref = idEq + "  " + nom->text().trimmed();
        journaliser("Ajout", ref,
                    QString("Type : %1  •  Quantité : %2  •  État : %3  •  Localisation : %4")
                        .arg(type->currentText()).arg(quantite->value())
                        .arg(etat->currentText(), localisation->text()));
        QStringList motifs;
        if (etat->currentText() == "Hors service")
            motifs << "Alerte état : équipement hors service";
        if (quantite->value() < seuil->value())
            motifs << QString("Alerte quantité : stock insuffisant (%1 / %2)")
                          .arg(quantite->value()).arg(seuil->value());
        if (!motifs.isEmpty())
            envoyerAlerteEmail("[Smart Fire Station] Alerte équipement : " + ref,
                               corpsEmail(QStringList{"• " + ref + " — " + motifs.join(" ; ")}), ref);
    }

    flashLigne(table, ligneDeId(table, idEq), [this]() { colorierLignes(); });

    if (quantite->value() < seuil->value())
        toast(this, QString("✓  Équipement ajouté   •   ⚠ stock insuffisant (%1 / %2)")
                        .arg(quantite->value()).arg(seuil->value()), "#ef6c00");
    else
        toast(this, "✓  Équipement ajouté");

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
void smart::modifier()
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
    // Contrôle de saisie (guide, étape 6) : la base refuse l'inverse
    // (contrainte ck_equipement_dates).
    if (dateMaintenance->date() < dateAchat->date()) {
        marquerErreur(dateMaintenance, true);
        toast(this, "⚠  La date d'entretien doit être après la date d'achat", "#c62828");
        return;
    }
    marquerErreur(dateMaintenance, false);
    if (prochaineMaintenance->date() <= dateMaintenance->date()) {
        toast(this, "⚠  La prochaine maintenance doit être après la dernière", "#c62828");
        return;
    }

    struct Champ { QString label; int col; QString nouveau; };
    QVector<Champ> champs = {
        {"Nom", 1, nom->text().trimmed()},
        {"Type", 2, type->currentText()},
        {"Quantité", 3, QString::number(quantite->value())},
        {"Seuil", 4, QString::number(seuil->value())},
        {"État", 5, etat->currentText()},
        {"Localisation", 6, localisation->text()},
        {"Date d'achat", 7, dateAchat->date().toString(FMT)},
        {"Dernière maintenance", 8, dateMaintenance->date().toString(FMT)},
        {"Prochaine maintenance", 9, prochaineMaintenance->date().toString(FMT)},
        {"Marque", 11, marque->text()},
        {"Modèle", 12, modele->text()},
        {"Remarques", 13, remarques->text()}
    };

    // L'identifiant de la ligne sélectionnée : c'est lui qui désigne
    // l'enregistrement à modifier (clause WHERE de l'UPDATE).
    const QString idEq = table->item(r, 0)->text();

    QStringList changements;
    bool alerteConcernee = false;
    for (const Champ &c : champs) {
        QString ancien = table->item(r, c.col)->text();
        if (ancien != c.nouveau) {
            changements << QString("%1 : %2 → %3").arg(c.label, ancien, c.nouveau);
            if (c.col == 3 || c.col == 4 || c.col == 5) alerteConcernee = true;
        }
    }

    // Étape 3 du guide : la modification se fait dans Oracle, par le Modèle.
    // On ne modifie plus les cellules du tableau à la main.
    Equipement eq(idEq, nom->text().trimmed(), type->currentText(), marque->text(),
                  modele->text(), quantite->value(), etat->currentText(),
                  dateAchat->date(), dateMaintenance->date(), QString());

    if (!eq.modifier()) {
        QMessageBox::critical(this, "Modification",
                              "Modification non effectuée\n\n" + eq.derniereErreur());
        return;
    }
    {   // seuil, prochaine maintenance, remarques, localisation : enregistres dans Oracle
        EquipementExtra::Infos ex;
        ex.seuil = seuil->value();
        ex.prochaine = prochaineMaintenance->date();
        ex.remarques = remarques->text().trimmed();
        ex.localisation = localisation->text().trimmed();
        QString errEx;
        if (!EquipementExtra::enregistrer(idEq, ex, &errEx))
            QMessageBox::warning(this, "Base de données",
                                 "Seuil, prochaine maintenance, remarques et localisation non enregistrés :\n" + errEx);
    }
    chargerDepuisBase();
    r = ligneDeId(table, idEq);

    QString ref = idEq + "  " + nom->text().trimmed();
    if (!changements.isEmpty())
        journaliser("Modification", ref, changements.join("  •  "));
    if (alerteConcernee) {
        QStringList motifs;
        if (etat->currentText() == "Hors service")
            motifs << "Alerte état : équipement hors service";
        if (quantite->value() < seuil->value())
            motifs << QString("Alerte quantité : stock insuffisant (%1 / %2)")
                          .arg(quantite->value()).arg(seuil->value());
        if (!motifs.isEmpty())
            envoyerAlerteEmail("[Smart Fire Station] Alerte équipement : " + ref,
                               corpsEmail(QStringList{"• " + ref + " — " + motifs.join(" ; ")}), ref);
    }

    colorierLignes();

    flashLigne(table, r, [this]() { colorierLignes(); });

    if (quantite->value() < seuil->value())
        toast(this, QString("✓  Équipement modifié   •   ⚠ stock insuffisant (%1 / %2)")
                        .arg(quantite->value()).arg(seuil->value()), "#ef6c00");
    else
        toast(this, "✓  Équipement modifié");
}

// =====================================================================
void smart::rechercher()
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
void smart::trier()
{
    int colonne;
    switch (triCombo->currentIndex()) {
    case 0: colonne = 1; break;   // Nom
    case 1: colonne = 5; break;   // État
    case 2: colonne = 9; break;   // Prochaine maintenance
    case 3: colonne = 3; break;   // Quantité
    default: return;
    }

    table->sortItems(colonne, Qt::AscendingOrder);
    colorierLignes();
    rechercher();
}

// =====================================================================
//  Couleur des lignes selon l'alerte (UNE SEULE alerte : état ou quantité)
//    rouge  : alerte état  -> équipement hors service
//    orange : alerte quantité -> quantité < seuil minimum
//  + calcul du score de santé de chaque équipement (maintenance prédictive)
// =====================================================================
void smart::colorierLignes()
{
    for (int r = 0; r < table->rowCount(); r++) {
        if (!table->item(r, 5) || !table->item(r, 3) || !table->item(r, 4)
            || !table->item(r, 7) || !table->item(r, 8) || !table->item(r, 9))
            continue;

        QString e = table->item(r, 5)->text();
        int q = table->item(r, 3)->text().toInt();
        int s = table->item(r, 4)->text().toInt();

        // plus de ligne entière colorée (comme le Personnel) : l'état a sa case colorée,
        // et un stock insuffisant est signalé dans la case « Qté »
        QBrush brush(Qt::NoBrush);

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

        // stock insuffisant : case « Qté » en orange (couleur « à surveiller » de la charte)
        if (QTableWidgetItem *it = table->item(r, 3)) {
            QFont f = it->font();
            f.setBold(q < s);
            it->setFont(f);
            it->setBackground(QBrush(Qt::NoBrush));      // la jauge du delegue indique deja le stock bas
            it->setToolTip(q < s ? QString("Stock insuffisant : %1 / seuil %2").arg(q).arg(s) : QString());
        }

        // la cellule "Actions" prend la même couleur que la ligne
        if (QWidget *w = table->cellWidget(r, 10))
            w->setStyleSheet(brush.style() == Qt::NoBrush
                                 ? QString("QWidget#actCell{background:transparent;}")
                                 : QString("QWidget#actCell{background:%1;}").arg(brush.color().name()));
    }
    majCartes();
}

// =====================================================================
//  MÉTIER 1 : alerte automatique unique (quantité OU état)
//  Notification par EMAIL (voir envoyerAlerteEmail)
// =====================================================================
void smart::afficherAlertes(bool afficherSiVide)
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
            stock.push_back(qMakePair(n, QString("Quantité %1 / seuil minimum %2  •  il manque %3")
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
    for (const auto &x : stock) lignesMail << "• [QUANTITÉ] " + x.first + " — " + x.second;
    const QString sujetMail = QString("[Smart Fire Station] %1 équipement(s) en alerte").arg(concernes);
    const QString corpsMail = corpsEmail(lignesMail);
    const QString refMail   = QString("%1 équipement(s)").arg(concernes);

    if (!afficherSiVide) {      // alerte automatique : envoi de l'email, sans fenêtre
        envoyerAlerteEmail(sujetMail, corpsMail, refMail);
        return;
    }

    // ---------- fenêtre ----------
    QDialog *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle("Alerte équipements");
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
    QLabel *bt = new QLabel("🚨  Alerte équipements");
    bt->setStyleSheet("color:white;font-size:20px;font-weight:bold;");
    QLabel *bs = new QLabel(QString("%1 équipement(s) en alerte  •  %2").arg(concernes).arg(today.toString(FMT)));
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
    section("🟠  Alerte quantité : stock insuffisant", "#ef6c00", stock);

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
    btnMail->setStyleSheet(
        "QPushButton{background:#f5a623;color:black;border:none;border-radius:6px;"
        "padding:6px 18px;font-weight:bold;}"
        "QPushButton:hover{background:#e0941a;}");
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
void smart::majCartes()
{
    int total = 0, dispo = 0, hs = 0, alertes = 0, qteTotale = 0, nbBas = 0;
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
        if (bas) nbBas++;

        auto ligne = [&](const QString &icone, const QString &txt) {
            if (lignes.size() < 6)
                lignes << QString("<p style='margin:3px 0'>%1&nbsp; <b>%2</b> "
                                  "<span style='color:#777'>— %3</span></p>")
                              .arg(icone, nom.toHtmlEscaped(), txt);
        };
        if (horsService) ligne("🔴", "hors service");
        if (bas)         ligne("🟠", QString("stock insuffisant (%1 / %2)").arg(q).arg(s));
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
    // cartes de la page Équipements
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


    // carte d'alerte stock / état sous le tableau
    if (QLabel *l = findChild<QLabel *>("alerteResume")) {
        if (QWidget *carte = l->parentWidget()) carte->setVisible(alertes > 0);
        l->setText(QString("<span style='font-size:14px;font-weight:700;color:#981a27'>%1 équipement(s) en alerte</span><br>"
                           "<span style='color:#5a5560'>%2 hors service  •  %3 en stock insuffisant</span>")
                       .arg(alertes).arg(hs).arg(nbBas));
    }

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
            b->setStyleSheet(QString("QProgressBar{background:") + (g_sombre ? "#3b3b47" : "#eee") + QString(";border:none;border-radius:4px;}"
                                                                                                             "QProgressBar::chunk{background:%1;border-radius:4px;}").arg(col));
        }
    }

    majEtatVide(table);
    statusBar()->showMessage(QString("📦  %1 équipement(s)     •     🚨  %2 alerte(s)     •     Smart Fire Station © 2026")
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

    if (QLabel *l = findChild<QLabel *>("accAlertes"))
        l->setText(lignes.isEmpty() ? QString("✓&nbsp; Aucune alerte pour le moment")
                                    : lignes.join(""));

    if (QWidget *w = findChild<QWidget *>("accDonut"))
        static_cast<DonutWidget *>(w)->setData(
            {"Disponible", "En mission", "En maintenance", "Hors service"},
            {nbEtat[0], nbEtat[1], nbEtat[2], nbEtat[3]},
            {QColor("#2e7d32"), QColor("#1565c0"), QColor("#ef6c00"), QColor("#c62828")});
}

// =====================================================================
void smart::setUtilisateur(const QString &nomUtilisateur, const QString &role)
{
    utilisateurCourant = nomUtilisateur;
    if (QLabel *l = findChild<QLabel *>("lblUser"))
        l->setText("👤  " + nomUtilisateur + "  ▾");
    if (QLabel *l = findChild<QLabel *>("lblBienvenue"))
        l->setText("Bonjour, " + nomUtilisateur + " 👋");
    if (QLabel *l = findChild<QLabel *>("lblRole"))
        l->setText(role + "  •  " + QLocale(QLocale::French).toString(QDate::currentDate(), "dddd dd MMMM yyyy"));
}

// =====================================================================
//  JOURNAL D'ACTIVITÉ (remplace la présentation "CRUD")
//  Chaque opération (ajout, modification, suppression,
//  alerte, planification, export) est enregistrée avec date et heure.
// =====================================================================
void smart::journaliser(const QString &action, const QString &equipement, const QString &details)
{
    journal.push_back(QStringList{
        QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm:ss"),
        action, equipement, details, utilisateurCourant});
}

void smart::afficherJournal()
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
    QLabel *bs = new QLabel("Historique de toutes les opérations sur les équipements");
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
    t->setHorizontalHeaderLabels({"Date / heure", "Action", "Équipement", "Détails", "Utilisateur"});
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
    pdf->setStyleSheet(
        "QPushButton{background:#f5a623;color:black;border:none;border-radius:6px;"
        "padding:6px 18px;font-weight:bold;}"
        "QPushButton:hover{background:#e0941a;}");
    connect(pdf, &QPushButton::clicked, dlg, [=]() {
        QString file = QFileDialog::getSaveFileName(
            dlg, "Exporter le journal",
            QDir::homePath() + "/Documents/journal_equipements.pdf", "PDF (*.pdf)");
        if (file.isEmpty()) return;

        QString html = "<h2 style='color:#c40000'>Journal d'activité - équipements</h2>"
                       "<table border='1' cellspacing='0' cellpadding='4'><tr>";
        const QStringList ent = {"Date / heure", "Action", "Équipement", "Détails", "Utilisateur"};
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
//  Score de santé par équipement + recommandation + planification auto
// =====================================================================
namespace {

// anneau de sante moyenne (anime)
class AnneauSante : public QWidget {
public:
    explicit AnneauSante(QWidget *parent = nullptr) : QWidget(parent) { setFixedSize(124, 124); }
    void setPct(int pct, const QColor &c) {
        m_c = c;
        QVariantAnimation *a = new QVariantAnimation(this);
        a->setDuration(1000);
        a->setStartValue(0.0);
        a->setEndValue(double(pct));
        a->setEasingCurve(QEasingCurve::OutCubic);
        connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            m_v = v.toDouble();
            update();
        });
        a->start(QAbstractAnimation::DeleteWhenStopped);
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r(12, 12, width() - 24, height() - 24);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#efe4e4"), 12));
        p.drawEllipse(r);
        if (m_v > 0.0) {
            p.setPen(QPen(m_c, 12, Qt::SolidLine, Qt::RoundCap));
            p.drawArc(r, 90 * 16, -int(m_v * 3.6 * 16));
        }
        QFont f = font();
        f.setPixelSize(26);
        f.setBold(true);
        p.setFont(f);
        p.setPen(QColor("#302934"));
        p.drawText(QRectF(0, height() / 2.0 - 22, width(), 30), Qt::AlignCenter, QString("%1 %").arg(int(m_v + 0.5)));
        f.setPixelSize(10);
        f.setBold(false);
        p.setFont(f);
        p.setPen(QColor("#888"));
        p.drawText(QRectF(0, height() / 2.0 + 8, width(), 16), Qt::AlignCenter, "santé moyenne");
    }
private:
    double m_v = 0.0;
    QColor m_c = QColor("#2e7d32");
};

// colonne "Equipement" : identifiant en petit, nom en gras
class EquipDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override {
        QStyleOptionViewItem o(opt);
        initStyleOption(&o, idx);
        const QString nom = o.text;
        o.text.clear();
        QStyle *st = o.widget ? o.widget->style() : QApplication::style();
        st->drawControl(QStyle::CE_ItemViewItem, &o, p, o.widget);
        p->save();
        QFont f = o.font;
        f.setPixelSize(10);
        f.setBold(true);
        p->setFont(f);
        p->setPen(cTexteDoux());
        p->drawText(QRectF(opt.rect.left() + 10, opt.rect.top() + 9, opt.rect.width() - 14, 16),
                    Qt::AlignLeft | Qt::AlignVCenter, idx.data(Qt::UserRole).toString());
        f.setPixelSize(13);
        p->setFont(f);
        p->setPen(cTexteFort());
        p->drawText(QRectF(opt.rect.left() + 10, opt.rect.top() + 26, opt.rect.width() - 14, 22),
                    Qt::AlignLeft | Qt::AlignVCenter, nom);
        p->restore();
    }
};

// colonne "Facteurs de risque" : pastilles colorees
class FacteursDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override {
        QStyleOptionViewItem o(opt);
        initStyleOption(&o, idx);
        o.text.clear();
        QStyle *st = o.widget ? o.widget->style() : QApplication::style();
        st->drawControl(QStyle::CE_ItemViewItem, &o, p, o.widget);

        const QStringList f = idx.data(Qt::UserRole).toStringList();
        QStringList textes;
        for (const QString &t : f) textes << court(t);
        QFont ft = o.font;
        ft.setPixelSize(11);
        ft.setBold(true);
        const QFontMetrics fm(ft);
        const int h = 22, gap = 4, marge = 8;
        const double gauche = opt.rect.left() + marge;
        const double droite = opt.rect.right() - marge;

        // 1er passage : nombre de lignes (pour centrer verticalement)
        int lignes = 1;
        double x = gauche;
        for (const QString &t : textes) {
            const double w = fm.horizontalAdvance(t) + 20;
            if (x + w > droite && x > gauche) { x = gauche; ++lignes; }
            x += w + 6;
        }
        const double bloc = lignes * h + (lignes - 1) * gap;
        double y = opt.rect.top() + (opt.rect.height() - bloc) / 2.0;

        p->save();
        p->setClipRect(opt.rect);                    // rien ne deborde sur les autres cases
        p->setRenderHint(QPainter::Antialiasing);
        p->setFont(ft);
        x = gauche;
        for (int k = 0; k < f.size(); ++k) {
            const QString &orig = f[k];
            const QString &t = textes[k];
            const double w = fm.horizontalAdvance(t) + 20;
            if (x + w > droite && x > gauche) { x = gauche; y += h + gap; }
            QColor fond("#e3eefa"), texte("#26598f");
            if (orig.contains("hors service") || orig.contains("retard")) { fond = QColor("#ffd8dc"); texte = QColor("#9d1020"); }
            else if (orig.contains("maintenance")) { fond = QColor("#fdebd0"); texte = QColor("#9d5500"); }
            else if (orig.contains("aucun")) { fond = QColor("#d7f4df"); texte = QColor("#176d37"); }
            p->setPen(Qt::NoPen);
            p->setBrush(fond);
            p->drawRoundedRect(QRectF(x, y, w, h), 11, 11);
            p->setPen(texte);
            p->drawText(QRectF(x, y, w, h), Qt::AlignCenter, t);
            x += w + 6;
        }
        p->restore();
    }

private:
    // version courte d'un facteur de risque (pastille compacte)
    static QString court(const QString &t) {
        const QRegularExpressionMatch m = QRegularExpression("\\d+").match(t);
        const QString n = m.hasMatch() ? m.captured(0) : QString();
        if (t.contains("hors service")) return "Hors service";
        if (t.contains("en retard")) return "Retard " + n + " j";
        if (t.contains("maintenance dans")) return "Dans " + n + " j";
        if (t.contains("en cours de maintenance")) return "En maintenance";
        if (t.contains("âge")) return "Âge " + n + " ans";
        if (t.contains("aucun")) return "Aucun risque";
        return t;
    }
};

// barre segmentee : repartition critiques / a surveiller / bons (animee)
class BarreSegments : public QWidget {
public:
    explicit BarreSegments(QWidget *parent = nullptr) : QWidget(parent) {}
    void setValeurs(int c, int s, int b) {
        m_v[0] = c; m_v[1] = s; m_v[2] = b;
        QVariantAnimation *a = new QVariantAnimation(this);
        a->setDuration(1000);
        a->setStartValue(0.0);
        a->setEndValue(1.0);
        a->setEasingCurve(QEasingCurve::OutCubic);
        connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) { m_p = v.toDouble(); update(); });
        a->start(QAbstractAnimation::DeleteWhenStopped);
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath coupe;
        coupe.addRoundedRect(QRectF(rect()), height() / 2.0, height() / 2.0);
        p.setClipPath(coupe);
        p.fillRect(rect(), QColor("#efe4e4"));
        const int total = m_v[0] + m_v[1] + m_v[2];
        if (total <= 0) return;
        const QColor cols[3] = {QColor("#c62828"), QColor("#ef6c00"), QColor("#2e7d32")};
        double x = 0.0;
        for (int k = 0; k < 3; ++k) {
            if (m_v[k] <= 0) continue;
            const double w = width() * double(m_v[k]) / total * m_p;
            p.fillRect(QRectF(x, 0, w, height()), cols[k]);
            x += w;
            p.fillRect(QRectF(x - 1.5, 0, 1.5, height()), Qt::white);     // fin separateur
        }
    }
private:
    int m_v[3] = {0, 0, 0};
    double m_p = 0.0;
};

// planning : diagramme de Gantt des cycles de maintenance (derniere -> prochaine)
class PlanningGantt : public QWidget {
public:
    struct Ent { QString id; QString nom; int score; QDate derniere; QDate prochaine; QDate suggeree; };

    explicit PlanningGantt(QWidget *parent = nullptr) : QWidget(parent) { setMouseTracking(true); }

    void setData(const QVector<Ent> &d, const QDate &aujourdhui) { m_all = d; m_today = aujourdhui; appliquer(); }
    void setFiltre(int niveau, const QString &q) { m_niv = niveau; m_q = q; appliquer(); }
    void animer() {
        QVariantAnimation *a = new QVariantAnimation(this);
        a->setDuration(1100);
        a->setStartValue(0.0);
        a->setEndValue(1.0);
        a->setEasingCurve(QEasingCurve::OutCubic);
        connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) { m_p = v.toDouble(); update(); });
        a->start(QAbstractAnimation::DeleteWhenStopped);
    }

protected:
    void leaveEvent(QEvent *) override {
        m_hov = -1;
        QToolTip::hideText();
        update();
    }

    void mouseMoveEvent(QMouseEvent *e) override {
        const int i = int((e->position().y() - 46.0) / 36.0);
        const int nouveau = (e->position().y() >= 46.0 && i >= 0 && i < m_rows.size()) ? i : -1;
        if (nouveau != m_hov) { m_hov = nouveau; update(); }
        if (m_hov >= 0) {
            const Ent &x = m_rows[m_hov];
            QString t = QString("<b>%1</b>  %2<br>Santé : <b>%3 %</b>")
                            .arg(x.id.toHtmlEscaped(), x.nom.toHtmlEscaped()).arg(x.score);
            if (x.derniere.isValid())
                t += "<br>Dernière : " + x.derniere.toString("dd/MM/yyyy");
            if (x.prochaine.isValid()) {
                const int j = int(m_today.daysTo(x.prochaine));
                t += "<br>Prochaine : " + x.prochaine.toString("dd/MM/yyyy")
                     + (j < 0 ? QString("  (retard %1 j)").arg(-j) : QString("  (dans %1 j)").arg(j));
            }
            QToolTip::showText(e->globalPosition().toPoint(), t, this);
        } else {
            QToolTip::hideText();
        }
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);
        p.fillRect(rect(), Qt::white);

        const int gauche = 210, droite = 26, haut = 46, hl = 36, plage = 240;
        const QDate dmin = m_today.addDays(-75);
        const double largeur = width() - gauche - droite;
        auto X = [&](const QDate &d) {
            double j = double(dmin.daysTo(d));
            j = qBound(0.0, j, double(plage));
            return gauche + j / plage * largeur;
        };
        const QFont base = font();
        auto fnt = [&](int px, bool gras) {
            QFont f = base;
            f.setPixelSize(px);
            f.setBold(gras);
            return f;
        };

        // reperes de mois
        const QLocale fr(QLocale::French);
        QDate m(dmin.year(), dmin.month(), 1);
        m = m.addMonths(1);
        while (m < dmin.addDays(plage)) {
            const double x = X(m);
            p.setPen(QPen(QColor("#efe4e4"), 1));
            p.drawLine(QPointF(x, haut - 10), QPointF(x, height() - 6));
            p.setFont(fnt(11, true));
            p.setPen(QColor("#9a9aa8"));
            p.drawText(QRectF(x + 5, 12, 80, 18), Qt::AlignLeft | Qt::AlignVCenter, fr.toString(m, "MMM yy"));
            m = m.addMonths(1);
        }

        if (m_rows.isEmpty()) {
            p.setFont(fnt(14, false));
            p.setPen(QColor("#999"));
            p.drawText(rect(), Qt::AlignCenter, "Aucun équipement à afficher");
            return;
        }

        for (int i = 0; i < m_rows.size(); ++i) {
            const Ent &e = m_rows[i];
            const double y = haut + i * hl;
            if (i == m_hov)
                p.fillRect(QRectF(0, y, width(), hl), QColor("#ffe9eb"));
            else if (i % 2)
                p.fillRect(QRectF(0, y, width(), hl), QColor("#fbf7f8"));

            // etiquette
            p.setFont(fnt(10, true));
            p.setPen(QColor("#9a9aa8"));
            p.drawText(QRectF(14, y + 3, gauche - 24, 14), Qt::AlignLeft | Qt::AlignVCenter, e.id);
            p.setFont(fnt(13, true));
            p.setPen(QColor("#302934"));
            p.drawText(QRectF(14, y + 16, gauche - 24, 18), Qt::AlignLeft | Qt::AlignVCenter,
                       QFontMetrics(fnt(13, true)).elidedText(e.nom, Qt::ElideRight, gauche - 26));

            const QColor c = e.score < 40 ? QColor("#c62828") : (e.score < 70 ? QColor("#ef6c00") : QColor("#2e7d32"));
            const QDate d0 = e.derniere.isValid() ? e.derniere : m_today.addDays(-30);
            const QDate d1 = e.prochaine.isValid() ? e.prochaine : m_today.addDays(180);
            const double x0 = X(d0);
            const double x1 = qMax(X(d1), x0 + 8.0);
            const double cy = y + hl / 2.0;

            // cycle complet (clair) + partie ecoulee (pleine, animee)
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(c.red(), c.green(), c.blue(), 55));
            p.drawRoundedRect(QRectF(x0, cy - 7, x1 - x0, 14), 7, 7);
            const double xt = qBound(x0, X(m_today), x1);
            if (xt > x0 + 1) {
                QLinearGradient g(x0, 0, xt, 0);
                g.setColorAt(0, c.lighter(145));
                g.setColorAt(1, c);
                p.setBrush(g);
                p.drawRoundedRect(QRectF(x0, cy - 7, (xt - x0) * m_p, 14), 7, 7);
            }

            // retard : trait pointille rouge jusqu'a aujourd'hui
            if (e.prochaine.isValid() && e.prochaine < m_today) {
                p.setPen(QPen(QColor("#c62828"), 2, Qt::DashLine));
                p.drawLine(QPointF(X(e.prochaine), cy), QPointF(X(m_today) * m_p + X(e.prochaine) * (1.0 - m_p), cy));
                p.setFont(fnt(10, true));
                p.setPen(QColor("#c62828"));
                p.drawText(QRectF(X(e.prochaine) + 8, y + 1, 130, 13), Qt::AlignLeft | Qt::AlignVCenter,
                           QString("retard %1 j").arg(int(e.prochaine.daysTo(m_today))));
            }

            // prochaine maintenance (cercle)
            if (e.prochaine.isValid()) {
                p.setPen(QPen(c, 2.4));
                p.setBrush(Qt::white);
                p.drawEllipse(QPointF(X(e.prochaine), cy), 6.5, 6.5);
            }
            // date suggeree (losange dore)
            if (e.score < 70 && e.suggeree.isValid()) {
                const double sx = X(e.suggeree);
                QPolygonF lo;
                lo << QPointF(sx, cy - 8) << QPointF(sx + 8, cy) << QPointF(sx, cy + 8) << QPointF(sx - 8, cy);
                p.setPen(QPen(QColor("#b88300"), 1.5));
                p.setBrush(QColor("#ffc247"));
                p.drawPolygon(lo);
            }
        }

        // ligne "aujourd'hui"
        const double xa = X(m_today);
        p.setPen(QPen(QColor("#c40000"), 2));
        p.drawLine(QPointF(xa, 32), QPointF(xa, height() - 6));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#c40000"));
        p.drawRoundedRect(QRectF(xa - 40, 8, 80, 20), 10, 10);
        p.setFont(fnt(10, true));
        p.setPen(Qt::white);
        p.drawText(QRectF(xa - 40, 8, 80, 20), Qt::AlignCenter, "Aujourd'hui");
    }

private:
    int code(int score) const { return score < 40 ? 1 : (score < 70 ? 2 : 3); }

    void appliquer() {
        m_rows.clear();
        for (const Ent &e : m_all) {
            if (m_niv != 0 && code(e.score) != m_niv) continue;
            if (!m_q.isEmpty() && !e.nom.contains(m_q, Qt::CaseInsensitive) && !e.id.contains(m_q, Qt::CaseInsensitive)) continue;
            m_rows.push_back(e);
        }
        setMinimumHeight(46 + int(m_rows.size()) * 36 + 16);
        update();
    }

    QVector<Ent> m_all;
    QVector<Ent> m_rows;
    QDate m_today;
    QString m_q;
    int m_niv = 0;
    int m_hov = -1;
    double m_p = 1.0;
};
}

void smart::afficherMaintenancePredictive()
{
    const QDate today = QDate::currentDate();
    g_hoverRow = -1;

    struct Ligne {
        int row; int score; QString id; QString nom; QStringList facteurs; QString reco;
        QDate suggeree; QDate prochaine; QDate derniere;
    };
    QVector<Ligne> lignes;
    int somme = 0, nbCrit = 0, nbSurv = 0, nbBon = 0;

    for (int r = 0; r < table->rowCount(); r++) {
        if (!table->item(r, 5) || !table->item(r, 7) || !table->item(r, 8) || !table->item(r, 9))
            continue;
        const QString e = table->item(r, 5)->text();
        const QDate da = QDate::fromString(table->item(r, 7)->text(), FMT);
        const QDate dm = QDate::fromString(table->item(r, 8)->text(), FMT);
        const QDate pm = QDate::fromString(table->item(r, 9)->text(), FMT);

        const int sc = scoreSante(e, da, dm, pm);
        QDate sug;
        const QString reco = recommandation(sc, pm, sug);
        somme += sc;
        if (sc < 40) ++nbCrit; else if (sc < 70) ++nbSurv; else ++nbBon;
        lignes.push_back({r, sc, table->item(r, 0)->text(), table->item(r, 1)->text(),
                          facteursSante(e, da, dm, pm), reco, sug, pm, dm});
    }
    std::sort(lignes.begin(), lignes.end(), [](const Ligne &a, const Ligne &b) { return a.score < b.score; });
    const int total = int(lignes.size());
    const int moyenne = total ? int(somme / double(total) + 0.5) : 0;

    // regle de planification : score < 70 et (pas de date, date depassee, ou date trop lointaine)
    auto aPlanifier = [today](const Ligne &l, QDate &nouvelle) {
        if (l.score >= 70) return false;
        nouvelle = l.suggeree;
        if (l.derniere.isValid() && nouvelle <= l.derniere) nouvelle = l.derniere.addDays(1);
        return !l.prochaine.isValid() || l.prochaine < today || nouvelle < l.prochaine;
    };
    int nbPlanifiables = 0;
    for (const Ligne &l : lignes) {
        QDate n;
        if (aPlanifier(l, n)) ++nbPlanifiables;
    }

    auto relatif = [today](const QDate &d) -> QString {
        if (!d.isValid()) return QString();
        const int j = int(today.daysTo(d));
        if (j == 0) return "aujourd'hui";
        if (j > 0) return QString("dans %1 j").arg(j);
        return QString("retard %1 j").arg(-j);
    };

    // ---------- fenetre ----------
    QDialog *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle("Maintenance prédictive");
    dlg->resize(1260, 780);
    dlg->setStyleSheet(
        "QDialog{background:#f4f4f6;}"
        "QLabel{border:none;background:transparent;}"
        "QLineEdit{border:2px solid #dfe2e7;border-radius:10px;padding:7px 12px;background:white;color:#302934;}"
        "QLineEdit:focus{border:2px solid #bd1e2c;}");

    QVBoxLayout *root = new QVBoxLayout(dlg);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ----- bandeau -----
    QFrame *banner = new QFrame;
    banner->setObjectName("maintBanner");
    banner->setFixedHeight(88);
    banner->setStyleSheet("QFrame#maintBanner{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #d40000,stop:1 #7a0000);}");
    QHBoxLayout *bh = new QHBoxLayout(banner);
    bh->setContentsMargins(28, 10, 28, 10);
    bh->setSpacing(16);
    QLabel *badge = new QLabel("🛠");
    badge->setAlignment(Qt::AlignCenter);
    badge->setFixedSize(52, 52);
    badge->setStyleSheet("background:rgba(255,255,255,0.2);color:white;border-radius:26px;font-size:24px;");
    QVBoxLayout *bt = new QVBoxLayout;
    bt->setSpacing(2);
    QLabel *titre = new QLabel("Maintenance prédictive");
    titre->setStyleSheet("color:white;font-size:23px;font-weight:bold;");
    QLabel *sous = new QLabel("Score de santé calculé automatiquement pour chaque équipement");
    sous->setStyleSheet("color:#ffd6d6;font-size:12px;");
    bt->addWidget(titre);
    bt->addWidget(sous);
    QLabel *date = new QLabel("Situation au " + today.toString(FMT));
    date->setStyleSheet("color:white;background:rgba(255,255,255,0.18);border-radius:12px;padding:6px 14px;"
                        "font-size:11px;font-weight:bold;");
    bh->addWidget(badge);
    bh->addLayout(bt, 1);
    bh->addWidget(date);
    root->addWidget(banner);

    QWidget *corps = new QWidget;
    QVBoxLayout *cv = new QVBoxLayout(corps);
    cv->setContentsMargins(24, 16, 24, 10);
    cv->setSpacing(12);

    // ----- indicateurs -----
    QHBoxLayout *kpis = new QHBoxLayout;
    kpis->setSpacing(14);

    const QString couleurMoy = moyenne >= 70 ? "#2e7d32" : (moyenne >= 40 ? "#ef6c00" : "#c62828");
    QFrame *carteMoy = new QFrame;
    carteMoy->setObjectName("kpiMoy");
    carteMoy->setStyleSheet("QFrame#kpiMoy{background:white;border:1px solid #e6e6e6;border-radius:14px;}");
    ombre(carteMoy);
    QHBoxLayout *mh = new QHBoxLayout(carteMoy);
    mh->setContentsMargins(16, 10, 18, 10);
    mh->setSpacing(14);
    AnneauSante *anneau = new AnneauSante;
    anneau->setPct(moyenne, QColor(couleurMoy));
    QVBoxLayout *mt = new QVBoxLayout;
    mt->setSpacing(2);
    QLabel *mt1 = new QLabel("Santé du parc");
    mt1->setStyleSheet("color:#302934;font-size:16px;font-weight:bold;");
    QLabel *mt2 = new QLabel(QString("%1 équipement(s) suivis").arg(total));
    mt2->setStyleSheet("color:#777;font-size:12px;");
    mt->addStretch();
    mt->addWidget(mt1);
    mt->addWidget(mt2);
    mt->addSpacing(6);
    BarreSegments *repartition = new BarreSegments;
    repartition->setFixedHeight(12);
    repartition->setValeurs(nbCrit, nbSurv, nbBon);
    mt->addWidget(repartition);
    QLabel *legRep = new QLabel(QString("<span style='color:#c62828'>●</span> %1 critiques &nbsp;&nbsp; "
                                        "<span style='color:#ef6c00'>●</span> %2 à surveiller &nbsp;&nbsp; "
                                        "<span style='color:#2e7d32'>●</span> %3 bons").arg(nbCrit).arg(nbSurv).arg(nbBon));
    legRep->setTextFormat(Qt::RichText);
    legRep->setStyleSheet("color:#777;font-size:11px;");
    mt->addWidget(legRep);
    mt->addStretch();
    mh->addWidget(anneau);
    mh->addLayout(mt, 1);
    kpis->addWidget(carteMoy, 3);

    auto compter = [](QLabel *l, int cible) {
        QVariantAnimation *an = new QVariantAnimation(l);
        an->setDuration(900);
        an->setStartValue(0);
        an->setEndValue(cible);
        an->setEasingCurve(QEasingCurve::OutCubic);
        QObject::connect(an, &QVariantAnimation::valueChanged, l, [l](const QVariant &v) { l->setText(QString::number(v.toInt())); });
        an->start(QAbstractAnimation::DeleteWhenStopped);
    };
    auto carteKpi = [&](const QString &nomObj, const QString &libelle, const QString &sousTitre,
                        const QString &couleur, int valeur) -> QFrame * {
        QFrame *f = new QFrame;
        f->setObjectName(nomObj);
        f->setStyleSheet(QString("QFrame#%1{background:white;border:1px solid #e6e6e6;border-left:5px solid %2;"
                                 "border-radius:14px;border-top-left-radius:3px;border-bottom-left-radius:3px;}")
                             .arg(nomObj, couleur));
        ombre(f);
        QVBoxLayout *l = new QVBoxLayout(f);
        l->setContentsMargins(18, 12, 14, 12);
        l->setSpacing(0);
        QLabel *lib = new QLabel(libelle);
        lib->setStyleSheet("color:#555;font-size:12px;font-weight:bold;");
        QLabel *val = new QLabel("0");
        val->setStyleSheet(QString("color:%1;font-size:34px;font-weight:bold;").arg(couleur));
        QLabel *st = new QLabel(sousTitre);
        st->setStyleSheet("color:#999;font-size:11px;");
        l->addWidget(lib);
        l->addWidget(val);
        l->addWidget(st);
        compter(val, valeur);
        return f;
    };
    kpis->addWidget(carteKpi("kpiCrit", "Critiques", "score < 40 %", "#c62828", nbCrit), 2);
    kpis->addWidget(carteKpi("kpiSurv", "À surveiller", "40 à 69 %", "#ef6c00", nbSurv), 2);
    kpis->addWidget(carteKpi("kpiBon", "En bon état", "score ≥ 70 %", "#2e7d32", nbBon), 2);
    cv->addLayout(kpis);

    // ----- filtres + recherche + aide -----
    auto filtre = std::make_shared<int>(0);
    const QString styleChip =
        "QPushButton{background:white;color:#a61725;border:1px solid #e0b5b9;border-radius:15px;"
        "padding:6px 16px;font-size:12px;font-weight:bold;}"
        "QPushButton:hover{background:#fff1f2;}"
        "QPushButton:checked{background:#c40000;color:white;border:1px solid #c40000;}";
    const QStringList nomsFiltres = {QString("Tous (%1)").arg(total), QString("Critiques (%1)").arg(nbCrit),
                                     QString("À surveiller (%1)").arg(nbSurv), QString("Bons (%1)").arg(nbBon)};
    QList<QPushButton *> chips;
    QHBoxLayout *barre = new QHBoxLayout;
    barre->setSpacing(8);
    for (int k = 0; k < nomsFiltres.size(); ++k) {
        QPushButton *b = new QPushButton(nomsFiltres[k]);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(styleChip);
        chips.push_back(b);
        barre->addWidget(b);
    }
    chips[0]->setChecked(true);

    QLineEdit *rech = new QLineEdit;
    rech->setPlaceholderText("🔍  Rechercher un équipement…");
    rech->setClearButtonEnabled(true);
    rech->setMinimumWidth(260);
    barre->addSpacing(10);
    barre->addWidget(rech);
    barre->addStretch(1);

    // vue : liste ou planning (diagramme de Gantt)
    QPushButton *segListe = new QPushButton("☰  Liste");
    QPushButton *segPlan = new QPushButton("▤  Planning");
    for (QPushButton *b : {segListe, segPlan}) {
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
    }
    segListe->setChecked(true);
    segListe->setStyleSheet("QPushButton{background:white;color:#a61725;border:1px solid #e0b5b9;"
                            "border-top-left-radius:15px;border-bottom-left-radius:15px;padding:6px 16px;"
                            "font-size:12px;font-weight:bold;}"
                            "QPushButton:checked{background:#c40000;color:white;border:1px solid #c40000;}");
    segPlan->setStyleSheet("QPushButton{background:white;color:#a61725;border:1px solid #e0b5b9;"
                           "border-top-right-radius:15px;border-bottom-right-radius:15px;padding:6px 16px;"
                           "font-size:12px;font-weight:bold;}"
                           "QPushButton:checked{background:#c40000;color:white;border:1px solid #c40000;}");
    QHBoxLayout *segCtl = new QHBoxLayout;
    segCtl->setSpacing(0);
    segCtl->addWidget(segListe);
    segCtl->addWidget(segPlan);
    barre->addLayout(segCtl);
    barre->addSpacing(8);

    QPushButton *btnInfo = new QPushButton("ⓘ  Comment est calculé le score ?");
    btnInfo->setCheckable(true);
    btnInfo->setCursor(Qt::PointingHandCursor);
    btnInfo->setStyleSheet(
        "QPushButton{background:#fff8e1;color:#5d4b00;border:1px solid #ffe082;border-radius:15px;"
        "padding:6px 16px;font-size:12px;font-weight:bold;}"
        "QPushButton:hover{background:#ffefb0;}"
        "QPushButton:checked{background:#ffe082;}");
    barre->addWidget(btnInfo);
    cv->addLayout(barre);

    QFrame *info = new QFrame;
    info->setObjectName("infoBox");
    info->setStyleSheet("QFrame#infoBox{background:#fff8e1;border:1px solid #ffe082;border-radius:12px;}"
                        "QLabel{border:none;background:transparent;}");
    QHBoxLayout *ih = new QHBoxLayout(info);
    ih->setContentsMargins(18, 12, 18, 12);
    ih->setSpacing(24);
    auto critere = [&](const QString &ti, const QString &tx) {
        QVBoxLayout *c = new QVBoxLayout;
        c->setSpacing(2);
        QLabel *a1 = new QLabel(ti);
        a1->setStyleSheet("color:#5d4b00;font-size:12px;font-weight:bold;");
        QLabel *b1 = new QLabel(tx);
        b1->setWordWrap(true);
        b1->setStyleSheet("color:#7a6a2a;font-size:11px;");
        c->addWidget(a1);
        c->addWidget(b1);
        ih->addLayout(c, 1);
    };
    critere("Cycle de maintenance", "Jusqu'à −70 points : part du cycle (dernière → prochaine) déjà écoulée, retard compris.");
    critere("Âge de l'équipement", "Jusqu'à −20 points : environ 3 points par année depuis l'achat.");
    critere("État", "−15 points si en maintenance. Hors service : score plafonné à 15 %.");
    critere("Niveaux", "≥ 70 % bon  •  40 à 69 % à surveiller  •  < 40 % critique.");
    info->hide();
    cv->addWidget(info);
    connect(btnInfo, &QPushButton::toggled, dlg, [info](bool on) { info->setVisible(on); });

    // ----- tableau -----
    QTableWidget *t = new QTableWidget(total, 7);
    t->setHorizontalHeaderLabels({"Équipement", "Santé", "Niveau", "Facteurs de risque",
                                  "Prochaine maintenance", "Recommandation", "Date suggérée"});
    for (int i = 0; i < total; i++) {
        const Ligne &l = lignes[i];
        const int code = l.score < 40 ? 1 : (l.score < 70 ? 2 : 3);
        const QString niveau = code == 1 ? "Critique" : (code == 2 ? "À surveiller" : "Bon");

        QTableWidgetItem *it0 = new QTableWidgetItem(l.nom);
        it0->setData(Qt::UserRole, l.id);
        t->setItem(i, 0, it0);
        t->setItem(i, 1, new QTableWidgetItem(QString::number(l.score)));
        QTableWidgetItem *it2 = new QTableWidgetItem(niveau);
        it2->setData(Qt::UserRole + 1, code);
        t->setItem(i, 2, it2);
        QTableWidgetItem *it3 = new QTableWidgetItem;
        it3->setData(Qt::UserRole, l.facteurs);
        it3->setToolTip(l.facteurs.join("\n"));
        t->setItem(i, 3, it3);

        QString prochTxt = l.prochaine.isValid() ? l.prochaine.toString(FMT) : QString("—");
        QTableWidgetItem *it4 = new QTableWidgetItem(prochTxt);
        const int jp = l.prochaine.isValid() ? int(today.daysTo(l.prochaine)) : 9999;
        it4->setForeground(QBrush(jp < 0 ? QColor("#c62828") : (jp <= 15 ? QColor("#ef6c00") : QColor("#555"))));
        if (jp < 0) { QFont f = it4->font(); f.setBold(true); it4->setFont(f); }
        t->setItem(i, 4, it4);

        const QString symbole = l.score < 40 ? "⚡  " : (l.score < 70 ? "📅  " : "✓  ");
        QTableWidgetItem *it5 = new QTableWidgetItem(symbole + l.reco);
        it5->setForeground(QBrush(l.score < 40 ? QColor("#c62828") : (l.score < 70 ? QColor("#ef6c00") : QColor("#2e7d32"))));
        QFont fr = it5->font();
        fr.setBold(l.score < 70);
        it5->setFont(fr);
        t->setItem(i, 5, it5);

        QString sugTxt = l.suggeree.isValid() ? l.suggeree.toString(FMT) + "  (" + relatif(l.suggeree) + ")" : QString("—");
        t->setItem(i, 6, new QTableWidgetItem(sugTxt));
    }
    t->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    t->setColumnWidth(0, 210);
    t->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    t->setColumnWidth(1, 140);
    t->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    t->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    t->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    t->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    t->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    t->setItemDelegateForColumn(0, new EquipDelegate(t));
    t->setItemDelegateForColumn(1, new SanteDelegate(t));
    t->setItemDelegateForColumn(2, new EtatDelegate(t));
    t->setItemDelegateForColumn(3, new FacteursDelegate(t));
    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(64);
    t->horizontalHeader()->setHighlightSections(false);
    t->setAlternatingRowColors(true);
    t->setShowGrid(false);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setFocusPolicy(Qt::NoFocus);
    t->setStyleSheet(
        "QTableWidget{border:1px solid #e4e6ea;border-radius:12px;background:white;"
        "alternate-background-color:#fbf7f8;selection-background-color:#ffe5e7;selection-color:#302934;}"
        "QHeaderView{background:#5b1722;border:none;}"
        "QHeaderView::section{background:#5b1722;color:white;font-weight:700;font-size:11px;"
        "border:none;border-right:1px solid #762834;padding:10px 6px;}");
    new CoinsArrondisEquip(t, 12);

    // ---- planning : diagramme de Gantt des cycles de maintenance ----
    PlanningGantt *gantt = new PlanningGantt;
    {
        QVector<PlanningGantt::Ent> donnees;
        for (const Ligne &l : lignes)
            donnees.push_back({l.id, l.nom, l.score, l.derniere, l.prochaine, l.suggeree});
        gantt->setData(donnees, today);
    }
    QScrollArea *scrollPlan = new QScrollArea;
    scrollPlan->setWidgetResizable(true);
    scrollPlan->setFrameShape(QFrame::NoFrame);
    scrollPlan->setStyleSheet("QScrollArea{background:white;border:1px solid #e4e6ea;border-radius:12px;}");
    scrollPlan->setWidget(gantt);
    QWidget *pagePlan = new QWidget;
    QVBoxLayout *ppl = new QVBoxLayout(pagePlan);
    ppl->setContentsMargins(0, 0, 0, 0);
    ppl->setSpacing(8);
    QLabel *legendePlan = new QLabel(
        "<span style='color:#2e7d32'>■</span> cycle écoulé (dernière maintenance → aujourd'hui) &nbsp;&nbsp; "
        "<span style='color:#bbb'>■</span> cycle restant &nbsp;&nbsp; ○ prochaine maintenance &nbsp;&nbsp; "
        "<span style='color:#d39a00'>◆</span> date suggérée &nbsp;&nbsp; <span style='color:#c40000'>|</span> aujourd'hui");
    legendePlan->setTextFormat(Qt::RichText);
    legendePlan->setStyleSheet("color:#777;font-size:11px;");
    ppl->addWidget(legendePlan);
    ppl->addWidget(scrollPlan, 1);

    QStackedWidget *pages = new QStackedWidget;
    pages->addWidget(t);
    pages->addWidget(pagePlan);
    cv->addWidget(pages, 1);
    root->addWidget(corps, 1);

    connect(segListe, &QPushButton::clicked, dlg, [=]() {
        segListe->setChecked(true);
        segPlan->setChecked(false);
        pages->setCurrentIndex(0);
    });
    connect(segPlan, &QPushButton::clicked, dlg, [=]() {
        segListe->setChecked(false);
        segPlan->setChecked(true);
        pages->setCurrentIndex(1);
        gantt->animer();
    });

    // filtre + recherche
    auto appliquer = [=]() {
        const QString q = rech->text().trimmed();
        for (int r = 0; r < t->rowCount(); ++r) {
            const int code = t->item(r, 2)->data(Qt::UserRole + 1).toInt();
            const QString nom = t->item(r, 0)->text();
            const QString id = t->item(r, 0)->data(Qt::UserRole).toString();
            const bool ok = (*filtre == 0 || code == *filtre)
                            && (q.isEmpty() || nom.contains(q, Qt::CaseInsensitive) || id.contains(q, Qt::CaseInsensitive));
            t->setRowHidden(r, !ok);
        }
        gantt->setFiltre(*filtre, q);
    };
    for (int k = 0; k < chips.size(); ++k) {
        connect(chips[k], &QPushButton::clicked, dlg, [=]() {
            *filtre = k;
            for (int j = 0; j < chips.size(); ++j)
                chips[j]->setChecked(j == k);
            appliquer();
        });
    }
    connect(rech, &QLineEdit::textChanged, dlg, [=]() { appliquer(); });

    // ----- pied de page -----
    QFrame *foot = new QFrame;
    foot->setStyleSheet("QFrame{background:white;border-top:1px solid #e6e6e6;}");
    QHBoxLayout *fl = new QHBoxLayout(foot);
    fl->setContentsMargins(24, 12, 24, 12);
    QLabel *aide = new QLabel(nbPlanifiables > 0
                                  ? QString("%1 équipement(s) peuvent être replanifiés : score < 70 % avec une maintenance en retard ou trop lointaine.").arg(nbPlanifiables)
                                  : QString("Aucune replanification nécessaire pour le moment."));
    aide->setStyleSheet("color:#888;font-size:11px;");
    fl->addWidget(aide);
    fl->addStretch();

    QPushButton *btnPlan = new QPushButton(QString("📅  Planifier automatiquement (%1)").arg(nbPlanifiables));
    btnPlan->setFixedHeight(38);
    btnPlan->setCursor(Qt::PointingHandCursor);
    btnPlan->setEnabled(nbPlanifiables > 0);
    btnPlan->setToolTip(nbPlanifiables > 0 ? "Avance la prochaine maintenance à la date suggérée"
                                           : "Aucune maintenance à replanifier");
    btnPlan->setStyleSheet(
        "QPushButton{background:#6a1b9a;color:white;border:none;border-radius:10px;"
        "padding:6px 20px;font-weight:bold;}"
        "QPushButton:hover{background:#4a148c;}"
        "QPushButton:disabled{background:#d8d2de;color:#8c8498;}");
    QPushButton *btnPdf = new QPushButton("📄  Exporter le rapport (PDF)");
    btnPdf->setFixedHeight(38);
    btnPdf->setCursor(Qt::PointingHandCursor);
    btnPdf->setStyleSheet("QPushButton{background:white;color:#a61725;border:2px solid #c94d59;border-radius:10px;"
                          "padding:6px 18px;font-weight:bold;}"
                          "QPushButton:hover{background:#fff1f2;}");
    connect(btnPdf, &QPushButton::clicked, dlg, [=]() {
        const QString fichier = QFileDialog::getSaveFileName(
            dlg, "Exporter le rapport",
            QDir::homePath() + "/Documents/rapport_maintenance_predictive.pdf", "PDF (*.pdf)");
        if (fichier.isEmpty()) return;

        QString html = "<h2 style='color:#c40000;margin-bottom:2px'>Rapport de maintenance prédictive</h2>"
                       "<p style='color:#777'>Situation au " + today.toString(FMT) + " &bull; " + QString::number(total)
                       + " équipement(s) &bull; santé moyenne du parc : <b>" + QString::number(moyenne) + " %</b></p>"
                                                                                                          "<table width='100%' cellspacing='8' cellpadding='8'><tr>"
                                                                                                          "<td bgcolor='#ffd8dc' align='center'><b style='font-size:20px;color:#9d1020'>" + QString::number(nbCrit) + "</b><br>Critiques</td>"
                                                   "<td bgcolor='#fdebd0' align='center'><b style='font-size:20px;color:#9d5500'>" + QString::number(nbSurv) + "</b><br>À surveiller</td>"
                                                   "<td bgcolor='#d7f4df' align='center'><b style='font-size:20px;color:#176d37'>" + QString::number(nbBon) + "</b><br>En bon état</td>"
                                                  "</tr></table><br>"
                                                  "<table width='100%' cellspacing='0' cellpadding='6' border='1' style='border-color:#e0d4d4'><tr bgcolor='#5b1722'>";
        const QStringList entetes = {"Équipement", "Santé", "Niveau", "Facteurs de risque",
                                     "Prochaine maintenance", "Recommandation", "Date suggérée"};
        for (const QString &h : entetes)
            html += "<th><font color='white'>" + h + "</font></th>";
        html += "</tr>";
        for (const Ligne &l : lignes) {
            const bool crit = l.score < 40;
            const bool surv = l.score < 70;
            const QString fond = crit ? "#ffd8dc" : (surv ? "#fdebd0" : "#d7f4df");
            const QString niveau = crit ? "Critique" : (surv ? "À surveiller" : "Bon");
            QStringList facteurs;
            for (const QString &f : l.facteurs) facteurs << f.toHtmlEscaped();
            html += "<tr><td><b>" + l.nom.toHtmlEscaped() + "</b><br><small>" + l.id.toHtmlEscaped() + "</small></td>"
                                                                                                       "<td align='center'><b>" + QString::number(l.score) + " %</b></td>"
                                                 "<td align='center' bgcolor='" + fond + "'><b>" + niveau + "</b></td>"
                                                "<td>" + facteurs.join(" &bull; ") + "</td>"
                                                  "<td>" + (l.prochaine.isValid() ? l.prochaine.toString(FMT) : QString("—")) + "</td>"
                                                                                           "<td>" + l.reco.toHtmlEscaped() + "</td>"
                                               "<td>" + (l.suggeree.isValid() ? l.suggeree.toString(FMT) : QString("—")) + "</td></tr>";
        }
        html += "</table>";

        QTextDocument doc;
        doc.setHtml(html);
        QPrinter imprimante(QPrinter::PrinterResolution);
        imprimante.setOutputFormat(QPrinter::PdfFormat);
        imprimante.setOutputFileName(fichier);
        imprimante.setPageSize(QPageSize(QPageSize::A4));
        imprimante.setPageOrientation(QPageLayout::Landscape);
        doc.print(&imprimante);
        QMessageBox::information(dlg, "PDF", "Export réussi !");
        journaliser("Export", "Maintenance prédictive", "Rapport PDF de la maintenance prédictive");
    });
    fl->addWidget(btnPdf);
    fl->addWidget(btnPlan);

    QPushButton *fermer = new QPushButton("Fermer");
    fermer->setFixedHeight(38);
    fermer->setMinimumWidth(120);
    fermer->setCursor(Qt::PointingHandCursor);
    fermer->setStyleSheet(
        "QPushButton{background:#c40000;color:white;border:none;border-radius:10px;"
        "padding:6px 18px;font-weight:bold;}"
        "QPushButton:hover{background:#8f0000;}");
    connect(fermer, &QPushButton::clicked, dlg, &QDialog::accept);
    fl->addWidget(fermer);
    root->addWidget(foot);

    connect(btnPlan, &QPushButton::clicked, dlg, [=]() {
        int n = 0;
        for (const Ligne &l : lignes) {
            QDate nouvelle;
            if (!aPlanifier(l, nouvelle)) continue;
            QTableWidgetItem *it = table->item(l.row, 9);
            const QDate ancienne = QDate::fromString(it->text(), FMT);
            it->setText(nouvelle.toString(FMT));
            {   // la nouvelle date est aussi enregistree dans Oracle
                EquipementExtra::Infos ex;
                ex.seuil = table->item(l.row, 4)->text().toInt();
                ex.prochaine = nouvelle;
                ex.remarques = table->item(l.row, 13)->text();
                ex.localisation = table->item(l.row, 6)->text();
                EquipementExtra::enregistrer(l.id, ex);
            }
            journaliser("Planification", l.id + "  " + l.nom,
                        QString("Prochaine maintenance : %1 → %2  •  score de santé %3 %")
                            .arg(ancienne.isValid() ? ancienne.toString(FMT) : QString("—"), nouvelle.toString(FMT))
                            .arg(l.score));
            n++;
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
void smart::envoyerAlerteEmail(const QString &sujet, const QString &corps, const QString &ref, bool manuel)
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

    // version HTML mise en forme, construite a partir des lignes d'alerte du message texte
    QStringList lignesAlerte;
    for (const QString &l : corps.split('\n'))
        if (l.startsWith("• ")) lignesAlerte << l;
    const QString html = lignesAlerte.isEmpty() ? QString() : htmlEmailAlerte(lignesAlerte);

    envoyerAsync(this, c, sujet, corps, html, [this, ref, c](bool ok, const QString &err) {
        if (ok) {
            journaliser("Alerte", ref, "Email d'alerte envoyé à " + c.destinataire + "  •  réponse serveur : " + err);
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
namespace {
// petite roue de chargement (pendant l'envoi de l'email de test)
class RouePatience : public QWidget {
public:
    explicit RouePatience(QWidget *parent = nullptr) : QWidget(parent) {
        setFixedSize(22, 22);
        m_t = new QTimer(this);
        connect(m_t, &QTimer::timeout, this, [this]() { m_a = (m_a + 16) % 360; update(); });
        hide();
    }
    void demarrer() { show(); m_t->start(30); }
    void arreter()  { m_t->stop(); hide(); }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor("#1565c0"), 3, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(QRectF(3, 3, 16, 16), -m_a * 16, 270 * 16);
    }
private:
    QTimer *m_t = nullptr;
    int m_a = 0;
};
}

void smart::afficherParametresEmail()
{
    ConfigEmail c = chargerConfigEmail();

    QDialog *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle("Paramètres email d'alerte");
    dlg->resize(940, 680);
    dlg->setStyleSheet(
        "QDialog{background:#f4f4f6;}"
        "QLabel{border:none;background:transparent;}"
        "QLabel#fieldLabel{color:#302934;font-weight:700;font-size:12px;}"
        "QLineEdit,QSpinBox{border:2px solid #dfe2e7;border-radius:10px;padding:7px 10px;"
        "background:#fbfbfc;color:#302934;min-height:20px;}"
        "QLineEdit:focus,QSpinBox:focus{border:2px solid #bd1e2c;background:white;}"
        "QSpinBox::up-button,QSpinBox::down-button{border:none;width:18px;}"
        "QCheckBox{color:#444;font-size:12px;spacing:8px;}"
        "QCheckBox::indicator{width:18px;height:18px;border-radius:5px;border:2px solid #c9c9d1;background:white;}"
        "QCheckBox::indicator:checked{background:#c40000;border:2px solid #c40000;}");

    QVBoxLayout *root = new QVBoxLayout(dlg);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ----- bandeau -----
    QFrame *banner = new QFrame;
    banner->setObjectName("mailBanner");
    banner->setFixedHeight(84);
    banner->setStyleSheet("QFrame#mailBanner{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #d40000,stop:1 #7a0000);}");
    QHBoxLayout *bh = new QHBoxLayout(banner);
    bh->setContentsMargins(28, 10, 28, 10);
    bh->setSpacing(16);
    QLabel *badge = new QLabel("✉");
    badge->setAlignment(Qt::AlignCenter);
    badge->setFixedSize(48, 48);
    badge->setStyleSheet("background:rgba(255,255,255,0.2);color:white;border-radius:24px;font-size:22px;");
    QVBoxLayout *bt = new QVBoxLayout;
    bt->setSpacing(2);
    QLabel *titre = new QLabel("Alerte par email");
    titre->setStyleSheet("color:white;font-size:22px;font-weight:bold;");
    QLabel *sous = new QLabel("Recevez automatiquement les alertes (état / quantité) dans la boîte du responsable");
    sous->setStyleSheet("color:#ffd6d6;font-size:12px;");
    bt->addWidget(titre);
    bt->addWidget(sous);
    bh->addWidget(badge);
    bh->addLayout(bt, 1);
    QLabel *ssl = new QLabel("🔒  Connexion sécurisée SSL / TLS");
    ssl->setStyleSheet("color:white;background:rgba(255,255,255,0.18);border-radius:12px;padding:5px 12px;"
                       "font-size:11px;font-weight:bold;");
    bh->addWidget(ssl);
    root->addWidget(banner);

    // ----- corps : formulaire (gauche) + aide / apercu (droite) -----
    QWidget *corps = new QWidget;
    QHBoxLayout *ch = new QHBoxLayout(corps);
    ch->setContentsMargins(24, 18, 24, 12);
    ch->setSpacing(20);

    const QString styleCarte = "QFrame#carte{background:white;border:1px solid #e6e0e0;border-radius:14px;}"
                               "QLabel{border:none;background:transparent;}";

    QFrame *formCarte = new QFrame;
    formCarte->setObjectName("carte");
    formCarte->setStyleSheet(styleCarte);
    ombre(formCarte);
    QVBoxLayout *fv = new QVBoxLayout(formCarte);
    fv->setContentsMargins(20, 16, 20, 16);
    fv->setSpacing(8);

    QLabel *t1 = new QLabel("CONFIGURATION");
    t1->setStyleSheet("color:#981a27;font-size:11px;font-weight:bold;letter-spacing:2px;");
    fv->addWidget(t1);

    // champs
    QLineEdit *serveur = new QLineEdit(c.serveur);
    QSpinBox *port = new QSpinBox;
    port->setRange(1, 65535);
    port->setValue(c.port);
    QLineEdit *expediteur = new QLineEdit(c.expediteur);
    expediteur->setPlaceholderText("exemple@gmail.com");
    QLineEdit *mdp = new QLineEdit(c.mdp);
    mdp->setEchoMode(QLineEdit::Password);
    mdp->setPlaceholderText("code de 16 caractères");
    QLineEdit *dest = new QLineEdit(c.destinataire);
    dest->setPlaceholderText("chef.caserne@exemple.com");
    QCheckBox *chkAuto = new QCheckBox("Envoyer automatiquement les alertes par email");
    chkAuto->setChecked(c.automatique);

    auto etiquette = [](const QString &t) {
        QLabel *l = new QLabel(t);
        l->setObjectName("fieldLabel");
        return l;
    };
    auto pastilleEtat = []() {
        QLabel *l = new QLabel;
        l->setFixedSize(24, 24);
        l->setAlignment(Qt::AlignCenter);
        return l;
    };
    QLabel *etSrv = pastilleEtat();
    QLabel *etExp = pastilleEtat();
    QLabel *etMdp = pastilleEtat();
    QLabel *etDest = pastilleEtat();

    // fournisseurs (remplissent serveur + port)
    fv->addWidget(etiquette("Fournisseur"));
    QHBoxLayout *chips = new QHBoxLayout;
    chips->setSpacing(8);
    struct Prefixe { QString nom; QString hote; int port; };
    const QList<Prefixe> fournisseurs = {
        {"Gmail", "smtp.gmail.com", 465},
        {"Outlook", "smtp.office365.com", 587},
        {"Yahoo", "smtp.mail.yahoo.com", 465}
    };
    for (const Prefixe &f : fournisseurs) {
        QPushButton *b = new QPushButton(f.nom);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet("QPushButton{background:white;color:#a61725;border:1px solid #e0b5b9;border-radius:13px;"
                         "padding:5px 16px;font-size:12px;font-weight:bold;}"
                         "QPushButton:hover{background:#c40000;color:white;border:1px solid #c40000;}");
        connect(b, &QPushButton::clicked, dlg, [serveur, port, f]() {
            serveur->setText(f.hote);
            port->setValue(f.port);
        });
        chips->addWidget(b);
    }
    chips->addStretch();
    fv->addLayout(chips);

    QHBoxLayout *ligneSrv = new QHBoxLayout;
    ligneSrv->setSpacing(10);
    QVBoxLayout *cSrv = new QVBoxLayout;
    cSrv->setSpacing(3);
    cSrv->addWidget(etiquette("Serveur SMTP"));
    QHBoxLayout *rSrv = new QHBoxLayout;
    rSrv->setSpacing(6);
    rSrv->addWidget(serveur, 1);
    rSrv->addWidget(etSrv);
    cSrv->addLayout(rSrv);
    QVBoxLayout *cPort = new QVBoxLayout;
    cPort->setSpacing(3);
    cPort->addWidget(etiquette("Port"));
    cPort->addWidget(port);
    port->setFixedWidth(96);
    ligneSrv->addLayout(cSrv, 1);
    ligneSrv->addLayout(cPort);
    fv->addLayout(ligneSrv);

    auto ligneChamp = [&](const QString &lib, QLineEdit *champ, QLabel *etat, QWidget *extra = nullptr) {
        fv->addWidget(etiquette(lib));
        QHBoxLayout *r = new QHBoxLayout;
        r->setSpacing(6);
        r->addWidget(champ, 1);
        if (extra) r->addWidget(extra);
        r->addWidget(etat);
        fv->addLayout(r);
    };
    ligneChamp("Email expéditeur", expediteur, etExp);

    QPushButton *oeil = new QPushButton("👁");
    oeil->setCheckable(true);
    oeil->setCursor(Qt::PointingHandCursor);
    oeil->setToolTip("Afficher / masquer le mot de passe");
    oeil->setFixedSize(36, 36);
    oeil->setStyleSheet("QPushButton{background:#fbfbfc;border:2px solid #dfe2e7;border-radius:10px;font-size:14px;}"
                        "QPushButton:hover{background:#fff1f2;}"
                        "QPushButton:checked{background:#fde4e7;border:2px solid #c94d59;}");
    connect(oeil, &QPushButton::toggled, dlg, [mdp](bool on) {
        mdp->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
    });
    ligneChamp("Mot de passe d'application", mdp, etMdp, oeil);
    QLabel *hintMdp = new QLabel;
    hintMdp->setStyleSheet("color:#ef6c00;font-size:11px;font-weight:bold;");
    fv->addWidget(hintMdp);

    ligneChamp("Email destinataire (responsable / chef de caserne)", dest, etDest);
    fv->addSpacing(4);
    fv->addWidget(chkAuto);

    // etat global + resultat du test
    QLabel *pilule = new QLabel;
    pilule->setAlignment(Qt::AlignCenter);
    fv->addWidget(pilule);

    RouePatience *roue = new RouePatience;
    QLabel *statut = new QLabel;
    statut->setWordWrap(true);
    statut->setMinimumHeight(30);
    QHBoxLayout *rs = new QHBoxLayout;
    rs->setSpacing(8);
    rs->addWidget(roue);
    rs->addWidget(statut, 1);
    fv->addLayout(rs);
    fv->addStretch(1);
    ch->addWidget(formCarte, 11);

    // ----- colonne droite : etapes + apercu -----
    QVBoxLayout *droite = new QVBoxLayout;
    droite->setSpacing(14);

    QFrame *etapes = new QFrame;
    etapes->setObjectName("carte");
    etapes->setStyleSheet(styleCarte);
    ombre(etapes);
    QVBoxLayout *ev = new QVBoxLayout(etapes);
    ev->setContentsMargins(18, 14, 18, 14);
    ev->setSpacing(10);
    QLabel *t2 = new QLabel("OBTENIR LE MOT DE PASSE D'APPLICATION (GMAIL)");
    t2->setStyleSheet("color:#981a27;font-size:11px;font-weight:bold;letter-spacing:1px;");
    ev->addWidget(t2);
    auto etape = [&](int n, const QString &ti, const QString &su) {
        QHBoxLayout *r = new QHBoxLayout;
        r->setSpacing(12);
        QLabel *num = new QLabel(QString::number(n));
        num->setFixedSize(28, 28);
        num->setAlignment(Qt::AlignCenter);
        num->setStyleSheet("background:#c40000;color:white;border-radius:14px;font-weight:bold;font-size:13px;");
        QVBoxLayout *tx = new QVBoxLayout;
        tx->setSpacing(1);
        QLabel *a = new QLabel(ti);
        a->setStyleSheet("color:#302934;font-size:12px;font-weight:bold;");
        QLabel *b = new QLabel(su);
        b->setStyleSheet("color:#777;font-size:11px;");
        b->setWordWrap(true);
        tx->addWidget(a);
        tx->addWidget(b);
        r->addWidget(num, 0, Qt::AlignTop);
        r->addLayout(tx, 1);
        ev->addLayout(r);
    };
    etape(1, "Activer la validation en 2 étapes", "Compte Google > Sécurité > Validation en deux étapes");
    etape(2, "Créer un mot de passe d'application", "Nom : FireStation. Google affiche un code de 16 caractères");
    etape(3, "Coller le code dans le champ ci-contre", "Jamais votre vrai mot de passe Gmail");
    QPushButton *ouvrir = new QPushButton("Ouvrir la page Google");
    ouvrir->setCursor(Qt::PointingHandCursor);
    ouvrir->setStyleSheet("QPushButton{background:white;color:#a61725;border:2px solid #c94d59;border-radius:10px;"
                          "padding:7px 14px;font-weight:bold;font-size:12px;}"
                          "QPushButton:hover{background:#fff1f2;}");
    connect(ouvrir, &QPushButton::clicked, dlg, []() {
        QDesktopServices::openUrl(QUrl("https://myaccount.google.com/apppasswords"));
    });
    ev->addWidget(ouvrir);
    droite->addWidget(etapes);

    // apercu du message recu
    QFrame *apercu = new QFrame;
    apercu->setObjectName("carte");
    apercu->setStyleSheet(styleCarte);
    ombre(apercu);
    QVBoxLayout *av = new QVBoxLayout(apercu);
    av->setContentsMargins(18, 14, 18, 14);
    av->setSpacing(8);
    QLabel *t3 = new QLabel("APERÇU DE L'ALERTE REÇUE");
    t3->setStyleSheet("color:#981a27;font-size:11px;font-weight:bold;letter-spacing:1px;");
    av->addWidget(t3);

    QHBoxLayout *entete = new QHBoxLayout;
    entete->setSpacing(10);
    QLabel *avatar = new QLabel("SF");
    avatar->setFixedSize(34, 34);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setStyleSheet("background:#c40000;color:white;border-radius:17px;font-weight:bold;font-size:12px;");
    QVBoxLayout *qui = new QVBoxLayout;
    qui->setSpacing(0);
    QLabel *prevDe = new QLabel;
    prevDe->setTextFormat(Qt::PlainText);
    prevDe->setStyleSheet("color:#302934;font-size:12px;font-weight:bold;");
    QLabel *prevA = new QLabel;
    prevA->setTextFormat(Qt::PlainText);
    prevA->setStyleSheet("color:#777;font-size:11px;");
    qui->addWidget(prevDe);
    qui->addWidget(prevA);
    entete->addWidget(avatar);
    entete->addLayout(qui, 1);
    av->addLayout(entete);

    QLabel *objet = new QLabel("[Smart Fire Station] 2 équipement(s) en alerte");
    objet->setStyleSheet("color:#302934;font-size:13px;font-weight:bold;");
    objet->setWordWrap(true);
    av->addWidget(objet);
    QLabel *corpsMail = new QLabel(
        "<span style='color:#c62828'>●</span> <b>EQ-006 Défibrillateur</b> : État hors service<br>"
        "<span style='color:#ef6c00'>●</span> <b>EQ-009 Casque</b> : quantité 1 / seuil 5<br>"
        "<span style='color:#999'>Merci de prendre les mesures nécessaires.</span>");
    corpsMail->setTextFormat(Qt::RichText);
    corpsMail->setWordWrap(true);
    corpsMail->setStyleSheet("color:#444;font-size:12px;background:#faf5f5;border-radius:10px;padding:10px 12px;");
    av->addWidget(corpsMail);
    av->addStretch(1);
    droite->addWidget(apercu, 1);
    ch->addLayout(droite, 9);
    root->addWidget(corps, 1);

    // ----- pied de page -----
    QFrame *foot = new QFrame;
    foot->setStyleSheet("QFrame{background:white;border-top:1px solid #e6e6e6;}");
    QHBoxLayout *fl = new QHBoxLayout(foot);
    fl->setContentsMargins(24, 12, 24, 12);

    QPushButton *btnTest = new QPushButton("✉  Envoyer un email de test");
    btnTest->setFixedHeight(38);
    btnTest->setCursor(Qt::PointingHandCursor);
    btnTest->setStyleSheet(
        "QPushButton{background:#f5a623;color:black;border:none;border-radius:10px;"
        "padding:6px 18px;font-weight:bold;}"
        "QPushButton:hover{background:#e0941a;}QPushButton:disabled{background:#ddd;color:#888;}");
    fl->addWidget(btnTest);
    fl->addStretch();

    QPushButton *btnAnnuler = new QPushButton("Annuler");
    btnAnnuler->setFixedHeight(38);
    btnAnnuler->setCursor(Qt::PointingHandCursor);
    btnAnnuler->setStyleSheet(
        "QPushButton{background:#eee;color:#333;border:none;border-radius:10px;"
        "padding:6px 20px;font-weight:bold;}QPushButton:hover{background:#ddd;}");
    QPushButton *btnSave = new QPushButton("Enregistrer");
    btnSave->setFixedHeight(38);
    btnSave->setMinimumWidth(130);
    btnSave->setCursor(Qt::PointingHandCursor);
    btnSave->setStyleSheet(
        "QPushButton{background:#c40000;color:white;border:none;border-radius:10px;"
        "padding:6px 20px;font-weight:bold;}QPushButton:hover{background:#8f0000;}");
    fl->addWidget(btnAnnuler);
    fl->addWidget(btnSave);
    root->addWidget(foot);

    // ----- logique -----
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

    auto emailValide = [](const QString &s) {
        static const QRegularExpression re("^[\\w.+-]+@[\\w-]+(\\.[\\w-]+)+$");
        return re.match(s.trimmed()).hasMatch();
    };
    auto pastiller = [](QLabel *l, bool ok, bool vide) {
        if (vide) {
            l->setText(QString());
            l->setStyleSheet("background:transparent;");
        } else if (ok) {
            l->setText("✓");
            l->setStyleSheet("background:#e3f5ea;color:#1e7a3c;border-radius:12px;font-weight:bold;font-size:13px;");
        } else {
            l->setText("✕");
            l->setStyleSheet("background:#fde4e7;color:#c62828;border-radius:12px;font-weight:bold;font-size:13px;");
        }
    };
    auto afficherStatut = [=](int genre, const QString &texte) {   // 0 info, 1 succes, 2 erreur
        const QString coul[] = {"#1565c0", "#1e7a3c", "#c62828"};
        const QString fond[] = {"#e6f0fb", "#e3f5ea", "#fde4e7"};
        statut->setText(texte);
        statut->setStyleSheet(QString("color:%1;background:%2;border-radius:10px;padding:8px 12px;"
                                      "font-size:12px;font-weight:bold;").arg(coul[genre], fond[genre]));
    };

    auto maj = [=]() {
        const bool srvOk = !serveur->text().trimmed().isEmpty();
        const bool expOk = emailValide(expediteur->text());
        QString code = mdp->text();
        code.remove(' ');
        const bool mdpOk = !code.isEmpty();
        const bool destOk = emailValide(dest->text());

        pastiller(etSrv, srvOk, serveur->text().isEmpty());
        pastiller(etExp, expOk, expediteur->text().isEmpty());
        pastiller(etMdp, mdpOk, mdp->text().isEmpty());
        pastiller(etDest, destOk, dest->text().isEmpty());

        if (serveur->text().contains("gmail", Qt::CaseInsensitive) && mdpOk && code.size() != 16)
            hintMdp->setText(QString("⚠  Un mot de passe d'application Gmail contient 16 caractères (%1 / 16)").arg(code.size()));
        else
            hintMdp->setText(QString());

        int erreurs = 0;
        if (!srvOk) ++erreurs;
        if (!expOk) ++erreurs;
        if (!mdpOk) ++erreurs;
        if (!destOk) ++erreurs;
        if (erreurs == 0) {
            pilule->setText("✓  Configuration complète : prête pour l'envoi");
            pilule->setStyleSheet("background:#e3f5ea;color:#1e7a3c;border-radius:11px;padding:6px 10px;"
                                  "font-size:12px;font-weight:bold;");
        } else {
            pilule->setText(QString("%1 champ(s) à compléter ou corriger").arg(erreurs));
            pilule->setStyleSheet("background:#fdebd0;color:#9d5500;border-radius:11px;padding:6px 10px;"
                                  "font-size:12px;font-weight:bold;");
        }

        prevDe->setText("Smart Fire Station <" + (expOk ? expediteur->text().trimmed() : QString("expéditeur")) + ">");
        prevA->setText("À : " + (destOk ? dest->text().trimmed() : QString("destinataire")));
    };
    connect(serveur, &QLineEdit::textChanged, dlg, maj);
    connect(expediteur, &QLineEdit::textChanged, dlg, maj);
    connect(mdp, &QLineEdit::textChanged, dlg, maj);
    connect(dest, &QLineEdit::textChanged, dlg, maj);
    maj();

    connect(btnTest, &QPushButton::clicked, dlg, [=]() {
        ConfigEmail x = lireConfig();
        if (!configEmailComplete(x)) {
            afficherStatut(2, "⚠  Remplissez tous les champs avec des adresses email valides (avec @).");
            return;
        }
        afficherStatut(0, "Envoi de l'email de test en cours…");
        roue->demarrer();
        btnTest->setEnabled(false);
        envoyerAsync(dlg, x, "[Smart Fire Station] Email de test",
                     "Bonjour,\n\nCeci est un email de test envoyé par Smart Fire Station.\n"
                     "Si vous le recevez, l'envoi des alertes par email fonctionne.\n\n"
                     "— Smart Fire Station",
                     htmlEmailTest(),
                     [=](bool ok, const QString &err) {
                         roue->arreter();
                         btnTest->setEnabled(true);
                         if (ok)
                             afficherStatut(1, "✓  Email accepté par le serveur pour " + x.destinataire + ".\n"
                                                                                                          "Réponse du serveur : " + err + "\n"
                                                           "Vérifiez la boîte de réception, les Spams et les onglets Promotions / Notifications.");
                         else
                             afficherStatut(2, "⚠  Échec : " + err);
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

// =====================================================================
//  Bascule mode clair / mode sombre
// =====================================================================
void smart::appliquerTheme(bool sombre)
{
    static const QPalette paletteClaire = QApplication::palette();

    g_sombre = sombre;
    setUpdatesEnabled(false);

    if (sombre) {
        QPalette p;
        p.setColor(QPalette::Window, QColor("#16161b"));
        p.setColor(QPalette::WindowText, QColor("#e4e4ea"));
        p.setColor(QPalette::Base, QColor("#23232b"));
        p.setColor(QPalette::AlternateBase, QColor("#292932"));
        p.setColor(QPalette::Text, QColor("#e4e4ea"));
        p.setColor(QPalette::Button, QColor("#2c2c36"));
        p.setColor(QPalette::ButtonText, QColor("#e4e4ea"));
        p.setColor(QPalette::ToolTipBase, QColor("#2b2b2b"));
        p.setColor(QPalette::ToolTipText, QColor("#ffffff"));
        p.setColor(QPalette::PlaceholderText, QColor("#8a8a96"));
        p.setColor(QPalette::Highlight, QColor("#c40000"));
        QApplication::setPalette(p);
    } else {
        QApplication::setPalette(paletteClaire);
    }

    themeSurArbre(this, sombre);

    if (QPushButton *b = findChild<QPushButton *>("btnTheme"))
        b->setText(sombre ? "☀" : "🌙");

    QSettings("SmartFireStation", "Equipements").setValue("theme/sombre", sombre);

    colorierLignes();       // couleurs des lignes, cartes, carte "priorité"

    setUpdatesEnabled(true);
    for (QWidget *w : findChildren<QWidget *>())
        w->update();
    update();
}

// =====================================================================
//  Plan des emplacements : vue "plan d'architecte" de la caserne
//  vert = libre, rouge = occupe, bleu = emplacement actuel
// =====================================================================
namespace {

// anneau d'occupation anime (theme sombre)
class DonutOcc : public QWidget {
public:
    explicit DonutOcc(QWidget *parent = nullptr) : QWidget(parent) { setFixedSize(132, 132); }
    void setPct(int pct, const QColor &c) {
        m_c = c;
        QVariantAnimation *a = new QVariantAnimation(this);
        a->setDuration(900);
        a->setStartValue(0.0);
        a->setEndValue(double(pct));
        a->setEasingCurve(QEasingCurve::OutCubic);
        connect(a, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            m_v = v.toDouble();
            update();
        });
        a->start(QAbstractAnimation::DeleteWhenStopped);
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r(14, 14, width() - 28, height() - 28);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#262c3a"), 12));
        p.drawEllipse(r);
        if (m_v > 0.0) {
            p.setPen(QPen(QColor(m_c.red(), m_c.green(), m_c.blue(), 60), 18, Qt::SolidLine, Qt::RoundCap));
            p.drawArc(r, 90 * 16, -int(m_v * 3.6 * 16));                 // halo
            p.setPen(QPen(m_c, 12, Qt::SolidLine, Qt::RoundCap));
            p.drawArc(r, 90 * 16, -int(m_v * 3.6 * 16));
        }
        QFont f = font();
        f.setPixelSize(27);
        f.setBold(true);
        p.setFont(f);
        p.setPen(QColor("#e9edf5"));
        p.drawText(QRectF(0, height() / 2.0 - 22, width(), 30), Qt::AlignCenter, QString("%1 %").arg(int(m_v + 0.5)));
        f.setPixelSize(10);
        f.setBold(true);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);
        p.setFont(f);
        p.setPen(QColor("#8b95a9"));
        p.drawText(QRectF(0, height() / 2.0 + 8, width(), 16), Qt::AlignCenter, "OCCUPÉ");
    }
private:
    double m_v = 0.0;
    QColor m_c = QColor("#2ee59d");
};

class PlanEmplacements : public QWidget {
public:
    // type : 0 garage, 1 vehicules, 2 magasin, 3 bureau, 4 salle de garde
    struct Zone { int type; QString titre; QRectF r; QStringList places; QColor accent; };
    struct Slot { QString nom; int zone; QRectF r; };
    struct StatZone { int type; QString titre; int libres; int total; };

    std::function<void(const QString &)> surChoix;     // clic sur un emplacement libre
    std::function<void(const QString &)> surSurvol;    // souris sur un emplacement ("" si aucun)

    PlanEmplacements(const QMap<QString, QString> &occ, const QString &actuel, QWidget *parent = nullptr)
        : QWidget(parent), m_occ(occ), m_actuel(actuel)
    {
        setMinimumSize(700, 470);
        setMouseTracking(true);

        m_zones = {
            { 0, "Garage",         QRectF(24, 24, 952, 196),   { "Garage 1", "Garage 2", "Garage 3", "Garage 4" },          QColor("#ff8a5c") },
            { 1, "Véhicules",      QRectF(512, 240, 464, 206), { "Vehicule 1", "Vehicule 2", "Vehicule 3", "Vehicule 4" }, QColor("#9a9bff") },
            { 2, "Magasin",        QRectF(24, 240, 464, 206),  { "Magasin A", "Magasin B", "Magasin C", "Magasin D" },       QColor("#4fd1c5") },
            { 3, "Bureau",         QRectF(24, 466, 464, 150),  { "Bureau 1", "Bureau 2" },                                  QColor("#ffc247") },
            { 4, "Salle de garde", QRectF(512, 466, 464, 150), { "Salle de garde" },                                        QColor("#63b3ed") }
        };

        for (int z = 0; z < m_zones.size(); ++z) {
            const QStringList &pl = m_zones[z].places;
            for (int i = 0; i < pl.size(); ++i) {
                QRectF r;
                if (z == 0)      r = QRectF(44 + i * 232, 70, 212, 134);
                else if (z == 1) r = QRectF(532 + (i % 2) * 222, 290 + (i / 2) * 74, 208, 64);
                else if (z == 2) r = QRectF(44 + (i % 2) * 222, 290 + (i / 2) * 74, 208, 64);
                else if (z == 3) r = QRectF(44 + i * 222, 514, 208, 88);
                else             r = QRectF(532, 514, 424, 88);
                m_slots.push_back({pl[i], z, r});
                m_hv.push_back(0.0);
            }
        }

        for (int x : {90, 210, 330, 670, 790, 910}) {
            m_lampes.push_back(QPointF(x, 230));
            m_lampes.push_back(QPointF(x, 456));
        }
        for (int y : {290, 400, 520, 600})
            m_lampes.push_back(QPointF(500, y));

        QTimer *t = new QTimer(this);
        connect(t, &QTimer::timeout, this, [this]() { tick(); });
        t->start(16);
    }

    void setFiltre(int f)                { m_filtre = f; update(); }
    void setRecherche(const QString &s)  { m_recherche = s.trimmed(); update(); }
    void zoomPlus()   { zoomAutour(m_zoom * 1.25, QPointF(width() / 2.0, height() / 2.0)); }
    void zoomMoins()  { zoomAutour(m_zoom / 1.25, QPointF(width() / 2.0, height() / 2.0)); }
    void recentrer()  { m_zoom = 1.0; m_pan = QPointF(0, 0); }
    void lancerAlerte() { if (!m_alerte) { m_alerte = true; m_alerteT = 0.0; } }
    void basculerPluie(bool on) { m_pluie = on; }

    // suggestion : premier emplacement libre de la zone recommandee pour ce type d'equipement
    void setSuggestionZone(int z) {
        m_sugg = -1;
        if (z < 0) return;
        for (int i = 0; i < m_slots.size(); ++i)
            if (m_slots[i].zone == z && !estPris(m_slots[i].nom)) { m_sugg = i; break; }
    }
    QString nomSuggestion() const { return m_sugg >= 0 ? m_slots[m_sugg].nom : QString(); }
    void choisirSuggestion() { if (m_sugg >= 0 && m_pick < 0) { m_pick = m_sugg; m_pickT = 0.0; } }

    bool estPris(const QString &n) const   { return m_occ.contains(n) && n != m_actuel; }
    bool estActuel(const QString &n) const { return !m_actuel.isEmpty() && n == m_actuel; }
    QString occupant(const QString &n) const { return m_occ.value(n); }
    int nbTotal() const { return int(m_slots.size()); }
    int nbPris() const {
        int c = 0;
        for (const Slot &s : m_slots) if (estPris(s.nom)) ++c;
        return c;
    }
    QList<StatZone> statsZones() const {
        QList<StatZone> l;
        for (const Zone &z : m_zones) {
            int libres = 0;
            for (const QString &n : z.places) if (!estPris(n)) ++libres;
            l.push_back({z.type, z.titre, libres, int(z.places.size())});
        }
        return l;
    }

protected:
    void leaveEvent(QEvent *) override {
        m_dansCarte = false;
        m_tiltTx = 0.0;
        m_tiltTy = 0.0;
        if (m_hover != -1) {
            m_hover = -1;
            if (surSurvol) surSurvol(QString());
        }
    }

    void wheelEvent(QWheelEvent *e) override {
        zoomAutour(m_zoom * std::pow(1.0015, e->angleDelta().y()), e->position());
        e->accept();
    }

    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() != Qt::LeftButton) return;
        m_pressPos = e->position();
        m_lastPos = e->position();
        m_pressSlot = slotAt(e->position());
        m_drag = false;
    }

    void mouseMoveEvent(QMouseEvent *e) override {
        const QPointF pos = e->position();
        m_souris = pos;
        m_dansCarte = true;
        m_tiltTy = (pos.x() / qMax(1, width()) - 0.5) * 7.0;       // la carte se penche vers la souris
        m_tiltTx = -(pos.y() / qMax(1, height()) - 0.5) * 5.0;
        if (e->buttons() & Qt::LeftButton) {
            if (!m_drag && (pos - m_pressPos).manhattanLength() > 6) {
                m_drag = true;
                setCursor(Qt::ClosedHandCursor);
                QToolTip::hideText();
            }
            if (m_drag) {
                m_pan += pos - m_lastPos;
                m_lastPos = pos;
                return;
            }
        }
        m_lastPos = pos;

        const int i = slotAt(pos);
        if (i != m_hover) {
            m_hover = i;
            if (surSurvol) surSurvol(i >= 0 ? m_slots[i].nom : QString());
        }
        if (i < 0) {
            setCursor(Qt::OpenHandCursor);
            QToolTip::hideText();
        } else if (estPris(m_slots[i].nom)) {
            setCursor(Qt::ForbiddenCursor);
            QToolTip::showText(e->globalPosition().toPoint(),
                               "Occupé par : " + m_occ.value(m_slots[i].nom), this);
        } else {
            setCursor(Qt::PointingHandCursor);
            QToolTip::showText(e->globalPosition().toPoint(),
                               estActuel(m_slots[i].nom) ? "Emplacement actuel" : "Libre : cliquer pour choisir", this);
        }
    }

    void mouseReleaseEvent(QMouseEvent *e) override {
        if (e->button() != Qt::LeftButton) return;
        const bool etaitDrag = m_drag;
        m_drag = false;
        if (etaitDrag) {
            setCursor(Qt::OpenHandCursor);
            return;
        }
        const int i = slotAt(e->position());
        if (i >= 0 && i == m_pressSlot && !estPris(m_slots[i].nom) && m_pick < 0) {
            m_pick = i;          // petite onde, puis le choix est valide
            m_pickT = 0.0;
        }
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);
        p.fillRect(rect(), QColor("#0e1218"));

        double s, ox, oy;
        calcul(s, ox, oy);

        const QFont base = font();
        auto fnt = [&](int px, bool gras, double esp = 0.0) {
            QFont f = base;
            f.setPixelSize(px);
            f.setBold(gras);
            if (esp > 0.0) f.setLetterSpacing(QFont::AbsoluteSpacing, esp);
            return f;
        };
        auto melange = [](const QColor &a, const QColor &b, double t) {
            return QColor(int(a.red() + (b.red() - a.red()) * t),
                          int(a.green() + (b.green() - a.green()) * t),
                          int(a.blue() + (b.blue() - a.blue()) * t));
        };

        p.save();
        QTransform tr = matriceCarte();            // zoom + deplacement + legere perspective 3D
        if (m_alerte && m_alerteT < 0.7) {         // secousse a l'ouverture de l'alerte
            const double amp = (1.0 - m_alerteT / 0.7) * 7.0;
            tr = tr * QTransform::fromTranslate(std::sin(m_alerteT * 80.0) * amp, std::cos(m_alerteT * 95.0) * amp * 0.6);
        }
        p.setTransform(tr, false);

        // quadrillage de points (fond "plan technique")
        {
            QPolygonF pts;
            for (int x = -400; x < 1500; x += 40)
                for (int y = -400; y < 1200; y += 40)
                    pts << QPointF(x, y);
            p.setPen(QPen(QColor(255, 255, 255, 20), 2.4, Qt::SolidLine, Qt::RoundCap));
            p.drawPoints(pts);
        }

        p.translate(40, 40);     // le plan (0..1000 x 0..640) est entoure d'une marge

        // terrain
        const QRectF sol(8, 8, 984, 624);
        p.setPen(QPen(QColor("#2b3242"), 2));
        p.setBrush(QColor("#121720"));
        p.drawRoundedRect(sol, 22, 22);

        // balayage lumineux discret (effet "radar")
        p.save();
        QPainterPath coupeSol;
        coupeSol.addRoundedRect(sol, 22, 22);
        p.setClipPath(coupeSol);
        const double bx = -200 + std::fmod(m_phase * 120.0, 1500.0);
        QLinearGradient radar(bx, 0, bx + 220, 0);
        radar.setColorAt(0.0, QColor(80, 200, 255, 0));
        radar.setColorAt(0.5, QColor(80, 200, 255, 16));
        radar.setColorAt(1.0, QColor(80, 200, 255, 0));
        p.fillRect(QRectF(bx, 8, 220, 624), radar);
        if (m_intro < 1.0) {                       // onde d'ouverture
            const double e1 = easeOutCubic(qMin(1.0, m_intro * 1.1));
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(80, 200, 255, int(170 * (1.0 - e1))), 3));
            p.drawEllipse(QPointF(500, 320), 30 + 900 * e1, 30 + 900 * e1);
        }
        p.restore();

        // reperes d'angle
        p.setPen(QPen(QColor("#ff5468"), 3, Qt::SolidLine, Qt::RoundCap));
        const double L = 22;
        p.drawLine(QPointF(8, 8 + L), QPointF(8, 8));          p.drawLine(QPointF(8, 8), QPointF(8 + L, 8));
        p.drawLine(QPointF(992 - L, 8), QPointF(992, 8));      p.drawLine(QPointF(992, 8), QPointF(992, 8 + L));
        p.drawLine(QPointF(8, 632 - L), QPointF(8, 632));      p.drawLine(QPointF(8, 632), QPointF(8 + L, 632));
        p.drawLine(QPointF(992 - L, 632), QPointF(992, 632));  p.drawLine(QPointF(992, 632), QPointF(992, 632 - L));

        // routes
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#1b212b"));
        p.drawRect(QRectF(12, 220, 976, 20));
        p.drawRect(QRectF(12, 446, 976, 20));
        p.drawRect(QRectF(488, 240, 24, 430));
        QPen dash(QColor(255, 255, 255, 55), 2);
        dash.setDashPattern({5, 5});
        p.setPen(dash);
        p.drawLine(QPointF(16, 230), QPointF(984, 230));
        p.drawLine(QPointF(16, 456), QPointF(984, 456));
        p.drawLine(QPointF(500, 244), QPointF(500, 664));

        // entree principale
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#ff5468"));
        QPolygonF fleche;
        fleche << QPointF(500, 632) << QPointF(492, 648) << QPointF(508, 648);
        p.drawPolygon(fleche);
        p.setFont(fnt(10, true, 1.6));
        p.setPen(QColor("#8b95a9"));
        p.drawText(QRectF(440, 658, 120, 16), Qt::AlignCenter, "ENTRÉE");

        dessinerLampes(p);     // lampadaires le long des routes

        // ----- zones (elles "se construisent" l'une apres l'autre) -----
        for (int z = 0; z < m_zones.size(); ++z) {
            const Zone &zn = m_zones[z];
            const QColor ac = zn.accent;
            const double ez = easeOutCubic(qBound(0.0, m_intro * 1.7 - z * 0.12, 1.0));
            p.save();
            p.setOpacity(ez);
            p.translate(zn.r.center());
            p.scale(0.92 + 0.08 * ez, 0.92 + 0.08 * ez);
            p.translate(-zn.r.center());

            p.setPen(QPen(QColor(ac.red(), ac.green(), ac.blue(), 95), 1.4));
            p.setBrush(QColor("#171c26"));
            p.drawRoundedRect(zn.r, 16, 16);

            // pastille d'icone + titre
            const QRectF ib(zn.r.left() + 14, zn.r.top() + 9, 28, 28);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(ac.red(), ac.green(), ac.blue(), 38));
            p.drawRoundedRect(ib, 8, 8);
            icone(p, zn.type, ib, ac);

            p.setFont(fnt(12, true, 1.8));
            p.setPen(QColor("#c9d1e1"));
            p.drawText(QRectF(zn.r.left() + 52, zn.r.top() + 9, zn.r.width() - 190, 28),
                       Qt::AlignLeft | Qt::AlignVCenter, zn.titre.toUpper());

            int libres = 0;
            for (const QString &n : zn.places) if (!estPris(n)) ++libres;
            const QColor cc = libres == 0 ? QColor("#ff5468") : QColor("#2ee59d");
            pastille(p, QRectF(zn.r.right() - 124, zn.r.top() + 13, 108, 20),
                     QString("%1 / %2 LIBRES").arg(libres).arg(zn.places.size()), cc, fnt(10, true, 0.8));

            p.setPen(QPen(QColor(255, 255, 255, 14), 1));
            p.drawLine(QPointF(zn.r.left() + 14, zn.r.top() + 46), QPointF(zn.r.right() - 14, zn.r.top() + 46));
            p.restore();
        }

        // ----- emplacements -----
        for (int i = 0; i < m_slots.size(); ++i) {
            const Slot &sl = m_slots[i];
            const QRectF &r = sl.r;
            const bool pris = estPris(sl.nom);
            const bool act = estActuel(sl.nom);
            const bool garage = (sl.zone == 0);
            const double hv = m_hv[i];
            const bool trouve = correspond(sl);

            double t = m_intro * 1.7 - 0.25 - i * 0.05;
            t = qBound(0.0, t, 1.0);
            const double opaque = t * (visible(sl) ? 1.0 : 0.2);
            if (opaque <= 0.001) continue;

            const QColor etat = act ? QColor("#4da3ff") : (pris ? QColor("#ff5468") : QColor("#2ee59d"));
            const QString etatTxt = act ? "ACTUEL" : (pris ? "OCCUPÉ" : "LIBRE");

            p.save();
            p.setOpacity(opaque);
            p.translate(0, (1.0 - (1.0 - std::pow(1.0 - t, 3))) * 16.0 - 3.0 * hv);

            // halo au survol (libre seulement)
            if (hv > 0.02 && !pris) {
                p.setPen(Qt::NoPen);
                for (int g : {9, 6, 3}) {
                    p.setBrush(QColor(etat.red(), etat.green(), etat.blue(), int(18 * hv)));
                    p.drawRoundedRect(r.adjusted(-g, -g, g, g), 12 + g, 12 + g);
                }
            }
            // impulsion sur l'emplacement actuel
            if (act) {
                const double ph = std::fmod(m_phase, 1.0);
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(QColor(77, 163, 255, int(180 * (1.0 - ph))), 2.5));
                const double g = 3 + 12 * ph;
                p.drawRoundedRect(r.adjusted(-g, -g, g, g), 14 + g, 14 + g);
            }
            // halo dore : resultat de recherche
            if (!m_recherche.isEmpty() && trouve) {
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(QColor("#ffc247"), 3));
                p.drawRoundedRect(r.adjusted(-5, -5, 5, 5), 16, 16);
            }

            // emplacement recommande : anneau dore qui respire
            if (i == m_sugg && !pris) {
                const double ph = 0.5 + 0.5 * std::sin(m_phase * 3.0);
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(QColor(255, 194, 71, int(110 + 120 * ph)), 2.6));
                const double g = 4 + 3 * ph;
                p.drawRoundedRect(r.adjusted(-g, -g, g, g), 14 + g, 14 + g);
            }

            // ombre + carte
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 90));
            p.drawRoundedRect(r.translated(0, 4), 12, 12);
            p.setPen(QPen(QColor(etat.red(), etat.green(), etat.blue(), int(95 + 130 * hv)), 1.5));
            p.setBrush(melange(QColor("#1f2635"), QColor("#2a3550"), hv));
            p.drawRoundedRect(r, 12, 12);

            // hachures : emplacement occupe
            if (pris) {
                p.save();
                QPainterPath cp;
                cp.addRoundedRect(r, 12, 12);
                p.setClipPath(cp);
                p.setPen(QPen(QColor(255, 84, 104, 20), 3));
                for (double k = -r.height(); k < r.width(); k += 11)
                    p.drawLine(QPointF(r.left() + k, r.bottom()), QPointF(r.left() + k + r.height(), r.top()));
                p.restore();
            }

            const bool parti = m_alerte && i < 4 && alerteU(i) > 0.02;
            const QColor sousCol = parti ? QColor("#ffb547") : (pris ? QColor("#ff8a98") : (act ? QColor("#7fbcff") : QColor("#8b95a9")));
            const QString sousTxt = parti ? "En intervention" : (act ? "Emplacement actuel" : (pris ? m_occ.value(sl.nom) : "Place disponible"));

            if (garage) {
                // portes roulantes
                p.setPen(QPen(QColor(255, 255, 255, 20), 2));
                for (int k = 0; k < 4; ++k)
                    p.drawLine(QPointF(r.left() + 16, r.top() + 16 + k * 8),
                               QPointF(r.right() - 88, r.top() + 16 + k * 8));
                pastille(p, QRectF(r.right() - 74, r.top() + 12, 60, 18), etatTxt, etat, fnt(10, true, 0.8));

                p.setFont(fnt(16, true));
                p.setPen(QColor("#e9edf5"));
                p.drawText(QRectF(r.left() + 16, r.top() + 46, r.width() - 32, 24),
                           Qt::AlignLeft | Qt::AlignVCenter, sl.nom);
                QFontMetrics fm(fnt(12, false));
                p.setFont(fnt(12, false));
                p.setPen(sousCol);
                p.drawText(QRectF(r.left() + 16, r.top() + 70, r.width() - 32, 18),
                           Qt::AlignLeft | Qt::AlignVCenter,
                           fm.elidedText(sousTxt, Qt::ElideRight, int(r.width() - 32)));
                dessinerCamion(p, QPointF(r.center().x() - 30, r.top() + 92), etat, (pris || act) && !parti, m_phase);
            } else {
                const double cy = r.center().y();
                const QRectF ib(r.left() + 12, cy - 19, 38, 38);
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(etat.red(), etat.green(), etat.blue(), 34));
                p.drawRoundedRect(ib, 10, 10);
                icone(p, m_zones[sl.zone].type, ib, etat);

                p.setFont(fnt(15, true));
                p.setPen(QColor("#e9edf5"));
                p.drawText(QRectF(r.left() + 60, cy - 25, r.width() - 72, 22),
                           Qt::AlignLeft | Qt::AlignVCenter, sl.nom);

                const double larg = r.width() - 60 - 78;
                QFontMetrics fm(fnt(12, false));
                p.setFont(fnt(12, false));
                p.setPen(sousCol);
                p.drawText(QRectF(r.left() + 60, cy + 2, larg, 18),
                           Qt::AlignLeft | Qt::AlignVCenter, fm.elidedText(sousTxt, Qt::ElideRight, int(larg)));
                pastille(p, QRectF(r.right() - 68, cy + 3, 56, 18), etatTxt, etat, fnt(9, true, 0.6));
            }

            // pastille "RECOMMANDE" avec etoile
            if (i == m_sugg && !pris && m_pick != i) {
                const QRectF pill(r.left() + 10, r.top() - 11, 108, 20);
                p.setPen(Qt::NoPen);
                p.setBrush(QColor("#ffc247"));
                p.drawRoundedRect(pill, 10, 10);
                QPolygonF etoile;
                const QPointF ce(pill.left() + 13, pill.center().y());
                for (int k = 0; k < 10; ++k) {
                    const double ang = -3.14159265 / 2.0 + k * 3.14159265 / 5.0;
                    const double rad = (k % 2 == 0) ? 5.6 : 2.4;
                    etoile << QPointF(ce.x() + std::cos(ang) * rad, ce.y() + std::sin(ang) * rad);
                }
                p.setBrush(QColor("#3a2a05"));
                p.drawPolygon(etoile);
                p.setFont(fnt(9, true, 0.8));
                p.setPen(QColor("#3a2a05"));
                p.drawText(pill.adjusted(24, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, "RECOMMANDÉ");
            }

            // validation : onde + confettis + badge
            if (m_pick == i) {
                const double u = m_pickT;
                const double ue = easeOutCubic(u);
                const QPointF c0 = r.center();
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(QColor(etat.red(), etat.green(), etat.blue(), int(230 * (1.0 - u))), 4));
                const double g = 4 + 60 * ue;
                p.drawRoundedRect(r.adjusted(-g, -g, g, g), 14 + g, 14 + g);

                p.setPen(Qt::NoPen);
                for (int k = 0; k < 28; ++k) {
                    const double ang = k * 0.2244 + std::sin(k * 7.1) * 0.3;
                    const double v = 70.0 + 90.0 * (0.5 + 0.5 * std::sin(k * 3.7));
                    const QPointF q = c0 + QPointF(std::cos(ang) * v * ue, std::sin(ang) * v * ue + 60.0 * u * u);
                    QColor cc = (k % 3 == 0) ? QColor("#2ee59d") : ((k % 3 == 1) ? QColor("#ffc247") : QColor("#ffffff"));
                    cc.setAlpha(int(255 * (1.0 - u)));
                    p.setBrush(cc);
                    const double rr = 3.6 * (1.0 - 0.5 * u);
                    p.drawEllipse(q, rr, rr);
                }

                const double b = qMin(1.0, u * 2.4);
                p.setBrush(QColor(46, 229, 157, int(245 * b)));
                p.drawEllipse(c0, 24.0 * b, 24.0 * b);
                if (b > 0.55) {
                    p.setPen(QPen(QColor("#0b0e13"), 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                    p.setBrush(Qt::NoBrush);
                    QPolygonF coche;
                    coche << QPointF(c0.x() - 9, c0.y()) << QPointF(c0.x() - 3, c0.y() + 7) << QPointF(c0.x() + 10, c0.y() - 7);
                    p.drawPolyline(coche);
                }
            }
            p.restore();
        }

        dessinerRoute(p);      // itineraire anime depuis l'entree vers la blasa survolee
        dessinerSortie(p);     // camions qui partent en intervention
        dessinerBraises(p);    // braises qui montent
        dessinerPluie(p);      // pluie (optionnelle)
        p.restore();

        dessinerSurcouches(p, s, ox, oy, fnt);
    }

private:
    // pastille d'etat (texte colore sur fond teinte)
    static void pastille(QPainter &p, const QRectF &r, const QString &txt, const QColor &c, const QFont &f) {
        p.save();
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(c.red(), c.green(), c.blue(), 38));
        p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
        p.setFont(f);
        p.setPen(c);
        p.drawText(r, Qt::AlignCenter, txt);
        p.restore();
    }

    // icones vectorielles : 0 garage, 1 voiture, 2 caisse, 3 bureau, 4 bouclier
    static void icone(QPainter &p, int type, const QRectF &box, const QColor &c) {
        p.save();
        p.setPen(QPen(c, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        const double w0 = box.width(), h0 = box.height();
        const QRectF q = box.adjusted(w0 * 0.24, h0 * 0.24, -w0 * 0.24, -h0 * 0.24);
        const double w = q.width(), h = q.height(), cx = q.center().x();
        if (type == 0) {
            QPolygonF toit;
            toit << QPointF(q.left(), q.top() + h * 0.42) << QPointF(cx, q.top()) << QPointF(q.right(), q.top() + h * 0.42);
            p.drawPolyline(toit);
            p.drawRect(QRectF(q.left() + w * 0.1, q.top() + h * 0.42, w * 0.8, h * 0.58));
            p.drawLine(QPointF(q.left() + w * 0.28, q.top() + h * 0.7), QPointF(q.right() - w * 0.28, q.top() + h * 0.7));
            p.drawLine(QPointF(q.left() + w * 0.28, q.top() + h * 0.86), QPointF(q.right() - w * 0.28, q.top() + h * 0.86));
        } else if (type == 1) {
            QPolygonF cab;
            cab << QPointF(q.left() + w * 0.1, q.top() + h * 0.5) << QPointF(q.left() + w * 0.3, q.top() + h * 0.1)
                << QPointF(q.right() - w * 0.3, q.top() + h * 0.1) << QPointF(q.right() - w * 0.1, q.top() + h * 0.5);
            p.drawPolyline(cab);
            p.drawRoundedRect(QRectF(q.left(), q.top() + h * 0.5, w, h * 0.28), 2, 2);
            p.drawEllipse(QPointF(q.left() + w * 0.26, q.bottom() - h * 0.06), w * 0.1, w * 0.1);
            p.drawEllipse(QPointF(q.right() - w * 0.26, q.bottom() - h * 0.06), w * 0.1, w * 0.1);
        } else if (type == 2) {
            p.drawRect(q);
            p.drawLine(QPointF(q.left(), q.top() + h * 0.34), QPointF(q.right(), q.top() + h * 0.34));
            p.drawLine(QPointF(cx, q.top()), QPointF(cx, q.top() + h * 0.34));
        } else if (type == 3) {
            p.drawRoundedRect(QRectF(q.left(), q.top(), w, h * 0.65), 2, 2);
            p.drawLine(QPointF(cx, q.top() + h * 0.65), QPointF(cx, q.bottom() - h * 0.04));
            p.drawLine(QPointF(cx - w * 0.22, q.bottom() - h * 0.04), QPointF(cx + w * 0.22, q.bottom() - h * 0.04));
        } else {
            QPolygonF b;
            b << QPointF(cx, q.top()) << QPointF(q.right(), q.top() + h * 0.18) << QPointF(q.right(), q.top() + h * 0.55)
              << QPointF(cx, q.bottom()) << QPointF(q.left(), q.top() + h * 0.55) << QPointF(q.left(), q.top() + h * 0.18);
            p.drawPolygon(b);
            QPolygonF v;
            v << QPointF(cx - w * 0.2, q.top() + h * 0.5) << QPointF(cx - w * 0.04, q.top() + h * 0.65) << QPointF(cx + w * 0.22, q.top() + h * 0.34);
            p.drawPolyline(v);
        }
        p.restore();
    }

    // petit camion de pompiers vectoriel (plein = equipement present, pointille = place libre)
    static void dessinerCamion(QPainter &p, const QPointF &o, const QColor &c, bool plein, double phase) {
        p.save();
        p.translate(o);
        if (plein) {
            p.setPen(QPen(c.lighter(125), 1.5));
            p.setBrush(QColor(c.red(), c.green(), c.blue(), 190));
        } else {
            p.setPen(QPen(QColor(c.red(), c.green(), c.blue(), 150), 1.6, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
        }
        p.drawRoundedRect(QRectF(0, 10, 40, 24), 4, 4);
        QPolygonF cab;
        cab << QPointF(40, 16) << QPointF(52, 16) << QPointF(60, 26) << QPointF(60, 34) << QPointF(40, 34);
        p.drawPolygon(cab);

        p.setPen(QPen(plein ? QColor(255, 255, 255, 220) : QColor(c.red(), c.green(), c.blue(), 150), 1.5));
        p.drawLine(QPointF(3, 5), QPointF(37, 5));
        p.drawLine(QPointF(3, 8), QPointF(37, 8));
        for (int x = 6; x <= 34; x += 7)
            p.drawLine(QPointF(x, 5), QPointF(x, 8));

        if (plein) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, 200));
            QPolygonF vitre;
            vitre << QPointF(44, 19) << QPointF(51, 19) << QPointF(56, 26) << QPointF(44, 26);
            p.drawPolygon(vitre);
        }

        p.setPen(QPen(QColor("#0b0e13"), 1.5));
        p.setBrush(QColor("#9aa4b8"));
        p.drawEllipse(QPointF(12, 35), 5.5, 5.5);
        p.drawEllipse(QPointF(48, 35), 5.5, 5.5);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#0b0e13"));
        p.drawEllipse(QPointF(12, 35), 2, 2);
        p.drawEllipse(QPointF(48, 35), 2, 2);

        if (plein) {                                  // gyrophares : rouge / bleu en alternance
            const bool rouge = std::sin(phase * 9.0) > 0.0;
            const QPointF bp = rouge ? QPointF(44, 12) : QPointF(51, 12);
            const QColor lc = rouge ? QColor("#ff3b4e") : QColor("#3d9bff");
            QRadialGradient halo(bp, 17);
            halo.setColorAt(0, QColor(lc.red(), lc.green(), lc.blue(), 220));
            halo.setColorAt(1, QColor(lc.red(), lc.green(), lc.blue(), 0));
            p.setPen(Qt::NoPen);
            p.setBrush(halo);
            p.drawEllipse(bp, 17, 17);
            p.setBrush(rouge ? QColor("#ff3b4e") : QColor("#5a1f26"));
            p.drawEllipse(QPointF(44, 12), 2.6, 2.6);
            p.setBrush(rouge ? QColor("#1f3150") : QColor("#3d9bff"));
            p.drawEllipse(QPointF(51, 12), 2.6, 2.6);
        }
        p.restore();
    }

    // indicateur "en direct", boussole, echelle et mini-carte (par-dessus la carte)
    template <typename F>
    void dessinerSurcouches(QPainter &p, double s, double ox, double oy, F fnt) {
        // pluie : ambiance sombre + eclairs
        if (m_pluieA > 0.05) {
            p.fillRect(rect(), QColor(8, 16, 40, int(55 * m_pluieA)));
            const double f = std::fmod(m_phase, 9.0);
            double eclair = 0.0;
            if (f < 0.12) eclair = 1.0 - f / 0.12;
            else if (f > 0.35 && f < 0.45) eclair = 0.6 * (1.0 - (f - 0.35) / 0.10);
            if (eclair > 0.0)
                p.fillRect(rect(), QColor(220, 230, 255, int(80 * eclair * m_pluieA)));
        }

        // alerte : stroboscope rouge / bleu + bandeau
        if (m_alerte) {
            const double t = m_alerteT;
            const double fondu = qMax(0.0, qMin(1.0, qMin(t / 0.4, (7.4 - t) / 0.8)));
            const double pls = std::sin(t * 7.0);
            const QColor sc = pls > 0.0 ? QColor(255, 40, 60) : QColor(50, 140, 255);
            const double force = 0.25 + 0.22 * std::fabs(pls);
            QRadialGradient vg(QPointF(width() / 2.0, height() / 2.0), qMax(width(), height()) * 0.75);
            vg.setColorAt(0.5, QColor(sc.red(), sc.green(), sc.blue(), 0));
            vg.setColorAt(1.0, QColor(sc.red(), sc.green(), sc.blue(), int(255 * force * fondu)));
            p.setPen(Qt::NoPen);
            p.setBrush(vg);
            p.drawRect(rect());

            const double bw = 480, bh = 50;
            double by = -60.0 + 120.0 * easeOutCubic(qMin(1.0, t / 0.5));
            if (t > 6.8) by -= (t - 6.8) / 0.6 * 130.0;
            const QRectF bandeau((width() - bw) / 2.0, by, bw, bh);
            QLinearGradient bg(bandeau.topLeft(), bandeau.topRight());
            bg.setColorAt(0, QColor("#ff2a44"));
            bg.setColorAt(1, QColor("#a00018"));
            p.setPen(QPen(QColor(255, 255, 255, 120), 1.5));
            p.setBrush(bg);
            p.drawRoundedRect(bandeau, 12, 12);
            // triangle d'avertissement
            const QPointF tc(bandeau.left() + 34, bandeau.center().y());
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, 235));
            QPolygonF tri;
            tri << QPointF(tc.x(), tc.y() - 14) << QPointF(tc.x() - 14, tc.y() + 11) << QPointF(tc.x() + 14, tc.y() + 11);
            p.drawPolygon(tri);
            p.setPen(QColor("#c40000"));
            p.setFont(fnt(15, true));
            p.drawText(QRectF(tc.x() - 14, tc.y() - 8, 28, 22), Qt::AlignCenter, "!");
            p.setFont(fnt(15, true, 1.6));
            p.setPen(Qt::white);
            p.drawText(bandeau.adjusted(60, 0, -16, 0), Qt::AlignLeft | Qt::AlignVCenter, "ALERTE : DÉPART EN INTERVENTION");
        }

        // projecteur doux qui suit la souris
        if (m_dansCarte) {
            QRadialGradient lum(m_souris, 240);
            lum.setColorAt(0, QColor(255, 255, 255, 16));
            lum.setColorAt(1, QColor(255, 255, 255, 0));
            p.setPen(Qt::NoPen);
            p.setBrush(lum);
            p.drawEllipse(m_souris, 240, 240);
        }

        // en direct
        const double pulse = 0.5 + 0.5 * std::sin(m_phase * 3.0);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(46, 229, 157, int(90 + 150 * pulse)));
        p.drawEllipse(QPointF(22, 22), 4.5, 4.5);
        p.setFont(fnt(10, true, 1.4));
        p.setPen(QColor("#8b95a9"));
        p.drawText(QRectF(34, 14, 220, 16), Qt::AlignLeft | Qt::AlignVCenter,
                   "EN DIRECT  •  " + QTime::currentTime().toString("HH:mm"));

        // boussole
        const QPointF cb(width() - 34, 34);
        p.setPen(QPen(QColor("#2f384b"), 1.5));
        p.setBrush(QColor(21, 26, 35, 230));
        p.drawEllipse(cb, 19, 19);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#ff5468"));
        QPolygonF nord;
        nord << QPointF(cb.x(), cb.y() - 14) << QPointF(cb.x() - 5, cb.y()) << QPointF(cb.x() + 5, cb.y());
        p.drawPolygon(nord);
        p.setBrush(QColor("#4a5468"));
        QPolygonF sud;
        sud << QPointF(cb.x(), cb.y() + 14) << QPointF(cb.x() - 5, cb.y()) << QPointF(cb.x() + 5, cb.y());
        p.drawPolygon(sud);

        // echelle (1 unite du plan = environ 0,12 m)
        const double pxParMetre = s / 0.12;
        const int paliers[] = {5, 10, 20, 50, 100};
        int metres = 100;
        for (int m : paliers) {
            if (m * pxParMetre >= 60) { metres = m; break; }
        }
        const double longueur = metres * pxParMetre;
        const QPointF ec(width() - 20 - longueur, height() - 18);
        p.setPen(QPen(QColor("#6b7690"), 1.6));
        p.drawLine(ec, ec + QPointF(longueur, 0));
        p.drawLine(ec + QPointF(0, -4), ec + QPointF(0, 4));
        p.drawLine(ec + QPointF(longueur, -4), ec + QPointF(longueur, 4));
        p.setFont(fnt(10, true));
        p.setPen(QColor("#8b95a9"));
        p.drawText(QRectF(ec.x(), ec.y() - 18, longueur, 14), Qt::AlignCenter, QString("%1 m").arg(metres));

        // mini-carte : seulement quand on a zoome
        if (m_zoom > 1.08) {
            const double mw = 180, mh = mw * 720.0 / 1080.0;
            const QRectF mr(16, height() - mh - 16, mw, mh);
            const double k = mw / 1080.0;
            p.save();
            QPainterPath coupe;
            coupe.addRoundedRect(mr, 10, 10);
            p.setClipPath(coupe);
            p.fillRect(mr, QColor("#0b0e13"));
            p.save();
            p.translate(mr.topLeft());
            p.scale(k, k);
            p.translate(40, 40);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#121720"));
            p.drawRect(QRectF(8, 8, 984, 624));
            p.setBrush(QColor("#1b212b"));
            p.drawRect(QRectF(12, 220, 976, 20));
            p.drawRect(QRectF(12, 446, 976, 20));
            p.drawRect(QRectF(488, 240, 24, 430));
            for (const Zone &z : m_zones) {
                p.setPen(QPen(z.accent, 4));
                p.setBrush(QColor("#171c26"));
                p.drawRoundedRect(z.r, 14, 14);
            }
            p.setPen(Qt::NoPen);
            for (const Slot &sl : m_slots) {
                QColor col("#2ee59d");
                if (estPris(sl.nom)) col = QColor("#ff5468");
                if (estActuel(sl.nom)) col = QColor("#4da3ff");
                p.setBrush(col);
                p.drawEllipse(sl.r.center(), 17, 17);
            }
            p.restore();
            const QRectF vue(mr.left() + (0 - ox) / s * k, mr.top() + (0 - oy) / s * k,
                             width() / s * k, height() / s * k);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor("#ff5468"), 1.8));
            p.drawRect(vue.intersected(mr).adjusted(1, 1, -1, -1));
            p.restore();
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor("#2f384b"), 1.6));
            p.drawRoundedRect(mr, 10, 10);
        }
    }

    static double easeOutCubic(double t) { return 1.0 - std::pow(1.0 - t, 3.0); }

    // legere perspective 3D : la carte se penche vers la souris + effet d'ouverture
    QTransform inclinaison() const {
        const double e = easeOutCubic(qMin(1.0, m_intro * 1.3));
        const double k = 0.86 + 0.14 * e;
        const QPointF c(width() / 2.0, height() / 2.0);
        QTransform t;
        t.translate(c.x(), c.y());
        t.scale(k, k);
        t.rotate(m_tiltX + (1.0 - e) * 18.0, Qt::XAxis);
        t.rotate(m_tiltY, Qt::YAxis);
        t.translate(-c.x(), -c.y());
        return t;
    }

    QTransform matriceCarte() const {
        double s, ox, oy;
        calcul(s, ox, oy);
        QTransform a;
        a.translate(ox, oy);
        a.scale(s, s);
        return a * inclinaison();
    }

    // trajet depuis l'entree jusqu'a l'emplacement, en suivant les routes du site
    QPainterPath routeVers(int idx) const {
        const QRectF &r = m_slots[idx].r;
        const int z = m_slots[idx].zone;
        const double cx = r.center().x();
        QPainterPath pa;
        pa.moveTo(500, 662);
        if (z == 0) {
            pa.lineTo(500, 230);
            pa.lineTo(cx, 230);
            pa.lineTo(cx, r.bottom());
        } else if (z == 1 || z == 2) {
            pa.lineTo(500, 359);
            pa.lineTo(cx, 359);
            pa.lineTo(cx, r.top() < 359 ? r.bottom() : r.top());
        } else {
            pa.lineTo(500, 456);
            pa.lineTo(cx, 456);
            pa.lineTo(cx, r.top());
        }
        return pa;
    }

    // itineraire anime : trait pointille qui avance + comete lumineuse + distance
    void dessinerRoute(QPainter &p) const {
        if (m_routeIdx < 0 || m_routeA < 0.02) return;
        const QPainterPath full = routeVers(m_routeIdx);
        const double a = m_routeA;
        const double reveal = qMin(1.0, m_routeR * 1.4);

        QPainterPath part;
        const int N = 180;
        part.moveTo(full.pointAtPercent(0.0));
        for (int k = 1; k <= N; ++k)
            part.lineTo(full.pointAtPercent(double(k) / N * reveal));

        p.save();
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(46, 229, 157, int(46 * a)), 12, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(part);
        QPen tirets(QColor(46, 229, 157, int(235 * a)), 3.4, Qt::CustomDashLine, Qt::RoundCap, Qt::RoundJoin);
        tirets.setDashPattern({1.2, 2.6});
        tirets.setDashOffset(-m_phase * 9.0);
        p.setPen(tirets);
        p.drawPath(part);

        // comete qui parcourt le trajet
        const double u = std::fmod(m_phase * 0.5, 1.0) * reveal;
        p.setPen(Qt::NoPen);
        for (int k = 9; k >= 0; --k) {
            const double uu = u - k * 0.012;
            if (uu < 0.0) continue;
            const QPointF q = full.pointAtPercent(uu);
            p.setBrush(QColor(255, 255, 255, int(235 * (1.0 - k / 10.0) * a)));
            const double rr = 6.0 - k * 0.45;
            p.drawEllipse(q, rr, rr);
        }
        const QPointF tete = full.pointAtPercent(u);
        QRadialGradient halo(tete, 20);
        halo.setColorAt(0, QColor(46, 229, 157, int(210 * a)));
        halo.setColorAt(1, QColor(46, 229, 157, 0));
        p.setBrush(halo);
        p.drawEllipse(tete, 20, 20);
        {   // petit camion qui roule vers l'emplacement
            const double ang = full.angleAtPercent(u);
            p.save();
            p.translate(tete);
            p.rotate(-ang);
            if (ang > 90.0 && ang < 270.0) p.scale(1, -1);
            p.scale(0.75, 0.75);
            QLinearGradient phare(60, 0, 150, 0);
            phare.setColorAt(0, QColor(255, 255, 220, int(120 * a)));
            phare.setColorAt(1, QColor(255, 255, 220, 0));
            p.setPen(Qt::NoPen);
            p.setBrush(phare);
            QPolygonF faisceau;
            faisceau << QPointF(58, 4) << QPointF(150, -22) << QPointF(150, 30) << QPointF(58, 12);
            p.drawPolygon(faisceau);
            p.setOpacity(a);
            dessinerCamion(p, QPointF(-30, -21), QColor("#2ee59d"), true, m_phase);
            p.restore();
        }

        // depart (entree) et arrivee
        const double pulse = std::fmod(m_phase * 1.2, 1.0);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 84, 104, int(200 * (1.0 - pulse) * a)), 2));
        p.drawEllipse(QPointF(500, 662), 6 + 14 * pulse, 6 + 14 * pulse);
        if (reveal > 0.97) {
            const QPointF fin = full.pointAtPercent(1.0);
            p.setPen(QPen(QColor(46, 229, 157, int(220 * (1.0 - pulse) * a)), 2.4));
            p.drawEllipse(fin, 8 + 14 * pulse, 8 + 14 * pulse);

            QFont f = font();
            f.setPixelSize(11);
            f.setBold(true);
            p.setFont(f);
            const QRectF pill(fin.x() + 14, fin.y() - 34, 74, 20);
            p.setPen(QPen(QColor("#2ee59d"), 1.2));
            p.setBrush(QColor(11, 14, 19, 235));
            p.drawRoundedRect(pill, 10, 10);
            p.setPen(QColor("#2ee59d"));
            p.drawText(pill, Qt::AlignCenter, QString("≈ %1 m").arg(int(full.length() * 0.12 + 0.5)));
        }
        p.restore();
    }

    static double hash01(int n) {
        const double v = std::sin(n * 12.9898) * 43758.5453;
        return v - std::floor(v);
    }

    // avancement (0..1) du camion i pendant l'alerte : les camions partent l'un apres l'autre
    double alerteU(int i) const {
        return qBound(0.0, (m_alerteT - (0.8 + i * 0.9)) / 3.4, 1.0);
    }

    // trajet de sortie : du garage vers la route, puis vers l'entree et dehors
    QPainterPath cheminSortie(int i) const {
        const QRectF &r = m_slots[i].r;
        const double cx = r.center().x();
        QPainterPath pa;
        pa.moveTo(cx, r.top() + 114);
        pa.lineTo(cx, 230);
        pa.lineTo(500, 230);
        pa.lineTo(500, 730);
        return pa;
    }

    void dessinerSortie(QPainter &p) const {
        if (!m_alerte) return;
        for (int i = 0; i < 4 && i < m_slots.size(); ++i) {
            const double u0 = alerteU(i);
            if (u0 <= 0.0 || u0 >= 1.0) continue;
            const double u = std::pow(u0, 1.5);
            const QPainterPath chemin = cheminSortie(i);
            const bool rouge = std::sin(m_alerteT * 9.0 + i) > 0.0;
            const QColor lueur = rouge ? QColor(255, 50, 70) : QColor(60, 150, 255);

            // trainee lumineuse
            p.setPen(Qt::NoPen);
            for (int k = 8; k >= 1; --k) {
                const double uu = u - k * 0.014;
                if (uu < 0.0) continue;
                p.setBrush(QColor(lueur.red(), lueur.green(), lueur.blue(), int(110 * (1.0 - k / 9.0))));
                const QPointF q = chemin.pointAtPercent(uu);
                p.drawEllipse(q, 12 - k * 0.8, 12 - k * 0.8);
            }

            const QPointF pt = chemin.pointAtPercent(u);
            const double ang = chemin.angleAtPercent(u);
            p.save();
            p.translate(pt);
            p.rotate(-ang);
            if (ang > 90.0 && ang < 270.0) p.scale(1, -1);

            QRadialGradient sol(QPointF(0, 0), 80);                  // lueur sur le sol
            sol.setColorAt(0, QColor(lueur.red(), lueur.green(), lueur.blue(), 140));
            sol.setColorAt(1, QColor(lueur.red(), lueur.green(), lueur.blue(), 0));
            p.setBrush(sol);
            p.drawEllipse(QPointF(0, 0), 80, 80);

            QLinearGradient phare(60, 0, 190, 0);                    // faisceau des phares
            phare.setColorAt(0, QColor(255, 255, 225, 150));
            phare.setColorAt(1, QColor(255, 255, 225, 0));
            p.setBrush(phare);
            QPolygonF faisceau;
            faisceau << QPointF(58, 4) << QPointF(190, -34) << QPointF(190, 42) << QPointF(58, 14);
            p.drawPolygon(faisceau);

            dessinerCamion(p, QPointF(-30, -21), QColor("#ff5468"), true, m_phase * 1.8);
            p.restore();
        }
    }

    // braises qui montent doucement (sans etat : tout est calcule a partir du temps)
    void dessinerBraises(QPainter &p) const {
        const double force = m_alerte ? 1.0 : 0.55;
        p.setPen(Qt::NoPen);
        for (int k = 0; k < 44; ++k) {
            const double T = 5.0 + 4.0 * hash01(k);
            const double u = std::fmod(m_phase / T + hash01(k + 11), 1.0);
            const double x = 24 + hash01(k + 3) * 952 + std::sin(m_phase * 0.8 + k) * 18;
            const double y = 626 - u * 650;
            const double al = std::sin(u * 3.14159265) * force;
            const double sz = 1.3 + 2.0 * hash01(k + 5);
            const double m = hash01(k + 9);
            p.setBrush(QColor(255, int(120 + 80 * m), 50, int(38 * al)));
            p.drawEllipse(QPointF(x, y), sz * 3.2, sz * 3.2);
            p.setBrush(QColor(255, int(190 + 50 * m), 110, int(220 * al)));
            p.drawEllipse(QPointF(x, y), sz, sz);
        }
    }

    // lampadaires : halo chaud qui vacille (plus fort sous la pluie)
    void dessinerLampes(QPainter &p) const {
        p.setPen(Qt::NoPen);
        for (int k = 0; k < m_lampes.size(); ++k) {
            const QPointF &l = m_lampes[k];
            const double f = 0.85 + 0.15 * std::sin(m_phase * 2.3 + k * 1.7);
            const double fort = 1.0 + 0.7 * m_pluieA;
            QRadialGradient g(l, 95);
            g.setColorAt(0, QColor(255, 205, 120, int(60 * f * fort)));
            g.setColorAt(1, QColor(255, 205, 120, 0));
            p.setBrush(g);
            p.drawEllipse(l, 95, 95);
            p.setBrush(QColor(255, 236, 190, int(230 * f)));
            p.drawEllipse(l, 2.6, 2.6);
        }
    }

    // pluie : gouttes obliques + ondes sur les routes (tout est calcule a partir du temps)
    void dessinerPluie(QPainter &p) const {
        if (m_pluieA < 0.02) return;
        p.save();
        QPainterPath coupe;
        coupe.addRoundedRect(QRectF(8, 8, 984, 624), 22, 22);
        p.setClipPath(coupe);

        p.setPen(QPen(QColor(170, 205, 255, int(100 * m_pluieA)), 1.4, Qt::SolidLine, Qt::RoundCap));
        for (int k = 0; k < 110; ++k) {
            const double v = 0.9 + 0.8 * hash01(k + 2);
            const double u = std::fmod(m_phase * v * 1.3 + hash01(k), 1.0);
            const double x = hash01(k + 1) * 1100 - 50 - u * 70;
            const double y = -30 + u * 700;
            p.drawLine(QPointF(x, y), QPointF(x - 5, y + 16 * v));
        }

        p.setBrush(Qt::NoBrush);
        for (int k = 0; k < 30; ++k) {
            const double life = std::fmod(m_phase * 0.9 + hash01(k + 40), 1.0);
            QPointF c;
            const int route = k % 3;
            if (route == 0)      c = QPointF(20 + hash01(k + 5) * 960, 230);
            else if (route == 1) c = QPointF(20 + hash01(k + 6) * 960, 456);
            else                 c = QPointF(500, 250 + hash01(k + 7) * 400);
            const double rx = 2 + 15 * life;
            p.setPen(QPen(QColor(190, 220, 255, int(90 * (1.0 - life) * m_pluieA)), 1.2));
            p.drawEllipse(c, rx, rx * 0.42);
        }
        p.restore();
    }

    bool visible(const Slot &s) const {
        if (m_filtre == 1 && estPris(s.nom)) return false;      // seulement les libres
        if (m_filtre == 2 && !estPris(s.nom)) return false;     // seulement les occupes
        if (!m_recherche.isEmpty() && !correspond(s)) return false;
        return true;
    }

    bool correspond(const Slot &s) const {
        if (m_recherche.isEmpty()) return false;
        return s.nom.contains(m_recherche, Qt::CaseInsensitive)
               || m_occ.value(s.nom).contains(m_recherche, Qt::CaseInsensitive);
    }

    void tick() {
        if (m_intro < 1.0) m_intro = qMin(1.0, m_intro + 0.016 / 1.7);
        m_phase += 0.016;
        if (m_phase > 1000.0) m_phase = 0.0;
        for (int i = 0; i < m_hv.size(); ++i) {
            const double cible = (i == m_hover && !estPris(m_slots[i].nom)) ? 1.0 : 0.0;
            m_hv[i] += (cible - m_hv[i]) * 0.22;
        }
        if (m_alerte) {
            m_alerteT += 0.016;
            if (m_alerteT > 7.4) m_alerte = false;
        }
        int cibleRoute = -1;
        double alphaRoute = 0.0;
        if (m_hover >= 0 && m_pick < 0 && !estPris(m_slots[m_hover].nom)) {
            cibleRoute = m_hover;
            alphaRoute = 1.0;
        } else if (m_sugg >= 0 && m_pick < 0 && !m_alerte) {
            cibleRoute = m_sugg;                      // sans survol : on montre le trajet recommande
            alphaRoute = 0.6;
        }
        if (cibleRoute >= 0) m_routeIdx = cibleRoute;
        m_routeA += (alphaRoute - m_routeA) * 0.10;
        m_routeR += ((cibleRoute >= 0 ? 1.0 : 0.0) - m_routeR) * 0.10;
        m_pluieA += ((m_pluie ? 1.0 : 0.0) - m_pluieA) * 0.04;
        m_tiltX += (m_tiltTx - m_tiltX) * 0.08;
        m_tiltY += (m_tiltTy - m_tiltY) * 0.08;
        if (m_pick >= 0) {
            m_pickT += 0.016 / 0.8;
            if (m_pickT >= 1.0) {
                const int i = m_pick;
                m_pick = -1;
                m_pickT = 0.0;
                if (surChoix) surChoix(m_slots[i].nom);
                return;
            }
        }
        update();
    }

    double echelleBase() const { return qMin(width() / 1080.0, height() / 720.0); }

    void calcul(double &s, double &ox, double &oy) const {
        s = echelleBase() * m_zoom;
        ox = (width() - 1080.0 * s) / 2.0 + m_pan.x();
        oy = (height() - 720.0 * s) / 2.0 + m_pan.y();
    }

    // zoom centre sur un point de l'ecran (le point reste fixe sous la souris)
    void zoomAutour(double nz, const QPointF &ancre) {
        nz = qBound(0.8, nz, 3.5);
        double s, ox, oy;
        calcul(s, ox, oy);
        const QPointF c((ancre.x() - ox) / s, (ancre.y() - oy) / s);
        m_zoom = nz;
        const double sn = echelleBase() * m_zoom;
        m_pan.setX(ancre.x() - c.x() * sn - (width() - 1080.0 * sn) / 2.0);
        m_pan.setY(ancre.y() - c.y() * sn - (height() - 720.0 * sn) / 2.0);
    }

    int slotAt(const QPointF &pos) const {
        bool ok = false;
        const QTransform inv = matriceCarte().inverted(&ok);
        if (!ok) return -1;
        const QPointF l = inv.map(pos) - QPointF(40.0, 40.0);
        for (int i = 0; i < m_slots.size(); ++i)
            if (m_slots[i].r.contains(l)) return i;
        return -1;
    }

    QMap<QString, QString> m_occ;
    QString m_actuel;
    QList<Zone> m_zones;
    QList<Slot> m_slots;
    QList<double> m_hv;
    QPointF m_pan = QPointF(0, 0);
    QPointF m_pressPos;
    QPointF m_lastPos;
    int m_pressSlot = -1;
    bool m_drag = false;
    double m_zoom = 1.0;
    int m_hover = -1;
    int m_pick = -1;
    int m_filtre = 0;
    QString m_recherche;
    double m_pickT = 0.0;
    double m_intro = 0.0;
    double m_phase = 0.0;
    double m_tiltX = 0.0, m_tiltY = 0.0, m_tiltTx = 0.0, m_tiltTy = 0.0;
    QPointF m_souris;
    bool m_dansCarte = false;
    int m_routeIdx = -1;
    double m_routeA = 0.0;
    bool m_alerte = false;
    double m_alerteT = 0.0;
    bool m_pluie = false;
    double m_pluieA = 0.0;
    int m_sugg = -1;
    double m_routeR = 0.0;
    QList<QPointF> m_lampes;
};
}

bool smart::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == localisation && ev->type() == QEvent::MouseButtonPress) {
        const QString choix = choisirEmplacement();
        if (!choix.isEmpty())
            localisation->setText(choix);
        return true;
    }
    return QMainWindow::eventFilter(obj, ev);
}

QString smart::choisirEmplacement()
{
    const int COL_NOM = 1;
    const int COL_LOC = 6;

    // emplacement -> nom de l'equipement qui l'occupe
    QMap<QString, QString> occupes;
    for (int r = 0; r < table->rowCount(); ++r) {
        QTableWidgetItem *itLoc = table->item(r, COL_LOC);
        QTableWidgetItem *itNom = table->item(r, COL_NOM);
        if (itLoc && itNom) {
            const QString loc = itLoc->text().trimmed();
            if (!loc.isEmpty())
                occupes.insert(loc, itNom->text());
        }
    }
    const QString actuel = localisation->text().trimmed();

    QDialog dlg(this);
    dlg.setWindowTitle("Plan des emplacements");
    dlg.resize(1260, 820);
    dlg.setStyleSheet("QDialog{background:#0b0e13;}"
                      "QLabel{border:none;background:transparent;}"
                      "QToolTip{background:#1a2030;color:#e9edf5;border:1px solid #2f384b;padding:6px 10px;}");

    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ----- bandeau -----
    QFrame *banner = new QFrame;
    banner->setObjectName("planBanner");
    banner->setFixedHeight(76);
    banner->setStyleSheet("QFrame#planBanner{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                          "stop:0 #b40012,stop:1 #3d0409);}");
    QVBoxLayout *bl = new QVBoxLayout(banner);
    bl->setContentsMargins(28, 10, 28, 10);
    bl->setSpacing(2);
    QLabel *bt = new QLabel("Plan interactif de la caserne");
    bt->setStyleSheet("color:white;font-size:21px;font-weight:bold;");
    QLabel *bs = new QLabel("Cliquez sur un emplacement libre pour l'attribuer   •   Molette : zoom   •   Glisser : déplacer");
    bs->setStyleSheet("color:#ffc9cf;font-size:12px;");
    bl->addWidget(bt);
    bl->addWidget(bs);
    root->addWidget(banner);

    {   // le titre s'ecrit lettre par lettre
        const QString plein = bt->text();
        bt->setMinimumHeight(bt->sizeHint().height());
        bt->setText(QString());
        QVariantAnimation *typo = new QVariantAnimation(bt);
        typo->setDuration(900);
        typo->setStartValue(0);
        typo->setEndValue(int(plein.size()));
        connect(typo, &QVariantAnimation::valueChanged, bt, [bt, plein](const QVariant &v) { bt->setText(plein.left(v.toInt())); });
        connect(typo, &QVariantAnimation::finished, bt, [bt, plein]() { bt->setText(plein); });
        typo->start(QAbstractAnimation::DeleteWhenStopped);
    }

    QWidget *corps = new QWidget;
    QHBoxLayout *ch = new QHBoxLayout(corps);
    ch->setContentsMargins(20, 16, 20, 16);
    ch->setSpacing(16);

    // ----- zone carte (gauche) : barre d'outils + plan -----
    QString choisi;
    PlanEmplacements *plan = new PlanEmplacements(occupes, actuel);

    QFrame *cartePlan = new QFrame;
    cartePlan->setObjectName("planCard");
    cartePlan->setStyleSheet("QFrame#planCard{background:#0e1218;border:1px solid #232a38;border-radius:16px;}");
    QVBoxLayout *cpl = new QVBoxLayout(cartePlan);
    cpl->setContentsMargins(10, 10, 10, 10);
    cpl->setSpacing(10);

    // barre d'outils : filtre, recherche, zoom
    const QString styleFiltre =
        "QPushButton{background:#1a2030;color:#aab4c8;border:1px solid #2b3242;border-radius:9px;"
        "padding:6px 14px;font-size:12px;font-weight:bold;}"
        "QPushButton:hover{background:#222b3d;color:white;}"
        "QPushButton:checked{background:#c40000;color:white;border:1px solid #ff5468;}";
    QPushButton *fTous = new QPushButton("Tous");
    QPushButton *fLibres = new QPushButton("Libres");
    QPushButton *fOcc = new QPushButton("Occupés");
    const QList<QPushButton *> filtres = {fTous, fLibres, fOcc};

    QHBoxLayout *barre = new QHBoxLayout;
    barre->setSpacing(8);
    for (int k = 0; k < filtres.size(); ++k) {
        QPushButton *b = filtres[k];
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(styleFiltre);
        barre->addWidget(b);
        connect(b, &QPushButton::clicked, &dlg, [filtres, plan, k]() {
            for (int j = 0; j < filtres.size(); ++j)
                filtres[j]->setChecked(j == k);
            plan->setFiltre(k);
        });
    }
    fTous->setChecked(true);

    QLineEdit *rech = new QLineEdit;
    rech->setPlaceholderText("Chercher un équipement ou un emplacement…");
    rech->setClearButtonEnabled(true);
    rech->setMinimumWidth(140);
    rech->setStyleSheet("QLineEdit{background:#1a2030;color:#e9edf5;border:1px solid #2b3242;border-radius:9px;"
                        "padding:7px 12px;font-size:12px;}"
                        "QLineEdit:focus{border:1px solid #ff5468;}");
    connect(rech, &QLineEdit::textChanged, plan, [plan](const QString &s) { plan->setRecherche(s); });
    barre->addSpacing(8);
    barre->addWidget(rech, 1);

    const QString styleZoom =
        "QPushButton{background:#1a2030;color:#e9edf5;border:1px solid #2b3242;border-radius:9px;"
        "font-size:16px;font-weight:bold;min-width:34px;max-width:34px;min-height:32px;max-height:32px;}"
        "QPushButton:hover{background:#c40000;border:1px solid #ff5468;}";
    QPushButton *bPlus = new QPushButton("+");
    QPushButton *bMoins = new QPushButton("−");
    QPushButton *bCentre = new QPushButton("⌖");
    bPlus->setToolTip("Zoom avant");
    bMoins->setToolTip("Zoom arrière");
    bCentre->setToolTip("Recentrer la carte");
    for (QPushButton *b : {bPlus, bMoins, bCentre}) {
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(styleZoom);
        barre->addWidget(b);
    }
    connect(bPlus, &QPushButton::clicked, plan, [plan]() { plan->zoomPlus(); });
    connect(bMoins, &QPushButton::clicked, plan, [plan]() { plan->zoomMoins(); });
    connect(bCentre, &QPushButton::clicked, plan, [plan]() { plan->recentrer(); });

    QPushButton *bAlerte = new QPushButton("Simuler une sortie");
    bAlerte->setCursor(Qt::PointingHandCursor);
    bAlerte->setToolTip("Les camions partent en intervention (démonstration)");
    bAlerte->setStyleSheet("QPushButton{background:#c40000;color:white;border:1px solid #ff5468;border-radius:9px;"
                           "padding:7px 14px;font-size:12px;font-weight:bold;}"
                           "QPushButton:hover{background:#e0001a;}");
    barre->addWidget(bAlerte);
    connect(bAlerte, &QPushButton::clicked, plan, [plan]() { plan->lancerAlerte(); });

    QPushButton *bPluie = new QPushButton("Pluie");
    bPluie->setCheckable(true);
    bPluie->setCursor(Qt::PointingHandCursor);
    bPluie->setToolTip("Ambiance pluie, éclairs et lampadaires");
    bPluie->setStyleSheet(styleFiltre);
    barre->addWidget(bPluie);
    connect(bPluie, &QPushButton::toggled, plan, [plan](bool on) { plan->basculerPluie(on); });

    cpl->addLayout(barre);
    cpl->addWidget(plan, 1);
    ch->addWidget(cartePlan, 1);

    plan->surChoix = [&choisi, &dlg](const QString &nom) {
        choisi = nom;
        dlg.accept();
    };

    // ----- panneau lateral (droite) -----
    const int total = plan->nbTotal();
    const int nbOcc = plan->nbPris();
    const int nbLibres = total - nbOcc;
    const int pct = total ? nbOcc * 100 / total : 0;
    const QString couleurPct = pct < 50 ? "#2ee59d" : (pct < 80 ? "#ffb547" : "#ff5468");

    QFrame *cote = new QFrame;
    cote->setObjectName("planSide");
    cote->setFixedWidth(300);
    cote->setStyleSheet("QFrame#planSide{background:#151922;border:1px solid #262c3a;border-radius:16px;}");
    QVBoxLayout *cv = new QVBoxLayout(cote);
    cv->setContentsMargins(18, 16, 18, 16);
    cv->setSpacing(10);

    QLabel *t1 = new QLabel("TAUX D'OCCUPATION");
    t1->setStyleSheet("color:#8b95a9;font-size:10px;font-weight:bold;letter-spacing:2px;");
    cv->addWidget(t1);

    DonutOcc *donut = new DonutOcc;
    donut->setPct(pct, QColor(couleurPct));
    cv->addWidget(donut, 0, Qt::AlignHCenter);

    auto puce = [](const QString &fond, const QString &bord, const QString &texte) {
        QLabel *l = new QLabel(texte);
        l->setAlignment(Qt::AlignCenter);
        l->setStyleSheet(QString("background:%1;color:%2;border:1px solid %2;border-radius:11px;"
                                 "padding:4px 10px;font-size:11px;font-weight:bold;").arg(fond, bord));
        return l;
    };
    QHBoxLayout *chips = new QHBoxLayout;
    chips->setSpacing(8);
    QLabel *chipLibres = puce("rgba(46,229,157,0.12)", "#2ee59d", QString("0 libre(s)"));
    QLabel *chipOcc = puce("rgba(255,84,104,0.12)", "#ff5468", QString("0 occupé(s)"));
    chips->addWidget(chipLibres);
    chips->addWidget(chipOcc);
    auto compter = [&](QLabel *l, int cible, const QString &suffixe) {
        QVariantAnimation *a = new QVariantAnimation(l);
        a->setDuration(1000);
        a->setStartValue(0);
        a->setEndValue(cible);
        a->setEasingCurve(QEasingCurve::OutCubic);
        connect(a, &QVariantAnimation::valueChanged, l, [l, suffixe](const QVariant &v) { l->setText(QString::number(v.toInt()) + suffixe); });
        a->start(QAbstractAnimation::DeleteWhenStopped);
    };
    compter(chipLibres, nbLibres, " libre(s)");
    compter(chipOcc, nbOcc, " occupé(s)");
    cv->addLayout(chips);

    // carte "survol"
    QFrame *surv = new QFrame;
    surv->setObjectName("survCard");
    surv->setStyleSheet("QFrame#survCard{background:#1b212d;border:1px solid #2b3242;border-radius:12px;}");
    QVBoxLayout *sl = new QVBoxLayout(surv);
    sl->setContentsMargins(14, 12, 14, 12);
    sl->setSpacing(4);
    QLabel *survTitre = new QLabel("Survolez un emplacement");
    survTitre->setStyleSheet("color:#e9edf5;font-size:15px;font-weight:bold;");
    QLabel *survEtat = new QLabel("Passez la souris sur le plan pour voir les détails.");
    survEtat->setWordWrap(true);
    survEtat->setStyleSheet("color:#8b95a9;font-size:12px;");
    sl->addWidget(survTitre);
    sl->addWidget(survEtat);
    surv->setMinimumHeight(88);
    cv->addWidget(surv);

    // ----- suggestion intelligente selon le type d'equipement du formulaire -----
    int zSugg = -1;
    QString raison;
    const QString typeEq = type->currentText();
    if (typeEq == "Protection")         { zSugg = 2; raison = "Matériel de protection : stockage au Magasin"; }
    else if (typeEq == "Secours")       { zSugg = 0; raison = "Matériel de secours : au plus près des camions"; }
    else if (typeEq == "Communication") { zSugg = 3; raison = "Radios et communication : au Bureau"; }
    else if (typeEq == "Médical")       { zSugg = 4; raison = "Matériel médical : Salle de garde, accès rapide"; }
    else if (typeEq == "Outillage")     { zSugg = 1; raison = "Outillage : à portée des véhicules"; }
    plan->setSuggestionZone(zSugg);

    QFrame *suggCarte = new QFrame;
    suggCarte->setObjectName("suggCard");
    suggCarte->setStyleSheet("QFrame#suggCard{background:#221d10;border:1px solid #6b5420;border-radius:12px;}");
    QVBoxLayout *sgl = new QVBoxLayout(suggCarte);
    sgl->setContentsMargins(14, 12, 14, 12);
    sgl->setSpacing(6);
    QLabel *suggTitre = new QLabel("SUGGESTION INTELLIGENTE");
    suggTitre->setStyleSheet("color:#ffc247;font-size:10px;font-weight:bold;letter-spacing:2px;");
    QLabel *suggTexte = new QLabel;
    suggTexte->setTextFormat(Qt::RichText);
    suggTexte->setWordWrap(true);
    suggTexte->setStyleSheet("color:#f4e6bd;font-size:12px;");
    QPushButton *suggBtn = new QPushButton("Attribuer automatiquement");
    suggBtn->setCursor(Qt::PointingHandCursor);
    suggBtn->setStyleSheet("QPushButton{background:#ffc247;color:#3a2a05;border:none;border-radius:9px;"
                           "padding:8px 10px;font-weight:bold;font-size:12px;}"
                           "QPushButton:hover{background:#ffd37a;}"
                           "QPushButton:disabled{background:#3a3220;color:#7c6f4d;}");
    const QString nomSugg = plan->nomSuggestion();
    if (!nomSugg.isEmpty()) {
        suggTexte->setText(QString("<span style='font-size:15px;font-weight:bold;color:#ffd37a'>%1</span><br>"
                                   "<span style='color:#c9b77e'>%2</span>")
                               .arg(nomSugg.toHtmlEscaped(), raison.toHtmlEscaped()));
    } else if (zSugg >= 0) {
        suggTexte->setText("La zone recommandée est complète. Choisissez une autre zone.");
        suggBtn->setEnabled(false);
    } else {
        suggTexte->setText("Choisissez d'abord un type d'équipement dans le formulaire pour obtenir une suggestion.");
        suggBtn->setEnabled(false);
    }
    connect(suggBtn, &QPushButton::clicked, plan, [plan]() { plan->choisirSuggestion(); });
    sgl->addWidget(suggTitre);
    sgl->addWidget(suggTexte);
    sgl->addWidget(suggBtn);
    cv->addWidget(suggCarte);

    plan->surSurvol = [plan, survTitre, survEtat](const QString &nom) {
        if (nom.isEmpty()) {
            survTitre->setText("Survolez un emplacement");
            survTitre->setStyleSheet("color:#e9edf5;font-size:15px;font-weight:bold;");
            survEtat->setText("Passez la souris sur le plan pour voir les détails.");
            survEtat->setStyleSheet("color:#8b95a9;font-size:12px;");
            return;
        }
        survTitre->setText(nom);
        survTitre->setStyleSheet("color:#e9edf5;font-size:15px;font-weight:bold;");
        if (plan->estActuel(nom)) {
            survEtat->setText("Emplacement actuel de cet équipement. Cliquez pour le conserver.");
            survEtat->setStyleSheet("color:#4da3ff;font-size:12px;font-weight:bold;");
        } else if (plan->estPris(nom)) {
            survEtat->setText("Occupé par : " + plan->occupant(nom));
            survEtat->setStyleSheet("color:#ff5468;font-size:12px;font-weight:bold;");
        } else {
            survEtat->setText("Libre. Cliquez pour l'attribuer.");
            survEtat->setStyleSheet("color:#2ee59d;font-size:12px;font-weight:bold;");
        }
    };

    // occupation par zone
    QLabel *t2 = new QLabel("OCCUPATION PAR ZONE");
    t2->setStyleSheet("color:#8b95a9;font-size:10px;font-weight:bold;letter-spacing:2px;");
    cv->addWidget(t2);

    const QList<PlanEmplacements::StatZone> stats = plan->statsZones();
    for (const PlanEmplacements::StatZone &sz : stats) {
        const int occZone = sz.total ? (sz.total - sz.libres) * 100 / sz.total : 0;
        const QString col = occZone < 50 ? "#2ee59d" : (occZone < 100 ? "#ffb547" : "#ff5468");

        QHBoxLayout *ligne = new QHBoxLayout;
        ligne->setSpacing(8);
        QLabel *nomZ = new QLabel(sz.titre);
        nomZ->setFixedWidth(100);
        nomZ->setStyleSheet("color:#c3cbdb;font-size:11px;font-weight:bold;");
        QProgressBar *bz = new QProgressBar;
        bz->setRange(0, 100);
        bz->setValue(occZone);
        bz->setTextVisible(false);
        bz->setFixedHeight(6);
        bz->setStyleSheet(QString("QProgressBar{background:#262c3a;border:none;border-radius:3px;}"
                                  "QProgressBar::chunk{background:%1;border-radius:3px;}").arg(col));
        QLabel *cpt = new QLabel(QString("%1/%2").arg(sz.total - sz.libres).arg(sz.total));
        cpt->setFixedWidth(30);
        cpt->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        cpt->setStyleSheet(QString("color:%1;font-size:11px;font-weight:bold;").arg(col));
        ligne->addWidget(nomZ);
        ligne->addWidget(bz, 1);
        ligne->addWidget(cpt);
        cv->addLayout(ligne);
    }

    cv->addStretch(1);

    // legende
    QHBoxLayout *leg = new QHBoxLayout;
    leg->setSpacing(6);
    leg->addWidget(puce("rgba(46,229,157,0.12)", "#2ee59d", "● Libre"));
    leg->addWidget(puce("rgba(255,84,104,0.12)", "#ff5468", "● Occupé"));
    leg->addWidget(puce("rgba(77,163,255,0.12)", "#4da3ff", "● Actuel"));
    cv->addLayout(leg);

    QPushButton *annuler = new QPushButton("Annuler");
    annuler->setCursor(Qt::PointingHandCursor);
    annuler->setFixedHeight(40);
    annuler->setStyleSheet("QPushButton{background:transparent;color:#ff8a98;border:1px solid #ff5468;"
                           "border-radius:10px;font-weight:bold;}"
                           "QPushButton:hover{background:#c40000;color:white;}");
    connect(annuler, &QPushButton::clicked, &dlg, &QDialog::reject);
    cv->addWidget(annuler);

    ch->addWidget(cote);
    root->addWidget(corps, 1);

    dlg.exec();
    return choisi;
}