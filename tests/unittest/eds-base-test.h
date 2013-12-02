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

#ifndef __EDS_BASE_TEST__
#define __EDS_BASE_TEST__

#include <QtCore/QProcess>

class EDSBaseTest
{
public:
    EDSBaseTest();
    ~EDSBaseTest();

protected:
    QProcess *m_process;

    void startEDS();
    void stopEDS();
    void wait(int msecs);
    bool removeDir(const QString & dirName);

};

#endif
