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

#include "qorganizer-eds-engine.h"
#include "eds-base-test.h"


using namespace QtOrganizer;

class CancelOperationTest : public QObject, public EDSBaseTest
{
    Q_OBJECT
private:
    QOrganizerEDSEngine *m_engine;
    QOrganizerCollection m_collection;

private Q_SLOTS:
    void init()
    {
        EDSBaseTest::init();

        m_engine = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());

        QtOrganizer::QOrganizerManager::Error error;
        m_collection = QOrganizerCollection();
        m_collection.setMetaData(QOrganizerCollection::KeyName, uniqueCollectionName());

        bool saveResult = m_engine->saveCollection(&m_collection, &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QtOrganizer::QOrganizerManager::NoError);
    }

    void cleanup()
    {
        delete m_engine;
        m_engine = 0;
        EDSBaseTest::cleanup();
    }

    void cancelOperationBeforeStart()
    {
        // initial collections
        QList<QOrganizerCollection> collections = m_engine->collections(0);
        int initialCollectionsCount = collections.count();

        QOrganizerEvent event;
        event.setStartDateTime(QDateTime::currentDateTime());
        event.setDisplayLabel("displayLabelValue");
        event.setDescription("descriptionValue");
        event.setCollectionId(m_collection.id());

        // Try cancel a create item operation
        QSignalSpy createdItem(m_engine, SIGNAL(itemsAdded(QList<QOrganizerItemId>)));
        QOrganizerItemSaveRequest req;
        req.setItem(event);
        m_engine->startRequest(&req);
        QCOMPARE(req.state(), QOrganizerAbstractRequest::ActiveState);
        m_engine->cancelRequest(&req);
        QCOMPARE(req.state(), QOrganizerAbstractRequest::CanceledState);
        QTRY_COMPARE(createdItem.count(), 0);

        // check if collection was not create
        collections = m_engine->collections(0);
        QCOMPARE(collections.count(), initialCollectionsCount);
    }

};

QTEST_MAIN(CancelOperationTest)

#include "cancel-operation-test.moc"
