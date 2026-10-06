// SPDX-License-Identifier: GPL-3.0-or-later
#include "UpdateChecker.h"

#include <KLocalizedString>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <algorithm>

namespace
{
const QString kReleasesPage = QStringLiteral("https://github.com/MineraleYT/kTomato/releases");

struct ParsedVersion {
    QList<int> numbers;
    QStringList preRelease; ///< Empty for a release.
};

ParsedVersion parseVersion(const QString &raw)
{
    QString cleaned = raw.trimmed();
    if (cleaned.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        cleaned.remove(0, 1);
    }
    const int plusIdx = cleaned.indexOf(QLatin1Char('+'));
    if (plusIdx >= 0) {
        cleaned.truncate(plusIdx); // build metadata does not order versions
    }
    ParsedVersion version;
    const int dashIdx = cleaned.indexOf(QLatin1Char('-'));
    if (dashIdx >= 0) {
        version.preRelease = cleaned.mid(dashIdx + 1).split(QLatin1Char('.'), Qt::SkipEmptyParts);
        cleaned.truncate(dashIdx);
    }
    const auto parts = cleaned.split(QLatin1Char('.'));
    version.numbers.reserve(parts.size());
    for (const QString &p : parts) {
        bool ok = false;
        const int n = p.toInt(&ok);
        version.numbers.append(ok ? n : 0);
    }
    return version;
}

int sign(qint64 n)
{
    return n > 0 ? 1 : (n < 0 ? -1 : 0);
}

/// Compares runs of digits by value and everything else as text: "rc2" < "rc10".
int naturalCompare(const QString &a, const QString &b)
{
    int i = 0;
    int j = 0;
    while (i < a.size() && j < b.size()) {
        if (a.at(i).isDigit() && b.at(j).isDigit()) {
            int ei = i;
            int ej = j;
            while (ei < a.size() && a.at(ei).isDigit()) {
                ++ei;
            }
            while (ej < b.size() && b.at(ej).isDigit()) {
                ++ej;
            }
            const qint64 na = a.mid(i, ei - i).left(18).toLongLong();
            const qint64 nb = b.mid(j, ej - j).left(18).toLongLong();
            if (na != nb) {
                return sign(na - nb);
            }
            i = ei;
            j = ej;
        } else {
            if (a.at(i) != b.at(j)) {
                return a.at(i).toLower() < b.at(j).toLower() ? -1 : 1;
            }
            ++i;
            ++j;
        }
    }
    return sign(qint64(a.size() - i) - qint64(b.size() - j));
}
} // namespace

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
{
}

UpdateChecker::~UpdateChecker() = default;

QString UpdateChecker::currentVersion() const
{
    const QByteArray overrideVer = qgetenv("KTOMATO_OVERRIDE_VERSION");
    if (!overrideVer.isEmpty()) {
        return QString::fromUtf8(overrideVer);
    }
#ifdef PROJECT_VERSION
    return QStringLiteral(PROJECT_VERSION);
#else
    return QCoreApplication::applicationVersion();
#endif
}

void UpdateChecker::reset()
{
    // Forget a request in flight: its reply must not overwrite the state later on.
    if (QNetworkReply *reply = m_reply.data()) {
        m_reply = nullptr;
        reply->abort();
    }
    m_status = Status::Idle;
    m_errorMessage.clear();
    Q_EMIT statusChanged();
}

QString UpdateChecker::safeReleaseUrl(const QString &url)
{
    const QUrl parsed(url, QUrl::StrictMode);
    if (parsed.isValid() && parsed.scheme() == QLatin1String("https")
        && parsed.host().compare(QLatin1String("github.com"), Qt::CaseInsensitive) == 0
        && parsed.userInfo().isEmpty() && parsed.port() == -1
        && parsed.path().startsWith(QLatin1String("/MineraleYT/kTomato/"), Qt::CaseInsensitive)) {
        return parsed.toString();
    }
    return kReleasesPage;
}

void UpdateChecker::setStatus(Status s)
{
    if (m_status != s) {
        m_status = s;
        Q_EMIT statusChanged();
    }
}

int UpdateChecker::compareVersions(const QString &v1, const QString &v2)
{
    const ParsedVersion p1 = parseVersion(v1);
    const ParsedVersion p2 = parseVersion(v2);
    const int maxLen = std::max(p1.numbers.size(), p2.numbers.size());
    for (int i = 0; i < maxLen; ++i) {
        const int n1 = i < p1.numbers.size() ? p1.numbers.at(i) : 0;
        const int n2 = i < p2.numbers.size() ? p2.numbers.at(i) : 0;
        if (n1 != n2) {
            return n1 > n2 ? 1 : -1;
        }
    }

    // Same version number: a release beats its pre-releases.
    if (p1.preRelease.isEmpty() || p2.preRelease.isEmpty()) {
        return p1.preRelease.isEmpty() ? (p2.preRelease.isEmpty() ? 0 : 1) : -1;
    }
    const int ids = std::min(p1.preRelease.size(), p2.preRelease.size());
    for (int i = 0; i < ids; ++i) {
        const int c = naturalCompare(p1.preRelease.at(i), p2.preRelease.at(i));
        if (c != 0) {
            return c;
        }
    }
    // "rc.1" < "rc.1.1": more identifiers sort later.
    return sign(qint64(p1.preRelease.size()) - qint64(p2.preRelease.size()));
}

void UpdateChecker::checkForUpdates()
{
    if (m_status == Status::Checking) {
        return;
    }

    if (!m_nam) {
        m_nam = new QNetworkAccessManager(this);
    }

    m_latestVersion.clear();
    m_releaseUrl.clear();
    m_errorMessage.clear();
    setStatus(Status::Checking);

    QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/MineraleYT/kTomato/releases/latest")));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("kTomato/%1").arg(currentVersion()));
    request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/vnd.github.v3+json"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(10000);

    QNetworkReply *reply = m_nam->get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply != m_reply) {
            return; // superseded by reset() or a newer request
        }
        m_reply = nullptr;
        if (reply->error() != QNetworkReply::NoError) {
            m_errorMessage = reply->errorString();
            setStatus(Status::Error);
            return;
        }

        const QByteArray data = reply->readAll();
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            m_errorMessage = i18n("Failed to parse release information.");
            setStatus(Status::Error);
            return;
        }

        const QJsonObject obj = doc.object();
        const QString tagName = obj.value(QStringLiteral("tag_name")).toString();
        const QString htmlUrl = obj.value(QStringLiteral("html_url")).toString();

        if (tagName.isEmpty()) {
            m_errorMessage = i18n("No version information found in release.");
            setStatus(Status::Error);
            return;
        }

        m_latestVersion = tagName;
        m_releaseUrl = safeReleaseUrl(htmlUrl);

        if (compareVersions(m_latestVersion, currentVersion()) > 0) {
            setStatus(Status::UpdateAvailable);
        } else {
            setStatus(Status::UpToDate);
        }
    });
}
