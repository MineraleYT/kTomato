// SPDX-License-Identifier: GPL-3.0-or-later
#include "CalendarSync.h"

#include <KConfigGroup>
#include <KLocalizedString>

#include <QCoreApplication>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimeZone>
#include <QUuid>
#include <QXmlStreamReader>

#include <algorithm>

namespace
{
const char kGroup[] = "Calendar";
const char kEnabled[] = "Enabled";
const char kProvider[] = "Provider";
const char kServerUrl[] = "ServerUrl";
const char kUsername[] = "Username";
const char kPassword[] = "AppPassword";
const char kCalendarUrl[] = "CalendarUrl";
const char kCalendarName[] = "CalendarName";
const char kIncludeNote[] = "IncludeNote";

/// Settings that belong to one provider; they live in the subgroup named after it.
bool isProviderKey(const char *key)
{
    for (const char *k : {kServerUrl, kUsername, kPassword, kCalendarUrl, kCalendarName}) {
        if (qstrcmp(key, k) == 0) {
            return true;
        }
    }
    return false;
}

const QString kNextcloud = QStringLiteral("nextcloud");
const QString kCaldav = QStringLiteral("caldav");

constexpr int kMaxQueue = 100;
constexpr int kMaxSent = 50;
constexpr int kLoginTimeoutMs = 15 * 60 * 1000;

QString appVersion()
{
#ifdef PROJECT_VERSION
    return QStringLiteral(PROJECT_VERSION);
#else
    return QCoreApplication::applicationVersion();
#endif
}

bool isLoopbackHost(const QString &host)
{
    return host == QLatin1String("localhost") || host == QLatin1String("127.0.0.1") || host == QLatin1String("::1")
        || host == QLatin1String("[::1]");
}

/// The URL as text, without a trailing slash and without query or fragment.
QString withoutTrailingSlash(QUrl url)
{
    url.setQuery(QString());
    url.setFragment(QString());
    QString path = url.path();
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    url.setPath(path);
    return url.toString(QUrl::FullyEncoded);
}

QString lastSegment(const QString &url)
{
    const QString path = QUrl(url).path();
    const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    return parts.isEmpty() ? QString() : parts.last();
}

QString formatUtc(qint64 ms)
{
    return QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::UTC).toString(QStringLiteral("yyyyMMdd'T'HHmmss'Z'"));
}

bool isSuccess(int http)
{
    return http >= 200 && http < 300;
}

QByteArray basicAuth(const QString &user, const QString &password)
{
    return "Basic " + (user + QLatin1Char(':') + password).toUtf8().toBase64();
}
} // namespace

// ---------------------------------------------------------------------------------------------
// Pure helpers
// ---------------------------------------------------------------------------------------------

QString CalendarSync::uidFor(const SessionRecord &record)
{
    return QStringLiteral("ktomato-%1-%2@io.github.mineraleyt.ktomato").arg(record.id).arg(record.startedAtMs);
}

QString CalendarSync::escapeText(const QString &text)
{
    QString out;
    out.reserve(text.size() + 8);
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('\\')) {
            out += QLatin1String("\\\\");
        } else if (c == QLatin1Char(';')) {
            out += QLatin1String("\\;");
        } else if (c == QLatin1Char(',')) {
            out += QLatin1String("\\,");
        } else if (c == QLatin1Char('\r')) {
            if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('\n')) {
                ++i;
            }
            out += QLatin1String("\\n");
        } else if (c == QLatin1Char('\n')) {
            out += QLatin1String("\\n");
        } else if (c.unicode() < 0x20 && c != QLatin1Char('\t')) {
            continue; // other control characters are not allowed in TEXT
        } else {
            out += c;
        }
    }
    return out;
}

QByteArray CalendarSync::foldLine(const QString &line)
{
    const QByteArray bytes = line.toUtf8();
    QByteArray out;
    int pos = 0;
    bool first = true;
    while (true) {
        const int limit = first ? 75 : 74; // a continuation line starts with one space
        const int remaining = int(bytes.size()) - pos;
        if (remaining <= limit) {
            out += (first ? "" : " ") + bytes.mid(pos) + "\r\n";
            break;
        }
        int end = pos + limit;
        while (end > pos && (static_cast<uchar>(bytes.at(end)) & 0xC0) == 0x80) {
            --end; // do not cut inside a multi-byte character
        }
        out += (first ? "" : " ") + bytes.mid(pos, end - pos) + "\r\n";
        pos = end;
        first = false;
    }
    return out;
}

