#ifndef SPLASHSCREEN_H
#define SPLASHSCREEN_H

#include <QWidget>
#include <QPixmap>
#include <QVariantAnimation>

// Ecran de demarrage : fond rouge sombre, logo dans un carre blanc arrondi,
// anneau lumineux avec un point qui tourne autour du logo.
class SplashScreen : public QWidget
{
    Q_OBJECT
public:
    explicit SplashScreen(QWidget *parent = nullptr);
    void demarrer(int dureeMs = 3000);   // lance l'animation puis emet termine()

signals:
    void termine();

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QPixmap logo;
    qreal angle = 0;        // position du point sur l'anneau (degres)
    qreal pulse = 0;        // pulsation de la lueur (0..1)
    QVariantAnimation *animRotation;
    QVariantAnimation *animPulse;
};

#endif // SPLASHSCREEN_H
