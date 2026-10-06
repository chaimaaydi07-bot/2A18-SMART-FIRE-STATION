#include "smart.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    smart w;
    w.setUtilisateur("Admin", "Chef de caserne");
    w.show();

    return a.exec();
}
