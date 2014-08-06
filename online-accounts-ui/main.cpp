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
#include "i18n.h"
#include "inactivity-timer.h"
#include "indicator-service.h"
#include "request-manager.h"
#include "service.h"
#include "signonui-service.h"

#include <QGuiApplication>
#include <QDBusConnection>
#include <QLibrary>
#include <QProcessEnvironment>

using namespace OnlineAccountsUi;

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    /* The testability driver is only loaded by QApplication but not by
     * QGuiApplication.  However, QApplication depends on QWidget which would
     * add some unneeded overhead => Let's load the testability driver on our
     * own.
     */
    if (app.arguments().contains(QStringLiteral("-testability"))) {
        QLibrary testLib(QStringLiteral("qttestability"));
        if (testLib.load()) {
            typedef void (*TasInitialize)(void);
            TasInitialize initFunction =
                (TasInitialize)testLib.resolve("qt_testability_init");
            if (initFunction) {
                initFunction();
            } else {
                qCritical("Library qttestability resolve failed!");
            }
        } else {
            qCritical("Library qttestability load failed!");
        }
    }

    /* read environment variables */
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    if (environment.contains(QLatin1String("OAU_LOGGING_LEVEL"))) {
        bool isOk;
        int value = environment.value(
            QLatin1String("OAU_LOGGING_LEVEL")).toInt(&isOk);
        if (isOk)
            setLoggingLevel(value);
    }

    /* default daemonTimeout to 5 seconds */
    int daemonTimeout = 5;

    /* override daemonTimeout if OAU_DAEMON_TIMEOUT is set */
    if (environment.contains(QLatin1String("OAU_DAEMON_TIMEOUT"))) {
        bool isOk;
        int value = environment.value(
            QLatin1String("OAU_DAEMON_TIMEOUT")).toInt(&isOk);
        if (isOk)
            daemonTimeout = value;
    }

    initTr(I18N_DOMAIN, NULL);

    RequestManager *requestManager = new RequestManager();

    Service *service = new Service();
    QDBusConnection connection = QDBusConnection::sessionBus();
    connection.registerService(OAU_SERVICE_NAME);
    connection.registerObject(OAU_OBJECT_PATH, service);

    SignOnUi::Service *signonuiService = new SignOnUi::Service();
    connection.registerService(SIGNONUI_SERVICE_NAME);
    connection.registerObject(SIGNONUI_OBJECT_PATH, signonuiService,
                              QDBusConnection::ExportAllContents);

    SignOnUi::IndicatorService *indicatorService =
        new SignOnUi::IndicatorService();
    connection.registerService(WEBCREDENTIALS_BUS_NAME);
    connection.registerObject(WEBCREDENTIALS_OBJECT_PATH,
                              indicatorService->serviceObject());


    InactivityTimer *inactivityTimer = 0;
    if (daemonTimeout > 0) {
        inactivityTimer = new InactivityTimer(daemonTimeout * 1000);
        inactivityTimer->watchObject(requestManager);
        inactivityTimer->watchObject(indicatorService);
        QObject::connect(inactivityTimer, SIGNAL(timeout()),
                         &app, SLOT(quit()));
    }

    int ret = app.exec();

    connection.unregisterService(WEBCREDENTIALS_BUS_NAME);
    connection.unregisterObject(WEBCREDENTIALS_OBJECT_PATH);
    delete indicatorService;

    connection.unregisterService(SIGNONUI_SERVICE_NAME);
    connection.unregisterObject(SIGNONUI_OBJECT_PATH);
    delete signonuiService;

    connection.unregisterService(OAU_SERVICE_NAME);
    connection.unregisterObject(OAU_OBJECT_PATH);
    delete service;

    delete requestManager;

    delete inactivityTimer;

    return ret;
}

