#pragma once

#include "ProjectListModel.h"

#include <QObject>
#include <QSortFilterProxyModel>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

namespace ByteForge::Hub
{
    class HubController : public QObject
    {
        Q_OBJECT
        QML_ELEMENT
        QML_SINGLETON

        Q_PROPERTY(QAbstractItemModel* projects READ projects CONSTANT)
        Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY searchChanged)
        Q_PROPERTY(QString engineVersion READ engineVersion CONSTANT)

    public:
        explicit HubController(QObject* parent = nullptr);

        [[nodiscard]] QAbstractItemModel* projects() { return &m_Filtered; }

        [[nodiscard]] QString search() const { return m_Search; }
        void setSearch(const QString& search);

        [[nodiscard]] QString engineVersion() const;

        Q_INVOKABLE void refresh();

        Q_INVOKABLE void openProject(const QString& path);

        Q_INVOKABLE void addExistingProject(const QUrl& folder);
        Q_INVOKABLE void removeFromList(const QString& path);
        Q_INVOKABLE void showInFolder(const QString& path);

    Q_SIGNALS:
        void searchChanged();
        void errorOccurred(const QString& message);

    private:
        void reportError(const QString& message);

        [[nodiscard]] static QString findEditor();

    private:
        ProjectListModel m_Projects;
        QSortFilterProxyModel m_Filtered;
        QString m_Search;
    };
}
