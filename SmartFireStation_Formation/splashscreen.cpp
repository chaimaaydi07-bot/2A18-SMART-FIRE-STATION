#include "splashscreen.h"
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QConicalGradient>
#include <QTimer>
#include <QPropertyAnimation>
#include <QScreen>
#include <QGuiApplication>
#include <cmath>

SplashScreen::SplashScreen(QWidget *parent)
    : QWidget(parent),
      logo(QPixmap(":/images/logo.png")),
      animRotation(new QVariantAnimation(this)),
      animPulse(new QVariantAnimation(this))
{
    setWindowTitle("Smart Fire Station — Gestion des formations");
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    resize(520, 520);

    // rotation continue du point lumineux
    animRotation->setStartValue(0.0);
    animRotation->setEndValue(360.0);
    animRotation->setDuration(2200);
    animRotation->setLoopCount(-1);
    connect(animRotation, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        angle = v.toReal();
        update();
    });

    // pulsation de la lueur
    animPulse->setStartValue(0.0);
    animPulse->setKeyValueAt(0.5, 1.0);
    animPulse->setEndValue(0.0);
    animPulse->setDuration(1600);
    animPulse->setLoopCount(-1);
    connect(animPulse, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        pulse = v.toReal();
    });

    // centrage sur l'ecran
    if (QScreen *s = QGuiApplication::primaryScreen())
        move(s->availableGeometry().center() - rect().center());
}

void SplashScreen::demarrer(int dureeMs)
{
    setWindowOpacity(0.0);
    show();
    animRotation->start();
    animPulse->start();

    // apparition en fondu
    QPropertyAnimation *in = new QPropertyAnimation(this, "windowOpacity", this);
    in->setDuration(500);
    in->setStartValue(0.0);
    in->setEndValue(1.0);
    in->start(QAbstractAnimation::DeleteWhenStopped);

    // disparition en fondu a la fin, puis signal termine()
    QTimer::singleShot(dureeMs, this, [this]() {
        QPropertyAnimation *out = new QPropertyAnimation(this, "windowOpacity", this);
        out->setDuration(450);
        out->setStartValue(1.0);
        out->setEndValue(0.0);
        connect(out, &QPropertyAnimation::finished, this, [this]() {
            animRotation->stop();
            animPulse->stop();
            hide();
            emit termine();
        });
        out->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

void SplashScreen::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const QPointF c(width() / 2.0, height() / 2.0);

    // fond : degrade radial rouge sombre -> presque noir
    QRadialGradient fond(c, width() * 0.75);
    fond.setColorAt(0.0, QColor("#7a0f1c"));
    fond.setColorAt(0.55, QColor("#3a0b14"));
    fond.setColorAt(1.0, QColor("#1a0508"));
    p.fillRect(rect(), fond);

    // anneau lumineux (lueur + trait fin)
    const qreal rayon = 200;
    for (int i = 6; i >= 1; --i) {
        QColor glow("#ff5a4d");
        glow.setAlphaF((0.05 + 0.04 * pulse) * (7 - i) / 6.0);
        p.setPen(QPen(glow, i * 3.0));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(c, rayon, rayon);
    }
    QConicalGradient cg(c, -angle);
    cg.setColorAt(0.0, QColor(255, 140, 120, 255));
    cg.setColorAt(0.35, QColor(255, 90, 77, 140));
    cg.setColorAt(1.0, QColor(255, 90, 77, 30));
    p.setPen(QPen(QBrush(cg), 3));
    p.drawEllipse(c, rayon, rayon);

    // point lumineux qui tourne
    const qreal rad = angle * 3.14159265358979 / 180.0;
    const QPointF pt(c.x() + rayon * std::cos(rad), c.y() - rayon * std::sin(rad));
    QRadialGradient halo(pt, 18);
    halo.setColorAt(0.0, QColor(255, 220, 190, 255));
    halo.setColorAt(0.4, QColor(255, 120, 90, 160));
    halo.setColorAt(1.0, QColor(255, 90, 77, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(halo);
    p.drawEllipse(pt, 18, 18);
    p.setBrush(QColor("#fff2e8"));
    p.drawEllipse(pt, 4.5, 4.5);

    // carre blanc arrondi + logo
    const qreal cote = 270;
    QRectF carre(c.x() - cote / 2, c.y() - cote / 2, cote, cote);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 70));                       // ombre
    p.drawRoundedRect(carre.translated(0, 6), 36, 36);
    p.setBrush(Qt::white);
    p.drawRoundedRect(carre, 36, 36);
    if (!logo.isNull()) {
        QPainterPath clip;
        clip.addRoundedRect(carre.adjusted(6, 6, -6, -6), 30, 30);
        p.setClipPath(clip);
        p.drawPixmap(carre.adjusted(6, 6, -6, -6).toRect(), logo);
        p.setClipping(false);
    }

    // texte
    p.setPen(QColor("#ffd9d4"));
    QFont f("Segoe UI", 10, QFont::Bold);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 3);
    p.setFont(f);
    p.drawText(QRectF(0, height() - 52, width(), 28), Qt::AlignCenter, "SMART FIRE STATION");
}
