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

#include "debug.h"
#include "globals.h"
#include "onlineaccountsui_adaptor.h"
#include "request.h"
#include "request-manager.h"
#include "service.h"

using namespace OnlineAccountsUi;

Service::Service(QObject *parent):
    QObject(parent)
{
    new OnlineAccountsUiAdaptor(this);
}

Service::~Service()
{
}

QVariantMap Service::requestAccess(const QVariantMap &options)
{
    DEBUG() << "Got request:" << options;

    /* The following line tells QtDBus not to generate a reply now */
    setDelayedReply(true);

    Request *request = Request::newRequest(connection(),
                                           message(),
                                           options,
                                           this);
    if (request) {
        if (!request->allowMultiple() && Request::find(options)) {
            setDelayedReply(false);
            return QVariantMap();
        }
        RequestManager *manager = RequestManager::instance();
        manager->enqueue(request);
    } else {
        sendErrorReply(OAU_ERROR_INVALID_PARAMETERS,
                       QStringLiteral("Invalid request"));
    }

    return QVariantMap();
}