QByteArray CalendarSync::buildIcs(const SessionRecord &record, bool includeNote, const QString &uid, const QDateTime &stamp)
{
    const qint64 start = record.startedAtMs;
    qint64 end = record.endedAtMs;
    if (end <= start) {
        end = start + qint64(std::max(record.durationSec, 60)) * 1000;
    }
    const QString summary = record.presetName.trimmed().isEmpty() ? i18n("Focus session") : record.presetName.trimmed();

    QStringList lines;
    lines << QStringLiteral("BEGIN:VCALENDAR") << QStringLiteral("VERSION:2.0") << QStringLiteral("PRODID:-//kTomato//EN")
          << QStringLiteral("CALSCALE:GREGORIAN") << QStringLiteral("BEGIN:VEVENT")
          << QStringLiteral("UID:") + escapeText(uid)
          << QStringLiteral("DTSTAMP:") + formatUtc(stamp.toMSecsSinceEpoch())
          << QStringLiteral("DTSTART:") + formatUtc(start) << QStringLiteral("DTEND:") + formatUtc(end)
          << QStringLiteral("SUMMARY:") + escapeText(summary);
    if (!record.category.trimmed().isEmpty()) {
        lines << QStringLiteral("CATEGORIES:") + escapeText(record.category.trimmed());
    }
    if (includeNote && !record.note.trimmed().isEmpty()) {
        lines << QStringLiteral("DESCRIPTION:") + escapeText(record.note.trimmed());
    }
    lines << QStringLiteral("STATUS:CONFIRMED") << QStringLiteral("TRANSP:OPAQUE") << QStringLiteral("END:VEVENT")
          << QStringLiteral("END:VCALENDAR");

    QByteArray out;
    for (const QString &line : std::as_const(lines)) {
        out += foldLine(line);
    }
    return out;
}

bool CalendarSync::isUrlAllowed(const QUrl &url, QString *error)
{
    const QString scheme = url.scheme().toLower();
    if (scheme == QLatin1String("https") && !url.host().isEmpty()) {
        return true;
    }
    if (scheme == QLatin1String("http") && isLoopbackHost(url.host().toLower())) {
        return true;
    }
    if (error) {
        if (scheme == QLatin1String("http")) {
            *error = i18n("Plain %1 addresses are not allowed because the password would be sent unencrypted. Use an %2 address.", QStringLiteral("http://"), QStringLiteral("https://"));
        } else {
            *error = i18n("This is not a valid %1 address.", QStringLiteral("https://"));
        }
    }
    return false;
}

QString CalendarSync::normalizeUrl(const QString &input, QString *error)
{
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    static const QRegularExpression schemeRe(QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]*://"));
    if (!schemeRe.match(trimmed).hasMatch()) {
        if (error) {
            *error = i18n("Please type the full address, starting with %1", QStringLiteral("https://"));
        }
        return {};
    }
    QUrl url(trimmed, QUrl::StrictMode);
    if (!url.isValid() || url.host().isEmpty()) {
        if (error) {
            *error = i18n("This is not a valid web address.");
        }
        return {};
    }
    if (!url.userInfo().isEmpty()) {
        if (error) {
            *error = i18n("Do not put the user name or password in the address; enter them in their own fields.");
        }
        return {};
    }
    if (!isUrlAllowed(url, error)) {
        return {};
    }
    url.setScheme(url.scheme().toLower());
    return withoutTrailingSlash(url);
}

QList<CalendarSync::CalendarInfo> CalendarSync::parseCalendars(const QByteArray &data, const QUrl &requestUrl)
{
    struct Response {
        QString href;
        QString name;
        QString color;
        bool isCalendar = false;
        bool hasComponentSet = false;
        bool vevent = false;
    };

    QList<CalendarInfo> result;
    QXmlStreamReader xml(data);
    Response cur;
    bool inResponse = false;
    bool inResourceType = false;
    bool inComponentSet = false;

    static const QStringList ignored = {QStringLiteral("inbox"), QStringLiteral("outbox"), QStringLiteral("trashbin"),
                                        QStringLiteral("contact_birthdays")};

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString name = xml.name().toString();
            if (name == QLatin1String("response")) {
                cur = Response();
                inResponse = true;
            } else if (!inResponse) {
                continue;
            } else if (name == QLatin1String("href") && cur.href.isEmpty()) {
                cur.href = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
            } else if (name == QLatin1String("displayname")) {
                cur.name = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
            } else if (name == QLatin1String("calendar-color")) {
                cur.color = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
            } else if (name == QLatin1String("resourcetype")) {
                inResourceType = true;
            } else if (name == QLatin1String("calendar") && inResourceType) {
                cur.isCalendar = true;
            } else if (name == QLatin1String("supported-calendar-component-set")) {
                cur.hasComponentSet = true;
                inComponentSet = true;
            } else if (name == QLatin1String("comp") && inComponentSet) {
                if (xml.attributes().value(QLatin1String("name")).compare(QLatin1String("VEVENT"), Qt::CaseInsensitive) == 0) {
                    cur.vevent = true;
                }
            }
        } else if (xml.isEndElement()) {
            const QString name = xml.name().toString();
            if (name == QLatin1String("resourcetype")) {
                inResourceType = false;
            } else if (name == QLatin1String("supported-calendar-component-set")) {
                inComponentSet = false;
            } else if (name == QLatin1String("response") && inResponse) {
                inResponse = false;
                if (!cur.isCalendar || (cur.hasComponentSet && !cur.vevent) || cur.href.isEmpty()) {
                    continue;
                }
                const QUrl resolved = requestUrl.resolved(QUrl(cur.href));
                if (resolved.scheme() != requestUrl.scheme() || resolved.host() != requestUrl.host()
                    || resolved.port(0) != requestUrl.port(0)) {
                    continue; // never hand our credentials to another server
                }
                const QString url = withoutTrailingSlash(resolved);
                const QString segment = lastSegment(url);
                if (ignored.contains(segment.toLower())) {
                    continue;
                }
                CalendarInfo info;
                info.url = url;
                info.name = cur.name.isEmpty() ? segment : cur.name;
                info.color = cur.color;
                result.append(info);
            }
        }
    }
    if (xml.hasError()) {
        return {};
    }
    return result;
}

