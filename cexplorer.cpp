#include "cexplorer.h"
#include "cfilesystemmodel.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#endif

#include <QTreeView>
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
#include <QSplitter>
#include <QHeaderView>
#include <QToolButton>

CExplorer::CExplorer() {
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    QHBoxLayout *navLayout = new QHBoxLayout;
    backButton = new QToolButton(this);
    backButton->setText("<");
    forwardButton = new QToolButton(this);
    forwardButton->setText(">");
    locationBar = new QLineEdit(QString("This PC"), this);

    navLayout->addWidget(backButton);
    navLayout->addWidget(forwardButton);
    navLayout->addWidget(locationBar);

    mainLayout->addLayout(navLayout);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLay = new QVBoxLayout(leftPanel);
    leftLay->setContentsMargins(0,0,0,0);
    leftLay->setSpacing(0);

    pinnedList = new QListWidget(this);
    pinnedList->setViewMode(QListWidget::ListMode);
    pinnedList->setUniformItemSizes(true);
    pinnedList->setIconSize(QSize(16,16));
    pinnedList->setFixedHeight(140);
    leftLay->addWidget(pinnedList);

    model = new CFileSystemModel(this);
    model->setRootPath(QString{});
    treeView = new QTreeView(this);
    treeView->setModel(model);
    treeView->setRootIndex(QModelIndex());
    treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    leftLay->addWidget(treeView);

    splitter->addWidget(leftPanel);

    contentView = new QTableView(this);
    contentView->setModel(model);
    contentView->setRootIndex(QModelIndex());
    contentView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    contentView->setSelectionBehavior(QAbstractItemView::SelectRows);
    contentView->setAlternatingRowColors(true);
    contentView->setSortingEnabled(true);
    contentView->horizontalHeader()->setStretchLastSection(true);
    contentView->setShowGrid(false);

    splitter->addWidget(contentView);
    splitter->setStretchFactor(1, 3);
    mainLayout->addWidget(splitter);

    setCentralWidget(centralWidget);

    populatePinnedFolders();

    connect(treeView, &QTreeView::clicked, this, [=](const QModelIndex &index) {
        if (model->isDir(index)) {
            navigateTo(model->filePath(index));
        }
    });

    connect(treeView, &QTreeView::doubleClicked, this, [=](const QModelIndex &index) {
        if (!model->isDir(index)) {
            QString filePath = model->filePath(index);
            QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        }
    });

    connect(contentView, &QTableView::doubleClicked, this, [=](const QModelIndex &index) {
        if (!index.isValid())
            return;

        QString path = model->filePath(index);
        if (model->isDir(index)) {
            navigateTo(path);
        } else {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        }
    });

    connect(pinnedList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const QString path = item->data(Qt::UserRole).toString();
        navigateTo(path);
    });

    connect(backButton, &QToolButton::clicked, this, [=] {
        if (!backHistory.isEmpty()) {
            forwardHistory.push(locationBar->text());
            QString prev = backHistory.pop();
            updatingFromHistory = true;
            if (prev != QString("This PC")) {
                navigateTo(prev);
            } else {
                navigateTo(QString(""));
            }
            updatingFromHistory = false;
        }
    });

    connect(forwardButton, &QToolButton::clicked, this, [=] {
        if (!forwardHistory.isEmpty()) {
            backHistory.push(locationBar->text());
            QString next = forwardHistory.pop();
            updatingFromHistory = true;
            navigateTo(next);
            updatingFromHistory = false;
        }
    });

    connect(locationBar, &QLineEdit::returnPressed, this, &CExplorer::handleLocationBarInput);

    treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    contentView->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(treeView, &QTreeView::customContextMenuRequested,
            this, [this](const QPoint &pos) { showContextMenu(pos, treeView); });
    connect(contentView, &QTableView::customContextMenuRequested,
            this, [this](const QPoint &pos) { showContextMenu(pos, contentView); });
}

void CExplorer::navigateTo(const QString &path) {
    if (path.isEmpty()) {
        contentView->setRootIndex(QModelIndex());
        locationBar->setText("This PC");
        return;
    }

    QModelIndex idx = model->index(path);
    if (!idx.isValid()) return;

    contentView->setRootIndex(idx);
    locationBar->setText(path);

    if (!updatingFromHistory)
        backHistory.push(path);

    forwardHistory.clear(); // reset forward history on new navigation
}

