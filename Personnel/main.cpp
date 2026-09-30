#include <QApplication>
#include <QPalette>
#include "firestation.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setStyle("Fusion");

    // Palette claire forcée (ignore le mode sombre de Windows)
    QPalette p;
    p.setColor(QPalette::Window,          QColor("#f4f2ef"));
    p.setColor(QPalette::WindowText,      QColor("#222222"));
    p.setColor(QPalette::Base,            Qt::white);
    p.setColor(QPalette::AlternateBase,   QColor("#faf9f7"));
    p.setColor(QPalette::Text,            QColor("#222222"));
    p.setColor(QPalette::Button,          Qt::white);
    p.setColor(QPalette::ButtonText,      QColor("#222222"));
    p.setColor(QPalette::PlaceholderText, QColor("#999999"));
    p.setColor(QPalette::ToolTipBase,     Qt::white);
    p.setColor(QPalette::ToolTipText,     QColor("#222222"));
    p.setColor(QPalette::Highlight,       QColor("#f2a10c"));
    p.setColor(QPalette::HighlightedText, QColor("#222222"));
    app.setPalette(p);

    FireStation w;
    w.resize(1280, 820);
    w.show();
    return app.exec();
}