#ifndef CEXPLORER_H
#define CEXPLORER_H

#include "cfilesystemmodel.h"

#include <QMainWindow>
#include <QTreeView>

class CExplorer : public QMainWindow {
    Q_OBJECT

public:
    CExplorer();

private slots:
    void showContextMenu(const QPoint &pos);
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
    QTreeView *treeView;
    QModelIndex selectedIndex;
    QStringList cutPaths;
    bool isCutOperation = false;
};

#endif // CEXPLORER_H
