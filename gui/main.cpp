#include "mainwidget.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    mainwidget w;

    // Check for command-line arguments to set the detection URL
    QStringList args = a.arguments();
    if (args.size() > 1) {
        w.setDetectionUrl(args.at(1));
    }

    w.show();
    return a.exec();
}
