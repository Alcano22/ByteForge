#pragma once

#include "EditorListModel.h"
#include "ProjectListModel.h"
#include "ReleaseFeed.h"
#include "ReleaseListModel.h"

#include <Engine/Project/ProjectTemplate.h>
#include <HubCore/EditorInstalls.h>

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
        Q_PROPERTY(QAbstractItemModel* editors READ editors CONSTANT)
        Q_PROPERTY(QString installsDirectory READ installsDirectory CONSTANT)
        Q_PROPERTY(QAbstractItemModel* releases READ releases CONSTANT)
        Q_PROPERTY(bool checkingReleases READ checkingReleases NOTIFY releaseStateChanged)
        Q_PROPERTY(bool releasesChecked READ releasesChecked NOTIFY releaseStateChanged)
        Q_PROPERTY(QString releaseError READ releaseError NOTIFY releaseStateChanged)
        Q_PROPERTY(bool showPreReleases READ showPreReleases WRITE setShowPreReleases NOTIFY showPreReleasesChanged)

    public:
        explicit HubController(QObject* parent = nullptr);

        [[nodiscard]] QAbstractItemModel* projects() { return &m_Filtered; }

        [[nodiscard]] QString search() const { return m_Search; }
        void setSearch(const QString& search);

        [[nodiscard]] QString engineVersion() const;

        [[nodiscard]] QVariantList templates() const { return m_TemplateItems; }

        [[nodiscard]] QAbstractItemModel* editors() { return &m_EditorModel; }
        [[nodiscard]] QString installsDirectory() const;

        [[nodiscard]] QAbstractItemModel* releases() { return &m_ReleaseModel; }
        [[nodiscard]] bool checkingReleases() const { return m_ReleaseFeed.isLoading(); }
        [[nodiscard]] bool releasesChecked() const { return m_ReleaseFeed.hasChecked(); }
        [[nodiscard]] QString releaseError() const { return m_ReleaseFeed.errorMessage(); }

        [[nodiscard]] bool showPreReleases() const { return m_ShowPreReleases; }
        void setShowPreReleases(bool show);

        Q_INVOKABLE void refresh();

        Q_INVOKABLE void checkForReleases();

        Q_INVOKABLE void openProject(const QString& path);

        Q_INVOKABLE void addExistingProject(const QUrl& folder);
        Q_INVOKABLE void removeFromList(const QString& path);
        Q_INVOKABLE void showInFolder(const QString& path);

        Q_INVOKABLE void removeEditor(const QString& path);
        Q_INVOKABLE void openInstallsFolder();

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
        void releaseStateChanged();
        void showPreReleasesChanged();

    private:
        void reportError(const QString& message);

        void reloadEditors();
        [[nodiscard]] const EditorInstall* pickEditor() const;

        void updateReleaseModel();

    private:
        ProjectListModel m_Projects;
        QSortFilterProxyModel m_Filtered;
        QString m_Search;

        std::vector<ProjectTemplate> m_Templates;
        QVariantList m_TemplateItems;

        fs::path m_InstallsDirectory;
        std::vector<EditorInstall> m_Editors;
        EditorListModel m_EditorModel;

        ReleaseFeed m_ReleaseFeed;
        ReleaseListModel m_ReleaseModel;
        bool m_ShowPreReleases = false;
    };
}
