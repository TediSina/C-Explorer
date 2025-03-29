#include "cexplorer.h"

#include <QTreeView>
#include <QFileSystemModel>
#include <QVBoxLayout>
#include <QWidget>
#include <QDesktopServices>
#include <QUrl>

CExplorer::CExplorer() {
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    QFileSystemModel *model = new QFileSystemModel(this);
    model->setRootPath(""); // Sets the root to the filesystem

    QTreeView *treeView = new QTreeView(this);
    treeView->setModel(model);
    treeView->setRootIndex(QModelIndex()); // Starts at "This PC"

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