// ---------------------------------------------------------------------------------------------
// Construction and settings
// ---------------------------------------------------------------------------------------------

CalendarSync::CalendarSync(QObject *parent)
    : CalendarSync(KSharedConfig::openConfig(QStringLiteral("ktomatorc")), parent)
{
}

CalendarSync::CalendarSync(KSharedConfig::Ptr config, QObject *parent)
    : QObject(parent)
    , m_config(std::move(config))
    , m_retryDelays({30 * 1000, 2 * 60 * 1000, 10 * 60 * 1000})
{
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, &CalendarSync::trySend);
    connect(&m_pollTimer, &QTimer::timeout, this, &CalendarSync::pollLogin);
    m_openUrl = [](const QUrl &url) { return QDesktopServices::openUrl(url); };
    load();
    refreshIdleStatus();
}

CalendarSync::~CalendarSync() = default;

void CalendarSync::load()
{
    migrateLegacyKeys();
    const KConfigGroup group = m_config->group(QString::fromLatin1(kGroup));
    m_enabled = group.readEntry(kEnabled, false);
    m_provider = group.readEntry(kProvider, kNextcloud);
    if (m_provider != kNextcloud && m_provider != kCaldav) {
        m_provider = kNextcloud;
    }
    m_includeNote = group.readEntry(kIncludeNote, false);
    loadProviderFields();
}

void CalendarSync::loadProviderFields()
{
    const KConfigGroup group = m_config->group(QString::fromLatin1(kGroup)).group(m_provider);
    // Anything hand-edited into an unsafe form is dropped rather than used.
    m_serverUrl = normalizeUrl(group.readEntry(kServerUrl, QString()), nullptr);
    m_username = group.readEntry(kUsername, QString()).trimmed();
    m_password = group.readEntry(kPassword, QString());
    m_calendarUrl = normalizeUrl(group.readEntry(kCalendarUrl, QString()), nullptr);
    m_calendarName = group.readEntry(kCalendarName, QString());
}

void CalendarSync::migrateLegacyKeys()
{
    // Older versions kept the provider settings flat in [Calendar]; move them (once) into the
    // subgroup of the provider that was selected.
    static const char *const keys[] = {kServerUrl, kUsername, kPassword, kCalendarUrl, kCalendarName};
    KConfigGroup group = m_config->group(QString::fromLatin1(kGroup));
    bool any = false;
    for (const char *key : keys) {
        any = any || group.hasKey(key);
    }
    if (!any) {
        return;
    }
    QString provider = group.readEntry(kProvider, kNextcloud);
    if (provider != kNextcloud && provider != kCaldav) {
        provider = kNextcloud;
    }
    KConfigGroup target = group.group(provider);
    bool targetHasValues = false;
    for (const char *key : keys) {
        targetHasValues = targetHasValues || target.hasKey(key);
    }
    for (const char *key : keys) {
        if (!targetHasValues && group.hasKey(key)) {
            target.writeEntry(key, group.readEntry(key, QString()), KConfig::Normal);
        }
        group.deleteEntry(key);
    }
    m_config->sync();
}

void CalendarSync::save(const char *key, const QVariant &value)
{
    KConfigGroup group = m_config->group(QString::fromLatin1(kGroup));
    if (isProviderKey(key)) {
        group = group.group(m_provider);
    }
    if (value.typeId() == QMetaType::QString && value.toString().isEmpty()) {
        group.deleteEntry(key);
    } else {
        // Normal flags: never marked as shared/global, stays in this user's ktomatorc.
        group.writeEntry(key, value, KConfig::Normal);
    }
    m_config->sync();
}

bool CalendarSync::configured() const
{
    return !m_username.isEmpty() && !m_password.isEmpty() && !m_calendarUrl.isEmpty();
}

QString CalendarSync::calendarName() const
{
    if (!m_calendarName.isEmpty()) {
        return m_calendarName;
    }
    return m_calendarUrl.isEmpty() ? QString() : lastSegment(m_calendarUrl);
}

QVariantList CalendarSync::calendars() const
{
    QVariantList list;
    for (const CalendarInfo &c : m_calendars) {
        list.append(QVariantMap{{QStringLiteral("name"), c.name}, {QStringLiteral("url"), c.url}, {QStringLiteral("color"), c.color}});
    }
    return list;
}

void CalendarSync::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    save(kEnabled, enabled);
    if (!enabled) {
        // Nothing may be sent later for what happened while it was off.
        abortAll();
        clearQueue();
        m_retryTimer.stop();
        m_retryStep = 0;
        m_sendFailing = false;
        refreshIdleStatus();
    } else {
        trySend();
    }
    Q_EMIT enabledChanged();
}

