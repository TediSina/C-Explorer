#include "cfilesystemmodel.h"
#include <QBrush>
#include <QColor>

CFileSystemModel::CFileSystemModel(QObject *parent)
    : QFileSystemModel(parent) {}

void CFileSystemModel::setCutPaths(const QStringList &paths) {
    cutPathSet = QSet<QString>(paths.begin(), paths.end());
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}

void CFileSystemModel::clearCutPaths() {
    if (!cutPathSet.isEmpty()) {
        cutPathSet.clear();
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
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
