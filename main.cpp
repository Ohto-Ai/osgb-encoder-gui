#include "OSGB_EncoderGui.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    OSGB_EncoderGui w;
    w.show();
    return a.exec();
}
