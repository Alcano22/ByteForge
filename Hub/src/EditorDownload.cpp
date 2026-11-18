#include "EditorDownload.h"
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
    }

    EditorDownload::EditorDownload(EditorRelease release, fs::path installsDirectory, QObject* parent)
        : QObject(parent),
          m_Release(std::move(release)),
          m_InstallsDirectory(std::move(installsDirectory)),
          m_DownloadPath(EditorInstalls::GetDownloadPath(m_Release.Version, m_InstallsDirectory))
    {
        connect(&m_Unpacking, &QFutureWatcher<Result<EditorInstall>>::finished, this, &EditorDownload::onUnpacked);
    }

    EditorDownload::~EditorDownload()
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

    void EditorDownload::start()
    {
        setStage(Stage::Manifest);

        m_Reply = m_Network.get(makeRequest(QUrl(ToQString(m_Release.ManifestUrl))));

        QNetworkReply* reply = m_Reply.data();
        connect(reply, &QNetworkReply::finished, this, [this, reply] { onManifestFinished(reply); });
    }

    void EditorDownload::cancel()
    {
        if (!m_Reply) return;

        m_Cancelled = true;
        m_Reply->abort();
    }

    void EditorDownload::onManifestFinished(QNetworkReply* reply)
    {
        reply->deleteLater();
        m_Reply = nullptr;

        if (reply->error() != QNetworkReply::NoError)
        {
            fail(m_Cancelled ? QString() : QStringLiteral("Cannot download the release's manifest: %1")
                                               .arg(reply->errorString()));
            return;
        }

        const QByteArray body = reply->readAll();
        const Result<EditorManifest> manifest =
            EditorManifest::Parse(std::string_view(body.constData(), static_cast<usize>(body.size())));
        if (!manifest)
        {
            fail(ToQString(manifest.error()));
            return;
        }

        if (manifest->Version != m_Release.Version)
        {
            fail(QStringLiteral("The release of %1 contains the manifest of %2")
                     .arg(ToQString(m_Release.Version.ToString()), ToQString(manifest->Version.ToString())));
            return;
        }

        const EditorPackage* package = manifest->FindPackage(GetCurrentPlatform());
        if (package == nullptr)
        {
            fail(QStringLiteral("ByteForge %1 has no package for this system (%2)")
                     .arg(ToQString(m_Release.Version.ToString()), ToQString(GetCurrentPlatform())));
            return;
        }
        m_Package = *package;

        std::error_code error;
        fs::create_directories(m_InstallsDirectory, error);

        m_File.setFileName(PathToQString(m_DownloadPath));
        if (error || !m_File.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            fail(QStringLiteral("Cannot write to %1").arg(PathToQString(m_InstallsDirectory)));
            return;
        }

        m_Received = 0;
        m_Total = static_cast<qint64>(m_Package.Size);
        m_Hash.reset();
        setStage(Stage::Package);

        const QUrl manifestUrl(ToQString(m_Release.ManifestUrl));
        const QUrl packageUrl = manifestUrl.resolved(QUrl(QStringLiteral("./") + ToQString(m_Package.File)));

        m_Reply = m_Network.get(makeRequest(packageUrl));

        QNetworkReply* packageReply = m_Reply.data();
        connect(packageReply, &QNetworkReply::readyRead, this, [this, packageReply]
        {
            if (!consume(packageReply->readAll()))
                packageReply->abort();
        });
        connect(packageReply, &QNetworkReply::finished, this, [this, packageReply] { onPackageFinished(packageReply); });
    }

    bool EditorDownload::consume(const QByteArray& data)
    {
        if (data.isEmpty())
            return true;

        m_Received += data.size();

        if (m_Received > m_Total)
        {
            m_AbortReason = QStringLiteral("The download is larger than the release says");
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

    void EditorDownload::onPackageFinished(QNetworkReply* reply)
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

        if (m_Received != m_Total)
        {
            fail(QStringLiteral("The download is incomplete (%1 of %2 bytes)").arg(m_Received).arg(m_Total));
            return;
        }

        const QString sha256 = QString::fromLatin1(m_Hash.result().toHex());
        if (sha256 != ToQString(m_Package.Sha256))
        {
            fail(QStringLiteral("The download is damaged: its SHA-256 does not match the release"));
            return;
        }

        setStage(Stage::Unpacking);

        m_Unpacking.setFuture(QtConcurrent::run(
            [archive = m_DownloadPath, version = m_Release.Version, installs = m_InstallsDirectory]
            {
                return EditorInstalls::Install(archive, version, installs);
            }));
    }

    void EditorDownload::onUnpacked()
    {
        const Result<EditorInstall> installed = m_Unpacking.result();
        removeDownloadFile();

        if (!installed)
        {
            fail(ToQString(installed.error()));
            return;
        }

        APP_INFO("Installed ByteForge {} to '{}'", installed->Version.ToString(), ToUtf8(installed->Directory));

        setStage(Stage::Done);
        m_Finished = true;
        Q_EMIT finished({});
    }

    void EditorDownload::setStage(const Stage stage)
    {
        m_Stage = stage;
        Q_EMIT progressChanged();
    }

    void EditorDownload::fail(const QString& error)
    {
        if (m_Finished) return;
        m_Finished = true;

        m_File.close();
        removeDownloadFile();

        Q_EMIT finished(error);
    }

    void EditorDownload::removeDownloadFile()
    {
        std::error_code ignored;
        fs::remove(m_DownloadPath, ignored);
    }

    QNetworkRequest EditorDownload::makeRequest(const QUrl& url) const
    {
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          QStringLiteral("ByteForgeHub/%1").arg(ToQString(GetEngineVersion())));
        request.setTransferTimeout(TimeoutMilliseconds);
        return request;
    }
}