void CalendarSync::setProvider(const QString &provider)
{
    const QString p = provider.trimmed().toLower();
    if (p != kNextcloud && p != kCaldav) {
        return;
    }
    switchProvider(p);
}

void CalendarSync::switchProvider(const QString &provider)
{
    if (provider == m_provider) {
        return;
    }
    // Whatever is running (login flow, discovery, test) belongs to the provider being left.
    abortAll();
    m_provider = provider;
    save(kProvider, provider);

    const QString oldServer = m_serverUrl;
    const QString oldUser = m_username;
    const bool hadPassword = hasPassword();
    const QString oldCalendarUrl = m_calendarUrl;
    const QString oldCalendarName = calendarName();
    m_calendars.clear();
    loadProviderFields();

    Q_EMIT providerChanged();
    Q_EMIT calendarsChanged();
    if (m_serverUrl != oldServer) {
        Q_EMIT serverUrlChanged();
    }
    if (m_username != oldUser) {
        Q_EMIT usernameChanged();
    }
    if (hasPassword() != hadPassword) {
        Q_EMIT hasPasswordChanged();
    }
    if (m_calendarUrl != oldCalendarUrl) {
        Q_EMIT calendarUrlChanged();
    }
    if (calendarName() != oldCalendarName) {
        Q_EMIT calendarNameChanged();
    }
    credentialsChanged();
}

void CalendarSync::setServerUrl(const QString &url)
{
    QString error;
    const QString normalized = normalizeUrl(url, &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return;
    }
    if (normalized == m_serverUrl) {
        return;
    }
    m_serverUrl = normalized;
    m_calendars.clear();
    save(kServerUrl, normalized);
    Q_EMIT serverUrlChanged();
    Q_EMIT calendarsChanged();
    credentialsChanged();
}

void CalendarSync::setUsername(const QString &username)
{
    const QString name = username.trimmed();
    if (name == m_username) {
        return;
    }
    m_username = name;
    save(kUsername, name);
    Q_EMIT usernameChanged();
    credentialsChanged();
}

void CalendarSync::setPassword(const QString &password)
{
    const QString pw = password.trimmed();
    const bool had = hasPassword();
    m_password = pw;
    save(kPassword, pw);
    if (had != hasPassword()) {
        Q_EMIT hasPasswordChanged();
    }
    credentialsChanged();
}

void CalendarSync::setCalendarUrl(const QString &url)
{
    QString error;
    const QString normalized = normalizeUrl(url, &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return;
    }
    if (normalized == m_calendarUrl) {
        return;
    }
    m_calendarUrl = normalized;
    m_calendarName.clear();
    save(kCalendarUrl, normalized);
    save(kCalendarName, QString());
    Q_EMIT calendarUrlChanged();
    Q_EMIT calendarNameChanged();
    credentialsChanged();
}

void CalendarSync::setIncludeNote(bool include)
{
    if (include == m_includeNote) {
        return;
    }
    m_includeNote = include;
    save(kIncludeNote, include);
    Q_EMIT includeNoteChanged();
}

void CalendarSync::selectCalendar(const QString &url, const QString &name)
{
    QString error;
    const QString normalized = normalizeUrl(url, &error);
    if (normalized.isEmpty()) {
        setLastError(error.isEmpty() ? i18n("Choose a calendar first.") : error);
        return;
    }
    const bool urlChanged = normalized != m_calendarUrl;
    m_calendarUrl = normalized;
    m_calendarName = name.trimmed();
    save(kCalendarUrl, normalized);
    save(kCalendarName, m_calendarName);
    if (urlChanged) {
        Q_EMIT calendarUrlChanged();
    }
    Q_EMIT calendarNameChanged();
    credentialsChanged();
}

void CalendarSync::disconnect()
{
    abortAll();
    clearQueue();
    m_sent.clear();
    m_retryTimer.stop();
    m_retryStep = 0;
    m_paused = false;
    m_sendFailing = false;

    const bool hadPassword = hasPassword();
    m_password.clear();
    m_username.clear();
    m_calendarUrl.clear();
    m_calendarName.clear();
    m_calendars.clear();
    const bool wasEnabled = m_enabled;
    m_enabled = false;
    save(kPassword, QString());
    save(kUsername, QString());
    save(kCalendarUrl, QString());
    save(kCalendarName, QString());
    save(kEnabled, false);
    setLastError(QString());
    if (hadPassword) {
        Q_EMIT hasPasswordChanged();
    }
    Q_EMIT usernameChanged();
    Q_EMIT calendarUrlChanged();
    Q_EMIT calendarNameChanged();
    Q_EMIT calendarsChanged();
    if (wasEnabled) {
        Q_EMIT enabledChanged();
    }
    refreshIdleStatus();
}

void CalendarSync::credentialsChanged()
{
    // New credentials (or calendar) deserve a fresh attempt, immediately.
    m_paused = false;
    m_sendFailing = false;
    m_retryTimer.stop();
    m_retryStep = 0;
    refreshIdleStatus();
    trySend();
}

