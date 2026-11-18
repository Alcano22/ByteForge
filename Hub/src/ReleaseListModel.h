#pragma once

#include <HubCore/EditorReleases.h>

#include <QAbstractListModel>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <vector>

namespace ByteForge::Hub
{
    class ReleaseListModel : public QAbstractListModel
    {
        Q_OBJECT
        QML_ELEMENT
        QML_UNCREATABLE("Provided by HubController")

    public:
        enum Role
        {
            VersionRole = Qt::UserRole + 1,
            PreReleaseRole,
            PageUrlRole
        };
        Q_ENUM(Role)

        explicit ReleaseListModel(QObject* parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
        [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        void setReleases(std::vector<EditorRelease> releases);

    private:
        std::vector<EditorRelease> m_Releases;
    };
}
