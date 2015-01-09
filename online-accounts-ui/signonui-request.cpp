/*
 * This file is part of online-accounts-ui
 *
 * Copyright (C) 2011-2014 Canonical Ltd.
 *
 * Contact: Alberto Mardegan <alberto.mardegan@canonical.com>
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

#include "signonui-request.h"

#include "browser-request.h"
#include "debug.h"
#include "dialog-request.h"
#include "globals.h"
#include "notification.h"

#include <Accounts/Account>
#include <Accounts/Application>
#include <Accounts/Provider>
#include <OnlineAccountsPlugin/account-manager.h>
#include <OnlineAccountsPlugin/application-manager.h>
#include <OnlineAccountsPlugin/request-handler.h>
#include <QDBusArgument>
#include <QPointer>
#include <SignOn/uisessiondata.h>
#include <SignOn/uisessiondata_priv.h>
#include <sys/apparmor.h>

using namespace SignOnUi;

namespace SignOnUi {

class RequestPrivate: public QObject
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(Request)

public:
    RequestPrivate(Request *request);
    ~RequestPrivate();

private:
    void setWindow(QWindow *window);
    Accounts::Account *findAccount();

private Q_SLOTS:
    void onActionInvoked(const QString &action);
    void onNotificationClosed();

private:
    mutable Request *q_ptr;
    QVariantMap m_clientData;
    QPointer<RequestHandler> m_handler;
    OnlineAccountsUi::Notification *m_notification;
    Accounts::Account *m_account;
    QWindow *m_window;
};

} // namespace

RequestPrivate::RequestPrivate(Request *request):
    QObject(request),
    q_ptr(request),
    m_handler(0),
    m_notification(0),
    m_window(0)
{
    const QVariantMap &parameters = request->parameters();
    if (parameters.contains(SSOUI_KEY_CLIENT_DATA)) {
        QVariant variant = parameters[SSOUI_KEY_CLIENT_DATA];
        m_clientData = (variant.type() == QVariant::Map) ?
            variant.toMap() :
            qdbus_cast<QVariantMap>(variant.value<QDBusArgument>());
    }

    m_account = findAccount();
}

RequestPrivate::~RequestPrivate()
{
    delete m_notification;
    m_notification = 0;
}

void RequestPrivate::setWindow(QWindow *window)
{
    Q_Q(Request);

    /* Don't show the window yet: the user must be presented with a
     * snap-decision, and we'll show the window only if he decides to
     * authenticate. */
    if (Q_UNLIKELY(!m_account)) {
        QVariantMap result;
        result[SSOUI_KEY_ERROR] = SignOn::QUERY_ERROR_FORBIDDEN;
        q->setResult(result);
        return;
    }

    OnlineAccountsUi::ApplicationManager *appManager =
        OnlineAccountsUi::ApplicationManager::instance();
    Accounts::Application application =
        appManager->applicationFromProfile(q->clientApparmorProfile());

    OnlineAccountsUi::AccountManager *accountManager =
        OnlineAccountsUi::AccountManager::instance();
    Accounts::Provider provider =
        accountManager->provider(m_account->providerName());

    QString summary =
        QString("Please authorize %1 to access your %2 account %3").
        arg(application.isValid() ? application.displayName() : "Ubuntu").
        arg(provider.displayName()).
        arg(m_account->displayName());
    m_notification =
        new OnlineAccountsUi::Notification("Authentication request", summary);
    m_notification->addAction("cancel", "Cancel");
    m_notification->addAction("continue", "Authorize...");
    m_notification->setSnapDecision(true);
    QObject::connect(m_notification, SIGNAL(actionInvoked(const QString &)),
                     this, SLOT(onActionInvoked(const QString &)));
    QObject::connect(m_notification, SIGNAL(closed()),
                     this, SLOT(onNotificationClosed()));
    m_notification->show();
    m_window = window;
}

Accounts::Account *RequestPrivate::findAccount()
{
    Q_Q(Request);

    uint identity = q->identity();
    if (identity == 0)
        return 0;

    /* Find the account using this identity.
     * FIXME: there might be more than one!
     */
    OnlineAccountsUi::AccountManager *manager =
        OnlineAccountsUi::AccountManager::instance();
    Q_FOREACH(Accounts::AccountId accountId, manager->accountList()) {
        Accounts::Account *account = manager->account(accountId);
        if (account == 0) continue;

        QVariant value(QVariant::UInt);
        if (account->value("CredentialsId", value) != Accounts::NONE &&
            value.toUInt() == identity) {
            return account;
        }
    }

    // Not found
    return 0;
}

