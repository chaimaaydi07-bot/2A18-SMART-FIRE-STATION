#ifndef SPLASHSCREEN_H
#define SPLASHSCREEN_H

#include <QWidget>

class QLabel;
class QMovie;

// Écran de démarrage qui affiche le logo animé (GIF) de l'USPC,
// puis émet le signal finished() pour ouvrir la fenêtre principale.
class SplashScreen : public QWidget
{
    Q_OBJECT

public:
    explicit SplashScreen(const QString &gifPath,
                          int durationMs = 4000,
                          int size = 400,
                          QWidget *parent = nullptr);

signals:
    void finished();

private:
    QLabel *m_label;
    QMovie *m_movie;
};

#endif // SPLASHSCREEN_H