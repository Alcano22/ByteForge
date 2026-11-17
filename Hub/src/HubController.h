#pragma once

#include "ProjectListModel.h"

#include <Engine/Project/ProjectTemplate.h>

#include <QObject>
#include <QSortFilterProxyModel>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <vector>

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
        Q_PROPERTY(QVariantList templates READ templates CONSTANT)

    public:
        explicit HubController(QObject* parent = nullptr);

        [[nodiscard]] QAbstractItemModel* projects() { return &m_Filtered; }

        [[nodiscard]] QString search() const { return m_Search; }
        void setSearch(const QString& search);

        [[nodiscard]] QString engineVersion() const;

        [[nodiscard]] QVariantList templates() const { return m_TemplateItems; }

        Q_INVOKABLE void refresh();

        Q_INVOKABLE void openProject(const QString& path);

        Q_INVOKABLE void addExistingProject(const QUrl& folder);
        Q_INVOKABLE void removeFromList(const QString& path);
        Q_INVOKABLE void showInFolder(const QString& path);

        [[nodiscard]] Q_INVOKABLE QString suggestLocation() const;
        [[nodiscard]] Q_INVOKABLE QString suggestProjectName(const QString& location) const;

        [[nodiscard]] Q_INVOKABLE QString validateNewProject(const QString& name, const QString& location) const;

        [[nodiscard]] Q_INVOKABLE QString projectPath(const QString& location, const QString& name) const;

        [[nodiscard]] Q_INVOKABLE QString createProject(const QString& name, const QString& location,
                                                        const QString& templateId);

        [[nodiscard]] Q_INVOKABLE QUrl pathToUrl(const QString& path) const;
        [[nodiscard]] Q_INVOKABLE QString urlToPath(const QUrl& url) const;

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

        std::vector<ProjectTemplate> m_Templates;
        QVariantList m_TemplateItems;
    };
}
