#include "ProjectListModel.h"
#include "QtConversions.h"

#include <Engine/Project/Project.h>
#include <Engine/Project/ProjectSettings.h>
#include <Engine/Project/RecentProjects.h>

#include <QDir>

#include <chrono>

namespace ByteForge::Hub
{
    ProjectListModel::ProjectListModel(QObject* parent)
        : QAbstractListModel(parent)
    {
        reload();
    }

    int ProjectListModel::rowCount(const QModelIndex& parent) const
    {
        return parent.isValid() ? 0 : static_cast<int>(m_Entries.size());
    }

    QVariant ProjectListModel::data(const QModelIndex& index, const int role) const
    {
        if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
            return {};

        const Entry& entry = m_Entries[static_cast<usize>(index.row())];

        switch (role)
        {
            case Qt::DisplayRole:
            case NameRole:          return entry.Name;
            case PathRole:          return entry.Path;
            case LastOpenedRole:    return entry.LastOpened;
            case MissingRole:       return entry.Missing;
            case EngineVersionRole: return entry.EngineVersion;
            default:                return {};
        }
    }

    QHash<int, QByteArray> ProjectListModel::roleNames() const
    {
        return {
            { NameRole,          "name"          },
            { PathRole,          "path"          },
            { LastOpenedRole,    "lastOpened"    },
            { MissingRole,       "missing"       },
            { EngineVersionRole, "engineVersion" }
        };
    }

    void ProjectListModel::reload()
    {
        beginResetModel();
        m_Entries.clear();

        for (const RecentProject& project : RecentProjects::Load())
        {
            const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                project.LastOpened.time_since_epoch()).count();

            Entry entry{
                .Name       = ToQString(project.Name),
                .Path       = QDir::toNativeSeparators(PathToQString(project.Path)),
                .LastOpened = QDateTime::fromSecsSinceEpoch(seconds),
                .Missing    = !Project::IsProjectDirectory(project.Path)
            };

            if (!entry.Missing)
            {
                if (const Result<ProjectSettings> settings = ProjectSettings::Load(project.Path / ProjectSettings::FileName))
                {
                    entry.Name = ToQString(settings->Name);
                    entry.EngineVersion = ToQString(settings->EngineVersion);
                }
            }

            m_Entries.push_back(std::move(entry));
        }

        endResetModel();
    }
}
