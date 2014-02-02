/*
 * Copyright 2013 Canonical Ltd.
 *
 * This file is part of qtorganizer5-eds.
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


#include <QObject>
#include <QtTest>
#include <QDebug>

#include <QtOrganizer>

#include "qorganizer-eds-engineid.h"
#include "qorganizer-eds-collection-engineid.h"

using namespace QtOrganizer;

class ItemIdTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testRetriveItemId()
    {
        QString rId;
        QString id("qtorganizer:eds::system-calendar/20130814T212003Z-13580-1000-1995-22@ubuntu#100200023");
        QCOMPARE(QOrganizerEDSEngineId::toComponentId(id, &rId),
                 QStringLiteral("20130814T212003Z-13580-1000-1995-22@ubuntu"));
        QCOMPARE(rId, QStringLiteral("100200023"));
    }

    void testCreateFromString()
    {
        QOrganizerEDSEngineId id("system-calendar", "20130814T212003Z-13580-1000-1995-22@ubuntu");
        QCOMPARE(id.toString(),
                 QStringLiteral("system-calendar/20130814T212003Z-13580-1000-1995-22@ubuntu"));
    }

    void testCollectionIdFromString()
    {
        QOrganizerEDSCollectionEngineId id(QStringLiteral("qtorganizer:eds::system-calendar/20130814T212003Z-13580-1000-1995-22@ubuntu"));
        QOrganizerEDSCollectionEngineId id2(QStringLiteral("eds::system-calendar/20130814T212003Z-13580-1000-1995-22@ubuntu"));
        QOrganizerEDSCollectionEngineId id3(QStringLiteral("system-calendar/20130814T212003Z-13580-1000-1995-22@ubuntu"));
        QVERIFY(id.isEqualTo(&id2));
        QVERIFY(id2.isEqualTo(&id3));
    }

    void testCreateOrganizerId()
    {
        QOrganizerEDSEngineId id("system-calendar", "20130814T212003Z-13580-1000-1995-22@ubuntu");
        QOrganizerItemId id2(new QOrganizerEDSEngineId(id));
        QCOMPARE(id2.managerUri(), QStringLiteral("qtorganizer:eds:"));
        QString targetId = QString("qtorganizer:eds::") + id.toString();
        QCOMPARE(id2.toString(), targetId);
    }
};

QTEST_MAIN(ItemIdTest)

#include "itemid-test.moc"
