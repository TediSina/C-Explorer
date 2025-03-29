#ifndef CEXPLORER_H
#define CEXPLORER_H

#include <QMainWindow>
#include <QFileSystemModel>
#include <QTreeView>

class CExplorer : public QMainWindow {
    Q_OBJECT

public:
    CExplorer();

private slots:
    void showContextMenu(const QPoint &pos);
    void renameFile();
    void deleteFile();
    void copyFile();

private:
    QFileSystemModel *model;
    QTreeView *treeView;
    QModelIndex selectedIndex;
};

#endif // CEXPLORER_H
