#include "smart.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    smart w;
    w.show();
    return QApplication::exec();
}
