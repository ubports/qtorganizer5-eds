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
#include "qorganizer-eds-engine.h"

#include <QtCore>
#include <QtTest>

#include <libecal/libecal.h>

using namespace QtOrganizer;

EDSBaseTest::EDSBaseTest()
{
    qRegisterMetaType<QList<QOrganizerCollectionId> >();
    qRegisterMetaType<QList<QOrganizerItemId> >();
    QCoreApplication::addLibraryPath(QORGANIZER_DEV_PATH);
}

EDSBaseTest::~EDSBaseTest()
{
}

void EDSBaseTest::initTestCase()
{
    QTest::qWait(1000);
}


void EDSBaseTest::init()
{
}

void EDSBaseTest::cleanup()
{
}

QString EDSBaseTest::uniqueCollectionName() const
{
    return QUuid::createUuid().toString();
}
