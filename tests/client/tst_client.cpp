/*
 * Copyright (C) 2013 Canonical Ltd.
 *
 * Contact: Alberto Mardegan <alberto.mardegan@canonical.com>
 *
 * This file is part of OnlineAccountsClient.
 *
 * OnlineAccountsClient is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OnlineAccountsClient is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OnlineAccountsClient.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "access-control-service/globals.h"

#include <OnlineAccountsClient/Setup>
#include <QDBusConnection>
#include <QDebug>
#include <QFile>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QTest>

using namespace OnlineAccountsClient;

class Service: public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface",
                "com.canonical.OnlineAccounts.AccessControl")

public:
    Service(): QObject() {}
    QVariantMap options() const { return m_options; }

public Q_SLOTS:
    QVariantMap requestAccess(const QVariantMap &options) {
        m_options = options;
        return QVariantMap();
    }

private:
    QVariantMap m_options;
};

class SetupTest: public QObject
{
    Q_OBJECT

public:
    SetupTest();
    QVariantMap options() const { return m_service.options(); }

private Q_SLOTS:
    void initTestCase();
    void testProperties();
    void testExec();
    void testExecWithProvider();
    void testExecWithServiceType();

private:
    Service m_service;
};

SetupTest::SetupTest():
    QObject(0)
{
}

void SetupTest::initTestCase()
{
    QDBusConnection connection = QDBusConnection::sessionBus();
    connection.registerObject(ACCESS_CONTROL_OBJECT_PATH,
                              &m_service,
                              QDBusConnection::ExportAllContents);
    connection.registerService(ACCESS_CONTROL_SERVICE_NAME);
}

void SetupTest::testProperties()
{
    Setup setup;

    QCOMPARE(setup.providerId(), QString());
    QCOMPARE(setup.serviceTypeId(), QString());

    setup.setProviderId("ciao");
    QCOMPARE(setup.providerId(), QString("ciao"));

    setup.setServiceTypeId("hello");
    QCOMPARE(setup.serviceTypeId(), QString("hello"));
}

void SetupTest::testExec()
{
    Setup setup;

    QSignalSpy finished(&setup, SIGNAL(finished()));
    setup.exec();

    QVERIFY(finished.wait());
    QCOMPARE(options().contains(ACS_KEY_PROVIDER), false);
    QCOMPARE(options().contains(ACS_KEY_SERVICE_TYPE), false);
}

void SetupTest::testExecWithProvider()
{
    Setup setup;
    setup.setProviderId("lethal-provider");

    QSignalSpy finished(&setup, SIGNAL(finished()));
    setup.exec();

    QVERIFY(finished.wait());
    QCOMPARE(options().value(ACS_KEY_PROVIDER).toString(),
             QStringLiteral("lethal-provider"));
}

void SetupTest::testExecWithServiceType()
{
    Setup setup;
    setup.setServiceTypeId("e-mail");

    QSignalSpy finished(&setup, SIGNAL(finished()));
    setup.exec();

    QVERIFY(finished.wait());
    QCOMPARE(options().value(ACS_KEY_SERVICE_TYPE).toString(),
             QStringLiteral("e-mail"));
}

QTEST_MAIN(SetupTest);

#include "tst_client.moc"
