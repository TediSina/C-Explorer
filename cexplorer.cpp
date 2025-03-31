#include "cexplorer.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#endif

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
#include <QApplication>
#include <QDir>

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
    if (!selectedIndex.isValid()) return;

    QString filePath = model->filePath(selectedIndex);
    QFileInfo fileInfo(filePath);

    QMenu contextMenu(this);

    if (fileInfo.isFile()) {
        // File Context Menu
        QAction *renameAction = contextMenu.addAction("Rename");
        QAction *deleteAction = contextMenu.addAction("Delete");
        QAction *copyAction = contextMenu.addAction("Copy");

        connect(renameAction, &QAction::triggered, this, &CExplorer::renameFile);
        connect(deleteAction, &QAction::triggered, this, &CExplorer::deleteFile);
        connect(copyAction, &QAction::triggered, this, &CExplorer::copyFile);
    }
    else if (fileInfo.isDir() && !filePath.endsWith(":/")) {
        // Folder Context Menu (excluding drives)
        QAction *renameAction = contextMenu.addAction("Rename");
        QAction *deleteAction = contextMenu.addAction("Delete");
        QAction *copyAction = contextMenu.addAction("Copy");
        QAction *copyPathAction = contextMenu.addAction("Copy Folder Path");

        connect(renameAction, &QAction::triggered, this, &CExplorer::renameFolder);
        connect(deleteAction, &QAction::triggered, this, &CExplorer::deleteFolder);
        connect(copyAction, &QAction::triggered, this, &CExplorer::copyFolder);
        connect(copyPathAction, &QAction::triggered, this, &CExplorer::copyFolderPath);
    }
    else {
        // Drive Context Menu (Show Properties)
        QAction *propertiesAction = contextMenu.addAction("Properties");
        connect(propertiesAction, &QAction::triggered, this, &CExplorer::showDriveProperties);
    }

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

    // Confirm deletion
    if (QMessageBox::warning(this, "Delete", "Are you sure you want to delete this file?",
                             QMessageBox::Yes | QMessageBox::No) == QMessageBox::No) {
        return;
    }

#ifdef Q_OS_WIN
    // Convert file path to wchar_t for SHFileOperation
    std::wstring wFilePath = filePath.toStdWString() + L'\0'; // Needs double null termination

    SHFILEOPSTRUCTW fileOp;
    ZeroMemory(&fileOp, sizeof(fileOp));
    fileOp.wFunc = FO_DELETE;
    fileOp.pFrom = wFilePath.c_str();
    fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;  // Send to Recycle Bin

    // Try moving to Recycle Bin
    if (SHFileOperationW(&fileOp) == 0) {
        return; // Successfully moved to Recycle Bin
    }
#endif

    // If not on Windows or Recycle Bin move failed, delete permanently
    if (!QFile::remove(filePath)) {
        QMessageBox::warning(this, "Error", "Failed to delete the file.");
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

void CExplorer::renameFolder() {
    if (!selectedIndex.isValid()) return;

    QString oldPath = model->filePath(selectedIndex);
    QFileInfo folderInfo(oldPath);

    if (!folderInfo.isDir()) return;

    bool ok;
    QString newName = QInputDialog::getText(this, "Rename Folder",
                                            "Enter new folder name:", QLineEdit::Normal,
                                            folderInfo.fileName(), &ok);

    if (!ok || newName.isEmpty() || newName == folderInfo.fileName()) return;

    QString newPath = folderInfo.absoluteDir().absoluteFilePath(newName);

    QDir dir;
    if (dir.rename(oldPath, newPath)) {
        QMessageBox::information(this, "Success", "Folder renamed successfully.");
    } else {
        QMessageBox::warning(this, "Error", "Failed to rename folder.");
    }
}

void CExplorer::copyFolder() {

}

void CExplorer::copyFolderPath() {
    if (!selectedIndex.isValid()) return;

    QString folderPath = model->filePath(selectedIndex);
    QFileInfo folderInfo(folderPath);

    if (!folderInfo.isDir()) return;

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(folderPath);

    QMessageBox::information(this, "Copied", "Folder path copied to clipboard.");
}

void CExplorer::deleteFolder() {
    if (!selectedIndex.isValid()) return;

    QString folderPath = model->filePath(selectedIndex);
    QFileInfo folderInfo(folderPath);

    if (!folderInfo.isDir()) return; // Ensure it's a folder

    // Confirm deletion
    QMessageBox::StandardButton confirm = QMessageBox::warning(
        this, "Delete Folder",
        "Are you sure you want to delete this folder?\nThis action cannot be undone.",
        QMessageBox::Yes | QMessageBox::No);

    if (confirm != QMessageBox::Yes) return;

#ifdef Q_OS_WIN
    // Attempt to move folder to Recycle Bin (Windows)
    QString pathWithNull = QDir::toNativeSeparators(folderPath) + '\0';

    SHFILEOPSTRUCT fileOp = {};
    fileOp.wFunc = FO_DELETE;
    fileOp.pFrom = reinterpret_cast<LPCWSTR>(pathWithNull.utf16());
    fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;

    if (SHFileOperation(&fileOp) == 0) {
        return; // Successfully moved to Recycle Bin
    }
#endif

    // If Recycle Bin move fails or not on Windows, delete permanently
    QDir dir(folderPath);
    if (dir.removeRecursively()) {
        QMessageBox::information(this, "Deleted", "Folder deleted successfully.");
    } else {
        QMessageBox::warning(this, "Error", "Failed to delete folder.");
    }
}

void CExplorer::showDriveProperties() {
    if (!selectedIndex.isValid()) return;

    QString drivePath = model->filePath(selectedIndex);

#ifdef Q_OS_WIN
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(SHELLEXECUTEINFOW);
    sei.fMask = SEE_MASK_INVOKEIDLIST;
    sei.lpVerb = L"properties"; // Open properties window
    sei.lpFile = reinterpret_cast<LPCWSTR>(drivePath.utf16()); // Convert QString to LPCWSTR
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&sei)) {
        QMessageBox::warning(this, "Error", "Failed to open properties.");
    }
#else
    QMessageBox::information(this, "Unsupported", "Drive properties are only supported on Windows.");
#endif
}
