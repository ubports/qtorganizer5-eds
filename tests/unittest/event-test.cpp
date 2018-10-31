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

class EventTest : public QObject, public EDSBaseTest
{
    Q_OBJECT
private:
    static const QString collectionTypePropertyName;
    static const QString taskListTypeName;
    static int signalIndex;

    QDateTime m_itemRemovedTime;
    QDateTime m_requestFinishedTime;
    QOrganizerEDSEngine *m_engine;
    QOrganizerCollection m_collection;


private Q_SLOTS:
    void initTestCase()
    {
        EDSBaseTest::init();

        m_engine = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());

        QtOrganizer::QOrganizerManager::Error error;
        m_collection = QOrganizerCollection();
        m_collection.setMetaData(QOrganizerCollection::KeyName, uniqueCollectionName());
        m_collection.setExtendedMetaData(collectionTypePropertyName, taskListTypeName);

        QSignalSpy createdCollection(m_engine, SIGNAL(collectionsAdded(QList<QOrganizerCollectionId>)));
        bool saveResult = m_engine->saveCollection(&m_collection, &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QtOrganizer::QOrganizerManager::NoError);
        QTRY_COMPARE(createdCollection.count(), 1);
    }

    void cleanupTestCase()
    {
        delete m_engine;
        EDSBaseTest::cleanup();
    }

    void init()
    {
        signalIndex = 0;
        m_itemRemovedTime = QDateTime();
        m_requestFinishedTime = QDateTime();
    }

    //helper
    void itemRemoved()
    {
        m_itemRemovedTime = QDateTime::currentDateTime();
        // avoid both signals to be fired at the same time
        QTest::qSleep(100);
    }

    void requestFinished(QOrganizerAbstractRequest::State state)
    {
        if (state == QOrganizerAbstractRequest::FinishedState) {
            m_requestFinishedTime = QDateTime::currentDateTime();
            // avoid both signals to be fired at the same time
            QTest::qSleep(100);
        }
    }

    // test functions
    void testCreateEventWithReminder()
    {
        static QString displayLabelValue = QStringLiteral("Todo test");
        static QString descriptionValue = QStringLiteral("Todo description");

        QOrganizerTodo todo;
        todo.setCollectionId(m_collection.id());
        todo.setStartDateTime(QDateTime::currentDateTime());
        todo.setDisplayLabel(displayLabelValue);
        todo.setDescription(descriptionValue);

        QOrganizerItemVisualReminder vReminder;
        vReminder.setDataUrl(QUrl("http://www.alarms.com"));
        vReminder.setMessage("Test visual reminder");

        QOrganizerItemAudibleReminder aReminder;
        aReminder.setSecondsBeforeStart(10);
        aReminder.setRepetition(10, 20);
        aReminder.setDataUrl(QUrl("file://home/user/My Musics/play as alarm.wav"));

        todo.saveDetail(&aReminder);
        todo.saveDetail(&vReminder);

        QSignalSpy itemsAdded(m_engine, &QOrganizerManagerEngine::itemsAdded);
        QtOrganizer::QOrganizerManager::Error error;
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        QList<QOrganizerItem> items;
        items << todo;
        bool saveResult = m_engine->saveItems(&items,
                                              QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                              &errorMap,
                                              &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QtOrganizer::QOrganizerManager::NoError);
        QTRY_COMPARE(itemsAdded.count(), 1);

        QOrganizerItemSortOrder sort;
        QOrganizerItemFetchHint hint;
        QOrganizerItemIdFilter filter;

        QList<QOrganizerItemId> ids;
        ids << items[0].id();
        filter.setIds(ids);
        items = m_engine->items(filter,
                      QDateTime(),
                      QDateTime(),
                      10,
                      sort,
                      hint,
                      &error);
        QCOMPARE(items.count(), 1);

        // audible
        QList<QOrganizerItemDetail> reminders = items[0].details(QOrganizerItemDetail::TypeAudibleReminder);
        QCOMPARE(reminders.size(), 1);

        QOrganizerItemAudibleReminder aReminder2 = reminders[0];
        QCOMPARE(aReminder2.secondsBeforeStart(), aReminder.secondsBeforeStart());
        QCOMPARE(aReminder2.repetitionCount(), aReminder.repetitionCount());
        QCOMPARE(aReminder2.repetitionDelay(), aReminder.repetitionDelay());
        QCOMPARE(aReminder2.dataUrl(), aReminder.dataUrl());
        //QCOMPARE(aReminder2, aReminder);

        // visual
        reminders = items[0].details(QOrganizerItemDetail::TypeVisualReminder);
        QCOMPARE(reminders.size(), 1);

        QOrganizerItemVisualReminder vReminder2 = reminders[0];
        //vReminder.setRepetition(1, 0);
        QCOMPARE(vReminder2.secondsBeforeStart(), vReminder.secondsBeforeStart());
        QCOMPARE(vReminder2.repetitionCount(), vReminder.repetitionCount());
        QCOMPARE(vReminder2.repetitionDelay(), vReminder.repetitionDelay());
        QCOMPARE(vReminder2.dataUrl(), vReminder.dataUrl());
        QCOMPARE(vReminder2.message(), vReminder.message());
    }

    void testCreateEventWithEmptyReminder()
    {
        static QString displayLabelValue = QStringLiteral("Todo test with empty reminder");
        static QString descriptionValue = QStringLiteral("Todo description with empty reminder");

        QOrganizerTodo todo;
        todo.setCollectionId(m_collection.id());
        todo.setStartDateTime(QDateTime::currentDateTime());
        todo.setDisplayLabel(displayLabelValue);
        todo.setDescription(descriptionValue);

        QOrganizerItemVisualReminder vReminder;
        vReminder.setDataUrl(QUrl(""));
        vReminder.setMessage("reminder message");

        QOrganizerItemAudibleReminder aReminder;
        aReminder.setSecondsBeforeStart(0);
        aReminder.setDataUrl(QString());

        todo.saveDetail(&aReminder);
        todo.saveDetail(&vReminder);

        QtOrganizer::QOrganizerManager::Error error;
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        QList<QOrganizerItem> items;
        items << todo;
        bool saveResult = m_engine->saveItems(&items,
                                              QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                              &errorMap,
                                              &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QtOrganizer::QOrganizerManager::NoError);

        QOrganizerItemSortOrder sort;
        QOrganizerItemFetchHint hint;
        QOrganizerItemIdFilter filter;

        QList<QOrganizerItemId> ids;
        ids << items[0].id();
        filter.setIds(ids);
        items = m_engine->items(filter,
                      QDateTime(),
                      QDateTime(),
                      10,
                      sort,
                      hint,
                      &error);
        QCOMPARE(items.count(), 1);

        // audible
        QList<QOrganizerItemDetail> reminders = items[0].details(QOrganizerItemDetail::TypeAudibleReminder);
        QCOMPARE(reminders.size(), 1);

        QOrganizerItemAudibleReminder aReminder2 = reminders[0];
        QCOMPARE(aReminder2.isEmpty(), false);
        QCOMPARE(aReminder2.secondsBeforeStart(), 0);
        QCOMPARE(aReminder2.repetitionCount(), 0);
        QCOMPARE(aReminder2.repetitionDelay(), 0);
        QVERIFY(aReminder2.dataUrl().isEmpty());

        // visual
        reminders = items[0].details(QOrganizerItemDetail::TypeVisualReminder);
        QCOMPARE(reminders.size(), 1);

        QOrganizerItemVisualReminder vReminder2 = reminders[0];
        //vReminder.setRepetition(1, 0);
        QCOMPARE(vReminder2.isEmpty(), false);
        QCOMPARE(vReminder2.secondsBeforeStart(), vReminder.secondsBeforeStart());
        QCOMPARE(vReminder2.repetitionCount(), vReminder.repetitionCount());
        QCOMPARE(vReminder2.repetitionDelay(), vReminder.repetitionDelay());
        QVERIFY(vReminder2.dataUrl().isEmpty());
        QCOMPARE(vReminder2.message(), QString("reminder message"));
    }

    void testRemoveEvent()
    {
        static QString displayLabelValue = QStringLiteral("event to be removed");
        static QString descriptionValue = QStringLiteral("removable event");

        QOrganizerTodo todo;
        todo.setCollectionId(m_collection.id());
        todo.setStartDateTime(QDateTime::currentDateTime());
        todo.setDisplayLabel(displayLabelValue);
        todo.setDescription(descriptionValue);

        QtOrganizer::QOrganizerManager::Error error;
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        QList<QOrganizerItem> items;
        items << todo;
        bool saveResult = m_engine->saveItems(&items,
                                            QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                            &errorMap,
                                            &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QOrganizerManager::NoError);
        QCOMPARE(items.size(), 1);
        QVERIFY(errorMap.isEmpty());
        QVERIFY(!items[0].id().isNull());

        QOrganizerItemRemoveRequest req;
        connect(&req, SIGNAL(stateChanged(QOrganizerAbstractRequest::State)),
                this, SLOT(requestFinished(QOrganizerAbstractRequest::State)));
        connect(m_engine, SIGNAL(itemsRemoved(QList<QOrganizerItemId>)),
                this, SLOT(itemRemoved()));
        req.setItem(items[0]);

        m_engine->startRequest(&req);
        m_engine->waitForRequestFinished(&req, -1);

        // check if the signal item removed was fired after the request finish
        QTRY_VERIFY(m_requestFinishedTime.isValid());
        QTRY_VERIFY(m_itemRemovedTime.isValid());
        if (m_itemRemovedTime < m_requestFinishedTime) {
            qDebug() << "Item removed before request finish";
            qDebug() << "Removed time" << m_itemRemovedTime;
            qDebug() << "RequestFinished time" << m_requestFinishedTime;
        }
        QVERIFY(m_itemRemovedTime >= m_requestFinishedTime);
    }

    void testRemoveItemById()
    {
        static QString displayLabelValue = QStringLiteral("event to be removed");
        static QString descriptionValue = QStringLiteral("removable event");

        QOrganizerTodo todo;
        todo.setCollectionId(m_collection.id());
        todo.setStartDateTime(QDateTime::currentDateTime());
        todo.setDisplayLabel(displayLabelValue);
        todo.setDescription(descriptionValue);

        QSignalSpy itemsAdded(m_engine, &QOrganizerManagerEngine::itemsAdded);
        QtOrganizer::QOrganizerManager::Error error;
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        QList<QOrganizerItem> items;
        items << todo;
        bool saveResult = m_engine->saveItems(&items,
                                            QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                            &errorMap,
                                            &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(errorMap.isEmpty());
        QOrganizerItemId id = items[0].id();
        QVERIFY(!id.isNull());
        QTRY_COMPARE(itemsAdded.count(), 1);

        QOrganizerItemRemoveByIdRequest req;
        connect(&req, SIGNAL(stateChanged(QOrganizerAbstractRequest::State)),
                this, SLOT(requestFinished(QOrganizerAbstractRequest::State)));
        connect(m_engine, SIGNAL(itemsRemoved(QList<QOrganizerItemId>)),
                this, SLOT(itemRemoved()));
        req.setItemId(id);

        m_engine->startRequest(&req);
        m_engine->waitForRequestFinished(&req, -1);

        // check if the signal item removed was fired after the request finish
        QTRY_VERIFY(m_requestFinishedTime.isValid());
        QTRY_VERIFY(m_itemRemovedTime.isValid());
        QVERIFY(m_itemRemovedTime > m_requestFinishedTime);

        // check if item was removed
        QOrganizerItemSortOrder sort;
        QOrganizerItemFetchHint hint;
        QOrganizerItemIdFilter filter;

        QList<QOrganizerItemId> ids;
        ids << id;
        filter.setIds(ids);
        items = m_engine->items(filter,
                      QDateTime(),
                      QDateTime(),
                      10,
                      sort,
                      hint,
                      &error);
        QCOMPARE(items.count(), 0);
    }

    void testCreateEventWithoutCollection()
    {
        static QString displayLabelValue = QStringLiteral("event without collection");
        static QString descriptionValue = QStringLiteral("event without collection");

        QOrganizerEvent event;
        event.setStartDateTime(QDateTime::currentDateTime());
        event.setDisplayLabel(displayLabelValue);
        event.setDescription(descriptionValue);

        QSignalSpy itemsAdded(m_engine, &QOrganizerManagerEngine::itemsAdded);
        QtOrganizer::QOrganizerManager::Error error;
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        QList<QOrganizerItem> items;
        items << event;
        bool saveResult = m_engine->saveItems(&items,
                                              QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                              &errorMap,
                                              &error);

        QVERIFY(saveResult);
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(errorMap.isEmpty());
        QVERIFY(!items[0].id().isNull());
        QTRY_COMPARE(itemsAdded.count(), 1);

        // check if item was created on the default collection
        QOrganizerItemFetchHint hint;

        QList<QOrganizerItemId> ids;
        ids << items[0].id();
        items = m_engine->items(ids, hint, 0, 0);
        QCOMPARE(items.count(), 1);
        QOrganizerCollectionId collectionId = m_engine->defaultCollectionId();
        QCOMPARE(items[0].collectionId(), collectionId);
    }

    void testCreateMultipleItemsWithSameCollection()
    {
        static QString displayLabelValue = QStringLiteral("Multiple Item:%1");
        static QString descriptionValue = QStringLiteral("Multiple Item desc:%1");

        QList<QOrganizerItem> evs;
        for(int i=0; i<10; i++) {
            QOrganizerTodo todo;
            todo.setCollectionId(m_collection.id());
            todo.setStartDateTime(QDateTime::currentDateTime());
            todo.setDisplayLabel(displayLabelValue.arg(i));
            todo.setDescription(descriptionValue.arg(i));
            evs << todo;
        }

        QSignalSpy itemsAdded(m_engine, &QOrganizerManagerEngine::itemsAdded);
        QtOrganizer::QOrganizerManager::Error error;
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        bool saveResult = m_engine->saveItems(&evs,
                                              QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                              &errorMap,
                                              &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(errorMap.isEmpty());
        QCOMPARE(evs.count(), 10);
        Q_FOREACH(const QOrganizerItem &i, evs) {
            QVERIFY(!i.id().isNull());
        }
        QTRY_VERIFY(!itemsAdded.isEmpty());
    }

    void testCreateMultipleItemsWithDiffCollections()
    {
        static QString displayLabelValue = QStringLiteral("Multiple Item:%1");
        static QString descriptionValue = QStringLiteral("Multiple Item desc:%1");

        QList<QOrganizerItem> evs;
        for(int i=0; i<10; i++) {
            QOrganizerTodo todo;
            todo.setCollectionId(m_collection.id());
            todo.setStartDateTime(QDateTime(QDate(2013, 9, 3+1), QTime(0,30,0)));
            todo.setDisplayLabel(displayLabelValue.arg(i));
            todo.setDescription(descriptionValue.arg(i));
            evs << todo;
        }

        QtOrganizer::QOrganizerManager::Error error;
        QOrganizerCollection eventCollection = QOrganizerCollection();
        eventCollection.setMetaData(QOrganizerCollection::KeyName, uniqueCollectionName());
        bool saveResult = m_engine->saveCollection(&eventCollection, &error);
        QVERIFY(saveResult);

        for(int i=0; i<10; i++) {
            QOrganizerEvent ev;
            ev.setCollectionId(eventCollection.id());
            ev.setStartDateTime(QDateTime(QDate(2013, 10, 3+1), QTime(0,30,0)));
            ev.setDisplayLabel(displayLabelValue.arg(i));
            ev.setDescription(descriptionValue.arg(i));
            evs << ev;
        }

        QSignalSpy itemsAdded(m_engine, &QOrganizerManagerEngine::itemsAdded);
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        saveResult = m_engine->saveItems(&evs,
                                         QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                         &errorMap,
                                         &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(errorMap.isEmpty());
        QCOMPARE(evs.count(), 20);
        Q_FOREACH(const QOrganizerItem &i, evs) {
            QVERIFY(!i.id().isNull());
        }
        QTRY_VERIFY(!itemsAdded.isEmpty());
    }

    void testCauseErrorDuringCreateMultipleItems()
    {
        static QString displayLabelValue = QStringLiteral("Multiple Item:%1");
        static QString descriptionValue = QStringLiteral("Multiple Item desc:%1");

        QList<QOrganizerItem> evs;

        QOrganizerTodo todo;
        todo.setCollectionId(m_collection.id());
        todo.setStartDateTime(QDateTime(QDate(2013, 9, 1), QTime(0,30,0)));
        todo.setDisplayLabel(displayLabelValue.arg(1));
        todo.setDescription(descriptionValue.arg(1));
        evs << todo;

        // This item will cause error, because the collection ID is invalid
        QOrganizerEvent ev;
        QOrganizerCollectionId cid(m_engine->managerUri(), "XXXXXX");
        QVERIFY(!cid.isNull());
        QCOMPARE(cid.toString(), QStringLiteral("qtorganizer:eds::585858585858"));
        ev.setCollectionId(cid);
        ev.setStartDateTime(QDateTime(QDate(2013, 10, 2), QTime(0,30,0)));
        ev.setDisplayLabel(displayLabelValue.arg(2));
        ev.setDescription(descriptionValue.arg(2));
        evs << ev;

        todo.setStartDateTime(QDateTime(QDate(2013, 9, 3), QTime(0,30,0)));
        todo.setDisplayLabel(displayLabelValue.arg(3));
        todo.setDescription(descriptionValue.arg(3));
        evs << todo;

        QSignalSpy itemsAdded(m_engine, &QOrganizerManagerEngine::itemsAdded);
        QtOrganizer::QOrganizerManager::Error error;
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        bool saveResult = m_engine->saveItems(&evs,
                                              QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                              &errorMap,
                                              &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QOrganizerManager::NoError);
        QCOMPARE(evs.count(), 2);
        QCOMPARE(errorMap.size(), 1);
        QCOMPARE(errorMap[1], QOrganizerManager::InvalidCollectionError);
        QTRY_VERIFY(!itemsAdded.isEmpty());
    }

    void testCreateAllDayTodo()
    {
        static QString displayLabelValue = QStringLiteral("All day title");
        static QString descriptionValue = QStringLiteral("All day description");

        QDateTime eventDateTime = QDateTime(QDate(2013, 9, 3), QTime(0,30,0));
        QOrganizerTodo todo;
        todo.setCollectionId(m_collection.id());
        todo.setAllDay(true);
        todo.setStartDateTime(eventDateTime);
        todo.setDisplayLabel(displayLabelValue);
        todo.setDescription(descriptionValue);

        QSignalSpy itemsAdded(m_engine, &QOrganizerManagerEngine::itemsAdded);
        QtOrganizer::QOrganizerManager::Error error;
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        QList<QOrganizerItem> items;
        items << todo;
        bool saveResult = m_engine->saveItems(&items,
                                            QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                            &errorMap,
                                            &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(errorMap.isEmpty());
        QOrganizerItemId id = items[0].id();
        QVERIFY(!id.isNull());
        QTRY_COMPARE(itemsAdded.count(), 1);

        QOrganizerItemFetchHint hint;
        QList<QOrganizerItemId> ids;
        ids << items[0].id();
        items = m_engine->items(ids, hint, &errorMap, &error);
        QCOMPARE(items.count(), 1);

        QOrganizerTodo todoResult = static_cast<QOrganizerTodo>(items[0]);
        QCOMPARE(todoResult.isAllDay(), true);
        QCOMPARE(todoResult.startDateTime().date(), eventDateTime.date());
        QCOMPARE(todoResult.startDateTime().time(), QTime(0, 0, 0));
    }

    void testCreateAllDayEvent()
    {
        static QString displayLabelValue = QStringLiteral("All day title");
        static QString descriptionValue = QStringLiteral("All day description");

        QDateTime eventDateTime = QDateTime(QDate(2013, 9, 3), QTime(0,30,0));
        QOrganizerEvent event;
        event.setStartDateTime(eventDateTime);
        event.setEndDateTime(eventDateTime.addDays(1));
        event.setDisplayLabel(displayLabelValue);
        event.setDescription(descriptionValue);
        event.setAllDay(true);

        QSignalSpy itemsAdded(m_engine, &QOrganizerManagerEngine::itemsAdded);
        QtOrganizer::QOrganizerManager::Error error;
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        QList<QOrganizerItem> items;
        items << event;
        bool saveResult = m_engine->saveItems(&items,
                                            QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                            &errorMap,
                                            &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(errorMap.isEmpty());
        QOrganizerItemId id = items[0].id();
        QVERIFY(!id.isNull());
        QTRY_COMPARE(itemsAdded.count(), 1);

        QOrganizerItemFetchHint hint;
        QList<QOrganizerItemId> ids;
        ids << items[0].id();
        items = m_engine->items(ids, hint, &errorMap, &error);
        QCOMPARE(items.count(), 1);

        QOrganizerEvent eventResult = static_cast<QOrganizerEvent>(items[0]);
        QCOMPARE(eventResult.isAllDay(), true);
        QCOMPARE(eventResult.startDateTime().date(), eventDateTime.date());
        QCOMPARE(eventResult.startDateTime().time(), QTime(0, 0, 0));
        QCOMPARE(eventResult.endDateTime().date(), eventDateTime.date().addDays(1));
        QCOMPARE(eventResult.endDateTime().time(), QTime(0, 0, 0));
    }

    void testModifyAllDayEvent()
    {
        static QString displayLabelValue = QStringLiteral("All day title");
        static QString descriptionValue = QStringLiteral("All day description");

        QDateTime eventDateTime = QDateTime(QDate(2013, 9, 3), QTime(0,30,0));
        QOrganizerEvent event;
        event.setStartDateTime(eventDateTime);
        event.setEndDateTime(eventDateTime.addDays(1));
        event.setDisplayLabel(displayLabelValue);
        event.setDescription(descriptionValue);
        event.setAllDay(true);

        QSignalSpy itemsAdded(m_engine, &QOrganizerManagerEngine::itemsAdded);
        QtOrganizer::QOrganizerManager::Error error;
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        QList<QOrganizerItem> items;
        items << event;
        bool saveResult = m_engine->saveItems(&items,
                                            QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                            &errorMap,
                                            &error);
        QVERIFY(saveResult);

        QOrganizerEvent eventResult = static_cast<QOrganizerEvent>(items[0]);
        eventResult.setDescription(QStringLiteral("New description"));
        items.clear();
        items << eventResult;
        saveResult = m_engine->saveItems(&items,
                                         QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                         &errorMap,
                                         &error);
        QTRY_COMPARE(itemsAdded.count(), 1);

        QOrganizerItemFetchHint hint;
        QList<QOrganizerItemId> ids;
        ids << items[0].id();
        items = m_engine->items(ids, hint, &errorMap, &error);
        QCOMPARE(items.count(), 1);

        eventResult = static_cast<QOrganizerEvent>(items[0]);
        QCOMPARE(eventResult.description(), QStringLiteral("New description"));
        QCOMPARE(eventResult.isAllDay(), true);
        QCOMPARE(eventResult.startDateTime().date(), eventDateTime.date());
        QCOMPARE(eventResult.startDateTime().time(), QTime(0, 0, 0));
        QCOMPARE(eventResult.endDateTime().date(), eventDateTime.date().addDays(1));
        QCOMPARE(eventResult.endDateTime().time(), QTime(0, 0, 0));
    }

    void testCreateAllDayEventWithInvalidEndDate()
    {
        static QString displayLabelValue = QStringLiteral("All day title");
        static QString descriptionValue = QStringLiteral("All day description");

        QDateTime eventDateTime = QDateTime(QDate(2013, 9, 3), QTime(0,30,0));
        QOrganizerEvent event;
        event.setStartDateTime(eventDateTime);
        event.setEndDateTime(eventDateTime.addDays(-10));
        event.setDisplayLabel(displayLabelValue);
        event.setDescription(descriptionValue);
        event.setAllDay(true);

        QtOrganizer::QOrganizerManager::Error error;
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        QList<QOrganizerItem> items;
        items << event;
        QSignalSpy itemsAdded(m_engine, &QOrganizerManagerEngine::itemsAdded);
        bool saveResult = m_engine->saveItems(&items,
                                            QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                            &errorMap,
                                            &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(errorMap.isEmpty());
        QTRY_COMPARE(itemsAdded.count(), 1);
        QOrganizerItemId id = items[0].id();
        QVERIFY(!id.isNull());

        QOrganizerItemFetchHint hint;
        QList<QOrganizerItemId> ids;
        ids << items[0].id();
        items = m_engine->items(ids, hint, &errorMap, &error);
        QCOMPARE(items.count(), 1);

        QOrganizerEvent eventResult = static_cast<QOrganizerEvent>(items[0]);
        QCOMPARE(eventResult.isAllDay(), true);
        QCOMPARE(eventResult.startDateTime().date(), eventDateTime.date());
        QCOMPARE(eventResult.startDateTime().time(), QTime(0, 0, 0));
        QCOMPARE(eventResult.endDateTime().date(), eventDateTime.date().addDays(1));
        QCOMPARE(eventResult.endDateTime().time(), QTime(0, 0, 0));
    }

    void testCreateTodoEventWithStartDate()
    {
        static QString displayLabelValue = QStringLiteral("Event todo with start date");
        static QString descriptionValue = QStringLiteral("Event todo with start date description");
        static QDateTime startDateTime = QDateTime(QDate(2013, 9, 3), QTime(0,30,0));

        // create a new item
        QOrganizerTodo todo;
        todo.setCollectionId(m_collection.id());
        todo.setStartDateTime(startDateTime);
        todo.setDisplayLabel(displayLabelValue);
        todo.setDescription(descriptionValue);

        QtOrganizer::QOrganizerManager::Error error;
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        QList<QOrganizerItem> items;
        items << todo;
        QSignalSpy itemsAdded(m_engine, &QOrganizerManagerEngine::itemsAdded);
        bool saveResult = m_engine->saveItems(&items,
                                              QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                              &errorMap,
                                              &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(errorMap.isEmpty());
        QTRY_COMPARE(itemsAdded.count(), 1);

        // query by the new item
        QOrganizerItemFetchHint hint;
        QList<QOrganizerItemId> ids;
        ids << items[0].id();
        items = m_engine->items(ids, hint, &errorMap, &error);
        QCOMPARE(items.count(), 1);

        // compare start datetime
        QOrganizerTodo newTodo = static_cast<QOrganizerTodo>(items[0]);
        QCOMPARE(newTodo.startDateTime(), startDateTime);
    }

    void testCreateWithDiffTimeZone()
    {
        static QString displayLabelValue = QStringLiteral("Event with diff timezone");
        static QString descriptionValue = QStringLiteral("Event with diff timezone description");
        static QDateTime startDateTime = QDateTime(QDate(2013, 9, 3), QTime(0, 30, 0), QTimeZone("Asia/Bangkok"));

        // create a new item
        QOrganizerTodo todo;
        todo.setCollectionId(m_collection.id());
        todo.setStartDateTime(startDateTime);
        todo.setDisplayLabel(displayLabelValue);
        todo.setDescription(descriptionValue);

        QSignalSpy itemsAdded(m_engine, &QOrganizerManagerEngine::itemsAdded);
        QtOrganizer::QOrganizerManager::Error error;
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        QList<QOrganizerItem> items;
        items << todo;
        bool saveResult = m_engine->saveItems(&items,
                                              QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                              &errorMap,
                                              &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(errorMap.isEmpty());
        QTRY_COMPARE(itemsAdded.count(), 1);

        // query by the new item
        QOrganizerItemFetchHint hint;
        QList<QOrganizerItemId> ids;
        ids << items[0].id();
        items = m_engine->items(ids, hint, &errorMap, &error);
        QCOMPARE(items.count(), 1);

        // compare start datetime
        QOrganizerTodo newTodo = static_cast<QOrganizerTodo>(items[0]);
        QDateTime newStartDateTime = newTodo.startDateTime();
        QCOMPARE(newStartDateTime.timeSpec(), Qt::TimeZone);
        QCOMPARE(newStartDateTime.timeZone(), QTimeZone("Asia/Bangkok"));
        QCOMPARE(newTodo.startDateTime(), startDateTime);
    }

    void testEventWithTags()
    {
        static QString displayLabelValue = QStringLiteral("event with tag");
        static QString descriptionValue = QStringLiteral("event with tag descs");

        QOrganizerTodo todo;
        todo.setCollectionId(m_collection.id());
        todo.setStartDateTime(QDateTime::currentDateTime());
        todo.setDisplayLabel(displayLabelValue);
        todo.setDescription(descriptionValue);
        todo.setTags(QStringList() << "Tag0" << "Tag1" << "Tag2");

        QtOrganizer::QOrganizerManager::Error error;
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        QList<QOrganizerItem> items;
        QSignalSpy createdItem(m_engine, SIGNAL(itemsAdded(QList<QOrganizerItemId>)));
        items << todo;
        bool saveResult = m_engine->saveItems(&items,
                                              QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                              &errorMap,
                                              &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QOrganizerManager::NoError);
        QCOMPARE(items.size(), 1);
        QVERIFY(errorMap.isEmpty());
        QVERIFY(!items[0].id().isNull());

        QOrganizerTodo newTodo = static_cast<QOrganizerTodo>(items[0]);
        QStringList expectedTags;
        expectedTags << "Tag0" << "Tag1" << "Tag2";
        Q_FOREACH(QString tag, newTodo.tags()) {
            expectedTags.removeAll(tag);
        }
        QCOMPARE(expectedTags.size(), 0);

        // check saved item
        QTRY_COMPARE(createdItem.count(), 1);

        QOrganizerItemFetchHint hint;
        QList<QOrganizerItemId> ids;
        ids << items[0].id();
        items = m_engine->items(ids, hint, &errorMap, &error);
        QCOMPARE(items.count(), 1);

        newTodo = static_cast<QOrganizerTodo>(items[0]);
        expectedTags << "Tag0" << "Tag1" << "Tag2";
        Q_FOREACH(QString tag, newTodo.tags()) {
            expectedTags.removeAll(tag);
        }
        QCOMPARE(expectedTags.size(), 0);
    }

    void testFloatingTime()
    {
        QSKIP("Floating time is no longer supported");

        static QString displayLabelValue = QStringLiteral("event with floating time");
        static QString descriptionValue = QStringLiteral("event with floating time descs");

        QOrganizerTodo todo;
        todo.setCollectionId(m_collection.id());
        QDateTime startDate = QDateTime::currentDateTime();
        startDate = QDateTime(startDate.date(), startDate.time(), QTimeZone());
        todo.setStartDateTime(startDate);
        todo.setDisplayLabel(displayLabelValue);
        todo.setDescription(descriptionValue);

        QtOrganizer::QOrganizerManager::Error error;
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        QList<QOrganizerItem> items;
        QSignalSpy createdItem(m_engine, SIGNAL(itemsAdded(QList<QOrganizerItemId>)));
        items << todo;
        bool saveResult = m_engine->saveItems(&items,
                                              QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                              &errorMap,
                                              &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QOrganizerManager::NoError);
        QCOMPARE(items.size(), 1);
        QVERIFY(errorMap.isEmpty());
        QVERIFY(!items[0].id().isNull());
        // check saved item
        QTRY_COMPARE(createdItem.count(), 1);

        QOrganizerItemFetchHint hint;
        QList<QOrganizerItemId> ids;
        ids << items[0].id();
        items = m_engine->items(ids, hint, &errorMap, &error);
        QCOMPARE(items.count(), 1);

        QOrganizerTodo newTodo = static_cast<QOrganizerTodo>(items[0]);
        QCOMPARE(newTodo.startDateTime().timeSpec(), todo.startDateTime().timeSpec());
        QVERIFY(!newTodo.startDateTime().timeZone().isValid());
        QCOMPARE(newTodo.startDateTime().date(), startDate.date());
        QCOMPARE(newTodo.startDateTime().time().hour(), startDate.time().hour());
        QCOMPARE(newTodo.startDateTime().time().minute(), startDate.time().minute());

        // Update floating event
        QSignalSpy updateItem(m_engine, SIGNAL(itemsChanged(QList<QOrganizerItemId>)));
        startDate = QDateTime::currentDateTime();
        startDate.addSecs(360);
        startDate = QDateTime(startDate.date(), startDate.time(), QTimeZone());
        items << newTodo;
        saveResult = m_engine->saveItems(&items,
                                         QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                         &errorMap,
                                         &error);
        QTRY_COMPARE(updateItem.count(), 1);

        ids.clear();
        ids << items[0].id();
        items = m_engine->items(ids, hint, &errorMap, &error);
        QCOMPARE(items.count(), 1);

        newTodo = static_cast<QOrganizerTodo>(items[0]);
        QCOMPARE(newTodo.startDateTime().timeSpec(), todo.startDateTime().timeSpec());
        QVERIFY(!newTodo.startDateTime().timeZone().isValid());
        QCOMPARE(newTodo.startDateTime().date(), startDate.date());
        QCOMPARE(newTodo.startDateTime().time().hour(), startDate.time().hour());
        QCOMPARE(newTodo.startDateTime().time().minute(), startDate.time().minute());

        // Remove floating event
        QOrganizerItemRemoveByIdRequest req;
        connect(&req, SIGNAL(stateChanged(QOrganizerAbstractRequest::State)),
                this, SLOT(requestFinished(QOrganizerAbstractRequest::State)));
        connect(m_engine, SIGNAL(itemsRemoved(QList<QOrganizerItemId>)),
                this, SLOT(itemRemoved()));
        req.setItemId(newTodo.id());

        m_engine->startRequest(&req);
        m_engine->waitForRequestFinished(&req, -1);

        // check if the signal item removed was fired after the request finish
        QTRY_VERIFY(m_requestFinishedTime.isValid());
        QTRY_VERIFY(m_itemRemovedTime.isValid());
        QVERIFY(m_itemRemovedTime > m_requestFinishedTime);

        // check if item was removed
        QOrganizerItemSortOrder sort;
        QOrganizerItemIdFilter filter;
        ids.clear();
        ids << newTodo.id();
        filter.setIds(ids);
        items = m_engine->items(filter,
                      QDateTime(),
                      QDateTime(),
                      10,
                      sort,
                      hint,
                      &error);
        QCOMPARE(items.count(), 0);
    }

    void testCreateEventWithAttendees()
    {
        static QString displayLabelValue = QStringLiteral("event with collection attendee");
        static QString descriptionValue = QStringLiteral("event without collection");

        QOrganizerEvent event;
        event.setStartDateTime(QDateTime::currentDateTime());
        event.setDisplayLabel(displayLabelValue);
        event.setDescription(descriptionValue);


        QOrganizerEventAttendee attendee;
        attendee.setAttendeeId("Attendee ID");
        attendee.setEmailAddress("test@email.com");
        attendee.setName("Attendee Name");
        attendee.setParticipationRole(QOrganizerEventAttendee::RoleHost);
        attendee.setParticipationStatus(QOrganizerEventAttendee::StatusAccepted);
        event.saveDetail(&attendee);

        QtOrganizer::QOrganizerManager::Error error;
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        QList<QOrganizerItem> items;
        QSignalSpy createdItem(m_engine, SIGNAL(itemsAdded(QList<QOrganizerItemId>)));
        items << event;
        bool saveResult = m_engine->saveItems(&items,
                                              QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                              &errorMap,
                                              &error);
        QTRY_COMPARE(createdItem.count(), 1);
        QVERIFY(saveResult);
        QCOMPARE(error, QOrganizerManager::NoError);
        QCOMPARE(items.size(), 1);
        QVERIFY(errorMap.isEmpty());
        QVERIFY(!items[0].id().isNull());

        QList<QOrganizerItemId> ids;
        QOrganizerItemFetchHint hint;
        ids << items[0].id();
        QList<QOrganizerItem> newItems = m_engine->items(ids, hint, &errorMap, &error);
        QCOMPARE(newItems.size(), 1);

        QList<QOrganizerItemDetail> atts = newItems[0].details(QOrganizerItemDetail::TypeEventAttendee);
        QCOMPARE(atts.size(), 1);
        QOrganizerEventAttendee newAttendee = static_cast<QOrganizerEventAttendee>(atts[0]);
        QCOMPARE(newAttendee.attendeeId(), attendee.attendeeId());
        QCOMPARE(newAttendee.emailAddress(), attendee.emailAddress());
        QCOMPARE(newAttendee.name(), attendee.name());
        QCOMPARE(newAttendee.participationRole(), attendee.participationRole());
        QCOMPARE(newAttendee.participationStatus(), attendee.participationStatus());
    }

    // BUG: #1440878
    void testReminderOnTime()
    {
        static QString displayLabelValue = QStringLiteral("event reminder");
        static QString descriptionValue = QStringLiteral("event with reminder");

        QOrganizerEvent event;
        QOrganizerItemAudibleReminder aReminder;
        event.setStartDateTime(QDateTime::currentDateTime());
        event.setDisplayLabel(displayLabelValue);
        event.setDescription(descriptionValue);
        aReminder.setSecondsBeforeStart(0);
        aReminder.setDataUrl(QString());
        event.saveDetail(&aReminder);

        QOrganizerEvent event2;
        aReminder = QOrganizerItemAudibleReminder();
        event2.setStartDateTime(QDateTime::currentDateTime().addDays(2));
        event2.setDisplayLabel(displayLabelValue + "_2");
        event2.setDescription(descriptionValue);
        aReminder.setSecondsBeforeStart(60);
        aReminder.setDataUrl(QString());
        event2.saveDetail(&aReminder);

        QtOrganizer::QOrganizerManager::Error error;
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        QList<QOrganizerItem> items;
        QSignalSpy createdItem(m_engine, SIGNAL(itemsAdded(QList<QOrganizerItemId>)));
        items << event << event2;
        bool saveResult = m_engine->saveItems(&items,
                                              QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                              &errorMap,
                                              &error);
        QTRY_COMPARE(createdItem.count(), 1);
        QVERIFY(saveResult);

        QString vcard = getEventFromEvolution(items[0].id());
        QVERIFY(vcard.contains("TRIGGER;VALUE=DURATION;RELATED=START:PT0S"));

        vcard = getEventFromEvolution(items[1].id());
        QVERIFY(vcard.contains("TRIGGER;VALUE=DURATION;RELATED=START:-PT1M"));
    }

    // BUG: #1445577
    void testUTCEvent()
    {
        static QString displayLabelValue = QStringLiteral("UTC event");
        static QString descriptionValue = QStringLiteral("UTC event");
        const QDateTime startDate(QDateTime::currentDateTime().toUTC());

        QOrganizerEvent event;
        event.setStartDateTime(startDate);
        event.setDisplayLabel(displayLabelValue);
        event.setDescription(descriptionValue);

        QtOrganizer::QOrganizerManager::Error error;
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        QList<QOrganizerItem> items;
        QSignalSpy createdItem(m_engine, SIGNAL(itemsAdded(QList<QOrganizerItemId>)));
        items << event;
        bool saveResult = m_engine->saveItems(&items,
                                              QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                              &errorMap,
                                              &error);
        QTRY_COMPARE(createdItem.count(), 1);
        QVERIFY(saveResult);

        QList<QOrganizerItemId> ids;
        QOrganizerItemFetchHint hint;
        ids << items[0].id();
        QList<QOrganizerItem> newItems = m_engine->items(ids, hint, &errorMap, &error);
        QCOMPARE(newItems.size(), 1);

        QOrganizerEvent newEvent = static_cast<QOrganizerEvent>(newItems[0]);
        QCOMPARE(newEvent.startDateTime().timeZoneAbbreviation(), QStringLiteral("UTC"));
        QCOMPARE(newEvent.startDateTime().date(), startDate.date());
        QCOMPARE(newEvent.startDateTime().time().hour(), startDate.time().hour());
        QCOMPARE(newEvent.startDateTime().time().minute(), startDate.time().minute());
        QCOMPARE(newEvent.startDateTime().time().second(), startDate.time().second());
    }

    void testExtendedProperties()
    {
        static QString displayLabelValue = QStringLiteral("event with extended property");
        static QString descriptionValue = QStringLiteral("event with extended property");
        QOrganizerItemId itemId;
        QDateTime currentTime = QDateTime::currentDateTime();

        {
            // create a item with X-URL
            QOrganizerEvent event;
            event.setStartDateTime(currentTime);
            event.setEndDateTime(currentTime.addSecs(60 * 30));
            event.setDisplayLabel(displayLabelValue);
            event.setDescription(descriptionValue);

            QOrganizerItemExtendedDetail ex;
            ex.setName(QStringLiteral("X-URL"));
            ex.setData(QByteArray("http://canonical.com"));
            event.saveDetail(&ex);

            // save the new item
            QtOrganizer::QOrganizerManager::Error error;
            QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
            QList<QOrganizerItem> items;
            QSignalSpy createdItem(m_engine, SIGNAL(itemsAdded(QList<QOrganizerItemId>)));
            items << event;
            bool saveResult = m_engine->saveItems(&items,
                                                  QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                                  &errorMap,
                                                  &error);
            QTRY_COMPARE(createdItem.count(), 1);
            QVERIFY(saveResult);
            QCOMPARE(error, QOrganizerManager::NoError);
            QCOMPARE(items.size(), 1);
            QVERIFY(errorMap.isEmpty());
            QVERIFY(!items[0].id().isNull());
            QCOMPARE(items[0].details(QOrganizerItemDetail::TypeExtendedDetail).size(), 1);
            itemId = items[0].id();
        }

        // fetch for the item
        {
            QtOrganizer::QOrganizerManager::Error error;
            QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
            QList<QOrganizerItemId> ids;
            QOrganizerItemFetchHint hint;
            ids << itemId;
            QList<QOrganizerItem> items = m_engine->items(ids, hint, &errorMap, &error);
            QCOMPARE(items.size(), 1);

            QList<QOrganizerItemDetail> exs = items[0].details(QOrganizerItemDetail::TypeExtendedDetail);
            QCOMPARE(exs.size(), 1);
            QCOMPARE(exs[0].value(QOrganizerItemExtendedDetail::FieldName).toString(),
                    QStringLiteral("X-URL"));
            QCOMPARE(exs[0].value(QOrganizerItemExtendedDetail::FieldData).toByteArray(),
                    QByteArray("http://canonical.com"));
        }
    }

    void testFetchHint()
    {
        static QString displayLabelValue = QStringLiteral("event for fetch hint test");
        static QString descriptionValue = QStringLiteral("event for fetch hint test");
        QOrganizerItemId itemId;

        {
            QOrganizerEvent event;
            event.setStartDateTime(QDateTime::currentDateTime());
            event.setEndDateTime(QDateTime::currentDateTime().addSecs(60 * 30));
            event.setDisplayLabel(displayLabelValue);
            event.setDescription(descriptionValue);

            // TAGS
            event.setTags(QStringList() << "Tag0" << "Tag1" << "Tag2");

            // ExtendedDetails
            QOrganizerItemExtendedDetail ex;
            ex.setName(QStringLiteral("X-URL"));
            ex.setData(QByteArray("http://canonical.com"));
            event.saveDetail(&ex);

            // Reminders
            QOrganizerItemVisualReminder vReminder;
            vReminder.setDataUrl(QUrl("http://www.alarms.com"));
            vReminder.setMessage("Test visual reminder");
            event.saveDetail(&vReminder);

            QOrganizerItemAudibleReminder aReminder;
            aReminder.setSecondsBeforeStart(0);
            aReminder.setDataUrl(QUrl("http://www.audible.com"));
            event.saveDetail(&aReminder);

            // Attendee
            QOrganizerEventAttendee attendee;
            attendee.setAttendeeId("Attendee ID");
            attendee.setEmailAddress("test@email.com");
            attendee.setName("Attendee Name");
            attendee.setParticipationRole(QOrganizerEventAttendee::RoleHost);
            attendee.setParticipationStatus(QOrganizerEventAttendee::StatusAccepted);
            event.saveDetail(&attendee);

            // save the new item
            QtOrganizer::QOrganizerManager::Error error;
            QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
            QList<QOrganizerItem> items;
            QSignalSpy createdItem(m_engine, SIGNAL(itemsAdded(QList<QOrganizerItemId>)));
            items << event;
            bool saveResult = m_engine->saveItems(&items,
                                                  QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                                  &errorMap,
                                                  &error);
            QTRY_COMPARE(createdItem.count(), 1);
            QVERIFY(saveResult);
            itemId = items[0].id();
        }

        QtOrganizer::QOrganizerManager::Error error;
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        QList<QOrganizerItemId> ids;
        QOrganizerItemFetchHint hint;
        ids << itemId;

        // Fetch ExtendedDetails
        {
            QList<QOrganizerItemDetail::DetailType> details;
            details << QOrganizerItemDetail::TypeExtendedDetail;
            hint.setDetailTypesHint(details);
            QList<QOrganizerItem> items = m_engine->items(ids, hint, &errorMap, &error);
            QCOMPARE(items.size(), 1);
            QOrganizerEvent event = items[0];
            QList<QOrganizerItemDetail> xDetails = event.details(QOrganizerItemDetail::TypeExtendedDetail);
            QList<QOrganizerItemDetail> vreminders = event.details(QOrganizerItemDetail::TypeVisualReminder);
            QList<QOrganizerItemDetail> areminders = event.details(QOrganizerItemDetail::TypeAudibleReminder);
            QList<QOrganizerItemDetail> attendee = event.details(QOrganizerItemDetail::TypeEventAttendee);
            QStringList tags = event.tags();

            QCOMPARE(xDetails.size(), 1);
            QCOMPARE(vreminders.size(), 0);
            QCOMPARE(areminders.size(), 0);
            QCOMPARE(attendee.size(), 0);
            QCOMPARE(tags.size(), 0);
        }

        // Fetch TAGS, Attendee
        {
            QList<QOrganizerItemDetail::DetailType> details;
            details << QOrganizerItemDetail::TypeTag
                    << QOrganizerItemDetail::TypeEventAttendee;
            hint.setDetailTypesHint(details);
            QList<QOrganizerItem> items = m_engine->items(ids, hint, &errorMap, &error);
            QCOMPARE(items.size(), 1);
            QOrganizerEvent event = items[0];
            QList<QOrganizerItemDetail> xDetails = event.details(QOrganizerItemDetail::TypeExtendedDetail);
            QList<QOrganizerItemDetail> vreminders = event.details(QOrganizerItemDetail::TypeVisualReminder);
            QList<QOrganizerItemDetail> areminders = event.details(QOrganizerItemDetail::TypeAudibleReminder);
            QList<QOrganizerItemDetail> attendee = event.details(QOrganizerItemDetail::TypeEventAttendee);
            QStringList tags = event.tags();

            QCOMPARE(xDetails.size(), 0);
            QCOMPARE(vreminders.size(), 0);
            QCOMPARE(areminders.size(), 0);
            QCOMPARE(attendee.size(), 1);
            QCOMPARE(tags.size(), 3);
        }

        // Fetch TAGS, Reminders, Attendee
        {
            QList<QOrganizerItemDetail::DetailType> details;
            details << QOrganizerItemDetail::TypeTag
                    << QOrganizerItemDetail::TypeEventAttendee
                    << QOrganizerItemDetail::TypeReminder;
            hint.setDetailTypesHint(details);
            QList<QOrganizerItem> items = m_engine->items(ids, hint, &errorMap, &error);
            QCOMPARE(items.size(), 1);
            QOrganizerEvent event = items[0];
            QList<QOrganizerItemDetail> xDetails = event.details(QOrganizerItemDetail::TypeExtendedDetail);
            QList<QOrganizerItemDetail> vreminders = event.details(QOrganizerItemDetail::TypeVisualReminder);
            QList<QOrganizerItemDetail> areminders = event.details(QOrganizerItemDetail::TypeAudibleReminder);
            QList<QOrganizerItemDetail> attendee = event.details(QOrganizerItemDetail::TypeEventAttendee);
            QStringList tags = event.tags();

            QCOMPARE(xDetails.size(), 0);
            QCOMPARE(vreminders.size(), 1);
            QCOMPARE(areminders.size(), 1);
            QCOMPARE(attendee.size(), 1);
            QCOMPARE(tags.size(), 3);
        }

        // Fetch VisualReminders
        {
            QList<QOrganizerItemDetail::DetailType> details;
            details << QOrganizerItemDetail::TypeVisualReminder;
            hint.setDetailTypesHint(details);
            QList<QOrganizerItem> items = m_engine->items(ids, hint, &errorMap, &error);
            QCOMPARE(items.size(), 1);
            QOrganizerEvent event = items[0];
            QList<QOrganizerItemDetail> xDetails = event.details(QOrganizerItemDetail::TypeExtendedDetail);
            QList<QOrganizerItemDetail> vreminders = event.details(QOrganizerItemDetail::TypeVisualReminder);
            QList<QOrganizerItemDetail> areminders = event.details(QOrganizerItemDetail::TypeAudibleReminder);
            QList<QOrganizerItemDetail> attendee = event.details(QOrganizerItemDetail::TypeEventAttendee);
            QStringList tags = event.tags();

            QCOMPARE(xDetails.size(), 0);
            QCOMPARE(vreminders.size(), 1);
            QCOMPARE(areminders.size(), 0);
            QCOMPARE(attendee.size(), 0);
            QCOMPARE(tags.size(), 0);
        }

        // Fetch AudibleReminders
        {
            QList<QOrganizerItemDetail::DetailType> details;
            details << QOrganizerItemDetail::TypeAudibleReminder;
            hint.setDetailTypesHint(details);
            QList<QOrganizerItem> items = m_engine->items(ids, hint, &errorMap, &error);
            QCOMPARE(items.size(), 1);
            QOrganizerEvent event = items[0];
            QList<QOrganizerItemDetail> xDetails = event.details(QOrganizerItemDetail::TypeExtendedDetail);
            QList<QOrganizerItemDetail> vreminders = event.details(QOrganizerItemDetail::TypeVisualReminder);
            QList<QOrganizerItemDetail> areminders = event.details(QOrganizerItemDetail::TypeAudibleReminder);
            QList<QOrganizerItemDetail> attendee = event.details(QOrganizerItemDetail::TypeEventAttendee);
            QStringList tags = event.tags();

            QCOMPARE(xDetails.size(), 0);
            QCOMPARE(vreminders.size(), 0);
            QCOMPARE(areminders.size(), 1);
            QCOMPARE(attendee.size(), 0);
            QCOMPARE(tags.size(), 0);
        }
   }
};

const QString EventTest::collectionTypePropertyName = QStringLiteral("collection-type");
const QString EventTest::taskListTypeName = QStringLiteral("Task List");
int EventTest::signalIndex = 0;

QTEST_MAIN(EventTest)

#include "event-test.moc"
