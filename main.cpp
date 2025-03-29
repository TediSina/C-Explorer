#include <QApplication>
#include <QMainWindow>
#include <QTreeView>
#include <QFileSystemModel>
#include <QVBoxLayout>
#include <QWidget>
#include <QDesktopServices>
#include <QUrl>

class FileExplorer : public QMainWindow {
public:
    FileExplorer(QWidget *parent = nullptr) : QMainWindow(parent) {
        QWidget *centralWidget = new QWidget(this);
        QVBoxLayout *layout = new QVBoxLayout(centralWidget);

        QFileSystemModel *model = new QFileSystemModel(this);
        model->setRootPath(""); // Sets the root to the filesystem

        QTreeView *treeView = new QTreeView(this);
        treeView->setModel(model);
        treeView->setRootIndex(model->index(QDir::homePath())); // Starts at the home directory

        connect(treeView, &QTreeView::doubleClicked, this, [model](const QModelIndex &index) {
            if (!model->isDir(index)) { // Only open files
                QString filePath = model->filePath(index);
                QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
            }
        });

        layout->addWidget(treeView);
        centralWidget->setLayout(layout);
        setCentralWidget(centralWidget);
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    FileExplorer explorer;
    explorer.resize(800, 600);
    explorer.setWindowTitle("Qt File Explorer");
    explorer.show();
    return app.exec();
}