// ---------------------------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------------------------

void CalendarSync::setStatus(Status status, const QString &text)
{
    const QString scrubbed = scrub(text);
    if (status == m_status && scrubbed == m_statusText) {
        return;
    }
    m_status = status;
    m_statusText = scrubbed;
    Q_EMIT statusChanged();
}

void CalendarSync::setLastError(const QString &error)
{
    const QString scrubbed = scrub(error);
    if (scrubbed == m_lastError) {
        return;
    }
    m_lastError = scrubbed;
    Q_EMIT lastErrorChanged();
}

QString CalendarSync::scrub(const QString &text) const
{
    QString out = text;
    if (m_password.size() >= 4) {
        out.replace(m_password, QStringLiteral("***"));
    }
    return out;
}

void CalendarSync::refreshIdleStatus()
{
    if (m_loginActive || m_busy > 0) {
        return;
    }
    if (m_paused) {
        setStatus(Failed, i18n("Login rejected: check username and app password"));
    } else if (!configured()) {
        if (!m_username.isEmpty() && !m_password.isEmpty()) {
            setStatus(NotConfigured, i18n("Signed in as %1. Choose a calendar to finish.", m_username));
        } else {
            setStatus(NotConfigured, i18n("Not configured"));
        }
    } else if (m_sendFailing) {
        setStatus(Failed, i18n("Could not send events to the calendar; trying again later"));
    } else {
        setStatus(Ready, i18n("Connected as %1", m_username));
    }
}

QString CalendarSync::describeFailure(const Result &result) const
{
    QString text;
    if (result.http == 401 || result.http == 403) {
        text = i18n("Login rejected: check username and app password");
    } else if (result.http == 404) {
        text = i18n("Calendar not found");
    } else if (result.http > 0) {
        text = i18n("Server answered HTTP %1", result.http);
    } else {
        text = result.networkError.isEmpty() ? i18n("The server could not be reached") : result.networkError;
    }
    return scrub(text);
}

void CalendarSync::setRetryDelaysForTesting(const QList<int> &delaysMs)
{
    if (!delaysMs.isEmpty()) {
        m_retryDelays = delaysMs;
    }
}

// ---------------------------------------------------------------------------------------------
// HTTP
// ---------------------------------------------------------------------------------------------

QNetworkAccessManager *CalendarSync::network()
{
    if (!m_nam) {
        m_nam = new QNetworkAccessManager(this);
    }
    return m_nam;
}

void CalendarSync::request(const QByteArray &verb, const QUrl &url, const QByteArray &body,
                           const QList<QPair<QByteArray, QByteArray>> &headers, bool withAuth,
                           std::function<void(const Result &)> done)
{
    QString error;
    if (!isUrlAllowed(url, &error)) {
        Result r;
        r.networkError = error;
        const quint64 gen = m_generation;
        QTimer::singleShot(0, this, [this, done, r, gen]() {
            if (gen == m_generation) {
                done(r);
            }
        });
        return;
    }

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("kTomato/%1").arg(appVersion()));
    if (withAuth) {
        req.setRawHeader("Authorization", basicAuth(m_username, m_password));
    }
    for (const auto &h : headers) {
        req.setRawHeader(h.first, h.second);
    }
    // Only same-origin redirects (same scheme, host and port): the credentials never follow
    // a redirect to another server, and https never falls back to http.
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    req.setTransferTimeout(20000);

    QNetworkReply *reply = network()->sendCustomRequest(req, verb, body);
    m_replies.insert(reply);
    const quint64 gen = m_generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, gen, done]() {
        m_replies.remove(reply);
        reply->deleteLater();
        if (gen != m_generation) {
            return;
        }
        Result r;
        r.http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        r.body = reply->readAll();
        if (r.http == 0) {
            r.networkError = reply->errorString();
        }
        done(r);
    });
}

void CalendarSync::abortAll()
{
    ++m_generation;
    ++m_loginGeneration;
    const auto replies = m_replies;
    m_replies.clear();
    for (QNetworkReply *reply : replies) {
        reply->abort();
        reply->deleteLater();
    }
    m_busy = 0;
    m_inFlight = false;
    m_loginStarting = false;
    endLogin();
}

QUrl CalendarSync::eventUrl(const QString &uid) const
{
    return QUrl(m_calendarUrl + QLatin1Char('/') + QString::fromLatin1(QUrl::toPercentEncoding(uid, "@")) + QLatin1String(".ics"));
}

QUrl CalendarSync::discoveryUrl() const
{
    if (m_provider == kNextcloud) {
        if (m_serverUrl.isEmpty() || m_username.isEmpty()) {
            return {};
        }
        return QUrl(m_serverUrl + QLatin1String("/remote.php/dav/calendars/")
                    + QString::fromLatin1(QUrl::toPercentEncoding(m_username)) + QLatin1Char('/'));
    }
    const QString base = m_serverUrl.isEmpty() ? m_calendarUrl : m_serverUrl;
    return base.isEmpty() ? QUrl() : QUrl(base + QLatin1Char('/'));
}