void CExplorer::populatePinnedFolders()
{
    struct Item { QString name; QString path; };

    QList<Item> items = {
        { tr("Desktop"),   QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)  },
        { tr("This PC"),   QString{} },
        { tr("Downloads"), QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) },
        { tr("Pictures"),  QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) },
        { tr("Music"),     QStandardPaths::writableLocation(QStandardPaths::MusicLocation)    },
        { tr("Videos"),    QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)   }
    };

    QFileIconProvider iconProv;
    for (const Item &it : items) {
        auto *wItem = new QListWidgetItem(it.name);
        wItem->setData(Qt::UserRole, it.path);
        if (it.path.isEmpty())
            wItem->setIcon(QIcon(":/icons/pc.png"));
        else
            wItem->setIcon(iconProv.icon(QFileInfo(it.path)));
        pinnedList->addItem(wItem);
    }
}

void CExplorer::handleLocationBarInput() {
    QString inputPath = locationBar->text().trimmed();
    QFileInfo info(inputPath);

    if (info.exists()) {
        if (info.isDir()) {
            QModelIndex index = model->index(inputPath);
            if (index.isValid()) {
                contentView->setRootIndex(index);
                locationBar->setText(model->filePath(index));
            }
        } else if (info.isFile()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(inputPath));
        }
    } else {
        QModelIndex currentIndex = contentView->rootIndex();
        locationBar->setText(model->filePath(currentIndex));
    }
}

