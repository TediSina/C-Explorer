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
        QAction *pasteAction = contextMenu.addAction("Paste");
        QAction *copyPathAction = contextMenu.addAction("Copy File Path");
        QAction *createFileAction = contextMenu.addAction("Create New File");
        QAction *createFolderAction = contextMenu.addAction("Create New Folder");
        QAction *propertiesAction = contextMenu.addAction("Properties");

        connect(renameAction, &QAction::triggered, this, &CExplorer::renameFile);
        connect(deleteAction, &QAction::triggered, this, &CExplorer::deleteFile);
        connect(copyAction, &QAction::triggered, this, &CExplorer::copyFile);
        connect(copyPathAction, &QAction::triggered, this, &CExplorer::copyPath);
        connect(createFileAction, &QAction::triggered, this, &CExplorer::createFile);
        connect(pasteAction, &QAction::triggered, this, &CExplorer::paste);
        connect(createFolderAction, &QAction::triggered, this, &CExplorer::createFolder);
        connect(propertiesAction, &QAction::triggered, this, &CExplorer::showProperties);
    }
    else if (fileInfo.isDir() && !filePath.endsWith(":/")) {
        // Folder Context Menu (excluding drives)
        QAction *renameAction = contextMenu.addAction("Rename");
        QAction *deleteAction = contextMenu.addAction("Delete");
        QAction *copyAction = contextMenu.addAction("Copy");
        QAction *pasteAction = contextMenu.addAction("Paste");
        QAction *copyPathAction = contextMenu.addAction("Copy Folder Path");
        QAction *createFileAction = contextMenu.addAction("Create New File");
        QAction *createFolderAction = contextMenu.addAction("Create New Folder");
        QAction *propertiesAction = contextMenu.addAction("Properties");

        connect(renameAction, &QAction::triggered, this, &CExplorer::renameFolder);
        connect(deleteAction, &QAction::triggered, this, &CExplorer::deleteFolder);
        connect(copyAction, &QAction::triggered, this, &CExplorer::copyFolder);
        connect(copyPathAction, &QAction::triggered, this, &CExplorer::copyPath);
        connect(createFileAction, &QAction::triggered, this, &CExplorer::createFile);
        connect(pasteAction, &QAction::triggered, this, &CExplorer::paste);
        connect(createFolderAction, &QAction::triggered, this, &CExplorer::createFolder);
        connect(propertiesAction, &QAction::triggered, this, &CExplorer::showProperties);
    }
    else {
        // Drive Context Menu
        QAction *copyPathAction = contextMenu.addAction("Copy Drive Path");
        QAction *pasteAction = contextMenu.addAction("Paste");
        QAction *createFileAction = contextMenu.addAction("Create New File");
        QAction *createFolderAction = contextMenu.addAction("Create New Folder");
        QAction *propertiesAction = contextMenu.addAction("Properties");
        connect(copyPathAction, &QAction::triggered, this, &CExplorer::copyPath);
        connect(pasteAction, &QAction::triggered, this, &CExplorer::paste);
        connect(createFileAction, &QAction::triggered, this, &CExplorer::createFile);
        connect(createFolderAction, &QAction::triggered, this, &CExplorer::createFolder);
        connect(propertiesAction, &QAction::triggered, this, &CExplorer::showProperties);
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

bool CExplorer::copyFolderRecursively(const QString &sourceFolder, const QString &destinationFolder) {
    QDir sourceDir(sourceFolder);
    if (!sourceDir.exists())
        return false;

    QDir destDir(destinationFolder);
    if (!destDir.exists()) {
        if (!destDir.mkpath(".")) return false;
    }

    QFileInfoList entries = sourceDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const QFileInfo &entry : std::as_const(entries)) {
        QString srcPath = entry.absoluteFilePath();
        QString destPath = destinationFolder + QDir::separator() + entry.fileName();

        if (entry.isDir()) {
            if (!copyFolderRecursively(srcPath, destPath))
                return false;
        } else {
            if (!QFile::copy(srcPath, destPath))
                return false;
        }
    }
    return true;
}

void CExplorer::paste() {
    if (!selectedIndex.isValid()) return;

    QString destinationDirPath;
    QFileInfo selectedInfo(model->filePath(selectedIndex));

    if (selectedInfo.isDir()) {
        destinationDirPath = selectedInfo.absoluteFilePath();
    } else {
        destinationDirPath = selectedInfo.absolutePath();
    }

    const QClipboard *clipboard = QGuiApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();

    if (!mimeData->hasUrls()) {
        QMessageBox::warning(this, "Paste", "Clipboard does not contain any valid files or folders.");
        return;
    }

    QList<QUrl> urls = mimeData->urls();
    if (urls.isEmpty()) {
        QMessageBox::warning(this, "Paste", "No items found in clipboard.");
        return;
    }

    for (const QUrl &url : std::as_const(urls)) {
        QString sourcePath = url.toLocalFile();
        QFileInfo sourceInfo(sourcePath);

        if (!sourceInfo.exists()) {
            QMessageBox::warning(this, "Paste", "Source item does not exist:\n" + sourcePath);
            continue;
        }

        QString targetPath = destinationDirPath + QDir::separator() + sourceInfo.fileName();

        QString baseName = sourceInfo.completeBaseName();
        QString extension = sourceInfo.suffix();
        int counter = 1;
        while (QFile::exists(targetPath)) {
            targetPath = destinationDirPath + QDir::separator() +
                         QString("%1_%2%3").arg(baseName).arg(counter++)
                             .arg(extension.isEmpty() ? "" : "." + extension);
        }

        bool success = false;
        if (sourceInfo.isFile()) {
            success = QFile::copy(sourcePath, targetPath);
        } else if (sourceInfo.isDir()) {
            success = copyFolderRecursively(sourcePath, targetPath);
        }

        if (!success) {
            QMessageBox::warning(this, "Paste", "Failed to paste:\n" + sourcePath);
        }
    }

    QMessageBox::information(this, "Paste", "Paste operation completed.");
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
    if (!selectedIndex.isValid()) return;

    QString folderPath = model->filePath(selectedIndex);
    QFileInfo folderInfo(folderPath);

    if (!folderInfo.exists() || !folderInfo.isDir()) {
        QMessageBox::warning(this, "Copy", "Selected item is not a valid folder.");
        return;
    }

    QClipboard *clipboard = QGuiApplication::clipboard();
    QMimeData *mimeData = new QMimeData();

    QList<QUrl> urls;
    urls.append(QUrl::fromLocalFile(folderPath));
    mimeData->setUrls(urls);

    clipboard->setMimeData(mimeData);

    QMessageBox::information(this, "Copy", "Folder copied to clipboard!");
}