// ---------------------------------------------------------------------------------------------
// Queue
// ---------------------------------------------------------------------------------------------

void CalendarSync::clearQueue()
{
    if (m_queue.isEmpty()) {
        return;
    }
    m_queue.clear();
    Q_EMIT pendingCountChanged();
}

void CalendarSync::enqueueWorkSession(const SessionRecord &record)
{
    if (!m_enabled || !configured()) {
        return;
    }
    if (record.kind != SessionKind::Work || !record.completed) {
        return;
    }
    Pending item{record, uidFor(record)};
    for (Pending &queued : m_queue) {
        if (queued.uid == item.uid) {
            queued.record = record;
            return;
        }
    }
    if (m_queue.size() >= kMaxQueue) {
        // Never drop the one being sent right now.
        m_queue.removeAt(m_inFlight && m_queue.size() > 1 ? 1 : 0);
        setLastError(i18n("Too many calendar events are waiting to be sent: the oldest one was dropped."));
    }
    m_queue.append(item);
    Q_EMIT pendingCountChanged();
    trySend();
}

void CalendarSync::onNoteChanged(qint64 sessionId, const QString &note)
{
    if (!m_includeNote || !m_enabled || !configured()) {
        return;
    }
    for (Pending &queued : m_queue) {
        if (queued.record.id == sessionId) {
            queued.record.note = note;
            return;
        }
    }
    for (Pending &sent : m_sent) {
        if (sent.record.id == sessionId) {
            if (sent.record.note == note) {
                return;
            }
            sent.record.note = note;
            m_queue.append(sent);
            Q_EMIT pendingCountChanged();
            trySend();
            return;
        }
    }
}

void CalendarSync::trySend()
{
    if (!m_enabled || !configured() || m_paused || m_inFlight || m_queue.isEmpty() || m_retryTimer.isActive()) {
        return;
    }
    if (!isUrlAllowed(QUrl(m_calendarUrl))) {
        return;
    }
    m_inFlight = true;
    const Pending &item = m_queue.first();
    m_inFlightNote = item.record.note;
    const QByteArray ics = buildIcs(item.record, m_includeNote, item.uid);
    request("PUT", eventUrl(item.uid), ics, {{"Content-Type", "text/calendar; charset=utf-8"}}, true,
            [this](const Result &r) { onSendFinished(r); });
}

void CalendarSync::scheduleRetry()
{
    const int index = std::min(m_retryStep, int(m_retryDelays.size()) - 1);
    ++m_retryStep;
    m_retryTimer.start(m_retryDelays.at(index));
}

void CalendarSync::rememberSent(const Pending &item)
{
    for (Pending &sent : m_sent) {
        if (sent.uid == item.uid) {
            sent = item;
            return;
        }
    }
    m_sent.append(item);
    while (m_sent.size() > kMaxSent) {
        m_sent.removeFirst();
    }
}

void CalendarSync::onSendFinished(const Result &r)
{
    m_inFlight = false;
    if (m_queue.isEmpty()) {
        return;
    }
    if (isSuccess(r.http)) {
        Pending sentItem = m_queue.first();
        sentItem.record.note = m_inFlightNote;
        rememberSent(sentItem);
        // A note edited while this request was on its way: send the item again.
        const bool changedMeanwhile = m_includeNote && m_queue.first().record.note != m_inFlightNote;
        if (!changedMeanwhile) {
            m_queue.removeFirst();
        }
        m_retryStep = 0;
        m_sendFailing = false;
        m_lastSuccess = QDateTime::currentDateTime();
        setLastError(QString());
        Q_EMIT lastSuccessChanged();
        Q_EMIT pendingCountChanged();
        refreshIdleStatus();
        trySend();
        return;
    }

    const QString message = describeFailure(r);
    if (r.http == 401 || r.http == 403) {
        m_paused = true;
        setLastError(message);
        setStatus(Failed, message);
        return;
    }
    if (r.http >= 400 && r.http < 500 && r.http != 404 && r.http != 408 && r.http != 429) {
        // This event will never be accepted; do not let it hold back the others.
        m_queue.removeFirst();
        setLastError(i18n("The server refused an event (HTTP %1); it was skipped.", r.http));
        Q_EMIT pendingCountChanged();
        trySend();
        return;
    }
    m_sendFailing = true;
    setLastError(message);
    refreshIdleStatus();
    scheduleRetry();
}

// ---------------------------------------------------------------------------------------------
// Test connection and discovery
// ---------------------------------------------------------------------------------------------

