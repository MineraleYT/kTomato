// SPDX-License-Identifier: GPL-3.0-or-later
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

#include <memory>

#include "CalendarSync.h"

namespace
{
const QString kPassword = QStringLiteral("s3cretPW-xyz");

QByteArray basic(const QString &user, const QString &pw)
{
    return "Basic " + (user + QLatin1Char(':') + pw).toUtf8().toBase64();
}

QByteArray multistatus(const QString &prefix)
{
    // Relative hrefs, special Nextcloud collections, a task-only list and a non-calendar entry.
    auto p = [&](const char *name) {
        const QString n = QString::fromLatin1(name);
        return prefix.isEmpty() ? n : prefix + QLatin1Char(':') + n;
    };
    auto collection = [&](const QString &href, const QString &display, const QString &comps, bool calendar) {
        QString s = QStringLiteral("<%1><%2>%3</%2><%4><%5>").arg(p("response"), p("href"), href, p("propstat"), p("prop"));
        s += QStringLiteral("<%1>").arg(p("resourcetype")) + QStringLiteral("<%1/>").arg(p("collection"));
        if (calendar) {
            s += QStringLiteral("<cal:calendar xmlns:cal=\"urn:ietf:params:xml:ns:caldav\"/>");
        }
        s += QStringLiteral("</%1>").arg(p("resourcetype"));
        if (!display.isEmpty()) {
            s += QStringLiteral("<%1>%2</%1>").arg(p("displayname"), display);
        }
        if (!comps.isEmpty()) {
            s += QStringLiteral("<cal:supported-calendar-component-set xmlns:cal=\"urn:ietf:params:xml:ns:caldav\">%1"
                                "</cal:supported-calendar-component-set>")
                     .arg(comps);
        }
        s += QStringLiteral("<ic:calendar-color xmlns:ic=\"http://apple.com/ns/ical/\">#0082c9FF</ic:calendar-color>");
        s += QStringLiteral("</%1><%2>HTTP/1.1 200 OK</%2></%3></%4>").arg(p("prop"), p("status"), p("propstat"), p("response"));
        return s;
    };
    const QString vevent = QStringLiteral("<cal:comp xmlns:cal=\"urn:ietf:params:xml:ns:caldav\" name=\"VEVENT\"/>");
    const QString vtodo = QStringLiteral("<cal:comp xmlns:cal=\"urn:ietf:params:xml:ns:caldav\" name=\"VTODO\"/>");
    QString xml = QStringLiteral("<?xml version=\"1.0\"?><%1 xmlns:%2=\"DAV:\">").arg(p("multistatus"), prefix.isEmpty() ? QStringLiteral("x") : prefix);
    if (prefix.isEmpty()) {
        xml = QStringLiteral("<?xml version=\"1.0\"?><multistatus xmlns=\"DAV:\">");
    }
    xml += collection(QStringLiteral("/remote.php/dav/calendars/alice/"), QString(), QString(), false);
    xml += collection(QStringLiteral("/remote.php/dav/calendars/alice/inbox/"), QString(), vevent, true);
    xml += collection(QStringLiteral("/remote.php/dav/calendars/alice/outbox/"), QString(), vevent, true);
    xml += collection(QStringLiteral("/remote.php/dav/calendars/alice/trashbin/"), QString(), vevent, true);
    xml += collection(QStringLiteral("/remote.php/dav/calendars/alice/personal/"), QStringLiteral("Personal"), vevent + vtodo, true);
    xml += collection(QStringLiteral("work-team/"), QStringLiteral("Work"), vevent, true);
    xml += collection(QStringLiteral("/remote.php/dav/calendars/alice/tasks/"), QStringLiteral("Tasks"), vtodo, true);
    xml += collection(QStringLiteral("https://other.example.org/remote.php/dav/calendars/alice/evil/"), QStringLiteral("Evil"), vevent, true);
    xml += prefix.isEmpty() ? QStringLiteral("</multistatus>") : QStringLiteral("</%1>").arg(p("multistatus"));
    return xml.toUtf8();
}
} // namespace

class FakeServer : public QObject
{
public:
    struct Request {
        QByteArray method;
        QByteArray path;
        QByteArray body;
        QHash<QByteArray, QByteArray> headers;
        qint64 at = 0;
    };

    FakeServer()
    {
        m_clock.start();
        const bool ok = m_server.listen(QHostAddress::LocalHost, 0);
        Q_ASSERT(ok);
        Q_UNUSED(ok);
        connect(&m_server, &QTcpServer::newConnection, this, &FakeServer::onConnection);
    }

    ~FakeServer() override
    {
        for (QTcpSocket *s : m_buffers.keys()) {
            QObject::disconnect(s, nullptr, this, nullptr);
        }
    }

    QString base() const { return QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort()); }

    int count(const QByteArray &method, const QByteArray &prefix = QByteArray()) const
    {
        int n = 0;
        for (const Request &r : requests) {
            if (r.method == method && r.path.startsWith(prefix)) {
                ++n;
            }
        }
        return n;
    }
    QList<Request> of(const QByteArray &method) const
    {
        QList<Request> out;
        for (const Request &r : requests) {
            if (r.method == method) {
                out.append(r);
            }
        }
        return out;
    }

    QList<Request> requests;
    QByteArray expectedAuth;           ///< Required on the DAV paths when not empty.
    QList<int> putStatuses;            ///< Consumed one per PUT, then defaultPut.
    int defaultPut = 201;
    int deleteStatus = 204;
    QString prefix;                    ///< XML namespace prefix of the multistatus.
    int pollsBefore200 = 2;
    int polls = 0;
    QString loginServer;               ///< `server` in the final login answer.
    QString loginName = QStringLiteral("alice");
    QString appPassword = kPassword;
    QByteArray token = "tok123";
    bool onlyOneCalendar = false;
    bool noPersonal = false;
    QHash<QByteArray, QByteArray> stored;