void CExplorer::showContextMenu(const QPoint &pos, QAbstractItemView *view) {
    selectedIndex = view->indexAt(pos);
    if (!selectedIndex.isValid()) return;

    QString filePath = model->filePath(selectedIndex);
    QFileInfo fileInfo(filePath);

    QMenu contextMenu(this);

    if (fileInfo.isFile()) {
        // File Context Menu
        QAction *cutAction = contextMenu.addAction("Cut");
        QAction *copyAction = contextMenu.addAction("Copy");
        QAction *deleteAction = contextMenu.addAction("Delete");
        QAction *renameAction = contextMenu.addAction("Rename");
        QAction *pasteAction = contextMenu.addAction("Paste");
        QAction *copyPathAction = contextMenu.addAction("Copy File Path");
        QAction *createFileAction = contextMenu.addAction("Create New File");
        QAction *createFolderAction = contextMenu.addAction("Create New Folder");
        QAction *propertiesAction = contextMenu.addAction("Properties");

        connect(cutAction, &QAction::triggered, this, &CExplorer::cut);
        connect(copyAction, &QAction::triggered, this, &CExplorer::copy);
        connect(deleteAction, &QAction::triggered, this, &CExplorer::deleteItems);
        connect(renameAction, &QAction::triggered, this, &CExplorer::renameFile);
        connect(pasteAction, &QAction::triggered, this, &CExplorer::paste);
        connect(copyPathAction, &QAction::triggered, this, &CExplorer::copyPath);
        connect(createFileAction, &QAction::triggered, this, &CExplorer::createFile);
        connect(createFolderAction, &QAction::triggered, this, &CExplorer::createFolder);
        connect(propertiesAction, &QAction::triggered, this, &CExplorer::showProperties);
    }
    else if (fileInfo.isDir() && !filePath.endsWith(":/")) {
        // Folder Context Menu (excluding drives)
        QAction *cutAction = contextMenu.addAction("Cut");
        QAction *copyAction = contextMenu.addAction("Copy");
        QAction *deleteAction = contextMenu.addAction("Delete");
        QAction *renameAction = contextMenu.addAction("Rename");
        QAction *pasteAction = contextMenu.addAction("Paste");
        QAction *copyPathAction = contextMenu.addAction("Copy Folder Path");
        QAction *createFileAction = contextMenu.addAction("Create New File");
        QAction *createFolderAction = contextMenu.addAction("Create New Folder");
        QAction *propertiesAction = contextMenu.addAction("Properties");

        connect(cutAction, &QAction::triggered, this, &CExplorer::cut);
        connect(copyAction, &QAction::triggered, this, &CExplorer::copy);
        connect(deleteAction, &QAction::triggered, this, &CExplorer::deleteItems);
        connect(renameAction, &QAction::triggered, this, &CExplorer::renameFolder);
        connect(pasteAction, &QAction::triggered, this, &CExplorer::paste);
        connect(copyPathAction, &QAction::triggered, this, &CExplorer::copyPath);
        connect(createFileAction, &QAction::triggered, this, &CExplorer::createFile);
        connect(createFolderAction, &QAction::triggered, this, &CExplorer::createFolder);
        connect(propertiesAction, &QAction::triggered, this, &CExplorer::showProperties);
    }
    else {
        // Drive Context Menu
        QAction *pasteAction = contextMenu.addAction("Paste");
        QAction *copyPathAction = contextMenu.addAction("Copy Drive Path");
        QAction *createFileAction = contextMenu.addAction("Create New File");
        QAction *createFolderAction = contextMenu.addAction("Create New Folder");
        QAction *propertiesAction = contextMenu.addAction("Properties");
        connect(pasteAction, &QAction::triggered, this, &CExplorer::paste);
        connect(copyPathAction, &QAction::triggered, this, &CExplorer::copyPath);
        connect(createFileAction, &QAction::triggered, this, &CExplorer::createFile);
        connect(createFolderAction, &QAction::triggered, this, &CExplorer::createFolder);
        connect(propertiesAction, &QAction::triggered, this, &CExplorer::showProperties);
    }

    contextMenu.exec(view->viewport()->mapToGlobal(pos));
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

void CExplorer::copy() {
    // Get all selected indexes (to support multi-selection)
    QModelIndexList selectedIndexes;

    if (treeView->hasFocus())
        selectedIndexes = treeView->selectionModel()->selectedRows();
    else if (contentView->hasFocus())
        selectedIndexes = contentView->selectionModel()->selectedRows();

    if (selectedIndexes.isEmpty()) {
        QMessageBox::warning(this, "Copy", "No files or folders selected to copy.");
        return;
    }

    QList<QUrl> urls;
    QSet<QString> addedPaths;  // Avoid duplicate entries (especially due to multiple columns)

    for (const QModelIndex &index : std::as_const(selectedIndexes)) {
        // Skip duplicate indexes from different columns
        QString path = model->filePath(index);
        if (addedPaths.contains(path)) continue;

        addedPaths.insert(path);
        urls.append(QUrl::fromLocalFile(path));
    }

    if (urls.isEmpty()) {
        QMessageBox::warning(this, "Copy", "No valid file or folder paths found.");
        return;
    }

    QClipboard *clipboard = QGuiApplication::clipboard();
    QMimeData *mimeData = new QMimeData();
    mimeData->setUrls(urls);
    clipboard->setMimeData(mimeData);

    QMessageBox::information(this, "Copy", "Copied to clipboard!");
}

void CExplorer::cut() {
    QModelIndexList selectedIndexes;

    if (treeView->hasFocus())
        selectedIndexes = treeView->selectionModel()->selectedRows();
    else if (contentView->hasFocus())
        selectedIndexes = contentView->selectionModel()->selectedRows();

    if (selectedIndexes.isEmpty()) return;

    cutPaths.clear();
    QList<QUrl> urlList;

    for (const QModelIndex &index : std::as_const(selectedIndexes)) {
        QString path = model->filePath(index);
        cutPaths << path;
        urlList << QUrl::fromLocalFile(path);
    }

    QMimeData *mimeData = new QMimeData();
    mimeData->setUrls(urlList);

    QGuiApplication::clipboard()->setMimeData(mimeData);
    isCutOperation = true;

    // Set cut paths visually
    static_cast<CFileSystemModel *>(model)->setCutPaths(cutPaths);
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

        QString originalName = sourceInfo.fileName();
        QString targetPath = destinationDirPath + QDir::separator() + originalName;

        // Skip cut-paste if source and target paths are identical
        if (isCutOperation && cutPaths.contains(sourcePath) &&
            sourceInfo.absoluteFilePath() == QFileInfo(targetPath).absoluteFilePath()) {
            continue;
        }

        QString baseName;
        QString extension;
        bool isFile = sourceInfo.isFile();
        int counter = 1;

        // Separate base name and extension if it's a file
        if (isFile) {
            baseName = sourceInfo.completeBaseName();
            extension = sourceInfo.suffix();
        }

        // Conflict resolution
        bool renameInstead = false;
        bool cancelThisItem = false;

        if (QFile::exists(targetPath)) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Conflict Detected",
                QString("The file or folder '%1' already exists in the destination.\n\n"
                        "Do you want to overwrite it?")
                    .arg(originalName),
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                QMessageBox::No
                );

            if (reply == QMessageBox::Yes) {
                if (!moveToRecycleBin(targetPath)) {
                    QMessageBox::warning(this, "Paste", "Failed to delete existing item:\n" + targetPath);
                    continue;
                }
            } else if (reply == QMessageBox::No) {
                renameInstead = true;
            } else {
                cancelThisItem = true;
            }
        }

        if (cancelThisItem) continue;

        // If renameInstead is true, generate unique name
        while (renameInstead && QFile::exists(targetPath)) {
            QString newName;
            if (isFile) {
                newName = QString("%1_%2%3").arg(baseName).arg(counter++)
                .arg(extension.isEmpty() ? "" : "." + extension);
            } else {
                newName = QString("%1_%2").arg(originalName).arg(counter++);
            }

            targetPath = destinationDirPath + QDir::separator() + newName;
        }

        bool success = false;

        // If cutting, move instead of copy
        if (isCutOperation && cutPaths.contains(sourcePath)) {
            if (sourceInfo.isFile()) {
                success = QFile::rename(sourcePath, targetPath);
            } else if (sourceInfo.isDir()) {
                QDir sourceDir(sourcePath);
                success = sourceDir.rename(sourcePath, targetPath);
            }
        } else {
            if (sourceInfo.isFile()) {
                success = QFile::copy(sourcePath, targetPath);
            } else if (sourceInfo.isDir()) {
                success = copyFolderRecursively(sourcePath, targetPath);
            }
        }

        if (!success) {
            QMessageBox::warning(this, "Paste", "Failed to paste:\n" + sourcePath);
        }
    }

    // Clear cut state
    cutPaths.clear();
    isCutOperation = false;
    static_cast<CFileSystemModel *>(model)->clearCutPaths(); // Clear grayed items

    QMessageBox::information(this, "Paste", "Paste operation completed.");
}

