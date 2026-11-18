#pragma once

#include <HubCore/EditorInstalls.h>
#include <HubCore/EditorManifest.h>
#include <HubCore/EditorReleases.h>

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
    class EditorDownload : public QObject
    {
        Q_OBJECT

    public:
        enum class Stage : u8
        {
            Manifest,
            Package,
            Unpacking,
            Done
        };

        EditorDownload(EditorRelease release, fs::path installsDirectory, QObject* parent = nullptr);
        ~EditorDownload() override;

        EditorDownload(const EditorDownload&) = delete;
        EditorDownload& operator=(const EditorDownload&) = delete;

        void start();

        void cancel();
        [[nodiscard]] bool canCancel() const { return !m_Reply.isNull(); }
        [[nodiscard]] bool wasCancelled() const { return m_Cancelled; }

        [[nodiscard]] const SemanticVersion& version() const { return m_Release.Version; }
        [[nodiscard]] Stage stage() const { return m_Stage; }
        [[nodiscard]] qint64 received() const { return m_Received; }
        [[nodiscard]] qint64 total() const { return m_Total; }

    Q_SIGNALS:
        void progressChanged();

        void finished(const QString& error);

    private:
        void onManifestFinished(QNetworkReply* reply);
        void onPackageFinished(QNetworkReply* reply);
        void onUnpacked();

        bool consume(const QByteArray& data);

        void setStage(Stage stage);
        void fail(const QString& error);
        void removeDownloadFile();

        [[nodiscard]] QNetworkRequest makeRequest(const QUrl& url) const;

    private:
        EditorRelease m_Release;
        fs::path m_InstallsDirectory;
        fs::path m_DownloadPath;
        EditorPackage m_Package;

        QNetworkAccessManager m_Network;
        QPointer<QNetworkReply> m_Reply;
        QFile m_File;
        QCryptographicHash m_Hash{ QCryptographicHash::Sha256 };
        QFutureWatcher<Result<EditorInstall>> m_Unpacking;

        Stage m_Stage = Stage::Manifest;
        qint64 m_Received = 0;
        qint64 m_Total = 0;

        QString m_AbortReason;
        bool m_Cancelled = false;
        bool m_Finished = false;
    };
}
