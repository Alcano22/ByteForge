#pragma once

#include <HubCore/EditorReleases.h>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QString>

#include <vector>

namespace ByteForge::Hub
{
    class ReleaseFeed : public QObject
    {
        Q_OBJECT

    public:
        explicit ReleaseFeed(QString repository, QObject* parent = nullptr);

        void refresh();

        [[nodiscard]] bool isLoading() const { return !m_Reply.isNull(); }
        [[nodiscard]] bool hasChecked() const { return m_Checked; }
        [[nodiscard]] const QString& errorMessage() const { return m_Error; }

        [[nodiscard]] const std::vector<EditorRelease>& releases() const { return m_Releases; }

    Q_SIGNALS:
        void stateChanged();
        void releasesChanged();

    private:
        void onFinished(QNetworkReply* reply);

        [[nodiscard]] QString describeFailure(const QNetworkReply& reply) const;

    private:
        QString m_Repository;
        QNetworkAccessManager m_Network;
        QPointer<QNetworkReply> m_Reply;

        std::vector<EditorRelease> m_Releases;
        QString m_Error;
        bool m_Checked = false;
    };
}
