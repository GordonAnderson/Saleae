#include "saleae.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Saleae w;
    w.show();

    return a.exec();
}
