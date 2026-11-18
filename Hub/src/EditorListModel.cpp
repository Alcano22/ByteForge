#include "EditorListModel.h"
#include "QtConversions.h"

#include <QDir>

#include <utility>

namespace ByteForge::Hub
{
    EditorListModel::EditorListModel(QObject* parent)
        : QAbstractListModel(parent) {}

    int EditorListModel::rowCount(const QModelIndex& parent) const
    {
        return parent.isValid() ? 0 : static_cast<int>(m_Installs.size());
    }

    QVariant EditorListModel::data(const QModelIndex& index, const int role) const
    {
        if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
            return {};

        const EditorInstall& install = m_Installs[static_cast<usize>(index.row())];

        switch (role)
        {
            case Qt::DisplayRole:
            case VersionRole:     return ToQString(install.Version.ToString());
            case PathRole:        return QDir::toNativeSeparators(PathToQString(install.Directory));
            case DevelopmentRole: return install.Development;
            case PreReleaseRole:  return install.Version.IsPreRelease() && !install.Development;
            default:              return {};
        }
    }

    QHash<int, QByteArray> EditorListModel::roleNames() const
    {
        return {
            { VersionRole,     "version"     },
            { PathRole,        "path"        },
            { DevelopmentRole, "development" },
            { PreReleaseRole,  "preRelease"  }
        };
    }

    void EditorListModel::setInstalls(std::vector<EditorInstall> installs)
    {
        beginResetModel();
        m_Installs = std::move(installs);
        endResetModel();
    }
}