void CExplorer::copyPath() {
    if (!selectedIndex.isValid()) return;

    QString path = model->filePath(selectedIndex);

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(path);

    QMessageBox::information(this, "Copied", "Path copied to clipboard:\n" + path);
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

void CExplorer::createFile() {
    if (!selectedIndex.isValid()) return;

    QString dirPath = model->filePath(selectedIndex);
    QFileInfo info(dirPath);

    // If right-clicked on a file, use its parent directory
    if (info.isFile()) {
        dirPath = info.absolutePath();
    } else if (!info.isDir()) {
        QMessageBox::warning(this, "Error", "Cannot create file here.");
        return;
    }

    QDir dir(dirPath);

    // Ask the user for a file name (default is "NewFile.txt")
    bool ok;
    QString fileName = QInputDialog::getText(this, "Create File", "Enter file name:",
                                             QLineEdit::Normal, "NewFile.txt", &ok);
    if (!ok || fileName.isEmpty()) return;

    // Static forbidden characters regex: \ / : * ? " < > |
    static const QRegularExpression forbiddenChars(R"([\\/:*?"<>|])");

    if (fileName.contains(forbiddenChars)) {
        QMessageBox::warning(this, "Invalid Name", "The filename contains forbidden characters: \\ / : * ? \" < > |");
        return;
    }

    QString filePath = dir.filePath(fileName);

    // Ensure unique filename by adding `_1`, `_2`, etc. before the extension if needed
    int counter = 1;
    QString baseName = QFileInfo(fileName).completeBaseName(); // Name without extension
    QString extension = QFileInfo(fileName).suffix(); // File extension

    while (QFile::exists(filePath)) {
        if (!extension.isEmpty()) {
            filePath = dir.filePath(QString("%1_%2.%3").arg(baseName).arg(counter++).arg(extension));
        } else {
            filePath = dir.filePath(QString("%1_%2").arg(baseName).arg(counter++));
        }
    }

    // Create the file
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.close();
        QMessageBox::information(this, "Success", "File created successfully:\n" + filePath);
    } else {
        QMessageBox::warning(this, "Error", "Failed to create the file.");
    }
}

void CExplorer::createFolder() {
    if (!selectedIndex.isValid()) return;

    QString dirPath = model->filePath(selectedIndex);
    QFileInfo info(dirPath);

    // If it's a file, get the parent directory
    if (info.isFile()) {
        dirPath = info.absolutePath();
    } else if (!info.isDir()) {
        QMessageBox::warning(this, "Error", "Cannot create folder here.");
        return;
    }

    QDir dir(dirPath);

    // Ask the user for a folder name (default is "NewFolder")
    bool ok;
    QString folderName = QInputDialog::getText(this, "Create Folder", "Enter folder name:",
                                               QLineEdit::Normal, "NewFolder", &ok);
    if (!ok || folderName.isEmpty()) return;

    // Static forbidden characters regex for folder names
    static const QRegularExpression forbiddenChars(R"([\\/:*?"<>|])");

    if (folderName.contains(forbiddenChars)) {
        QMessageBox::warning(this, "Invalid Name", "The folder name contains forbidden characters: \\ / : * ? \" < > |");
        return;
    }

    QString folderPath = dir.filePath(folderName);

    // Ensure unique folder name by adding `_1`, `_2`, etc.
    int counter = 1;
    while (dir.exists(folderPath)) {
        folderPath = dir.filePath(QString("%1_%2").arg(folderName).arg(counter++));
    }

    // Create the folder
    if (dir.mkdir(folderPath)) {
        QMessageBox::information(this, "Success", "Folder created successfully:\n" + folderPath);
    } else {
        QMessageBox::warning(this, "Error", "Failed to create the folder.");
    }
}

void CExplorer::showProperties() {
    if (!selectedIndex.isValid()) return;

    QString path = model->filePath(selectedIndex);

#ifdef Q_OS_WIN
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(SHELLEXECUTEINFOW);
    sei.fMask = SEE_MASK_INVOKEIDLIST;
    sei.lpVerb = L"properties"; // Open properties window
    sei.lpFile = reinterpret_cast<LPCWSTR>(path.utf16()); // Convert QString to LPCWSTR
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&sei)) {
        QMessageBox::warning(this, "Error", "Failed to open properties.");
    }
#else
    QMessageBox::information(this, "Unsupported", "Properties are only supported on Windows.");
#endif
}
