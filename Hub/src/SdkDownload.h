#pragma once

#include <HubCore/DotNetPackage.h>

#include <QCryptographicHash>
#include <QFile>
#include <QFutureWatcher>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

namespace ByteForge::Hub
{
    class SdkDownload : public QObject
    {
        Q_OBJECT

    public:
        enum class Stage : u8
        {
            Metadata,
            Archive,
            Unpacking,
            Done
        };

        explicit SdkDownload(fs::path directory, QObject* parent = nullptr);
        ~SdkDownload() override;

        SdkDownload(const SdkDownload&) = delete;
        SdkDownload& operator=(const SdkDownload&) = delete;

        void start();

        void cancel();
        [[nodiscard]] bool canCancel() const { return !m_Reply.isNull(); }
        [[nodiscard]] bool wasCancelled() const { return m_Cancelled; }

        [[nodiscard]] QString version() const;
        [[nodiscard]] Stage stage() const { return m_Stage; }
        [[nodiscard]] qint64 received() const { return m_Received; }
        [[nodiscard]] qint64 total() const { return m_Total; }

        Q_SIGNALS:
            void progressChanged();

        void finished(const QString& error);

    private:
        void onMetadataFinished(QNetworkReply* reply);
        void onArchiveFinished(QNetworkReply* reply);
        void onUnpacked();

        bool consume(const QByteArray& data);

        void setStage(Stage stage);
        void fail(const QString& error);
        void removeDownloadFile();

        [[nodiscard]] QNetworkRequest makeRequest(const QUrl& url) const;

    private:
        fs::path m_Directory;
        fs::path m_DownloadPath;

        DotNetDownload m_Download;
        bool m_HasDownload = false;

        QNetworkAccessManager m_Network;
        QPointer<QNetworkReply> m_Reply;
        QFile m_File;
        QCryptographicHash m_Hash{ QCryptographicHash::Sha512 };
        QFutureWatcher<Result<DotNetInstallation>> m_Unpacking;

        Stage m_Stage = Stage::Metadata;
        qint64 m_Received = 0;
        qint64 m_Total = -1;

        QString m_AbortReason;
        bool m_Cancelled = false;
        bool m_Finished = false;
    };
}