void CalendarSync::testConnection()
{
    if (!configured()) {
        setLastError(i18n("Choose a calendar first."));
        refreshIdleStatus();
        return;
    }
    if (m_busy > 0) {
        return;
    }
    ++m_busy;
    setLastError(QString());
    setStatus(Working, i18n("Testing the connection…"));

    const QString uid = QStringLiteral("ktomato-test-") + QUuid::createUuid().toString(QUuid::Id128);
    SessionRecord record;
    record.startedAtMs = QDateTime::currentMSecsSinceEpoch();
    record.endedAtMs = record.startedAtMs + 60 * 1000;
    record.presetName = i18n("kTomato connection test");
    const QByteArray ics = buildIcs(record, false, uid);

    auto fail = [this](const Result &r) {
        --m_busy;
        const QString message = describeFailure(r);
        if (r.http == 401 || r.http == 403) {
            m_paused = true;
        }
        setLastError(message);
        setStatus(Failed, message);
    };

    request("PUT", eventUrl(uid), ics, {{"Content-Type", "text/calendar; charset=utf-8"}}, true, [this, uid, fail](const Result &put) {
        if (!isSuccess(put.http)) {
            fail(put);
            return;
        }
        request("DELETE", eventUrl(uid), QByteArray(), {}, true, [this](const Result &del) {
            --m_busy;
            m_paused = false;
            m_sendFailing = false;
            m_retryTimer.stop();
            m_retryStep = 0;
            if (isSuccess(del.http) || del.http == 404) {
                setLastError(QString());
            } else {
                setLastError(i18n("The connection works, but the test event could not be removed (%1).", describeFailure(del)));
            }
            setStatus(Ready, i18n("Connection test succeeded"));
            trySend();
        });
    });
}

void CalendarSync::discover(std::function<void(bool)> done)
{
    const QUrl url = discoveryUrl();
    if (!url.isValid() || m_username.isEmpty() || m_password.isEmpty()) {
        setLastError(i18n("Enter the server address, user name and password first."));
        refreshIdleStatus();
        done(false);
        return;
    }
    ++m_busy;
    setLastError(QString());
    setStatus(Working, i18n("Looking for calendars…"));
    const QByteArray body =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<d:propfind xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\" xmlns:a=\"http://apple.com/ns/ical/\">"
        "<d:prop><d:resourcetype/><d:displayname/><c:supported-calendar-component-set/><a:calendar-color/></d:prop>"
        "</d:propfind>";
    request("PROPFIND", url, body, {{"Depth", "1"}, {"Content-Type", "application/xml; charset=utf-8"}}, true,
            [this, url, done](const Result &r) {
                --m_busy;
                if (r.http == 207) {
                    m_calendars = parseCalendars(r.body, url);
                    Q_EMIT calendarsChanged();
                    setLastError(QString());
                    m_paused = false;
                    refreshIdleStatus();
                    done(true);
                    return;
                }
                if (r.http == 401 || r.http == 403) {
                    m_paused = true;
                }
                const QString message = describeFailure(r);
                setLastError(message);
                setStatus(Failed, message);
                done(false);
            });
}

void CalendarSync::refreshCalendars()
{
    if (m_busy > 0) {
        return;
    }
    discover([this](bool ok) {
        if (!ok) {
            return;
        }
        if (m_calendars.isEmpty()) {
            setStatus(NotConfigured, i18n("No calendars were found at this address."));
            return;
        }
        for (const CalendarInfo &c : std::as_const(m_calendars)) {
            if (c.url == m_calendarUrl && m_calendarName != c.name) {
                m_calendarName = c.name;
                save(kCalendarName, c.name);
                Q_EMIT calendarNameChanged();
            }
        }
        trySend();
    });
}

void CalendarSync::autoSelectCalendar()
{
    const CalendarInfo *pick = nullptr;
    if (m_calendars.size() == 1) {
        pick = &m_calendars.first();
    } else {
        for (const CalendarInfo &c : std::as_const(m_calendars)) {
            if (c.name.compare(QLatin1String("personal"), Qt::CaseInsensitive) == 0
                || lastSegment(c.url).compare(QLatin1String("personal"), Qt::CaseInsensitive) == 0) {
                pick = &c;
                break;
            }
        }
    }
    if (pick) {
        const QString url = pick->url;
        const QString name = pick->name;
        selectCalendar(url, name);
    } else if (m_calendars.isEmpty()) {
        setStatus(NotConfigured, i18n("No calendars were found at this address."));
    } else {
        setStatus(NotConfigured, i18n("Signed in as %1. Choose a calendar to finish.", m_username));
    }
}

// ---------------------------------------------------------------------------------------------
// Nextcloud Login Flow v2
// ---------------------------------------------------------------------------------------------