void RequestPrivate::onActionInvoked(const QString &action)
{
    Q_Q(Request);

    DEBUG() << action;

    QObject::disconnect(m_notification, 0, this, 0);
    m_notification->deleteLater();
    m_notification = 0;

    if (action == QStringLiteral("continue")) {
        q->setWindow(m_window);
    } else {
        q->cancel();
    }
}

void RequestPrivate::onNotificationClosed()
{
    Q_Q(Request);

    DEBUG();

    /* setResult() should have been called by onActionInvoked(), but calling it
     * twice won't harm because only the first invocation counts. */
    QVariantMap result;
    result[SSOUI_KEY_ERROR] = SignOn::QUERY_ERROR_FORBIDDEN;
    q->setResult(result);

    m_notification->deleteLater();
    m_notification = 0;
}

#ifndef NO_REQUEST_FACTORY
Request *Request::newRequest(int id,
                             const QString &clientProfile,
                             const QVariantMap &parameters,
                             QObject *parent)
{
    if (parameters.contains(SSOUI_KEY_OPENURL)) {
        return new SignOnUi::BrowserRequest(id, clientProfile,
                                            parameters, parent);
    } else {
        return new SignOnUi::DialogRequest(id, clientProfile,
                                           parameters, parent);
    }
}
#endif

static QString findClientProfile(const QString &clientProfile,
                                 const QVariantMap &parameters)
{
    QString profile = clientProfile;
    /* If the request is coming on the SignOnUi interface from an
     * unconfined process and it carries the SSOUI_KEY_PID key, it means that
     * it's coming from signond. In that case, we want to know what was the
     * client which originated the call.
     */
    if (profile == "unconfined" &&
        parameters.contains(SSOUI_KEY_PID)) {
        pid_t pid = parameters.value(SSOUI_KEY_PID).toUInt();
        char *con = NULL, *mode = NULL;
        int ret = aa_gettaskcon(pid, &con, &mode);
        if (Q_LIKELY(ret >= 0)) {
            profile = QString::fromUtf8(con);
            /* libapparmor allocates "con" and "mode" in a single allocation,
             * so freeing "con" is actually freeing both. */
            free(con);
        } else {
            qWarning() << "Couldn't get apparmor profile of PID" << pid;
        }
    }
    return profile;
}

Request::Request(int id,
                 const QString &clientProfile,
                 const QVariantMap &parameters,
                 QObject *parent):
    OnlineAccountsUi::Request(SIGNONUI_INTERFACE, id,
                              findClientProfile(clientProfile, parameters),
                              parameters, parent),
    d_ptr(new RequestPrivate(this))
{
}

Request::~Request()
{
}

QString Request::ssoId(const QVariantMap &parameters)
{
    return parameters[SSOUI_KEY_REQUESTID].toString();
}

QString Request::ssoId() const
{
    return Request::ssoId(parameters());
}

void Request::setWindow(QWindow *window)
{
    Q_D(Request);

    /* While a notification is shown, ignore any further calls to
     * setWindow(). */
    if (d->m_notification) return;

    /* The first time that this method is called, we handle it by presenting a
     * snap decision to the user.
     * Then, if this is called again with the same QWindow, it means that the
     * snap decision was accepted, and we show the window.
     */
    if (window == d->m_window ||
        /* If we are part of a prompt session, just show the window */
        !qgetenv("MIR_SOCKET").isEmpty()) {
        OnlineAccountsUi::Request::setWindow(window);
        d->m_window = 0;
    } else {
        d->setWindow(window);
    }
}

uint Request::identity() const
{
    return parameters().value(SSOUI_KEY_IDENTITY).toUInt();
}

QString Request::method() const
{
    return parameters().value(SSOUI_KEY_METHOD).toString();
}

QString Request::mechanism() const
{
    return parameters().value(SSOUI_KEY_MECHANISM).toString();
}

QString Request::providerId() const
{
    Q_D(const Request);
    return d->m_account ? d->m_account->providerName() :
        d->m_clientData.value("providerId").toString();
}

const QVariantMap &Request::clientData() const
{
    Q_D(const Request);
    return d->m_clientData;
}

void Request::setHandler(RequestHandler *handler)
{
    Q_D(Request);
    if (handler && d->m_handler) {
        qWarning() << "Request handler already set!";
        return;
    }
    d->m_handler = handler;
}

RequestHandler *Request::handler() const
{
    Q_D(const Request);
    return d->m_handler;
}

void Request::setCanceled()
{
    QVariantMap result;
    result[SSOUI_KEY_ERROR] = SignOn::QUERY_ERROR_CANCELED;

    setResult(result);
}

#include "signonui-request.moc"
