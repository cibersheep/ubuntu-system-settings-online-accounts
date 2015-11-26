include(../../common-project-config.pri)

TARGET = tst_online_accounts_hooks

CONFIG += \
    debug \
    link_pkgconfig

QT += \
    core \
    testlib \
    xml

PKGCONFIG += \
    accounts-qt5

DEFINES += \
    DEBUG_ENABLED \
    HOOK_PROCESS=\\\"../../click-hooks/online-accounts-hooks\\\"

SOURCES += \
    tst_online_accounts_hooks.cpp

check.commands = "xvfb-run -s '-screen 0 640x480x24' -a ./$${TARGET}"
check.depends = $${TARGET}
QMAKE_EXTRA_TARGETS += check
