#include "cfilesystemmodel.h"
#include <QBrush>
#include <QColor>

CFileSystemModel::CFileSystemModel(QObject *parent)
    : QFileSystemModel(parent) {}

void CFileSystemModel::setCutPaths(const QStringList &paths) {
    cutPathSet = QSet<QString>(paths.begin(), paths.end());

    for (const QString &path : std::as_const(cutPathSet)) {
        QModelIndex idx = index(path);
        if (idx.isValid()) {
            emit dataChanged(idx, idx.sibling(idx.row(), columnCount(idx.parent()) - 1));
        }
    }
}

void CFileSystemModel::clearCutPaths() {
    QSet<QString> clearedPaths = std::move(cutPathSet);
    cutPathSet.clear();

    for (const QString &path : clearedPaths) {
        QModelIndex idx = index(path);
        if (idx.isValid()) {
            emit dataChanged(idx, idx.sibling(idx.row(), columnCount(idx.parent()) - 1));
        }
    }
}

QVariant CFileSystemModel::data(const QModelIndex &index, int role) const {
    if (role == Qt::ForegroundRole) {
        QString path = filePath(index);
        if (cutPathSet.contains(path)) {
            return QBrush(Qt::gray);
        }
    }

    return QFileSystemModel::data(index, role);
}
