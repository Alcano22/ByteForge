#pragma once

#include "SdkDownload.h"

#include <Engine/Core/DotNetLocator.h>

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <optional>

namespace ByteForge::Hub
{
    class PackageController : public QObject
    {
        Q_OBJECT
        QML_ELEMENT
        QML_SINGLETON

        Q_PROPERTY(QString packagesDirectory READ packagesDirectory CONSTANT)
        Q_PROPERTY(int dotnetMajorVersion READ dotnetMajorVersion CONSTANT)

        Q_PROPERTY(QString dotnetSource READ dotnetSource NOTIFY dotnetChanged)
        Q_PROPERTY(QString dotnetVersion READ dotnetVersion NOTIFY dotnetChanged)
        Q_PROPERTY(QString dotnetPath READ dotnetPath NOTIFY dotnetChanged)

        Q_PROPERTY(bool busy READ busy NOTIFY progressChanged)
        Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
        Q_PROPERTY(QString status READ status NOTIFY progressChanged)
        Q_PROPERTY(bool canCancel READ canCancel NOTIFY progressChanged)

    public:
        explicit PackageController(QObject* parent = nullptr);

        [[nodiscard]] QString packagesDirectory() const;
        [[nodiscard]] int dotnetMajorVersion() const { return static_cast<int>(DotNetMajorVersion); }

        [[nodiscard]] QString dotnetSource() const;
        [[nodiscard]] QString dotnetVersion() const;
        [[nodiscard]] QString dotnetPath() const;

        [[nodiscard]] bool busy() const { return m_Download != nullptr; }
        [[nodiscard]] double progress() const;
        [[nodiscard]] QString status() const;
        [[nodiscard]] bool canCancel() const { return m_Download != nullptr && m_Download->canCancel(); }

        Q_INVOKABLE void refresh();

        Q_INVOKABLE void installDotNet();
        Q_INVOKABLE void cancel();
        Q_INVOKABLE void removeDotNet();

        Q_INVOKABLE void openPackagesFolder();

    Q_SIGNALS:
        void dotnetChanged();
        void progressChanged();
        void errorOccurred(const QString& message);

    private:
        void reportError(const QString& message);

        void onInstallFinished(const QString& error);

    private:
        fs::path m_DotNetDirectory;
        std::optional<DotNetInstallation> m_DotNet;

        SdkDownload* m_Download = nullptr;
    };
}
