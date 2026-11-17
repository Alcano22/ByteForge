#include "HubController.h"
#include "QtConversions.h"

#include <Engine/Core/Log.h>
#include <Engine/Core/Version.h>
#include <Engine/Project/Project.h>
#include <Engine/Project/RecentProjects.h>

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

namespace ByteForge::Hub
{
    namespace
    {
#ifdef Q_OS_WIN
        constexpr const char* EditorFileName = "Editor.exe";
#else
        constexpr const char* EditorFileName = "Editor";
#endif
    }

    HubController::HubController(QObject* parent)
        : QObject(parent)
    {
        m_Filtered.setSourceModel(&m_Projects);
        m_Filtered.setFilterRole(ProjectListModel::NameRole);
        m_Filtered.setFilterCaseSensitivity(Qt::CaseInsensitive);
    }

    void HubController::setSearch(const QString& search)
    {
        if (search == m_Search) return;

        m_Search = search;
        m_Filtered.setFilterFixedString(search);
        Q_EMIT searchChanged();
    }

    QString HubController::engineVersion() const
    {
        return ToQString(GetEngineVersion());
    }

    void HubController::refresh()
    {
        m_Projects.reload();
    }

    void HubController::openProject(const QString& path)
    {
        const Result<Project> project = Project::Open(ToPath(path));
        if (!project)
        {
            reportError(QStringLiteral("Cannot open the project: %1").arg(ToQString(project.error())));
            refresh();
            return;
        }

        const QString editor = findEditor();
        if (!QFileInfo::exists(editor))
        {
            reportError(QStringLiteral("The editor was not found at %1").arg(QDir::toNativeSeparators(editor)));
            return;
        }

        if (!QProcess::startDetached(editor, { QStringLiteral("--project"), PathToQString(project->GetRoot()) }))
            reportError(QStringLiteral("Cannot start the editor for '%1'").arg(ToQString(project->GetName())));
    }

    void HubController::addExistingProject(const QUrl& folder)
    {
        const Result<Project> project = Project::Open(ToPath(folder.toLocalFile()));
        if (!project)
        {
            reportError(ToQString(project.error()));
            return;
        }

        if (const Result<> added = RecentProjects::Touch(*project); !added)
            reportError(QStringLiteral("Cannot update the project list: %1").arg(ToQString(added.error())));

        refresh();
    }

    void HubController::removeFromList(const QString& path)
    {
        if (const Result<> removed = RecentProjects::Remove(ToPath(path)); !removed)
            reportError(QStringLiteral("Cannot update the project list: %1").arg(ToQString(removed.error())));

        refresh();
    }

    void HubController::showInFolder(const QString& path)
    {
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
            reportError(QStringLiteral("Cannot open %1").arg(path));
    }

    void HubController::reportError(const QString& message)
    {
        APP_WARN("{}", message.toStdString());
        Q_EMIT errorOccurred(message);
    }

    QString HubController::findEditor()
    {
        return QDir(QCoreApplication::applicationDirPath()).filePath(QString::fromLatin1(EditorFileName));
    }
}
