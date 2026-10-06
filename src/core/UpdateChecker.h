// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;
class QNetworkReply;

class UpdateChecker : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool checking READ isChecking NOTIFY statusChanged)
    Q_PROPERTY(bool hasUpdate READ hasUpdate NOTIFY statusChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY statusChanged)
    Q_PROPERTY(QString releaseUrl READ releaseUrl NOTIFY statusChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY statusChanged)

public:
    enum class Status {
        Idle,
        Checking,
        UpToDate,
        UpdateAvailable,
        Error
    };
    Q_ENUM(Status)

    explicit UpdateChecker(QObject *parent = nullptr);
    ~UpdateChecker() override;

    Status status() const { return m_status; }
    bool isChecking() const { return m_status == Status::Checking; }
    bool hasUpdate() const { return m_status == Status::UpdateAvailable; }
    QString currentVersion() const;
    QString latestVersion() const { return m_latestVersion; }
    QString releaseUrl() const { return m_releaseUrl; }
    QString errorMessage() const { return m_errorMessage; }

    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void reset();

    /// Compare two version strings ("v1.2.3", "1.2.3-rc1", ...), semver style: a release is
    /// newer than any pre-release of the same version, pre-releases compare naturally
    /// ("rc2" < "rc10"), build metadata ("+...") is ignored. Returns:
    /// > 0 if v1 > v2
    /// 0 if v1 == v2
    /// < 0 if v1 < v2
    static int compareVersions(const QString &v1, const QString &v2);

    /// `url` if it is an https link into this project on github.com, else the project's releases page.
    static QString safeReleaseUrl(const QString &url);

Q_SIGNALS:
    void statusChanged();

private:
    void setStatus(Status s);

    Status m_status = Status::Idle;
    QString m_latestVersion;
    QString m_releaseUrl;
    QString m_errorMessage;
    QNetworkAccessManager *m_nam = nullptr;
    QPointer<QNetworkReply> m_reply; ///< The request in flight; replies of older requests are ignored.
};
