#include "logoanime.h"

#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QTimer>
#include <QtMath>
#include <algorithm>

namespace {
// Positions dans l'image d'origine (logo de 1254 x 1254 pixels)
const double REF      = 1254.0;
const QRect  FLAMME(545, 95, 165, 250);              // zone de la flamme
const QPointF GYRO_G(552, 545), GYRO_D(706, 545);    // gyrophares du camion
const double FIN_EMBLEME = 875, DEBUT_USPC = 878, DEBUT_SOUS = 1000, DEBUT_DESKTOP = 1095;

double borne(double v) { return std::clamp(v, 0.0, 1.0); }
double adoucir(double t) { t = borne(t); return 1 - (1 - t) * (1 - t) * (1 - t); }
}

LogoAnime::LogoAnime(const QString &cheminImage, bool intro, bool rond, QWidget *parent)
    : QWidget(parent), m_intro(intro), m_rond(rond)
{
    setAttribute(Qt::WA_TranslucentBackground);
    m_source = QImage(cheminImage).convertToFormat(QImage::Format_ARGB32);

    // Le fond du PNG est un blanc cassé (254,254,254) : on le rend blanc pur
    // pour qu'aucun carré ne se voie à l'intérieur du cercle blanc.
    for (int y = 0; y < m_source.height(); ++y) {
        auto *ligne = reinterpret_cast<QRgb *>(m_source.scanLine(y));
        for (int x = 0; x < m_source.width(); ++x) {
            const QRgb c = ligne[x];
            if (qRed(c) >= 245 && qGreen(c) >= 245 && qBlue(c) >= 245)
                ligne[x] = qRgba(255, 255, 255, qAlpha(c));
        }
    }

    // Découpe de la flamme avec des bords progressivement transparents (ellipse douce)
    if (!m_source.isNull()) {
        const double k = m_source.width() / REF;
        const QRect zone(qRound(FLAMME.x() * k), qRound(FLAMME.y() * k),
                         qRound(FLAMME.width() * k), qRound(FLAMME.height() * k));
        m_flamme = m_source.copy(zone);
        const double cx = m_flamme.width() / 2.0, cy = m_flamme.height() / 2.0;
        const double rx = m_flamme.width() * 0.38, ry = m_flamme.height() * 0.45;
        for (int y = 0; y < m_flamme.height(); ++y) {
            auto *ligne = reinterpret_cast<QRgb *>(m_flamme.scanLine(y));
            for (int x = 0; x < m_flamme.width(); ++x) {
                const double d = qSqrt(qPow((x - cx) / rx, 2) + qPow((y - cy) / ry, 2));
                const double a = borne((1.0 - d) / 0.25);          // 1 au centre, 0 au bord
                const QRgb c = ligne[x];
                ligne[x] = qRgba(qRed(c), qGreen(c), qBlue(c), int(qAlpha(c) * a));
            }
        }
    }

    m_horloge.start();
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
    m_timer->start(16);                                            // environ 60 images/s
}

