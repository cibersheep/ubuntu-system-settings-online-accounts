/*
 * Copyright (C) 2013 Canonical Ltd.
 *
 * Contact: Alberto Mardegan <alberto.mardegan@canonical.com>
 *
 * This file is part of online-accounts-ui
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "account-manager.h"
#include "application-manager.h"
#include "debug.h"

#include <Accounts/Application>
#include <QFile>
#include <QSettings>

using namespace OnlineAccountsUi;

ApplicationManager *ApplicationManager::m_instance = 0;

namespace OnlineAccountsUi {
class ApplicationManagerPrivate
{
public:
    ApplicationManagerPrivate();

    bool applicationMatchesProfile(const Accounts::Application &application,
                                   const QString &profile) const;
};
} // namespace

ApplicationManagerPrivate::ApplicationManagerPrivate()
{
}

bool ApplicationManagerPrivate::applicationMatchesProfile(const Accounts::Application &application,
                                                          const QString &profile) const
{
    /* We don't restrict unconfined apps. */
    if (profile == QStringLiteral("unconfined")) return true;

    /* It's a confined app. We must make sure that the applicationId it
     * specified matches the apparmor profile.
     *
     * For click packages, this is relatively easy: we load the .desktop
     * file and checks whether the profile declared in the
     * X-Ubuntu-Application-ID field is the same we are seeing.
     * If we cannot determine that, then we assume that the application is not
     * a click package, and we don't restrict it. */
    QString desktopFilePath = application.desktopFilePath();
    if (!QFile::exists(desktopFilePath)) {
        DEBUG() << "Desktop file not found:" << desktopFilePath;
        /* Every app, be it click or a package from the archive, should have a
         * desktop file. If we don't find it, something is likely to be wrong
         * and it's safer not to continue. */
        return false;
    }

    QSettings desktopFile(desktopFilePath, QSettings::IniFormat);
    QString appId =
        desktopFile.value(QStringLiteral("Desktop Entry/X-Ubuntu-Application-ID")).
        toString();
    if (appId.isEmpty()) {
        // non click package?
        return true;
    }

    return appId == profile;
}

ApplicationManager *ApplicationManager::instance()
{
    if (!m_instance) {
        m_instance = new ApplicationManager;
    }

    return m_instance;
}

ApplicationManager::ApplicationManager(QObject *parent):
    QObject(parent),
    d_ptr(new ApplicationManagerPrivate)
{
}

ApplicationManager::~ApplicationManager()
{
    delete d_ptr;
}

QVariantMap ApplicationManager::applicationInfo(const QString &claimedAppId,
                                                const QString &profile)
{
    Q_D(const ApplicationManager);

    if (Q_UNLIKELY(profile.isEmpty())) return QVariantMap();

    QString applicationId = claimedAppId;
    if (profile.startsWith(applicationId)) {
        /* Click packages might declare just the package name as application
         * ID, but in order to find the correct application file we need the
         * complete ID (with application title and version. */
        applicationId = profile;
    }

    Accounts::Application application =
        AccountManager::instance()->application(applicationId);

    /* Make sure that the app is who it claims to be */
    if (!d->applicationMatchesProfile(application, profile)) {
        DEBUG() << "Given applicationId doesn't match profile";
        return QVariantMap();
    }

    QVariantMap app;
    app.insert(QStringLiteral("id"), applicationId);
    app.insert(QStringLiteral("displayName"), application.displayName());
    app.insert(QStringLiteral("icon"), application.iconName());
    app.insert(QStringLiteral("profile"), profile);

    /* List all the services supported by this application */
    QVariantList serviceIds;
    Accounts::ServiceList allServices =
        AccountManager::instance()->serviceList();
    Q_FOREACH(const Accounts::Service &service, allServices) {
        if (!application.serviceUsage(service).isEmpty()) {
            serviceIds.append(service.name());
        }
    }
    app.insert(QStringLiteral("services"), serviceIds);

    return app;
}

QVariantMap ApplicationManager::providerInfo(const QString &providerId) const
{
    Accounts::Provider provider =
        AccountManager::instance()->provider(providerId);

    QVariantMap info;
    info.insert(QStringLiteral("id"), providerId);
    info.insert(QStringLiteral("displayName"), provider.displayName());
    info.insert(QStringLiteral("icon"), provider.iconName());
    return info;
}
