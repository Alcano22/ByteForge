#include "ReleaseFeed.h"
#include "QtConversions.h"

#include <Engine/Core/Log.h>
#include <Engine/Core/Version.h>

#include <QDateTime>
#include <QLocale>
#include <QNetworkRequest>
#include <QUrl>

#include <string_view>
#include <utility>

namespace ByteForge::Hub
{
    namespace
    {
        constexpr int TimeoutMilliseconds = 15'000;

        constexpr int ReleasesPerPage = 50;
    }

    ReleaseFeed::ReleaseFeed(QString repository, QObject* parent)
        : QObject(parent), m_Repository(std::move(repository)) {}

    void ReleaseFeed::refresh()
    {
        if (isLoading()) return;

        QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/%1/releases?per_page=%2")
            .arg(m_Repository).arg(ReleasesPerPage)));

        request.setRawHeader("Accept", "application/vnd.github+json");
        request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          QStringLiteral("ByteForgeHub/%1").arg(ToQString(GetEngineVersion())));
        request.setTransferTimeout(TimeoutMilliseconds);

        m_Reply = m_Network.get(request);

        QNetworkReply* reply = m_Reply.data();
        connect(reply, &QNetworkReply::finished, this, [this, reply] { onFinished(reply); });

        Q_EMIT stateChanged();
    }

    void ReleaseFeed::onFinished(QNetworkReply* reply)
    {
        reply->deleteLater();
        m_Reply = nullptr;

        if (reply->error() != QNetworkReply::NoError)
        {
            m_Error = describeFailure(*reply);
            APP_WARN("Cannot check for editor versions: {}", m_Error.toStdString());
            Q_EMIT stateChanged();
            return;
        }

        const QByteArray body = reply->readAll();
        Result<std::vector<EditorRelease>> releases = EditorReleases::ParseGitHubReleases(
            std::string_view(body.constData(), static_cast<usize>(body.size())));

        if (!releases)
        {
            m_Error = ToQString(releases.error());
            Q_EMIT stateChanged();
            return;
        }

        m_Releases = std::move(*releases);
        m_Error.clear();
        m_Checked = true;

        Q_EMIT stateChanged();
        Q_EMIT releasesChanged();
    }

    QString ReleaseFeed::describeFailure(const QNetworkReply& reply) const
    {
        const int status = reply.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if ((status == 403 || status == 429) && reply.rawHeader("x-ratelimit-remaining") == "0")
        {
            const QDateTime reset = QDateTime::fromSecsSinceEpoch(reply.rawHeader("x-ratelimit-reset").toLongLong());
            return QStringLiteral("GitHub allows 60 checks per hour without an account. Try again at %1.")
                .arg(QLocale().toString(reset.toLocalTime().time(), QLocale::ShortFormat));
        }

        if (status == 404)
            return QStringLiteral("The repository %1 was not found on GitHub").arg(m_Repository);

        return QStringLiteral("Cannot reach GitHub: %1").arg(reply.errorString());
    }
}
