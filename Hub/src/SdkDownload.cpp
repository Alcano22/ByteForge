#include "SdkDownload.h"
#include "QtConversions.h"

#include <Engine/Core/Log.h>
#include <Engine/Core/Version.h>

#include <QtConcurrent/QtConcurrentRun>

#include <string_view>
#include <system_error>
#include <utility>

namespace ByteForge::Hub
{
    namespace
    {
        constexpr int TimeoutMilliseconds = 30'000;

        constexpr qint64 MaxArchiveSize = 2LL * 1024 * 1024 * 1024;
    }

    SdkDownload::SdkDownload(fs::path directory, QObject* parent)
        : QObject(parent),
          m_Directory(std::move(directory)),
          m_DownloadPath(DotNetPackage::GetDownloadPath(m_Directory))
    {
        connect(&m_Unpacking, &QFutureWatcher<Result<DotNetInstallation>>::finished, this, &SdkDownload::onUnpacked);
    }

    SdkDownload::~SdkDownload()
    {
        if (m_Reply)
        {
            m_Reply->disconnect(this);
            m_Reply->abort();
        }

        m_Unpacking.waitForFinished();

        m_File.close();
        removeDownloadFile();
    }

    void SdkDownload::start()
    {
        setStage(Stage::Metadata);

        m_Reply = m_Network.get(makeRequest(QUrl(ToQString(DotNetPackage::GetReleaseMetadataUrl()))));

        QNetworkReply* reply = m_Reply.data();
        connect(reply, &QNetworkReply::finished, this, [this, reply] { onMetadataFinished(reply); });
    }

    void SdkDownload::cancel()
    {
        if (!m_Reply) return;

        m_Cancelled = true;
        m_Reply->abort();
    }

    QString SdkDownload::version() const
    {
        return m_HasDownload ? ToQString(m_Download.Version.ToString()) : QString();
    }

    void SdkDownload::onMetadataFinished(QNetworkReply* reply)
    {
        reply->deleteLater();
        m_Reply = nullptr;

        if (reply->error() != QNetworkReply::NoError)
        {
            fail(m_Cancelled ? QString() : QStringLiteral("Cannot ask Microsoft for the newest .NET SDK: %1")
                                               .arg(reply->errorString()));
            return;
        }

        const QByteArray body = reply->readAll();
        Result<DotNetDownload> download = DotNetPackage::ParseLatestSdk(
            std::string_view(body.constData(), static_cast<usize>(body.size())), DotNetPackage::GetCurrentRid());
        if (!download)
        {
            fail(ToQString(download.error()));
            return;
        }

        m_Download = std::move(*download);
        m_HasDownload = true;

        std::error_code error;
        fs::create_directories(m_DownloadPath.parent_path(), error);

        m_File.setFileName(PathToQString(m_DownloadPath));
        if (error || !m_File.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            fail(QStringLiteral("Cannot write to %1").arg(PathToQString(m_DownloadPath.parent_path())));
            return;
        }

        m_Received = 0;
        m_Total = -1;
        m_Hash.reset();
        setStage(Stage::Archive);

        APP_INFO("Downloading the .NET {} SDK from '{}'", m_Download.Version.ToString(), m_Download.Url);

        m_Reply = m_Network.get(makeRequest(QUrl(ToQString(m_Download.Url))));

        QNetworkReply* archiveReply = m_Reply.data();
        connect(archiveReply, &QNetworkReply::readyRead, this, [this, archiveReply]
        {
            if (!consume(archiveReply->readAll()))
                archiveReply->abort();
        });
        connect(archiveReply, &QNetworkReply::downloadProgress, this, [this](qint64, const qint64 bytes)
        {
            if (bytes > 0 && bytes != m_Total)
            {
                m_Total = bytes;
                Q_EMIT progressChanged();
            }
        });
        connect(archiveReply, &QNetworkReply::finished, this, [this, archiveReply] { onArchiveFinished(archiveReply); });
    }

    bool SdkDownload::consume(const QByteArray& data)
    {
        if (data.isEmpty())
            return true;

        m_Received += data.size();

        if (m_Received > MaxArchiveSize)
        {
            m_AbortReason = QStringLiteral("The download is far larger than a .NET SDK");
            return false;
        }

        if (m_File.write(data) != data.size())
        {
            m_AbortReason = QStringLiteral("Cannot write the download: %1").arg(m_File.errorString());
            return false;
        }

        m_Hash.addData(data);
        Q_EMIT progressChanged();
        return true;
    }

    void SdkDownload::onArchiveFinished(QNetworkReply* reply)
    {
        reply->deleteLater();
        m_Reply = nullptr;

        if (reply->error() == QNetworkReply::NoError && !consume(reply->readAll()))
        {
            fail(m_AbortReason);
            return;
        }

        if (reply->error() != QNetworkReply::NoError)
        {
            if (m_Cancelled)
                fail({});
            else if (!m_AbortReason.isEmpty())
                fail(m_AbortReason);
            else
                fail(QStringLiteral("The download failed: %1").arg(reply->errorString()));
            return;
        }

        m_File.close();
        if (m_File.error() != QFileDevice::NoError)
        {
            fail(QStringLiteral("Cannot write the download: %1").arg(m_File.errorString()));
            return;
        }

        if (m_Total > 0 && m_Received != m_Total)
        {
            fail(QStringLiteral("The download is incomplete (%1 of %2 bytes)").arg(m_Received).arg(m_Total));
            return;
        }

        const QString sha512 = QString::fromLatin1(m_Hash.result().toHex());
        if (sha512 != ToQString(m_Download.Sha512))
        {
            fail(QStringLiteral("The download is damaged: its SHA-512 does not match Microsoft's"));
            return;
        }

        setStage(Stage::Unpacking);

        m_Unpacking.setFuture(QtConcurrent::run([archive = m_DownloadPath, directory = m_Directory]
        {
            return DotNetPackage::Install(archive, directory);
        }));
    }

    void SdkDownload::onUnpacked()
    {
        const Result<DotNetInstallation> installed = m_Unpacking.result();
        removeDownloadFile();

        if (!installed)
        {
            fail(ToQString(installed.error()));
            return;
        }

        APP_INFO("Installed the .NET {} SDK to '{}'",
                 installed->Sdk ? installed->Sdk->ToString() : m_Download.Version.ToString(),
                 ToUtf8(installed->Root));

        setStage(Stage::Done);
        m_Finished = true;
        Q_EMIT finished({});
    }

    void SdkDownload::setStage(const Stage stage)
    {
        m_Stage = stage;
        Q_EMIT progressChanged();
    }

    void SdkDownload::fail(const QString& error)
    {
        if (m_Finished) return;
        m_Finished = true;

        m_File.close();
        removeDownloadFile();

        Q_EMIT finished(error);
    }

    void SdkDownload::removeDownloadFile()
    {
        std::error_code ignored;
        fs::remove(m_DownloadPath, ignored);
    }

    QNetworkRequest SdkDownload::makeRequest(const QUrl& url) const
    {
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          QStringLiteral("ByteForgeHub/%1").arg(ToQString(GetEngineVersion())));
        request.setTransferTimeout(TimeoutMilliseconds);
        return request;
    }
}