private:
    void onConnection()
    {
        while (QTcpSocket *s = m_server.nextPendingConnection()) {
            m_buffers[s];
            connect(s, &QTcpSocket::readyRead, this, [this, s]() { process(s); });
            connect(s, &QTcpSocket::disconnected, this, [this, s]() {
                m_buffers.remove(s);
                s->deleteLater();
            });
        }
    }

    void process(QTcpSocket *s)
    {
        QByteArray &buf = m_buffers[s];
        buf += s->readAll();
        while (true) {
            const int idx = buf.indexOf("\r\n\r\n");
            if (idx < 0) {
                return;
            }
            const QList<QByteArray> lines = buf.left(idx).split('\n');
            Request req;
            const QList<QByteArray> first = lines.first().trimmed().split(' ');
            req.method = first.value(0);
            req.path = first.value(1);
            for (int i = 1; i < lines.size(); ++i) {
                const int colon = lines.at(i).indexOf(':');
                if (colon > 0) {
                    req.headers.insert(lines.at(i).left(colon).trimmed().toLower(), lines.at(i).mid(colon + 1).trimmed());
                }
            }
            const int len = req.headers.value("content-length").toInt();
            if (buf.size() < idx + 4 + len) {
                return;
            }
            req.body = buf.mid(idx + 4, len);
            buf.remove(0, idx + 4 + len);
            req.at = m_clock.elapsed();
            requests.append(req);
            respond(s, req);
        }
    }

    void reply(QTcpSocket *s, int status, const QByteArray &body = QByteArray(), const QByteArray &type = "application/json")
    {
        QByteArray out = "HTTP/1.1 " + QByteArray::number(status) + " X\r\nContent-Type: " + type + "\r\nContent-Length: "
            + QByteArray::number(body.size()) + "\r\nConnection: keep-alive\r\n\r\n" + body;
        s->write(out);
        s->flush();
    }

    void respond(QTcpSocket *s, const Request &req)
    {
        const QByteArray path = req.path;
        if (path == "/index.php/login/v2") {
            const QByteArray json = "{\"poll\":{\"token\":\"" + token + "\",\"endpoint\":\"" + base().toUtf8()
                + "/login/v2/poll\"},\"login\":\"" + base().toUtf8() + "/login/v2/flow/abc\"}";
            reply(s, 200, json);
            return;
        }
        if (path == "/login/v2/poll") {
            ++polls;
            if (req.body != "token=" + token || polls <= pollsBefore200) {
                reply(s, 404, "{}");
                return;
            }
            const QString server = loginServer.isEmpty() ? base() : loginServer;
            reply(s, 200,
                  QStringLiteral("{\"server\":\"%1\",\"loginName\":\"%2\",\"appPassword\":\"%3\"}").arg(server, loginName, appPassword).toUtf8());
            return;
        }
        if (!expectedAuth.isEmpty() && req.headers.value("authorization") != expectedAuth) {
            reply(s, 401, "no");
            return;
        }
        if (req.method == "PROPFIND") {
            QByteArray body;
            if (path.startsWith("/custom/calendar")) {
                body = "<d:multistatus xmlns:d=\"DAV:\"><d:response><d:href>/custom/calendar/</d:href><d:propstat><d:prop>"
                       "<d:resourcetype><d:collection/><c:calendar xmlns:c=\"urn:ietf:params:xml:ns:caldav\"/></d:resourcetype>"
                       "<d:displayname>Mine</d:displayname></d:prop></d:propstat></d:response>"
                       "<d:response><d:href>/custom/calendar/one.ics</d:href><d:propstat><d:prop><d:resourcetype/></d:prop></d:propstat></d:response>"
                       "</d:multistatus>";
            } else if (path.startsWith("/remote.php/dav/calendars/alice")) {
                body = multistatus(prefix);
                if (noPersonal) {
                    body.replace("/personal/", "/private/");
                    body.replace("Personal", "Private");
                }
                if (onlyOneCalendar) {
                    body.replace("work-team/", "inbox/");
                }
            } else {
                reply(s, 404);
                return;
            }
            reply(s, 207, body, "application/xml");
            return;
        }
        if (req.method == "PUT") {
            const int status = putStatuses.isEmpty() ? defaultPut : putStatuses.takeFirst();
            if (status >= 200 && status < 300) {
                stored.insert(path, req.body);
            }
            reply(s, status);
            return;
        }
        if (req.method == "DELETE") {
            stored.remove(path);
            reply(s, deleteStatus);
            return;
        }
        reply(s, 405);
    }

    QTcpServer m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QElapsedTimer m_clock;
};

class CalendarSyncTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<FakeServer> m_server;
    std::unique_ptr<CalendarSync> m_sync;
    QList<QUrl> m_opened;
    int m_counter = 0;

    QString configPath() const { return m_dir->filePath(QStringLiteral("ktomatorc")); }

    std::unique_ptr<CalendarSync> makeSync()
    {
        auto config = KSharedConfig::openConfig(configPath(), KConfig::SimpleConfig);
        auto sync = std::make_unique<CalendarSync>(config);
        sync->setRetryDelaysForTesting({40, 80});
        sync->setLoginPollIntervalForTesting(20);
        sync->setOpenUrlHandler([this](const QUrl &url) { m_opened.append(url); });
        return sync;
    }

    /// A configured, enabled custom CalDAV account pointing at the fake server.
    void configureCustom()
    {
        m_server->expectedAuth = basic(QStringLiteral("alice"), kPassword);
        m_sync->setProvider(QStringLiteral("caldav"));
        m_sync->setUsername(QStringLiteral("alice"));
        m_sync->setPassword(kPassword);
        m_sync->setCalendarUrl(m_server->base() + QStringLiteral("/cal/personal/"));
        m_sync->setEnabled(true);
    }

    static SessionRecord record(qint64 id = 7)
    {
        SessionRecord r;
        r.id = id;
        r.kind = SessionKind::Work;
        r.startedAtMs = 1700000000000;
        r.endedAtMs = r.startedAtMs + 25 * 60 * 1000;
        r.durationSec = 25 * 60;
        r.plannedSec = 25 * 60;
        r.completed = true;
        r.presetName = QStringLiteral("Deep work");
        r.category = QStringLiteral("Study");
        return r;
    }

    static QString unfold(const QByteArray &ics)
    {
        QByteArray copy = ics;
        copy.replace("\r\n ", "");
        return QString::fromUtf8(copy);
    }

    void checkNoSecrets()
    {
        QVERIFY(!m_sync->statusText().contains(kPassword));
        QVERIFY(!m_sync->lastError().contains(kPassword));
        QVERIFY(!m_sync->statusText().contains(QString::fromLatin1(basic(QStringLiteral("alice"), kPassword).mid(6))));
        QVERIFY(!m_sync->lastError().contains(QString::fromLatin1(basic(QStringLiteral("alice"), kPassword).mid(6))));
    }

    KConfigGroup diskGroup(const QString &sub = QString())
    {
        auto cfg = KSharedConfig::openConfig(configPath(), KConfig::SimpleConfig);
        cfg->reparseConfiguration();
        KConfigGroup g = cfg->group(QStringLiteral("Calendar"));
        return sub.isEmpty() ? g : g.group(sub);
    }

    void fillNextcloud(CalendarSync *s = nullptr)
    {
        s = s ? s : m_sync.get();
        s->setProvider(QStringLiteral("nextcloud"));
        s->setServerUrl(QStringLiteral("https://cloud.example.org"));
        s->setUsername(QStringLiteral("alice"));
        s->setPassword(QStringLiteral("nc-password-1"));
        s->selectCalendar(QStringLiteral("https://cloud.example.org/cal/personal"), QStringLiteral("Personal"));
    }

    void fillCaldav(CalendarSync *s = nullptr)
    {
        s = s ? s : m_sync.get();
        s->setProvider(QStringLiteral("caldav"));
        s->setServerUrl(QStringLiteral("https://dav.example.net"));
        s->setUsername(QStringLiteral("bob"));
        s->setPassword(QStringLiteral("cd-password-2"));
        s->selectCalendar(QStringLiteral("https://dav.example.net/cal/work"), QStringLiteral("Work"));
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain("ktomato");
        qRegisterMetaType<SessionRecord>();
    }

    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_server = std::make_unique<FakeServer>();
        m_sync = makeSync();
        m_opened.clear();
    }

    void cleanup()
    {
        m_sync.reset();
        m_server.reset();
        m_dir.reset();
    }

    // ----- iCalendar -----

    void icsBasics()
    {
        SessionRecord r = record();
        r.note = QStringLiteral("a;b,c\\d\nline2");
        const QDateTime stamp = QDateTime::fromMSecsSinceEpoch(1700000100000, QTimeZone::UTC);
        const QString uid = CalendarSync::uidFor(r);
        QCOMPARE(uid, QStringLiteral("ktomato-7-1700000000000@io.github.mineraleyt.ktomato"));
        QCOMPARE(CalendarSync::uidFor(r), uid); // stable

        const QByteArray ics = CalendarSync::buildIcs(r, false, uid, stamp);
        QVERIFY(ics.startsWith("BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//kTomato//EN\r\n"));
        QVERIFY(ics.endsWith("END:VEVENT\r\nEND:VCALENDAR\r\n"));
        QVERIFY(ics.contains("UID:ktomato-7-1700000000000@io.github.mineraleyt.ktomato\r\n"));
        QVERIFY(ics.contains("DTSTAMP:20231114T221500Z\r\n"));
        QVERIFY(ics.contains("DTSTART:20231114T221320Z\r\n"));
        QVERIFY(ics.contains("DTEND:20231114T223820Z\r\n"));
        QVERIFY(ics.contains("SUMMARY:Deep work\r\n"));
        QVERIFY(ics.contains("CATEGORIES:Study\r\n"));
        QVERIFY(ics.contains("STATUS:CONFIRMED\r\n"));
        QVERIFY(ics.contains("TRANSP:OPAQUE\r\n"));
        QVERIFY(!ics.contains("DESCRIPTION"));
        QVERIFY(!ics.contains("a;b"));
        QCOMPARE(CalendarSync::buildIcs(r, false, uid, stamp), ics);
        // No bare line feeds.
        QByteArray stripped = ics;
        stripped.replace("\r\n", "");
        QVERIFY(!stripped.contains('\n') && !stripped.contains('\r'));

        const QByteArray withNote = CalendarSync::buildIcs(r, true, uid, stamp);
        QVERIFY2(withNote.contains("DESCRIPTION:a\\;b\\,c\\\\d\\nline2\r\n"), withNote.constData());

        r.presetName.clear();
        r.category.clear();
        const QByteArray fallback = CalendarSync::buildIcs(r, true, uid, stamp);
        QVERIFY(fallback.contains("SUMMARY:Focus session\r\n"));
        QVERIFY(!fallback.contains("CATEGORIES"));
    }

    void icsEscapeText()
    {
        QCOMPARE(CalendarSync::escapeText(QStringLiteral("a\\b;c,d\r\ne\rf\ng")), QStringLiteral("a\\\\b\\;c\\,d\\ne\\nf\\ng"));
    }

    void icsFoldingMultibyte()
    {
        for (const QString &unit : {QStringLiteral("è"), QStringLiteral("日本"), QStringLiteral("😀"), QStringLiteral("a")}) {
            SessionRecord r = record();
            r.presetName = unit.repeated(120) + QStringLiteral(" end");
            const QString uid = CalendarSync::uidFor(r);
            const QByteArray ics = CalendarSync::buildIcs(r, false, uid);
            const QList<QByteArray> lines = ics.split('\n');
            for (QByteArray line : lines) {
                line.chop(line.endsWith('\r') ? 1 : 0);
                QVERIFY2(line.size() <= 75, "line too long");
                // Every physical line is valid UTF-8 on its own.
                QCOMPARE(QString::fromUtf8(line).toUtf8(), line);
            }
            QVERIFY(unfold(ics).contains(QStringLiteral("SUMMARY:") + r.presetName + QStringLiteral("\r\n")));
        }
        const QByteArray folded = CalendarSync::foldLine(QStringLiteral("X:") + QString(200, QLatin1Char('a')));
        QVERIFY(folded.startsWith("X:"));
        QCOMPARE(folded.indexOf("\r\n"), 75);
        QVERIFY(folded.contains("\r\n a"));
    }

    // ----- URLs -----

    void urlRules()
    {
        QString error;
        QCOMPARE(CalendarSync::normalizeUrl(QStringLiteral("  https://cloud.example.org/// "), &error), QStringLiteral("https://cloud.example.org"));
        QVERIFY(error.isEmpty());
        QCOMPARE(CalendarSync::normalizeUrl(QStringLiteral("https://h.org/dav/cal/"), &error), QStringLiteral("https://h.org/dav/cal"));
        QCOMPARE(CalendarSync::normalizeUrl(QStringLiteral("http://127.0.0.1:5232/"), &error), QStringLiteral("http://127.0.0.1:5232"));
        QCOMPARE(CalendarSync::normalizeUrl(QStringLiteral("http://localhost/x/"), &error), QStringLiteral("http://localhost/x"));
        QCOMPARE(CalendarSync::normalizeUrl(QStringLiteral(""), &error), QString());
        QVERIFY(error.isEmpty());

        for (const QString &bad : {QStringLiteral("cloud.example.org"), QStringLiteral("http://cloud.example.org"),
                                   QStringLiteral("ftp://cloud.example.org"), QStringLiteral("https://alice:pw@cloud.example.org"),
                                   QStringLiteral("https://")}) {
            error.clear();
            QVERIFY2(CalendarSync::normalizeUrl(bad, &error).isEmpty(), qPrintable(bad));
            QVERIFY2(!error.isEmpty(), qPrintable(bad));
        }
        QVERIFY(CalendarSync::isUrlAllowed(QUrl(QStringLiteral("https://x.org/"))));
        QVERIFY(CalendarSync::isUrlAllowed(QUrl(QStringLiteral("http://[::1]:8080/"))));
        QVERIFY(!CalendarSync::isUrlAllowed(QUrl(QStringLiteral("http://x.org/"))));
        QVERIFY(!CalendarSync::isUrlAllowed(QUrl(QStringLiteral("http://127.0.0.1.evil.org/"))));
    }

    void settersRejectInsecureUrls()
    {
        m_sync->setServerUrl(QStringLiteral("http://cloud.example.org"));
        QVERIFY(m_sync->serverUrl().isEmpty());
        QVERIFY(!m_sync->lastError().isEmpty());
        m_sync->setCalendarUrl(QStringLiteral("cloud.example.org/dav"));
        QVERIFY(m_sync->calendarUrl().isEmpty());
        m_sync->setServerUrl(QStringLiteral("https://cloud.example.org/"));
        QCOMPARE(m_sync->serverUrl(), QStringLiteral("https://cloud.example.org"));
    }

    void plainHttpToRemoteHostSendsNothing()
    {
        // A hand-edited config with an insecure address is ignored on load.
        {
            KConfigGroup g = KSharedConfig::openConfig(configPath(), KConfig::SimpleConfig)->group(QStringLiteral("Calendar"));
            g.writeEntry("Provider", "caldav");
            g = g.group(QStringLiteral("caldav"));
            g.writeEntry("CalendarUrl", "http://cloud.example.org/cal");
            g.writeEntry("Username", "alice");
            g.writeEntry("AppPassword", kPassword);
            g.parent().writeEntry("Enabled", true);
            g.sync();
        }
        auto sync = makeSync();
        QVERIFY(sync->calendarUrl().isEmpty());
        sync->enqueueWorkSession(record());
        QCOMPARE(sync->pendingCount(), 0);
    }

    // ----- Discovery -----

    void parseCalendarsTolerant()
    {
        for (const QString &prefix : {QString(), QStringLiteral("d"), QStringLiteral("D"), QStringLiteral("ns0")}) {
            const QUrl request(QStringLiteral("https://cloud.example.org/remote.php/dav/calendars/alice/"));
            const auto cals = CalendarSync::parseCalendars(multistatus(prefix), request);
            QCOMPARE(cals.size(), 2);
            QCOMPARE(cals.at(0).name, QStringLiteral("Personal"));
            QCOMPARE(cals.at(0).url, QStringLiteral("https://cloud.example.org/remote.php/dav/calendars/alice/personal"));
            QCOMPARE(cals.at(0).color, QStringLiteral("#0082c9FF"));
            QCOMPARE(cals.at(1).name, QStringLiteral("Work"));
            QCOMPARE(cals.at(1).url, QStringLiteral("https://cloud.example.org/remote.php/dav/calendars/alice/work-team"));
        }
        QVERIFY(CalendarSync::parseCalendars("<not xml", QUrl(QStringLiteral("https://x.org/"))).isEmpty());
    }

    void discoveryNextcloud()
    {
        m_server->expectedAuth = basic(QStringLiteral("alice"), kPassword);
        m_sync->setServerUrl(m_server->base());
        m_sync->setUsername(QStringLiteral("alice"));
        m_sync->setPassword(kPassword);
        QVERIFY(m_sync->hasPassword());
        QSignalSpy spy(m_sync.get(), &CalendarSync::calendarsChanged);
        m_sync->refreshCalendars();
        QCOMPARE(m_sync->status(), CalendarSync::Working);
        QTRY_VERIFY(!spy.isEmpty());
        const QVariantList cals = m_sync->calendars();
        QCOMPARE(cals.size(), 2);
        QCOMPARE(cals.at(0).toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Personal"));
        QVERIFY(cals.at(1).toMap().value(QStringLiteral("url")).toString().endsWith(QStringLiteral("/work-team")));
        const auto propfind = m_server->of("PROPFIND");
        QCOMPARE(propfind.size(), 1);
        QCOMPARE(propfind.first().path, QByteArray("/remote.php/dav/calendars/alice/"));
        QCOMPARE(propfind.first().headers.value("depth"), QByteArray("1"));
        QVERIFY(propfind.first().headers.value("user-agent").startsWith("kTomato/"));
        QVERIFY(propfind.first().body.contains("resourcetype"));
        // Not configured until a calendar is chosen.
        QCOMPARE(m_sync->status(), CalendarSync::NotConfigured);
        m_sync->selectCalendar(cals.at(1).toMap().value(QStringLiteral("url")).toString(), QStringLiteral("Work"));
        QCOMPARE(m_sync->status(), CalendarSync::Ready);
        QCOMPARE(m_sync->calendarName(), QStringLiteral("Work"));
        checkNoSecrets();
    }

    void discoveryCustomIsCalendar()
    {
        m_server->expectedAuth = basic(QStringLiteral("bob"), kPassword);
        m_sync->setProvider(QStringLiteral("caldav"));
        m_sync->setServerUrl(m_server->base() + QStringLiteral("/custom/calendar/"));
        m_sync->setUsername(QStringLiteral("bob"));
        m_sync->setPassword(kPassword);
        m_sync->refreshCalendars();
        QTRY_COMPARE(m_sync->calendars().size(), 1);
        QCOMPARE(m_sync->calendars().first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Mine"));
        QCOMPARE(m_sync->calendars().first().toMap().value(QStringLiteral("url")).toString(), m_server->base() + QStringLiteral("/custom/calendar"));
    }

    void discoveryWrongPassword()
    {
        m_server->expectedAuth = basic(QStringLiteral("alice"), QStringLiteral("other"));
        m_sync->setServerUrl(m_server->base());
        m_sync->setUsername(QStringLiteral("alice"));
        m_sync->setPassword(kPassword);
        m_sync->refreshCalendars();
        QTRY_COMPARE(m_sync->status(), CalendarSync::Failed);
        QVERIFY(m_sync->lastError().startsWith(QStringLiteral("Login rejected")));
        checkNoSecrets();
    }

    // ----- Sending -----

    void sendsEventOnCompletion()
    {
        configureCustom();
        QCOMPARE(m_sync->status(), CalendarSync::Ready);
        QVERIFY(!m_sync->lastSuccess().isValid());
        m_sync->enqueueWorkSession(record());
        QCOMPARE(m_sync->pendingCount(), 1);
        QTRY_COMPARE(m_sync->pendingCount(), 0);
        QVERIFY(m_sync->lastSuccess().isValid());
        const auto puts = m_server->of("PUT");
        QCOMPARE(puts.size(), 1);
        QCOMPARE(puts.first().path, QByteArray("/cal/personal/ktomato-7-1700000000000@io.github.mineraleyt.ktomato.ics"));
        QCOMPARE(puts.first().headers.value("content-type"), QByteArray("text/calendar; charset=utf-8"));
        QVERIFY(puts.first().headers.value("user-agent").startsWith("kTomato/"));
        QVERIFY(puts.first().body.contains("SUMMARY:Deep work"));
        QCOMPARE(m_sync->status(), CalendarSync::Ready);
        QVERIFY(m_sync->statusText().contains(QStringLiteral("alice")));
        checkNoSecrets();
    }

    void onlyCompletedWorkIsSent()
    {
        configureCustom();
        SessionRecord brk = record(1);
        brk.kind = SessionKind::ShortBreak;
        SessionRecord interrupted = record(2);
        interrupted.completed = false;
        m_sync->enqueueWorkSession(brk);
        m_sync->enqueueWorkSession(interrupted);
        QCOMPARE(m_sync->pendingCount(), 0);
        QTest::qWait(100);
        QCOMPARE(m_server->count("PUT"), 0);
    }

    void disabledSendsAndQueuesNothing()
    {
        configureCustom();
        m_sync->setEnabled(false);
        m_sync->enqueueWorkSession(record());
        QCOMPARE(m_sync->pendingCount(), 0);
        QTest::qWait(100);
        QVERIFY(m_server->requests.isEmpty());

        // Switching off drops what was waiting.
        m_server->defaultPut = 500;
        m_sync->setEnabled(true);
        m_sync->enqueueWorkSession(record());
        QTRY_VERIFY(m_server->count("PUT") >= 1);
        QCOMPARE(m_sync->pendingCount(), 1);
        m_sync->setEnabled(false);
        QCOMPARE(m_sync->pendingCount(), 0);
        const int puts = m_server->count("PUT");
        QTest::qWait(250);
        QCOMPARE(m_server->count("PUT"), puts);
    }

    void unconfiguredQueuesNothing()
    {
        m_sync->setEnabled(true);
        m_sync->enqueueWorkSession(record());
        QCOMPARE(m_sync->pendingCount(), 0);
        QCOMPARE(m_sync->status(), CalendarSync::NotConfigured);
        QVERIFY(m_server->requests.isEmpty());
    }

    void putAnswers_data()
    {
        QTest::addColumn<int>("http");
        QTest::addColumn<bool>("accepted");
        QTest::newRow("201") << 201 << true;
        QTest::newRow("204") << 204 << true;
        QTest::newRow("200") << 200 << true;
    }
    void putAnswers()
    {
        QFETCH(int, http);
        QFETCH(bool, accepted);
        configureCustom();
        m_server->defaultPut = http;
        m_sync->enqueueWorkSession(record());
        QTRY_COMPARE(m_sync->pendingCount(), 0);
        QVERIFY(accepted);
    }

    void put401PausesUntilCredentialsChange()
    {
        configureCustom();
        m_server->expectedAuth = basic(QStringLiteral("alice"), QStringLiteral("rotated"));
        m_sync->enqueueWorkSession(record(1));
        QTRY_COMPARE(m_sync->status(), CalendarSync::Failed);
        QVERIFY(m_sync->isSendingPaused());
        QVERIFY(m_sync->lastError().startsWith(QStringLiteral("Login rejected")));
        QVERIFY(m_sync->statusText().startsWith(QStringLiteral("Login rejected")));
        QCOMPARE(m_sync->pendingCount(), 1);

        m_sync->enqueueWorkSession(record(2));
        QCOMPARE(m_sync->pendingCount(), 2);
        QTest::qWait(250);
        QCOMPARE(m_server->count("PUT"), 1); // refused once, and not retried while paused
        QCOMPARE(m_server->requests.size(), 1);
        checkNoSecrets();

        m_sync->setPassword(QStringLiteral("rotated"));
        QTRY_COMPARE(m_sync->pendingCount(), 0);
        QCOMPARE(m_server->count("PUT"), 3); // the refused one and the two sent
        QCOMPARE(m_sync->status(), CalendarSync::Ready);
    }

    void put401ResumesAfterSuccessfulTest()
    {
        configureCustom();
        m_server->expectedAuth = basic(QStringLiteral("alice"), QStringLiteral("rotated"));
        m_sync->enqueueWorkSession(record(1));
        QTRY_VERIFY(m_sync->isSendingPaused());
        m_server->expectedAuth = basic(QStringLiteral("alice"), kPassword); // the server side was fixed
        m_sync->testConnection();
        QTRY_COMPARE(m_sync->status(), CalendarSync::Ready);
        QVERIFY(!m_sync->isSendingPaused());
        QTRY_COMPARE(m_sync->pendingCount(), 0);
    }

    void put404And500RetryWithBackoff()
    {
        configureCustom();
        m_server->putStatuses = {404, 500};
        m_sync->enqueueWorkSession(record());
        QTRY_VERIFY(m_server->count("PUT") >= 1);
        QTRY_COMPARE(m_sync->status(), CalendarSync::Failed);
        QCOMPARE(m_sync->lastError(), QStringLiteral("Calendar not found"));
        QTRY_COMPARE(m_sync->pendingCount(), 0);
        const auto puts = m_server->of("PUT");
        QCOMPARE(puts.size(), 3);
        QVERIFY2(puts.at(1).at - puts.at(0).at >= 35, "first retry came too early");
        QVERIFY2(puts.at(2).at - puts.at(1).at >= 70, "second retry came too early");
        QCOMPARE(m_sync->status(), CalendarSync::Ready);
        QVERIFY(m_sync->lastError().isEmpty());
    }

    void serverErrorMessage()
    {
        configureCustom();
        m_server->defaultPut = 503;
        m_sync->enqueueWorkSession(record());
        QTRY_VERIFY(!m_sync->lastError().isEmpty());
        QCOMPARE(m_sync->lastError(), QStringLiteral("Server answered HTTP 503"));
        QCOMPARE(m_sync->pendingCount(), 1); // kept for another try
    }

    void permanentRejectionDoesNotBlockQueue()
    {
        configureCustom();
        m_server->putStatuses = {400};
        m_sync->enqueueWorkSession(record(1));
        m_sync->enqueueWorkSession(record(2));
        QTRY_COMPARE(m_sync->pendingCount(), 0);
        QCOMPARE(m_server->stored.size(), 1);
    }

    void queueIsBounded()
    {
        configureCustom();
        m_sync->setRetryDelaysForTesting({600000});
        m_server->defaultPut = 500;
        m_sync->enqueueWorkSession(record(1));
        QTRY_VERIFY(m_server->count("PUT") == 1);
        QTRY_VERIFY(!m_sync->lastError().isEmpty());
        for (int i = 2; i <= 105; ++i) {
            m_sync->enqueueWorkSession(record(i));
        }
        QCOMPARE(m_sync->pendingCount(), 100);
        QVERIFY(m_sync->lastError().contains(QStringLiteral("oldest")));
    }

    void testConnectionResults()
    {
        configureCustom();
        m_sync->testConnection();
        QTRY_COMPARE(m_sync->status(), CalendarSync::Ready);
        QCOMPARE(m_server->count("PUT"), 1);
        QCOMPARE(m_server->count("DELETE"), 1);
        QVERIFY(m_server->stored.isEmpty());
        QVERIFY(m_server->of("PUT").first().path.contains("/cal/personal/ktomato-test-"));
        QVERIFY(m_sync->lastError().isEmpty());

        m_server->defaultPut = 404;
        m_sync->testConnection();
        QTRY_COMPARE(m_sync->status(), CalendarSync::Failed);
        QCOMPARE(m_sync->lastError(), QStringLiteral("Calendar not found"));

        m_server->defaultPut = 502;
        m_sync->testConnection();
        QTRY_COMPARE(m_sync->lastError(), QStringLiteral("Server answered HTTP 502"));

        m_server->defaultPut = 201;
        m_server->expectedAuth = basic(QStringLiteral("x"), QStringLiteral("y"));
        m_sync->testConnection();
        QTRY_VERIFY(m_sync->lastError().startsWith(QStringLiteral("Login rejected: check username and app password")));
        QCOMPARE(m_sync->status(), CalendarSync::Failed);
        checkNoSecrets();
    }

    void networkErrorIsReported()
    {
        configureCustom();
        const QString base = m_server->base();
        m_server.reset(); // nothing listens any more
        m_sync->testConnection();
        QTRY_COMPARE(m_sync->status(), CalendarSync::Failed);
        QVERIFY(!m_sync->lastError().isEmpty());
        QVERIFY(!m_sync->lastError().contains(kPassword));
        Q_UNUSED(base);
    }

    // ----- Notes -----

    void noteUpdateResendsSameUid()
    {
        configureCustom();
        m_sync->setIncludeNote(true);
        m_sync->enqueueWorkSession(record(9));
        QTRY_COMPARE(m_sync->pendingCount(), 0);
        QVERIFY(!m_server->of("PUT").first().body.contains("DESCRIPTION"));
        m_sync->onNoteChanged(9, QStringLiteral("wrote the report, finally"));
        QCOMPARE(m_sync->pendingCount(), 1);
        QTRY_COMPARE(m_sync->pendingCount(), 0);
        const auto puts = m_server->of("PUT");
        QCOMPARE(puts.size(), 2);
        QCOMPARE(puts.at(1).path, puts.at(0).path);
        QVERIFY(puts.at(1).body.contains("DESCRIPTION:wrote the report\\, finally"));
        // The same note again changes nothing.
        m_sync->onNoteChanged(9, QStringLiteral("wrote the report, finally"));
        QCOMPARE(m_sync->pendingCount(), 0);
        // Unknown sessions are ignored.
        m_sync->onNoteChanged(12345, QStringLiteral("x"));
        QCOMPARE(m_sync->pendingCount(), 0);
    }

    void noteUpdatesQueuedItem()
    {
        configureCustom();
        m_sync->setIncludeNote(true);
        m_server->putStatuses = {500};
        m_sync->enqueueWorkSession(record(3));
        QTRY_VERIFY(m_server->count("PUT") >= 1);
        m_sync->onNoteChanged(3, QStringLiteral("queued note"));
        QCOMPARE(m_sync->pendingCount(), 1);
        QTRY_COMPARE(m_sync->pendingCount(), 0);
        QVERIFY(m_server->of("PUT").last().body.contains("DESCRIPTION:queued note"));
        QCOMPARE(m_server->stored.size(), 1);
    }

    void noteNeverSentWhenIncludeNoteOff()
    {
        configureCustom();
        QVERIFY(!m_sync->includeNote());
        m_sync->enqueueWorkSession(record(4));
        QTRY_COMPARE(m_sync->pendingCount(), 0);
        m_sync->onNoteChanged(4, QStringLiteral("private note"));
        QCOMPARE(m_sync->pendingCount(), 0);
        QTest::qWait(100);
        QCOMPARE(m_server->count("PUT"), 1);
        for (const auto &req : std::as_const(m_server->requests)) {
            QVERIFY(!req.body.contains("private note"));
        }
        SessionRecord withNote = record(5);
        withNote.note = QStringLiteral("also private");
        m_sync->enqueueWorkSession(withNote);
        QTRY_COMPARE(m_server->count("PUT"), 2);
        QVERIFY(!m_server->of("PUT").last().body.contains("also private"));
    }

    // ----- Login Flow v2 -----

    void loginFlowHappyPath()
    {
        m_server->expectedAuth = basic(QStringLiteral("alice"), kPassword);
        m_sync->setServerUrl(m_server->base());
        m_sync->startNextcloudLogin();
        QTRY_COMPARE(m_sync->status(), CalendarSync::WaitingForBrowser);
        QCOMPARE(m_opened.size(), 1);
        QCOMPARE(m_opened.first().path(), QStringLiteral("/login/v2/flow/abc"));
        QVERIFY(m_sync->statusText().contains(QStringLiteral("browser")));
        QTRY_COMPARE(m_sync->status(), CalendarSync::Ready);
        QCOMPARE(m_sync->username(), QStringLiteral("alice"));
        QVERIFY(m_sync->hasPassword());
        QCOMPARE(m_sync->calendarName(), QStringLiteral("Personal"));
        QVERIFY(m_sync->calendarUrl().endsWith(QStringLiteral("/remote.php/dav/calendars/alice/personal")));
        QVERIFY(m_server->polls >= 3);
        // The login requests carried no credentials, and the poll sent the token as a form.
        for (const auto &req : std::as_const(m_server->requests)) {
            if (req.path == "/index.php/login/v2" || req.path == "/login/v2/poll") {
                QVERIFY(!req.headers.contains("authorization"));
                // Nextcloud uses this as the name of the new app password: just "kTomato".
                QCOMPARE(req.headers.value("user-agent"), QByteArray("kTomato"));
            }
            if (req.path == "/login/v2/poll") {
                QCOMPARE(req.headers.value("content-type"), QByteArray("application/x-www-form-urlencoded"));
                QCOMPARE(req.body, QByteArray("token=tok123"));
            }
        }
        checkNoSecrets();

        // The new account works, and persists.
        m_sync->setEnabled(true);
        m_sync->enqueueWorkSession(record());
        QTRY_COMPARE(m_sync->pendingCount(), 0);
        auto again = makeSync();
        QVERIFY(again->hasPassword());
        QCOMPARE(again->username(), QStringLiteral("alice"));
        QCOMPARE(again->calendarUrl(), m_sync->calendarUrl());
        QVERIFY(again->enabled());
    }

    void loginFlowSingleCalendarIsPicked()
    {
        m_server->onlyOneCalendar = true;
        m_sync->setServerUrl(m_server->base());
        m_sync->startNextcloudLogin();
        QTRY_COMPARE(m_sync->status(), CalendarSync::Ready);
    }

    void loginFlowLeavesChoiceToUser()
    {
        m_server->noPersonal = true;
        m_server->expectedAuth = basic(QStringLiteral("alice"), kPassword);
        m_sync->setServerUrl(m_server->base());
        m_sync->startNextcloudLogin();
        QTRY_COMPARE(m_sync->calendars().size(), 2);
        QTRY_COMPARE(m_sync->status(), CalendarSync::NotConfigured);
        QVERIFY(m_sync->hasPassword());
        QVERIFY(m_sync->calendarUrl().isEmpty());
        QVERIFY(m_sync->statusText().contains(QStringLiteral("calendar")));
        m_sync->selectCalendar(m_sync->calendars().first().toMap().value(QStringLiteral("url")).toString(), QStringLiteral("Private"));
        QCOMPARE(m_sync->status(), CalendarSync::Ready);
    }

    void loginFlowHostMismatchRejected()
    {
        m_server->loginServer = QStringLiteral("https://evil.example.org");
        m_sync->setServerUrl(m_server->base());
        m_sync->startNextcloudLogin();
        QTRY_COMPARE(m_sync->status(), CalendarSync::Failed);
        QVERIFY(!m_sync->hasPassword());
        QVERIFY(m_sync->username().isEmpty());
        QVERIFY(!m_sync->lastError().isEmpty());
        QVERIFY(!m_sync->lastError().contains(kPassword));
        QCOMPARE(m_server->count("PROPFIND"), 0);
    }

    void loginFlowInsecureServerRejected()
    {
        m_server->loginServer = QStringLiteral("http://example.org");
        m_sync->setServerUrl(m_server->base());
        m_sync->startNextcloudLogin();
        QTRY_COMPARE(m_sync->status(), CalendarSync::Failed);
        QVERIFY(!m_sync->hasPassword());
    }

    void loginFlowCancel()
    {
        m_server->pollsBefore200 = 1000000;
        m_sync->setServerUrl(m_server->base());
        m_sync->startNextcloudLogin();
        QTRY_COMPARE(m_sync->status(), CalendarSync::WaitingForBrowser);
        QTRY_VERIFY(m_server->polls >= 2);
        m_sync->cancelLogin();
        QCOMPARE(m_sync->status(), CalendarSync::NotConfigured);
        QTest::qWait(60); // lets an in-flight poll finish
        const int polls = m_server->polls;
        QTest::qWait(200);
        QCOMPARE(m_server->polls, polls);
        QVERIFY(!m_sync->hasPassword());
    }

    void loginFlowNeedsServerUrl()
    {
        m_sync->startNextcloudLogin();
        QVERIFY(!m_sync->lastError().isEmpty());
        QVERIFY(m_server->requests.isEmpty());
        QVERIFY(m_opened.isEmpty());
    }

    void loginFlowNotNextcloud()
    {
        // The fake answers 404 for unknown paths: use the custom path prefix as the "server".
        m_sync->setServerUrl(m_server->base() + QStringLiteral("/nothing"));
        m_sync->startNextcloudLogin();
        QTRY_COMPARE(m_sync->status(), CalendarSync::Failed);
        QVERIFY(m_opened.isEmpty());
    }

    // ----- Storage -----

    void passwordStorageAndDisconnect()
    {
        configureCustom();
        m_sync->setIncludeNote(true);
        QVERIFY(QFile::exists(configPath()));
        const QFileInfo info(configPath());
        QVERIFY2(!(info.permissions() & (QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ReadOther | QFileDevice::WriteOther)),
                 "ktomatorc must not be readable by others");
        KConfigGroup shared = KSharedConfig::openConfig(configPath(), KConfig::SimpleConfig)->group(QStringLiteral("Calendar"));
        KConfigGroup g = shared.group(QStringLiteral("caldav"));
        QCOMPARE(g.readEntry("AppPassword", QString()), kPassword);
        QVERIFY(!shared.hasKey("AppPassword"));
        QCOMPARE(shared.readEntry("Provider", QString()), QStringLiteral("caldav"));
        QVERIFY(shared.readEntry("Enabled", false));
        QVERIFY(shared.readEntry("IncludeNote", false));

        m_sync->enqueueWorkSession(record());
        m_sync->disconnect();
        QVERIFY(!m_sync->hasPassword());
        QVERIFY(!m_sync->enabled());
        QVERIFY(m_sync->username().isEmpty());
        QVERIFY(m_sync->calendarUrl().isEmpty());
        QVERIFY(m_sync->calendars().isEmpty());
        QCOMPARE(m_sync->pendingCount(), 0);
        QCOMPARE(m_sync->status(), CalendarSync::NotConfigured);
        g.config()->reparseConfiguration();
        QVERIFY(!g.hasKey("AppPassword"));
        QVERIFY(!g.hasKey("Username"));
        QVERIFY(!g.hasKey("CalendarUrl"));
        QVERIFY(!shared.readEntry("Enabled", false));
        QFile f(configPath());
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY(!f.readAll().contains(kPassword.toUtf8()));
    }

    // ----- Per-provider settings -----

    void providersAreSeparate()
    {
        fillNextcloud();
        m_sync->setProvider(QStringLiteral("caldav"));
        QVERIFY(m_sync->serverUrl().isEmpty());
        QVERIFY(m_sync->username().isEmpty());
        QVERIFY(!m_sync->hasPassword());
        QVERIFY(m_sync->calendarUrl().isEmpty());
        QVERIFY(m_sync->calendarName().isEmpty());
        QCOMPARE(m_sync->status(), CalendarSync::NotConfigured);
        QVERIFY(!diskGroup(QStringLiteral("caldav")).hasKey("Username"));

        fillCaldav();
        m_sync->setProvider(QStringLiteral("nextcloud"));
        QCOMPARE(m_sync->serverUrl(), QStringLiteral("https://cloud.example.org"));
        QCOMPARE(m_sync->username(), QStringLiteral("alice"));
        QVERIFY(m_sync->hasPassword());
        QCOMPARE(m_sync->calendarUrl(), QStringLiteral("https://cloud.example.org/cal/personal"));
        QCOMPARE(m_sync->calendarName(), QStringLiteral("Personal"));
        QCOMPARE(m_sync->status(), CalendarSync::Ready);
        m_sync->setProvider(QStringLiteral("caldav"));
        QCOMPARE(m_sync->serverUrl(), QStringLiteral("https://dav.example.net"));
        QCOMPARE(m_sync->username(), QStringLiteral("bob"));
        QCOMPARE(m_sync->calendarName(), QStringLiteral("Work"));

        const KConfigGroup nc = diskGroup(QStringLiteral("nextcloud"));
        const KConfigGroup cd = diskGroup(QStringLiteral("caldav"));
        QCOMPARE(nc.readEntry("ServerUrl", QString()), QStringLiteral("https://cloud.example.org"));
        QCOMPARE(nc.readEntry("Username", QString()), QStringLiteral("alice"));
        QCOMPARE(nc.readEntry("AppPassword", QString()), QStringLiteral("nc-password-1"));
        QCOMPARE(nc.readEntry("CalendarUrl", QString()), QStringLiteral("https://cloud.example.org/cal/personal"));
        QCOMPARE(nc.readEntry("CalendarName", QString()), QStringLiteral("Personal"));
        QCOMPARE(cd.readEntry("ServerUrl", QString()), QStringLiteral("https://dav.example.net"));
        QCOMPARE(cd.readEntry("Username", QString()), QStringLiteral("bob"));
        QCOMPARE(cd.readEntry("AppPassword", QString()), QStringLiteral("cd-password-2"));
        QCOMPARE(cd.readEntry("CalendarName", QString()), QStringLiteral("Work"));
        const KConfigGroup shared = diskGroup();
        for (const char *key : {"ServerUrl", "Username", "AppPassword", "CalendarUrl", "CalendarName"}) {
            QVERIFY2(!shared.hasKey(key), key);
        }
        QCOMPARE(shared.readEntry("Provider", QString()), QStringLiteral("caldav"));

        // A new instance sees the same thing.
        auto again = makeSync();
        QCOMPARE(again->provider(), QStringLiteral("caldav"));
        QCOMPARE(again->username(), QStringLiteral("bob"));
        again->setProvider(QStringLiteral("nextcloud"));
        QCOMPARE(again->username(), QStringLiteral("alice"));
    }

    void switchClearsCalendars()
    {
        m_server->expectedAuth = basic(QStringLiteral("alice"), kPassword);
        m_sync->setServerUrl(m_server->base());
        m_sync->setUsername(QStringLiteral("alice"));
        m_sync->setPassword(kPassword);
        m_sync->refreshCalendars();
        QTRY_VERIFY(!m_sync->calendars().isEmpty());
        QSignalSpy spy(m_sync.get(), &CalendarSync::calendarsChanged);
        m_sync->setProvider(QStringLiteral("caldav"));
        QVERIFY(m_sync->calendars().isEmpty());
        QCOMPARE(spy.count(), 1);
    }

    void switchEmitsSignalsOnce()
    {
        fillNextcloud();
        m_sync->setProvider(QStringLiteral("caldav"));
        fillCaldav();
        m_sync->setProvider(QStringLiteral("nextcloud"));

        QSignalSpy provider(m_sync.get(), &CalendarSync::providerChanged);
        QSignalSpy server(m_sync.get(), &CalendarSync::serverUrlChanged);
        QSignalSpy user(m_sync.get(), &CalendarSync::usernameChanged);
        QSignalSpy pw(m_sync.get(), &CalendarSync::hasPasswordChanged);
        QSignalSpy calUrl(m_sync.get(), &CalendarSync::calendarUrlChanged);
        QSignalSpy calName(m_sync.get(), &CalendarSync::calendarNameChanged);
        QSignalSpy cals(m_sync.get(), &CalendarSync::calendarsChanged);
        QSignalSpy enabled(m_sync.get(), &CalendarSync::enabledChanged);
        m_sync->setProvider(QStringLiteral("caldav"));
        QCOMPARE(provider.count(), 1);
        QCOMPARE(server.count(), 1);
        QCOMPARE(user.count(), 1);
        QCOMPARE(pw.count(), 0); // both have a password
        QCOMPARE(calUrl.count(), 1);
        QCOMPARE(calName.count(), 1);
        QCOMPARE(cals.count(), 1);
        QCOMPARE(enabled.count(), 0);

        // To an empty provider: the password signal fires too.
        m_sync->disconnect();
        m_sync->setProvider(QStringLiteral("nextcloud"));
        provider.clear();
        pw.clear();
        user.clear();
        m_sync->setProvider(QStringLiteral("caldav"));
        QCOMPARE(provider.count(), 1);
        QCOMPARE(pw.count(), 1); // caldav was disconnected above, nextcloud has a password
        m_sync->setProvider(QStringLiteral("nextcloud"));
        QCOMPARE(pw.count(), 2);
        QCOMPARE(user.count(), 2);

        // Same provider again: nothing at all.
        provider.clear();
        cals.clear();
        m_sync->setProvider(QStringLiteral("nextcloud"));
        QCOMPARE(provider.count(), 0);
        QCOMPARE(cals.count(), 0);
    }

    void migratesLegacyFlatConfig_data()
    {
        QTest::addColumn<QString>("provider");
        QTest::addColumn<QString>("other");
        QTest::newRow("caldav") << QStringLiteral("caldav") << QStringLiteral("nextcloud");
        QTest::newRow("nextcloud") << QStringLiteral("nextcloud") << QStringLiteral("caldav");
        QTest::newRow("no provider") << QString() << QStringLiteral("caldav");
    }

    void migratesLegacyFlatConfig()
    {
        QFETCH(QString, provider);
        QFETCH(QString, other);
        m_sync.reset();
        {
            auto cfg = KSharedConfig::openConfig(configPath(), KConfig::SimpleConfig);
            KConfigGroup g = cfg->group(QStringLiteral("Calendar"));
            if (!provider.isEmpty()) {
                g.writeEntry("Provider", provider);
            }
            g.writeEntry("Enabled", true);
            g.writeEntry("IncludeNote", true);
            g.writeEntry("ServerUrl", "https://old.example.org/");
            g.writeEntry("Username", "carol");
            g.writeEntry("AppPassword", "legacy-pw");
            g.writeEntry("CalendarUrl", "https://old.example.org/cal/x");
            g.writeEntry("CalendarName", "Old");
            cfg->sync();
        }
        const QString active = provider.isEmpty() ? QStringLiteral("nextcloud") : provider;
        auto check = [&](CalendarSync *s) {
            QCOMPARE(s->provider(), active);
            QVERIFY(s->enabled());
            QVERIFY(s->includeNote());
            QCOMPARE(s->serverUrl(), QStringLiteral("https://old.example.org"));
            QCOMPARE(s->username(), QStringLiteral("carol"));
            QVERIFY(s->hasPassword());
            QCOMPARE(s->calendarUrl(), QStringLiteral("https://old.example.org/cal/x"));
            QCOMPARE(s->calendarName(), QStringLiteral("Old"));
            const KConfigGroup shared = diskGroup();
            for (const char *key : {"ServerUrl", "Username", "AppPassword", "CalendarUrl", "CalendarName"}) {
                QVERIFY2(!shared.hasKey(key), key);
            }
            QVERIFY(shared.readEntry("Enabled", false));
            QVERIFY(shared.readEntry("IncludeNote", false));
            QCOMPARE(diskGroup(active).readEntry("Username", QString()), QStringLiteral("carol"));
            QCOMPARE(diskGroup(active).readEntry("AppPassword", QString()), QStringLiteral("legacy-pw"));
            QCOMPARE(diskGroup(active).readEntry("CalendarName", QString()), QStringLiteral("Old"));
            QVERIFY(!diskGroup(other).hasKey("Username"));
            QVERIFY(!diskGroup(other).hasKey("AppPassword"));
        };
        m_sync = makeSync();
        check(m_sync.get());
        // Idempotent: loading again changes nothing.
        m_sync = makeSync();
        check(m_sync.get());
        m_sync->setProvider(other);
        QVERIFY(m_sync->username().isEmpty());
        QVERIFY(!m_sync->hasPassword());
        m_sync = makeSync();
        QCOMPARE(m_sync->provider(), other);
        m_sync->setProvider(active);
        QCOMPARE(m_sync->username(), QStringLiteral("carol"));
    }

    void migrationKeepsExistingSubgroup()
    {
        m_sync.reset();
        {
            auto cfg = KSharedConfig::openConfig(configPath(), KConfig::SimpleConfig);
            KConfigGroup g = cfg->group(QStringLiteral("Calendar"));
            g.writeEntry("Provider", "caldav");
            g.writeEntry("Username", "flat");
            g.group(QStringLiteral("caldav")).writeEntry("Username", "already");
            cfg->sync();
        }
        m_sync = makeSync();
        QCOMPARE(m_sync->username(), QStringLiteral("already"));
        QVERIFY(!diskGroup().hasKey("Username"));
    }

    void disconnectWipesOnlyActiveProvider()
    {
        fillNextcloud();
        fillCaldav();
        m_sync->setEnabled(true);
        m_sync->disconnect();
        QVERIFY(!m_sync->enabled());
        QVERIFY(m_sync->username().isEmpty());
        QVERIFY(!m_sync->hasPassword());
        for (const char *key : {"ServerUrl", "Username", "AppPassword", "CalendarUrl", "CalendarName"}) {
            if (QByteArray(key) != "ServerUrl") { // the address is not wiped by disconnect(), as before
                QVERIFY2(!diskGroup(QStringLiteral("caldav")).hasKey(key), key);
            }
            QVERIFY2(diskGroup(QStringLiteral("nextcloud")).hasKey(key), key);
        }
        m_sync->setProvider(QStringLiteral("nextcloud"));
        QCOMPARE(m_sync->username(), QStringLiteral("alice"));
        QVERIFY(m_sync->hasPassword());
        QCOMPARE(m_sync->calendarName(), QStringLiteral("Personal"));
        QVERIFY(!m_sync->enabled());
    }

    void switchCancelsLogin()
    {
        m_server->pollsBefore200 = 1000000;
        m_sync->setServerUrl(m_server->base());
        m_sync->startNextcloudLogin();
        QTRY_COMPARE(m_sync->status(), CalendarSync::WaitingForBrowser);
        QTRY_VERIFY(m_server->polls >= 2);
        m_sync->setProvider(QStringLiteral("caldav"));
        QCOMPARE(m_sync->status(), CalendarSync::NotConfigured);
        QTest::qWait(60);
        const int polls = m_server->polls;
        QTest::qWait(200);
        QCOMPARE(m_server->polls, polls);
        QVERIFY(!m_sync->hasPassword());
        m_sync->setProvider(QStringLiteral("nextcloud"));
        QCOMPARE(m_sync->serverUrl(), m_server->base());
        QVERIFY(!m_sync->hasPassword());
    }

    void passwordOnlyInSubgroupAndFileIsPrivate()
    {
        fillNextcloud();
        fillCaldav();
        const KConfigGroup shared = diskGroup();
        QVERIFY(!shared.hasKey("AppPassword"));
        QCOMPARE(diskGroup(QStringLiteral("nextcloud")).readEntry("AppPassword", QString()), QStringLiteral("nc-password-1"));
        QCOMPARE(diskGroup(QStringLiteral("caldav")).readEntry("AppPassword", QString()), QStringLiteral("cd-password-2"));
        const QFileInfo info(configPath());
        QVERIFY(!(info.permissions() & (QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ReadOther | QFileDevice::WriteOther)));
    }

    void queuedEventsSurviveSwitch()
    {
        configureCustom();
        m_server->defaultPut = 500;
        m_sync->enqueueWorkSession(record(1));
        m_sync->enqueueWorkSession(record(2));
        QTRY_VERIFY(m_server->count("PUT") >= 1);
        QCOMPARE(m_sync->pendingCount(), 2);
        m_sync->setProvider(QStringLiteral("nextcloud"));
        QCOMPARE(m_sync->pendingCount(), 2);
        QVERIFY(m_sync->enabled());
        QCOMPARE(m_sync->status(), CalendarSync::NotConfigured);
        const int puts = m_server->count("PUT");
        QTest::qWait(250);
        QCOMPARE(m_server->count("PUT"), puts); // nothing is sent until configured
        QCOMPARE(m_sync->pendingCount(), 2);

        // Back to the configured provider: the queue drains there.
        m_server->defaultPut = 201;
        m_sync->setProvider(QStringLiteral("caldav"));
        QTRY_COMPARE(m_sync->pendingCount(), 0);
    }

    void defaults()
    {
        QVERIFY(!m_sync->enabled());
        QCOMPARE(m_sync->provider(), QStringLiteral("nextcloud"));
        QVERIFY(!m_sync->includeNote());
        QVERIFY(!m_sync->hasPassword());
        QCOMPARE(m_sync->status(), CalendarSync::NotConfigured);
        QVERIFY(!m_sync->lastSuccess().isValid());
        QCOMPARE(m_sync->pendingCount(), 0);
        m_sync->setProvider(QStringLiteral("bogus"));
        QCOMPARE(m_sync->provider(), QStringLiteral("nextcloud"));
        QVERIFY(m_server->requests.isEmpty()); // nothing happens on its own
    }
};

QTEST_GUILESS_MAIN(CalendarSyncTest)
#include "tst_calendarsync.moc"
