#pragma once

#include <HubCore/EditorInstalls.h>

#include <QAbstractListModel>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <vector>

namespace ByteForge::Hub
{
    class EditorListModel : public QAbstractListModel
    {
        Q_OBJECT
        QML_ELEMENT
        QML_UNCREATABLE("Provided by HubController")

    public:
        enum Role
        {
            VersionRole = Qt::UserRole + 1,
            PathRole,
            DevelopmentRole,
            PreReleaseRole
        };
        Q_ENUM(Role)

        explicit EditorListModel(QObject* parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
        [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        void setInstalls(std::vector<EditorInstall> installs);

    private:
        std::vector<EditorInstall> m_Installs;
    };
}
