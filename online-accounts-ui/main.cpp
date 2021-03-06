/*
 * Copyright (C) 2013-2015 Canonical Ltd.
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
#include "ui-server.h"

#include <QGuiApplication>
#include <QLibrary>
#include <QProcessEnvironment>
#include <QSettings>
#include <sys/apparmor.h>

using namespace OnlineAccountsUi;

int main(int argc, char **argv)
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QGuiApplication app(argc, argv);

    /* The testability driver is only loaded by QApplication but not by
     * QGuiApplication.  However, QApplication depends on QWidget which would
     * add some unneeded overhead => Let's load the testability driver on our
     * own.
     */
    if (getenv("QT_LOAD_TESTABILITY")) {
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

    QSettings settings("online-accounts-service");

    /* read environment variables */
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    if (environment.contains(QLatin1String("OAU_LOGGING_LEVEL"))) {
        bool isOk;
        int value = environment.value(
            QLatin1String("OAU_LOGGING_LEVEL")).toInt(&isOk);
        if (isOk)
            setLoggingLevel(value);
    } else {
        setLoggingLevel(settings.value("LoggingLevel", 1).toInt());
    }

    initTr(I18N_DOMAIN, NULL);

    QString socket;
    QString profile;
    QStringList arguments = app.arguments();
    for (int i = 0; i < arguments.count(); i++) {
        const QString &arg = arguments[i];
        if (arg == "--socket") {
            socket = arguments.value(++i);
        } else if (arg == "--profile") {
            profile = arguments.value(++i);
        }
    }
    if (Q_UNLIKELY(socket.isEmpty())) {
        qWarning() << "Missing --socket argument";
        return EXIT_FAILURE;
    }

    if (!profile.isEmpty()) {
        aa_change_profile(profile.toUtf8().constData());
    }

    UiServer server(socket);
    QObject::connect(&server, SIGNAL(finished()),
                     &app, SLOT(quit()));
    if (Q_UNLIKELY(!server.init())) {
        qWarning() << "Could not connect to socket";
        return EXIT_FAILURE;
    }

    return app.exec();
}
