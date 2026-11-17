#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <vector>

namespace ByteForge::Hub
{
    class ProjectListModel : public QAbstractListModel
    {
        Q_OBJECT
        QML_ELEMENT
        QML_UNCREATABLE("Provided by HubController")

    public:
        enum Role
        {
            NameRole = Qt::UserRole + 1,
            PathRole,
            LastOpenedRole,
            MissingRole,
            EngineVersionRole
        };
        Q_ENUM(Role)

        explicit ProjectListModel(QObject* parent = nullptr);

        [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
        [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        void reload();

    private:
        struct Entry
        {
            QString Name;
            QString Path;
            QDateTime LastOpened;
            bool Missing = false;
            QString EngineVersion;
        };

        std::vector<Entry> m_Entries;
    };
}
