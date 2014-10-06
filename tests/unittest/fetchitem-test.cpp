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
    QOrganizerEDSEngine *m_engine;
    QOrganizerCollection m_collection;
    QList<QOrganizerItem> m_events;

private Q_SLOTS:
    void initTestCase()
    {
        EDSBaseTest::initTestCase();
        const QString collectionName = uniqueCollectionName();
        EDSBaseTest::init();
        m_engine = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());

        // create test collection
        m_collection = QOrganizerCollection();
        QtOrganizer::QOrganizerManager::Error error;
        m_collection.setMetaData(QOrganizerCollection::KeyName, collectionName);
        QVERIFY(m_engine->saveCollection(&m_collection, &error));

        // create test events
        static QString displayLabelValue = QStringLiteral("Display Label %1");
        static QString descriptionValue = QStringLiteral("Description event %1");

        // use this becaue EDS does not store msecs
        QTime currentTime = QTime::currentTime();
        QDateTime date = QDateTime(QDateTime::currentDateTime().date(), QTime(currentTime.hour(), currentTime.minute(), currentTime.second()));
        m_events.clear();
        for(int i=0; i<10; i++) {
            QOrganizerEvent ev;
            ev.setCollectionId(m_collection.id());
            ev.setStartDateTime(date);
            ev.setEndDateTime(date.addSecs(60*30));
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
            date = date.addDays(1);
        }
    }

    void cleanupTestCase()
    {
        m_collection = QOrganizerCollection();
        m_events.clear();
        delete m_engine;
        m_engine = 0;

        EDSBaseTest::cleanup();
    }

    void testFetchById()
    {
        QList<QOrganizerItemId> request;
        request << m_events[4].id();

        QOrganizerItemFetchByIdRequest req;
        req.setIds(request);

        m_engine->startRequest(&req);
        m_engine->waitForRequestFinished(&req, 0);

        QList<QOrganizerItem> expected;
        expected << m_events[4];

        QCOMPARE(expected.size(), req.items().size());
        QList<QOrganizerItemDetail> dr = req.items()[0].details();
        Q_FOREACH(const QOrganizerItemDetail &de, m_events[4].details()) {
            Q_FOREACH(const QOrganizerItemDetail &d, dr) {
                if (de.type() == d.type()) {
                    if (de != d) {
                        qDebug() << "Detail not equal";
                        qDebug() << "\t" << de;
                        qDebug() << "\t" << d;
                        QFAIL("Retrieved item is not equal");
                    } else {
                        qDebug() << "Detail equal";
                    }
                }
            }
        }
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

    void testFetchWithoutDate()
    {
        QOrganizerItemFilter filter;
        QOrganizerItemFetchHint hint;
        QOrganizerManager::Error error;
        QList<QOrganizerItemSortOrder> sort;

        QList<QOrganizerItem> result = m_engine->items(filter, QDateTime(), QDateTime(), 100, sort, hint, &error);
        QCOMPARE(result.size(), 10);
    }
};

QTEST_MAIN(FetchItemTest)

#include "fetchitem-test.moc"
