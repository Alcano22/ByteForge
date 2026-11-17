#include "HubController.h"
#include "QtConversions.h"

#include <Engine/Core/Log.h>
#include <Engine/Core/Version.h>
#include <Engine/Core/FileLock.h>
#include <Engine/Project/Project.h>
#include <Engine/Project/RecentProjects.h>

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QVariantMap>

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace ByteForge::Hub
{
    namespace
    {
#ifdef Q_OS_WIN
        constexpr const char* EditorFileName = "Editor.exe";
#else
        constexpr const char* EditorFileName = "Editor";
#endif

        constexpr i32 MaxNameSuggestions = 1000;
    }

    HubController::HubController(QObject* parent)
        : QObject(parent),
          m_Templates(ProjectTemplate::FindAll(ProjectTemplate::GetBuiltInDirectory()))
    {
        m_Filtered.setSourceModel(&m_Projects);
        m_Filtered.setFilterRole(ProjectListModel::NameRole);
        m_Filtered.setFilterCaseSensitivity(Qt::CaseInsensitive);

        for (const ProjectTemplate& projectTemplate : m_Templates)
        {
            m_TemplateItems.append(QVariantMap{
                { QStringLiteral("id"),          ToQString(projectTemplate.Id)          },
                { QStringLiteral("name"),        ToQString(projectTemplate.Name)        },
                { QStringLiteral("description"), ToQString(projectTemplate.Description) }
            });
        }

        if (m_Templates.empty())
            APP_WARN("No project templates in '{}'", ToUtf8(ProjectTemplate::GetBuiltInDirectory()));
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

        {
            const Result<Scope<FileLock>> lock = project->AcquireEditorLock();
            if (!lock)
            {
                reportError(ToQString(lock.error()));
                return;
            }
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

    QString HubController::suggestLocation() const
    {
        for (const RecentProject& project : RecentProjects::Load())
        {
            std::error_code error;
            if (const fs::path parent = project.Path.parent_path(); fs::is_directory(parent, error))
                return QDir::toNativeSeparators(PathToQString(parent));
        }

        return QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    }

    QString HubController::suggestProjectName(const QString& location) const
    {
        const QString base = QStringLiteral("New Project");
        const fs::path folder = ToPath(location);

        for (i32 number = 1; number <= MaxNameSuggestions; ++number)
        {
            const QString name = number == 1 ? base : QStringLiteral("%1 %2").arg(base).arg(number);

            std::error_code error;
            if (!fs::exists(folder / ToPath(name), error))
                return name;
        }

        return base;
    }

    QString HubController::validateNewProject(const QString& name, const QString& location) const
    {
        if (const std::optional<std::string> invalid = Project::ValidateName(name.toStdString()))
            return ToQString(*invalid);

        if (location.trimmed().isEmpty())
            return QStringLiteral("Choose a location");

        const fs::path folder = ToPath(location);
        if (!folder.is_absolute())
            return QStringLiteral("The location must be a full path");

        std::error_code error;
        if (!fs::is_directory(folder, error))
            return QStringLiteral("The location does not exist");

        const fs::path target = folder / ToPath(name);
        if (fs::exists(target, error) && !(fs::is_directory(target, error) && fs::is_empty(target, error)))
            return QStringLiteral("'%1' already exists in this location").arg(name);

        if (m_Templates.empty())
            return QStringLiteral("No project templates were found next to the hub");

        return {};
    }

    QString HubController::projectPath(const QString& location, const QString& name) const
    {
        return QDir::toNativeSeparators(PathToQString(ToPath(location) / ToPath(name)));
    }

    QString HubController::createProject(const QString& name, const QString& location, const QString& templateId)
    {
        if (QString problem = validateNewProject(name, location); !problem.isEmpty())
            return problem;

        const auto projectTemplate = std::ranges::find(m_Templates, templateId.toStdString(), &ProjectTemplate::Id);
        if (projectTemplate == m_Templates.end())
            return QStringLiteral("Choose a template");

        const Result<Project> project = Project::Create({
            .Name              = name.toStdString(),
            .Location          = ToPath(location),
            .TemplateDirectory = projectTemplate->Directory
        });

        if (!project)
            return ToQString(project.error());

        APP_INFO("Created project '{}' at '{}'", project->GetName(), ToUtf8(project->GetRoot()));

        if (const Result<> added = RecentProjects::Touch(*project); !added)
            reportError(QStringLiteral("Cannot update the project list: %1").arg(ToQString(added.error())));

        refresh();
        openProject(PathToQString(project->GetRoot()));
        return {};
    }

    QUrl HubController::pathToUrl(const QString& path) const
    {
        return QUrl::fromLocalFile(path);
    }

    QString HubController::urlToPath(const QUrl& url) const
    {
        return QDir::toNativeSeparators(url.toLocalFile());
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