bool CExplorer::moveToRecycleBin(const QString &path) {
    QFileInfo fileInfo(path);

#ifdef Q_OS_WIN
    QString pathWithNull = QDir::toNativeSeparators(path) + '\0';

    SHFILEOPSTRUCT fileOp = {};
    fileOp.wFunc = FO_DELETE;
    fileOp.pFrom = reinterpret_cast<LPCWSTR>(pathWithNull.utf16());
    fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;

    if (SHFileOperation(&fileOp) == 0) {
        return true;
    }
#endif

    if (fileInfo.isDir()) {
        QDir dir(path);
        return dir.removeRecursively();
    } else {
        return QFile::remove(path);
    }
}

void CExplorer::deleteItems() {
    QModelIndexList selectedIndexes;

    if (treeView->hasFocus())
        selectedIndexes = treeView->selectionModel()->selectedRows();
    else if (contentView->hasFocus())
        selectedIndexes = contentView->selectionModel()->selectedRows();

    if (selectedIndexes.isEmpty()) return;

    QMessageBox::StandardButton confirm = QMessageBox::warning(
        this, "Delete",
        QString("Are you sure you want to delete the selected %1item%2?\nThis action cannot be undone.")
            .arg(selectedIndexes.count() == 1 ? "" : QString::number(selectedIndexes.count()) + " ",
                    selectedIndexes.count() > 1 ? "s" : ""),
        QMessageBox::Yes | QMessageBox::No
        );

    if (confirm != QMessageBox::Yes) return;

    for (const QModelIndex &index : std::as_const(selectedIndexes)) {
        QString path = model->filePath(index);
        QFileInfo fileInfo(path);

#ifdef Q_OS_WIN
        QString pathWithNull = QDir::toNativeSeparators(path) + '\0';

        SHFILEOPSTRUCT fileOp = {};
        fileOp.wFunc = FO_DELETE;
        fileOp.pFrom = reinterpret_cast<LPCWSTR>(pathWithNull.utf16());
        fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;

        if (SHFileOperation(&fileOp) == 0) {
            continue; // Successfully moved to Recycle Bin
        }
#endif

        if (fileInfo.isDir()) {
            QDir dir(path);
            if (!dir.removeRecursively()) {
                QMessageBox::warning(this, "Error", "Failed to delete folder:\n" + path);
            }
        } else {
            if (!QFile::remove(path)) {
                QMessageBox::warning(this, "Error", "Failed to delete file:\n" + path);
            }
        }
    }
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

void CExplorer::copyPath() {
    if (!selectedIndex.isValid()) return;

    QString path = model->filePath(selectedIndex);

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(path);

    QMessageBox::information(this, "Copied", "Path copied to clipboard:\n" + path);
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
