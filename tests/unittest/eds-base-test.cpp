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
}

EDSBaseTest::~EDSBaseTest()
{
}

void EDSBaseTest::init()
{
}


void EDSBaseTest::cleanup()
{
    static QStringList defaultSources;

    if (defaultSources.isEmpty()) {
        defaultSources << "qtorganizer:eds::birthdays"
                       << "qtorganizer:eds::system-calendar"
                       << "qtorganizer:eds::system-memo-list"
                       << "qtorganizer:eds::system-task-list";
    }
    QOrganizerEDSEngine *engine = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());
    Q_FOREACH(const QOrganizerCollection &col, engine->collections(0)) {
        if (defaultSources.contains(col.id().toString())) {
            continue;
        }
        QSignalSpy removeCollection(engine, SIGNAL(collectionsRemoved(QList<QOrganizerCollectionId>)));
        QVERIFY(engine->removeCollection(col.id(), 0));
        QTRY_COMPARE(removeCollection.count(), 1);

        QList<QVariant> args = removeCollection.takeFirst();
        QCOMPARE(args.count(), 1);
        QCOMPARE(args[0].value<QList<QOrganizerCollectionId> >().at(0).toString(), col.id().toString());
    }

    delete engine;
}

void EDSBaseTest::appendToRemove(const QtOrganizer::QOrganizerItemId &id)
{
    m_newItems << id;
}