void LogoAnime::paintEvent(QPaintEvent *)
{
    if (m_source.isNull()) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const int cote = std::min(width(), height());
    const QRectF zone((width() - cote) / 2.0, (height() - cote) / 2.0, cote, cote);

    // Cercle blanc (barre latérale) : le logo est dessiné à l'intérieur
    QRectF cible = zone;
    if (m_rond) {
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawEllipse(zone);
        QPainterPath cercle;
        cercle.addEllipse(zone);
        p.setClipPath(cercle);
        const double marge = cote * 0.15;   // espace blanc autour du logo
        cible = zone.adjusted(marge, marge, -marge, -marge);
    }

    // Mise à l'échelle de haute qualité, à la résolution réelle de l'écran
    const qreal dpr = devicePixelRatioF();
    const QSize taille(qRound(cible.width()), qRound(cible.height()));
    if (m_cache.isNull() || m_cacheTaille != taille || !qFuzzyCompare(m_cacheDpr, dpr)) {
        m_cache = QPixmap::fromImage(m_source.scaled(taille * dpr, Qt::KeepAspectRatio,
                                                     Qt::SmoothTransformation));
        m_cache.setDevicePixelRatio(dpr);
        m_cacheTaille = taille;
        m_cacheDpr = dpr;
    }

    const double s = cible.width() / REF;                           // image d'origine -> écran
    auto pos = [&](double x, double y) { return QPointF(cible.x() + x * s, cible.y() + y * s); };
    const double t = m_horloge.elapsed() / 1000.0;

    // Dessine une bande horizontale du logo (y0..y1), avec opacité, glissement et zoom
    auto bande = [&](double y0, double y1, double opacite, double decalage, double zoom, double pivotY) {
        if (opacite <= 0) return;
        p.save();
        p.setOpacity(opacite);
        const QPointF pivot = pos(REF / 2, pivotY);
        p.translate(pivot);
        p.scale(zoom, zoom);
        p.translate(-pivot);
        p.translate(0, decalage * s);
        p.setClipRect(QRectF(cible.x(), cible.y() + y0 * s, cible.width(), (y1 - y0) * s),
                      Qt::IntersectClip);
        p.drawPixmap(cible.topLeft(), m_cache);
        p.restore();
    };

    // --- Introduction (écran de démarrage) ou affichage direct
    const double debutBoucle = m_intro ? 0.85 : 0.0;
    if (m_intro) {
        const double e = adoucir(t / 0.85);
        bande(0, FIN_EMBLEME, e, 0, 0.6 + 0.4 * e, 437);           // emblème : zoom + fondu
        const double u = adoucir((t - 0.70) / 0.60);
        bande(DEBUT_USPC, DEBUT_SOUS, u, 40 * (1 - u), 1, 0);      // « USPC » monte
        const double v = adoucir((t - 1.15) / 0.55);
        bande(DEBUT_SOUS, DEBUT_DESKTOP, v, 25 * (1 - v), 1, 0);   // nom complet
        const double w = adoucir((t - 1.45) / 0.40);
        bande(DEBUT_DESKTOP, REF, w, 0, 1, 0);                     // « DESKTOP APP »
    } else {
        p.drawPixmap(cible.topLeft(), m_cache);
    }
    if (t < debutBoucle) return;

    // --- Boucle : flamme qui vacille
    const double ph = (t - debutBoucle) * 2.8;
    const double zoomFlamme = 1 + 0.05 * qSin(ph * 2.1) + 0.03 * qSin(ph * 5.3);
    const double fw = FLAMME.width() * s * zoomFlamme, fh = FLAMME.height() * s * zoomFlamme;
    const QPointF basFlamme = pos(FLAMME.x() + FLAMME.width() / 2.0, FLAMME.y() + FLAMME.height());
    const QRectF rectFlamme(basFlamme.x() - fw / 2, basFlamme.y() - fh, fw, fh);
    p.drawImage(rectFlamme, m_flamme);
    const double eclat = 0.18 * std::max(0.0, qSin(ph * 3.7));    // petits éclats de lumière
    if (eclat > 0) {
        p.save();
        p.setOpacity(eclat);
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        p.drawImage(rectFlamme, m_flamme);
        p.restore();
    }

    // --- Boucle : gyrophares qui clignotent en alternance
    const bool gauche = int((t - debutBoucle) / 0.24) % 2 == 0;
    const QPointF centre = pos(gauche ? GYRO_G.x() : GYRO_D.x(), GYRO_G.y());
    const double r = 34 * s;
    QRadialGradient halo(centre, r);
    halo.setColorAt(0.0, QColor(255, 210, 60, 230));
    halo.setColorAt(0.4, QColor(255, 170, 30, 150));
    halo.setColorAt(1.0, QColor(255, 150, 0, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(halo);
    p.drawEllipse(centre, r, r * 0.8);
}
