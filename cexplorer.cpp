#include "cexplorer.h"
#include <shlobj.h>

#include <QTreeView>
#include <QFileSystemModel>
#include <QVBoxLayout>
#include <QWidget>
#include <QDesktopServices>
#include <QUrl>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QInputDialog>
#include <QFile>
#include <QClipboard>
#include <QMimeData>
#include <QGuiApplication>

CExplorer::CExplorer() {
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    model = new QFileSystemModel(this);
    model->setRootPath("");

    treeView = new QTreeView(this);
    treeView->setModel(model);
    treeView->setRootIndex(QModelIndex());

    connect(treeView, &QTreeView::doubleClicked, this, [this](const QModelIndex &index) {
        if (!model->isDir(index)) {
            QString filePath = model->filePath(index);
            QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        }
    });

    treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(treeView, &QTreeView::customContextMenuRequested, this, &CExplorer::showContextMenu);

    layout->addWidget(treeView);
    centralWidget->setLayout(layout);
    setCentralWidget(centralWidget);
}

void CExplorer::showContextMenu(const QPoint &pos) {
    selectedIndex = treeView->indexAt(pos);
    if (!selectedIndex.isValid() || model->isDir(selectedIndex))
        return; // Don't show the menu if it's a folder or drive

    QMenu contextMenu(this);
    QAction *renameAction = contextMenu.addAction("Rename");
    QAction *deleteAction = contextMenu.addAction("Delete");
    QAction *copyAction = contextMenu.addAction("Copy");

    connect(renameAction, &QAction::triggered, this, &CExplorer::renameFile);
    connect(deleteAction, &QAction::triggered, this, &CExplorer::deleteFile);
    connect(copyAction, &QAction::triggered, this, &CExplorer::copyFile);

    contextMenu.exec(treeView->viewport()->mapToGlobal(pos));
}

void CExplorer::renameFile() {
    if (!selectedIndex.isValid()) return;

    QString oldName = model->filePath(selectedIndex);
    QString newName = QInputDialog::getText(this, "Rename File", "New name:", QLineEdit::Normal, model->fileName(selectedIndex));

    if (!newName.isEmpty()) {
        QString newPath = QFileInfo(oldName).absolutePath() + "/" + newName;
        QFile::rename(oldName, newPath);
    }
}

void CExplorer::deleteFile() {
    if (!selectedIndex.isValid()) return;

    QString filePath = model->filePath(selectedIndex);

    if (QMessageBox::warning(this, "Delete", "Are you sure you want to delete this file?",
                             QMessageBox::Yes | QMessageBox::No) == QMessageBox::No) {
        return;
    }

    // Convert file path to wchar_t for SHFileOperation
    std::wstring wFilePath = filePath.toStdWString() + L'\0'; // Needs double null termination

    SHFILEOPSTRUCT fileOp;
    ZeroMemory(&fileOp, sizeof(fileOp));
    fileOp.wFunc = FO_DELETE;
    fileOp.pFrom = wFilePath.c_str();
    fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;  // Send to Recycle Bin

    // Try moving to Recycle Bin
    if (SHFileOperation(&fileOp) != 0) {
        // If failed, delete permanently
        if (!QFile::remove(filePath)) {
            QMessageBox::warning(this, "Error", "Failed to delete the file.");
        }
    }
}

void CExplorer::copyFile() {
    if (!selectedIndex.isValid()) return;

    QString filePath = model->filePath(selectedIndex);

    QClipboard *clipboard = QGuiApplication::clipboard();
    QMimeData *mimeData = new QMimeData();

    // Set file path in clipboard (for pasting)
    QList<QUrl> urls;
    urls.append(QUrl::fromLocalFile(filePath));
    mimeData->setUrls(urls);

    clipboard->setMimeData(mimeData);

    QMessageBox::information(this, "Copy", "File copied to clipboard!");
}
