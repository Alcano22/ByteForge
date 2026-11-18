#include "HubController.h"
#include "QtConversions.h"

#include <Engine/Core/Log.h>
#include <Engine/Core/SemanticVersion.h>
#include <Engine/Core/Version.h>
#include <Engine/Core/FileLock.h>
#include <Engine/Project/Project.h>
#include <Engine/Project/ProjectSettings.h>
#include <Engine/Project/ProjectTemplate.h>
#include <Engine/Project/RecentProjects.h>
#include <HubCore/EditorChoice.h>

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QProcess>
#include <QStandardPaths>
#include <QVariantMap>

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <optional>
#include <system_error>
#include <utility>

namespace ByteForge::Hub
{
    namespace
    {
        constexpr i32 MaxNameSuggestions = 1000;

        fs::path QueryInstallsDirectory()
        {
            return ToPath(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)) /
                   "ByteForge" / "Editors";
        }

        std::string ReadProjectVersion(const Project& project)
        {
            const Result<ProjectSettings> settings = project.LoadSettings();
            return settings ? settings->EngineVersion : std::string();
        }
    }

    HubController::HubController(QObject* parent)
        : QObject(parent),
          m_InstallsDirectory(QueryInstallsDirectory()),
          m_ReleaseFeed(QStringLiteral(BYTEFORGE_RELEASE_REPOSITORY))
    {
        m_Filtered.setSourceModel(&m_Projects);
        m_Filtered.setFilterRole(ProjectListModel::NameRole);
        m_Filtered.setFilterCaseSensitivity(Qt::CaseInsensitive);

        EditorInstalls::RemoveLeftovers(m_InstallsDirectory);

        reloadEditors();

        connect(&m_ReleaseFeed, &ReleaseFeed::stateChanged, this, &HubController::releaseStateChanged);
        connect(&m_ReleaseFeed, &ReleaseFeed::releasesChanged, this, &HubController::updateReleaseModel);
        m_ReleaseFeed.refresh();
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

    QString HubController::installsDirectory() const
    {
        return QDir::toNativeSeparators(PathToQString(m_InstallsDirectory));
    }

    void HubController::setShowPreReleases(const bool show)
    {
        if (show == m_ShowPreReleases) return;

        m_ShowPreReleases = show;
        updateReleaseModel();
        Q_EMIT showPreReleasesChanged();
    }

    QString HubController::installingVersion() const
    {
        return m_Download != nullptr ? ToQString(m_Download->version().ToString()) : QString();
    }

    double HubController::installProgress() const
    {
        if (m_Download == nullptr || m_Download->stage() != EditorDownload::Stage::Package || m_Download->total() <= 0)
            return -1.0;

        return static_cast<double>(m_Download->received()) / static_cast<double>(m_Download->total());
    }

    QString HubController::installStatus() const
    {
        if (m_Download == nullptr)
            return {};

        switch (m_Download->stage())
        {
            case EditorDownload::Stage::Manifest:
                return QStringLiteral("Preparing...");
            case EditorDownload::Stage::Package:
                return QStringLiteral("Downloading %1 of %2").arg(QLocale().formattedDataSize(m_Download->received()),
                                                                  QLocale().formattedDataSize(m_Download->total()));
            case EditorDownload::Stage::Unpacking:
                return QStringLiteral("Unpacking...");
            case EditorDownload::Stage::Done:
                return QStringLiteral("Installed");
        }
        return {};
    }

    void HubController::refresh()
    {
        m_Projects.reload();
        reloadEditors();
    }

    void HubController::checkForReleases() { m_ReleaseFeed.refresh(); }

    void HubController::installEditor(const QString& version)
    {
        if (m_Download != nullptr)
        {
            reportError(QStringLiteral("ByteForge %1 is being installed, wait until it is done")
                            .arg(installingVersion()));
            return;
        }

        const std::optional<SemanticVersion> wanted = SemanticVersion::Parse(version.toStdString());
        const auto release = wanted ? std::ranges::find(m_ReleaseFeed.releases(), *wanted, &EditorRelease::Version)
                                    : m_ReleaseFeed.releases().end();
        if (release == m_ReleaseFeed.releases().end())
        {
            reportError(QStringLiteral("ByteForge %1 is not published on GitHub").arg(version));
            return;
        }

        m_Download = new EditorDownload(*release, m_InstallsDirectory, this);
        connect(m_Download, &EditorDownload::progressChanged, this, &HubController::installChanged);
        connect(m_Download, &EditorDownload::finished, this, &HubController::onInstallFinished);

        APP_INFO("Installing ByteForge {}", release->Version.ToString());
        m_Download->start();
        Q_EMIT installChanged();
    }

    void HubController::cancelInstall()
    {
        if (m_Download != nullptr)
            m_Download->cancel();
    }

    void HubController::installAndOpen(const QString& version, const QString& projectPath)
    {
        if (m_Download != nullptr)
        {
            reportError(QStringLiteral("ByteForge %1 is being installed, wait until it is done")
                            .arg(installingVersion()));
            return;
        }

        installEditor(version);

        if (m_Download != nullptr)
            m_OpenAfterInstall = projectPath;
    }

    bool HubController::isPublished(const QString& version) const
    {
        const std::optional<SemanticVersion> wanted = SemanticVersion::Parse(version.toStdString());
        return wanted && std::ranges::find(m_ReleaseFeed.releases(), *wanted, &EditorRelease::Version) !=
                         m_ReleaseFeed.releases().end();
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

        if (!ensureNotOpen(*project))
            return;

        const std::optional<SemanticVersion> version = SemanticVersion::Parse(ReadProjectVersion(*project));

        const EditorInstall* editor = version ? FindEditorFor(m_Editors, *version) : FindDefaultEditor(m_Editors);
        if (editor != nullptr)
        {
            launchEditor(*project, *editor);
            return;
        }

        if (!version)
        {
            reportError(QStringLiteral("No editor is installed. Install one on the Installs page."));
            return;
        }

        Q_EMIT editorMissing(path, ToQString(project->GetName()), ToQString(version->ToString()));
    }

    void HubController::openProjectWith(const QString& path, const QString& editorPath)
    {
        const Result<Project> project = Project::Open(ToPath(path));
        if (!project)
        {
            reportError(QStringLiteral("Cannot open the project: %1").arg(ToQString(project.error())));
            refresh();
            return;
        }

        const EditorInstall* editor = findEditor(editorPath);
        if (editor == nullptr)
        {
            reportError(QStringLiteral("%1 is not an installed editor").arg(editorPath));
            reloadEditors();
            return;
        }

        const std::string projectVersion = ReadProjectVersion(*project);
        if (GetVersionChange(projectVersion, editor->Version) == VersionChange::Downgrade)
        {
            reportError(QStringLiteral("'%1' belongs to ByteForge %2 and cannot be opened with the older %3")
                            .arg(ToQString(project->GetName()), ToQString(projectVersion),
                                 ToQString(editor->Version.ToString())));
            return;
        }

        if (!ensureNotOpen(*project)) return;

        launchEditor(*project, *editor);
    }

    QString HubController::versionChange(const QString& projectVersion, const QString& editorVersion) const
    {
        const std::optional<SemanticVersion> editor = SemanticVersion::Parse(editorVersion.toStdString());
        if (!editor)
            return QStringLiteral("unknown");

        switch (GetVersionChange(projectVersion.toStdString(), *editor))
        {
            case VersionChange::Same:      return QStringLiteral("same");
            case VersionChange::Upgrade:   return QStringLiteral("upgrade");
            case VersionChange::Downgrade: return QStringLiteral("downgrade");
            case VersionChange::Unknown:   return QStringLiteral("unknown");
        }
        return QStringLiteral("unknown");
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

    void HubController::removeEditor(const QString& path)
    {
        const fs::path directory = ToPath(path);
        const auto install = std::ranges::find(m_Editors, directory, &EditorInstall::Directory);

        if (install == m_Editors.end())
            reportError(QStringLiteral("%1 is not an installed editor").arg(path));
        else if (const Result<> removed = EditorInstalls::Remove(*install, m_InstallsDirectory); !removed)
            reportError(QStringLiteral("Cannot remove the editor: %1").arg(ToQString(removed.error())));
        else
            APP_INFO("Removed ByteForge {} from '{}'", install->Version.ToString(), ToUtf8(directory));

        reloadEditors();
    }

    void HubController::openInstallsFolder()
    {
        std::error_code error;
        fs::create_directories(m_InstallsDirectory, error);
        if (error)
        {
            reportError(QStringLiteral("Cannot create %1: %2")
                .arg(installsDirectory(), QString::fromStdString(error.message())));
            return;
        }

        showInFolder(installsDirectory());
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

        return {};
    }

    QString HubController::projectPath(const QString& location, const QString& name) const
    {
        return QDir::toNativeSeparators(PathToQString(ToPath(location) / ToPath(name)));
    }

    QVariantList HubController::templatesFor(const QString& editorPath) const
    {
        QVariantList items;

        const EditorInstall* editor = findEditor(editorPath);
        if (editor == nullptr)
            return items;

        for (const ProjectTemplate& projectTemplate : ProjectTemplate::FindAll(editor->GetTemplateDirectory()))
        {
            items.append(QVariantMap{
                { QStringLiteral("id"),          ToQString(projectTemplate.Id)          },
                { QStringLiteral("name"),        ToQString(projectTemplate.Name)        },
                { QStringLiteral("description"), ToQString(projectTemplate.Description) }
            });
        }

        return items;
    }

    QString HubController::createProject(const QString& name, const QString& location,
                                         const QString& templateId, const QString& editorPath)
    {
        if (QString problem = validateNewProject(name, location); !problem.isEmpty())
            return problem;

        const EditorInstall* found = findEditor(editorPath);
        if (found == nullptr)
            return QStringLiteral("Choose an editor version");

        const EditorInstall editor = *found;

        const std::vector<ProjectTemplate> templates = ProjectTemplate::FindAll(editor.GetTemplateDirectory());
        const auto projectTemplate = std::ranges::find(templates, templateId.toStdString(), &ProjectTemplate::Id);
        if (projectTemplate == templates.end())
            return QStringLiteral("Choose a template");

        const Result<Project> project = Project::Create({
            .Name              = name.toStdString(),
            .Location          = ToPath(location),
            .TemplateDirectory = projectTemplate->Directory,
            .EngineVersion     = editor.Version.ToString()
        });

        if (!project)
            return ToQString(project.error());

        APP_INFO("Created project '{}' for ByteForge {} at '{}'", project->GetName(), editor.Version.ToString(),
                 ToUtf8(project->GetRoot()));

        if (const Result<> added = RecentProjects::Touch(*project); !added)
            reportError(QStringLiteral("Cannot update the project list: %1").arg(ToQString(added.error())));

        refresh();

        launchEditor(*project, editor);
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

    void HubController::reloadEditors()
    {
        m_Editors.clear();

        const SemanticVersion hubVersion = SemanticVersion::Parse(GetEngineVersion()).value_or(SemanticVersion{});
        const fs::path hubDirectory = ToPath(QCoreApplication::applicationDirPath());

        if (std::optional<EditorInstall> development = EditorInstalls::FindDevelopment(hubDirectory, hubVersion))
            m_Editors.push_back(std::move(*development));

        std::ranges::move(EditorInstalls::FindInstalled(m_InstallsDirectory), std::back_inserter(m_Editors));

        m_EditorModel.setInstalls(m_Editors);

        updateReleaseModel();
    }

    const EditorInstall* HubController::findEditor(const QString& path) const
    {
        if (path.isEmpty())
            return nullptr;

        const auto editor = std::ranges::find(m_Editors, ToPath(path), &EditorInstall::Directory);
        return editor != m_Editors.end() ? &*editor : nullptr;
    }

    bool HubController::ensureNotOpen(const Project& project)
    {
        const Result<Scope<FileLock>> lock = project.AcquireEditorLock();
        if (lock)
            return true;

        reportError(ToQString(lock.error()));
        return false;
    }

    void HubController::launchEditor(const Project& project, const EditorInstall& editor)
    {
        const QString executable = PathToQString(editor.GetExecutable());
        if (!QFileInfo::exists(executable))
        {
            reportError(QStringLiteral("The editor was not found at %1").arg(QDir::toNativeSeparators(executable)));
            reloadEditors();
            return;
        }

        APP_INFO("Opening '{}' with ByteForge {}{}",
                 project.GetName(), editor.Version.ToString(), editor.Development ? " (development)" : "");

        if (!QProcess::startDetached(executable, { QStringLiteral("--project"), PathToQString(project.GetRoot()) }))
            reportError(QStringLiteral("Cannot start the editor for '%1'").arg(ToQString(project.GetName())));
    }

    void HubController::updateReleaseModel()
    {
        std::vector<EditorRelease> available;

        m_PublishedReleaseCount = 0;
        m_HiddenPreReleaseCount = 0;

        for (const EditorRelease& release : m_ReleaseFeed.releases())
        {
            if (release.PreRelease && !m_ShowPreReleases)
            {
                ++m_HiddenPreReleaseCount;
                continue;
            }

            ++m_PublishedReleaseCount;

            const bool installed = std::ranges::any_of(m_Editors, [&](const EditorInstall& install)
            {
                return !install.Development && install.Version == release.Version;
            });

            if (!installed)
                available.push_back(release);
        }

        m_ReleaseModel.setReleases(std::move(available));
        Q_EMIT releasesUpdated();
    }

    void HubController::onInstallFinished(const QString& error)
    {
        EditorDownload* download = m_Download;
        m_Download = nullptr;

        download->deleteLater();

        const QString openAfterwards = std::exchange(m_OpenAfterInstall, QString());
        const bool installed = error.isEmpty() && !download->wasCancelled();

        if (download->wasCancelled())
            APP_INFO("Installing ByteForge {} was cancelled", download->version().ToString());
        else if (!error.isEmpty())
        {
            reportError(QStringLiteral("Cannot install ByteForge %1: %2")
                            .arg(ToQString(download->version().ToString()), error));
        }

        reloadEditors();
        Q_EMIT installChanged();

        if (installed && !openAfterwards.isEmpty())
            openProject(openAfterwards);
    }
}
