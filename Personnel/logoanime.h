#ifndef LOGOANIME_H
#define LOGOANIME_H

#include <QElapsedTimer>
#include <QImage>
#include <QPixmap>
#include <QWidget>

class QTimer;

// Logo USPC animé, dessiné par Qt à partir du PNG haute résolution
// (net à toutes les tailles, contrairement à un GIF).
//  - intro = true : apparition de l'emblème puis du texte (écran de démarrage)
//  - rond  = true : logo dans un cercle blanc (barre latérale)
// Ensuite, en boucle : la flamme vacille et les gyrophares clignotent.
class LogoAnime : public QWidget
{
    Q_OBJECT

public:
    explicit LogoAnime(const QString &cheminImage, bool intro = false, bool rond = false,
                       QWidget *parent = nullptr);
    QSize sizeHint() const override { return QSize(200, 200); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QImage        m_source;      // logo complet (haute résolution)
    QImage        m_flamme;      // flamme découpée, bords adoucis
    QPixmap       m_cache;       // logo mis à l'échelle de l'écran
    QSize         m_cacheTaille;
    qreal         m_cacheDpr = 0;
    QElapsedTimer m_horloge;
    QTimer       *m_timer = nullptr;
    bool          m_intro;
    bool          m_rond;
};

#endif // LOGOANIME_H
