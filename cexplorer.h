#ifndef CEXPLORER_H
#define CEXPLORER_H

#include "cfilesystemmodel.h"

#include <QMainWindow>
#include <QTreeView>
#include <QTableView>
#include <QToolButton>
#include <QStack>
#include <QLineEdit>
#include <QListWidget>
#include <QStandardPaths>
#include <QFileIconProvider>

class CExplorer : public QMainWindow {
    Q_OBJECT

public:
    CExplorer();

private slots:
    void navigateTo(const QString &path);
    void showContextMenu(const QPoint &pos, QAbstractItemView *view);
    void handleLocationBarInput();
    void renameFile();
    void copy();
    void cut();
    bool copyFolderRecursively(const QString &sourceFolder, const QString &destinationFolder);
    void paste();
    bool moveToRecycleBin(const QString &path);
    void deleteItems();
    void renameFolder();
    void copyPath();
    void createFile();
    void createFolder();
    void showProperties();

private:
    CFileSystemModel *model;

    QToolButton *backButton;
    QToolButton *forwardButton;

    QStack<QString> backHistory;
    QStack<QString> forwardHistory;
    bool updatingFromHistory = false;

    QListWidget *pinnedList;
    QTreeView *treeView;
    QTableView *contentView;
    QLineEdit *locationBar;

    QModelIndex selectedIndex;
    QStringList cutPaths;
    bool isCutOperation = false;

    void populatePinnedFolders();
};

#endif // CEXPLORER_H