void CalendarSync::startNextcloudLogin()
{
    cancelLogin();
    switchProvider(kNextcloud);
    if (m_serverUrl.isEmpty()) {
        setLastError(i18n("Enter the Nextcloud address first, for example %1", QStringLiteral("https://cloud.example.org")));
        refreshIdleStatus();
        return;
    }
    setLastError(QString());
    m_loginStarting = true;
    ++m_busy;
    setStatus(Working, i18n("Contacting the server…"));
    const quint64 loginGen = m_loginGeneration;
    const QUrl serverUrl(m_serverUrl);

    // Nextcloud names the new app password after this request's User-Agent, and shows that name in
    // Settings > Security > Devices & sessions: plain "kTomato", without a version number.
    request("POST", QUrl(m_serverUrl + QLatin1String("/index.php/login/v2")), QByteArray(), {{"User-Agent", "kTomato"}}, false,
            [this, loginGen, serverUrl](const Result &r) {
                if (loginGen != m_loginGeneration) {
                    return;
                }
                m_loginStarting = false;
                --m_busy;
                auto failWith = [this](const QString &message) {
                    setLastError(message);
                    setStatus(Failed, message);
                };
                if (r.http != 200) {
                    failWith(r.http == 404 ? i18n("This does not look like a Nextcloud server.") : describeFailure(r));
                    return;
                }
                const QJsonObject root = QJsonDocument::fromJson(r.body).object();
                const QJsonObject poll = root.value(QLatin1String("poll")).toObject();
                const QString token = poll.value(QLatin1String("token")).toString();
                const QUrl endpoint(poll.value(QLatin1String("endpoint")).toString());
                const QUrl login(root.value(QLatin1String("login")).toString());
                if (token.isEmpty() || !endpoint.isValid() || !login.isValid()) {
                    failWith(i18n("The server gave an unexpected answer to the login request."));
                    return;
                }
                QString error;
                if (!isUrlAllowed(login, &error) || !isUrlAllowed(endpoint, &error)) {
                    failWith(error);
                    return;
                }
                if (endpoint.host().compare(serverUrl.host(), Qt::CaseInsensitive) != 0) {
                    failWith(i18n("The server redirected the login to a different host; it was refused."));
                    return;
                }
                m_pollEndpoint = endpoint;
                m_pollToken = token.toUtf8();
                m_loginActive = true;
                m_pollInFlight = false;
                m_loginDeadline = QDeadlineTimer(kLoginTimeoutMs);
                m_pollTimer.start(m_pollIntervalMs);
                setStatus(WaitingForBrowser, i18n("Waiting for you to authorize kTomato in the browser…"));
                if (m_openUrl) {
                    m_openUrl(login);
                }
            });
}

void CalendarSync::endLogin()
{
    m_pollTimer.stop();
    m_loginActive = false;
    m_pollInFlight = false;
    m_pollToken.clear();
}

void CalendarSync::cancelLogin()
{
    const bool wasActive = m_loginActive || m_loginStarting;
    ++m_loginGeneration;
    if (m_loginStarting) {
        m_loginStarting = false;
        --m_busy;
    }
    endLogin();
    if (wasActive) {
        refreshIdleStatus();
    }
}

void CalendarSync::pollLogin()
{
    if (!m_loginActive) {
        return;
    }
    if (m_loginDeadline.hasExpired()) {
        endLogin();
        setLastError(i18n("The login was not completed in time. Please try again."));
        refreshIdleStatus();
        return;
    }
    if (m_pollInFlight) {
        return;
    }
    m_pollInFlight = true;
    const quint64 loginGen = m_loginGeneration;
    const QByteArray body = "token=" + QUrl::toPercentEncoding(QString::fromUtf8(m_pollToken));
    request("POST", m_pollEndpoint, body, {{"Content-Type", "application/x-www-form-urlencoded"}, {"User-Agent", "kTomato"}}, false,
            [this, loginGen](const Result &r) {
                if (loginGen != m_loginGeneration || !m_loginActive) {
                    return;
                }
                m_pollInFlight = false;
                if (r.http == 200) {
                    finishLogin(r.body);
                } else if (r.http >= 400 && r.http < 500 && r.http != 404 && r.http != 408 && r.http != 429) {
                    endLogin();
                    const QString message = describeFailure(r);
                    setLastError(message);
                    setStatus(Failed, message);
                }
                // 404 means "not yet"; network errors and 5xx are retried until the deadline.
            });
}

void CalendarSync::finishLogin(const QByteArray &json)
{
    endLogin();
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    const QUrl server(root.value(QLatin1String("server")).toString());
    const QString loginName = root.value(QLatin1String("loginName")).toString().trimmed();
    const QString appPassword = root.value(QLatin1String("appPassword")).toString();
    const QUrl entered(m_serverUrl);

    QString error;
    if (!server.isValid() || !isUrlAllowed(server, &error) || server.host().compare(entered.host(), Qt::CaseInsensitive) != 0) {
        const QString message = i18n("The server returned an address that does not match the one you entered; the login was refused.");
        setLastError(message);
        setStatus(Failed, message);
        return;
    }
    if (loginName.isEmpty() || appPassword.isEmpty()) {
        const QString message = i18n("The server gave an unexpected answer to the login request.");
        setLastError(message);
        setStatus(Failed, message);
        return;
    }

    // A new account: forget the old calendar choice.
    const bool hadCalendar = !m_calendarUrl.isEmpty();
    m_calendarUrl.clear();
    m_calendarName.clear();
    save(kCalendarUrl, QString());
    save(kCalendarName, QString());
    if (hadCalendar) {
        Q_EMIT calendarUrlChanged();
        Q_EMIT calendarNameChanged();
    }
    const bool hadPassword = hasPassword();
    m_username = loginName;
    m_password = appPassword;
    save(kUsername, m_username);
    save(kPassword, m_password);
    Q_EMIT usernameChanged();
    if (!hadPassword) {
        Q_EMIT hasPasswordChanged();
    }
    m_paused = false;
    m_sendFailing = false;

    discover([this](bool ok) {
        if (ok) {
            autoSelectCalendar();
        }
    });
}
