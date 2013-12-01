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

    // Start new EDS process
    qDebug() << "Start" << EVOLUTION_CALENDAR_FACTORY;
    m_process->start(EVOLUTION_CALENDAR_FACTORY);
    m_process->waitForStarted();
    wait(1000);
    qDebug() << "started";
}

void EDSBaseTest::stopEDS()
{
    m_process->kill();
    m_process->waitForFinished();

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
{
    m_process = new QProcess();
}

EDSBaseTest::~EDSBaseTest()
{
    delete m_process;
}
