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

class FetchItemTest : public QObject, public EDSBaseTest
{
    Q_OBJECT
private:
    static const QString defaultCollectionName;

    QOrganizerEDSEngine *m_engine;
    QOrganizerCollection m_collection;
    QList<QOrganizerItem> m_events;

private Q_SLOTS:
    void initTestCase()
    {
        EDSBaseTest::init();
        m_engine = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());

        // create test collection
        m_collection = QOrganizerCollection();
        QtOrganizer::QOrganizerManager::Error error;
        m_collection.setMetaData(QOrganizerCollection::KeyName, defaultCollectionName);
        QVERIFY(m_engine->saveCollection(&m_collection, &error));

        // create test events
        static QString displayLabelValue = QStringLiteral("Display Label %1");
        static QString descriptionValue = QStringLiteral("Description event %1");

        m_events.clear();
        for(int i=0; i<10; i++) {
            QOrganizerEvent ev;
            ev.setCollectionId(m_collection.id());
            ev.setStartDateTime(QDateTime(QDate(2013, 9, i+1), QTime(0,30,0)));
            ev.setEndDateTime(QDateTime(QDate(2013, 9, i+1), QTime(0,45,0)));
            ev.setDisplayLabel(displayLabelValue.arg(i));
            ev.setDescription(descriptionValue.arg(i));

            QList<QOrganizerItem> evs;
            evs << ev;
            QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
            bool saveResult = m_engine->saveItems(&evs,
                                                  QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                                  &errorMap,
                                                  &error);
            QVERIFY(saveResult);
            QCOMPARE(error, QOrganizerManager::NoError);
            QVERIFY(errorMap.isEmpty());
            m_events << evs[0];
        }
    }

    void cleanupTestCase()
    {
        QOrganizerManager::Error error;
        QMap<int, QOrganizerManager::Error> errorMap;
        QList<QOrganizerItemId> ids;
        Q_FOREACH(QOrganizerItem i, m_events) {
            ids << i.id();
        }
        m_engine->removeItems(ids, &errorMap, &error);
        QCOMPARE(error, QOrganizerManager::NoError);
        QCOMPARE(errorMap.count(), 0);

        m_collection = QOrganizerCollection();
        m_events.clear();

        delete m_engine;
        m_engine = 0;

        EDSBaseTest::cleanup();
    }

    void testFetchById()
    {
        QList<QOrganizerItemId> request;
        request << m_events[4].id()
                << m_events[2].id();

        QOrganizerItemFetchByIdRequest req;
        req.setIds(request);

        m_engine->startRequest(&req);
        m_engine->waitForRequestFinished(&req, 0);

        QList<QOrganizerItem> expected;
        expected << m_events[4]
                 << m_events[2];

        QCOMPARE(expected.size(), req.items().size());
        Q_FOREACH(QOrganizerItem i, req.items()) {
            expected.removeAll(i);
        }
        QCOMPARE(expected.size(), 0);
    }

    void testFetchWithInvalidId()
    {
        // malformated id
        QList<QOrganizerItemId> request;
        request << QOrganizerItemId::fromString("qorganizer:eds::invalidcollection/invalidcontact");

        QOrganizerItemFetchByIdRequest req;
        req.setIds(request);

        m_engine->startRequest(&req);
        m_engine->waitForRequestFinished(&req, 0);

        QCOMPARE(req.items().size(), 0);
        QMap<int, QOrganizerManager::Error> errors = req.errorMap();
        QCOMPARE(errors.size(), 1);
        QCOMPARE(errors[0], QOrganizerManager::DoesNotExistError);

        // id does not exists
        request.clear();
        request << QOrganizerItemId::fromString("qtorganizer:eds::1386099272.14397.0@organizer/20131203T193432Z-14397-1000-14367-9@organizer");

        QOrganizerItemFetchByIdRequest reqNotFound;
        reqNotFound.setIds(request);

        m_engine->startRequest(&reqNotFound);
        m_engine->waitForRequestFinished(&reqNotFound, 0);

        QCOMPARE(reqNotFound.items().size(), 0);
        errors = reqNotFound.errorMap();
        QCOMPARE(errors.size(), 1);
        QCOMPARE(errors[0], QOrganizerManager::DoesNotExistError);
    }
};

const QString FetchItemTest::defaultCollectionName = QStringLiteral("TEST_FETCH_COLLECTION");

QTEST_MAIN(FetchItemTest)

#include "fetchitem-test.moc"
