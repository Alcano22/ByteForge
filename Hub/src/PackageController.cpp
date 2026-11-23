#include "PackageController.h"
#include "QtConversions.h"

#include <Engine/Core/Log.h>

#include <QDesktopServices>
#include <QDir>
#include <QLocale>
#include <QUrl>

#include <system_error>

namespace ByteForge::Hub
{
    PackageController::PackageController(QObject* parent)
        : QObject(parent),
          m_DotNetDirectory(DotNetLocator::GetHubDirectory())
    {
        DotNetPackage::RemoveLeftovers(m_DotNetDirectory);
        refresh();
    }

    QString PackageController::packagesDirectory() const
    {
        return QDir::toNativeSeparators(PathToQString(m_DotNetDirectory.parent_path()));
    }

    QString PackageController::dotnetSource() const
    {
        if (!m_DotNet)
            return {};

        return m_DotNet->Source == DotNetSource::Hub ? QStringLiteral("hub") : QStringLiteral("system");
    }

    QString PackageController::dotnetVersion() const
    {
        return m_DotNet && m_DotNet->Sdk ? ToQString(m_DotNet->Sdk->ToString()) : QString();
    }

    QString PackageController::dotnetPath() const
    {
        return m_DotNet ? QDir::toNativeSeparators(PathToQString(m_DotNet->Root)) : QString();
    }

    double PackageController::progress() const
    {
        if (m_Download == nullptr || m_Download->stage() != SdkDownload::Stage::Archive || m_Download->total() <= 0)
            return -1.0;

        return static_cast<double>(m_Download->received()) / static_cast<double>(m_Download->total());
    }

    QString PackageController::status() const
    {
        if (m_Download == nullptr)
            return {};

        switch (m_Download->stage())
        {
            case SdkDownload::Stage::Metadata:
                return QStringLiteral("Asking Microsoft for the newest SDK...");
            case SdkDownload::Stage::Archive:
            {
                const QString received = QLocale().formattedDataSize(m_Download->received());
                if (m_Download->total() <= 0)
                    return QStringLiteral("Downloading .NET %1 SDK: %2").arg(m_Download->version(), received);

                return QStringLiteral("Downloading .NET %1 SDK: %2 of %3")
                    .arg(m_Download->version(), received, QLocale().formattedDataSize(m_Download->total()));
            }
            case SdkDownload::Stage::Unpacking:
                return QStringLiteral("Unpacking...");
            case SdkDownload::Stage::Done:
                return QStringLiteral("Installed");
        }
        return {};
    }

    void PackageController::refresh()
    {
        m_DotNet = DotNetLocator::Find(DotNetComponent::Sdk);
        Q_EMIT dotnetChanged();
    }

    void PackageController::installDotNet()
    {
        if (m_Download != nullptr)
        {
            reportError(QStringLiteral("The .NET SDK is already being installed"));
            return;
        }

        m_Download = new SdkDownload(m_DotNetDirectory, this);
        connect(m_Download, &SdkDownload::progressChanged, this, &PackageController::progressChanged);
        connect(m_Download, &SdkDownload::finished, this, &PackageController::onInstallFinished);

        APP_INFO("Installing the .NET {} SDK", DotNetMajorVersion);
        m_Download->start();
        Q_EMIT progressChanged();
    }

    void PackageController::cancel()
    {
        if (m_Download != nullptr)
            m_Download->cancel();
    }

    void PackageController::removeDotNet()
    {
        if (m_Download != nullptr)
        {
            reportError(QStringLiteral("Wait until the .NET SDK is installed"));
            return;
        }

        if (const Result<> removed = DotNetPackage::Remove(m_DotNetDirectory); !removed)
            reportError(QStringLiteral("Cannot remove the .NET SDK: %1").arg(ToQString(removed.error())));
        else
            APP_INFO("Removed the .NET SDK from '{}'", ToUtf8(m_DotNetDirectory));

        refresh();
    }

    void PackageController::openPackagesFolder()
    {
        const fs::path directory = m_DotNetDirectory.parent_path();

        std::error_code error;
        fs::create_directories(directory, error);
        if (error)
        {
            reportError(QStringLiteral("Cannot create %1: %2")
                            .arg(packagesDirectory(), QString::fromStdString(error.message())));
            return;
        }

        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(PathToQString(directory))))
            reportError(QStringLiteral("Cannot open %1").arg(packagesDirectory()));
    }

    void PackageController::reportError(const QString& message)
    {
        APP_WARN("{}", message.toStdString());
        Q_EMIT errorOccurred(message);
    }

    void PackageController::onInstallFinished(const QString& error)
    {
        SdkDownload* download = m_Download;
        m_Download = nullptr;

        download->deleteLater();

        if (download->wasCancelled())
            APP_INFO("Installing the .NET SDK was cancelled");
        else if (!error.isEmpty())
            reportError(QStringLiteral("Cannot install the .NET SDK: %1").arg(error));

        refresh();
        Q_EMIT progressChanged();
    }
}
