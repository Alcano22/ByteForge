#include "ReleaseListModel.h"
#include "QtConversions.h"

#include <utility>

namespace ByteForge::Hub
{
    ReleaseListModel::ReleaseListModel(QObject* parent)
        : QAbstractListModel(parent) {}

    int ReleaseListModel::rowCount(const QModelIndex& parent) const
    {
        return parent.isValid() ? 0 : static_cast<int>(m_Releases.size());
    }

    QVariant ReleaseListModel::data(const QModelIndex& index, const int role) const
    {
        if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
            return {};

        const EditorRelease& release = m_Releases[static_cast<usize>(index.row())];

        switch (role)
        {
            case Qt::DisplayRole:
            case VersionRole:    return ToQString(release.Version.ToString());
            case PreReleaseRole: return release.PreRelease;
            case PageUrlRole:    return ToQString(release.PageUrl);
            default:             return {};
        }
    }

    QHash<int, QByteArray> ReleaseListModel::roleNames() const
    {
        return {
            { VersionRole,    "version"    },
            { PreReleaseRole, "preRelease" },
            { PageUrlRole,    "pageUrl"    }
        };
    }

    void ReleaseListModel::setReleases(std::vector<EditorRelease> releases)
    {
        beginResetModel();
        m_Releases = std::move(releases);
        endResetModel();
    }
}
