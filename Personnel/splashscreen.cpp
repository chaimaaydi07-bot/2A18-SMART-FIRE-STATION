#include "splashscreen.h"

#include <QGuiApplication>
#include <QLabel>
#include <QMovie>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

SplashScreen::SplashScreen(const QString &gifPath, int durationMs, int size, QWidget *parent)
    : QWidget(parent, Qt::SplashScreen | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_DeleteOnClose);      // libère la mémoire à la fermeture
    setStyleSheet("background-color: white;");
    setFixedSize(size, size);

    // Le QLabel affiche le GIF animé via QMovie
    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);

    m_movie = new QMovie(gifPath, QByteArray(), this);
    m_movie->setScaledSize(QSize(size, size));
    m_label->setMovie(m_movie);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_label);

    // Centrer la fenêtre sur l'écran
    const QRect screen = QGuiApplication::primaryScreen()->availableGeometry();
    move(screen.center() - rect().center());

    m_movie->start();

    // Après durationMs, on arrête l'animation et on ouvre la fenêtre principale
    QTimer::singleShot(durationMs, this, [this]() {
        m_movie->stop();
        emit finished();
        close();
    });
}