#include "cexplorer.h"

#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/C-Explorer.ico"));

    CExplorer explorer;
    explorer.resize(800, 600);
    explorer.setWindowTitle("C-Explorer");
    explorer.show();
    return app.exec();
}
