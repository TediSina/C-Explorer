#ifndef CFILESYSTEMMODEL_H
#define CFILESYSTEMMODEL_H

#include <QFileSystemModel>
#include <QSet>
#include <QObject>

class CFileSystemModel : public QFileSystemModel
{
    Q_OBJECT

public:
    explicit CFileSystemModel(QObject *parent = nullptr);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void setCutPaths(const QStringList &paths);
    void clearCutPaths();

private:
    QSet<QString> cutPathSet;
};

#endif // CFILESYSTEMMODEL_H
