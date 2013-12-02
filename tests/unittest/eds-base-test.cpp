/*
 * Copyright 2013 Canonical Ltd.
 *
 * This file is part of ubuntu-pim-service.
 *
 * contact-service-app is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * contact-service-app is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "eds-base-test.h"

#include <QtCore>
#include <QtTest>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusServiceWatcher>

#include <libecal/libecal.h>

bool EDSBaseTest::removeDir(const QString & dirName)
{
    bool result = true;
    QDir dir(dirName);

    if (dir.exists(dirName)) {
        Q_FOREACH(QFileInfo info, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden  | QDir::AllDirs | QDir::Files, QDir::DirsFirst)) {
            if (info.isDir()) {
                result = removeDir(info.absoluteFilePath());
            }
            else {
                result = QFile::remove(info.absoluteFilePath());
            }

            if (!result) {
                return result;
            }
        }
        result = dir.rmdir(dirName);
    }
    return result;
}

void EDSBaseTest::startEDS()
{
    // Make sure that the data dir is empty
    removeDir(QString("%1/.local/share/evolution").arg(TMP_DIR));

    QDBusConnection con = QDBusConnection::sessionBus();
    QDBusServiceWatcher watcher(EVOLUTION_CALENDAR_SERVICE,
                                con, QDBusServiceWatcher::WatchForRegistration);
    QEventLoop eventLoop;
    eventLoop.connect(&watcher, SIGNAL(serviceRegistered(QString)), SLOT(quit()));

    // Start new EDS process
    if (m_process) {
        delete m_process;
    }
    qDebug() << "ENV" << QProcess::systemEnvironment();
    m_process = new QProcess();
    m_process->start(EVOLUTION_CALENDAR_FACTORY);

    // wait for service to appear
    qDebug() << "Wait service";
    eventLoop.exec();
    m_process->waitForStarted();
    wait(500);
}

void EDSBaseTest::stopEDS()
{
    QDBusConnection con = QDBusConnection::sessionBus();
    QDBusServiceWatcher watcher(EVOLUTION_CALENDAR_SERVICE,
                                con, QDBusServiceWatcher::WatchForUnregistration);

    QEventLoop eventLoop;
    eventLoop.connect(&watcher, SIGNAL(serviceUnregistered(QString)), SLOT(quit()));
    QTimer::singleShot(100, m_process, SLOT(kill()));

    // Wait for service disappear
    qDebug() << "Wait for finish";
    eventLoop.exec();
    m_process->waitForFinished();
    delete m_process;
    m_process = 0;

    // clear data
    removeDir(QString("%1/.local/share/evolution").arg(TMP_DIR));
}

void EDSBaseTest::wait(int msecs)
{
    if (msecs > 0) {
        QEventLoop eventLoop;
        QTimer::singleShot(msecs, &eventLoop, SLOT(quit()));
        eventLoop.exec();
    }
}

EDSBaseTest::EDSBaseTest()
    : m_process(0)
{
}

EDSBaseTest::~EDSBaseTest()
{
    if (m_process) {
        delete m_process;
    }
}
