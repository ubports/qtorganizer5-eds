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
    static const QString defaultCollectionName;
    static const QString defaultTaskCollectionName;
    static const QString collectionTypePropertyName;
    static const QString taskListTypeName;

    QDateTime m_itemRemovedTime;
    QDateTime m_requestFinishedTime;
    QOrganizerEDSEngine *m_engine;
    QOrganizerCollection m_collection;

private Q_SLOTS:
    void init()
    {
        clear();

        m_engine = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());

        QtOrganizer::QOrganizerManager::Error error;
        m_collection.setMetaData(QOrganizerCollection::KeyName, defaultCollectionName);
        m_collection.setExtendedMetaData(collectionTypePropertyName, taskListTypeName);

        bool saveResult = m_engine->saveCollection(&m_collection, &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QtOrganizer::QOrganizerManager::NoError);
    }

    void cleanup()
    {
        delete m_engine;
        m_collection = QOrganizerCollection();
    }

    //helper
    void itemRemoved()
    {
        m_itemRemovedTime = QDateTime::currentDateTime();
    }

    void requestFinished(QOrganizerAbstractRequest::State state)
    {
        if (state == QOrganizerAbstractRequest::FinishedState) {
            m_requestFinishedTime  = QDateTime::currentDateTime();
        }
    }

    // test functions
    void testCreateEventWithReminder()
    {
        static QString displayLabelValue = QStringLiteral("Todo test");
        static QString descriptionValue = QStringLiteral("Todo description");

        QOrganizerTodo todo;
        todo.setCollectionId(m_collection.id());
        todo.setStartDateTime(QDateTime(QDate(2013, 9, 3), QTime(0,30,0)));
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

    void testRemoveEvent()
    {
        static QString displayLabelValue = QStringLiteral("event to be removed");
        static QString descriptionValue = QStringLiteral("removable event");

        QOrganizerTodo todo;
        todo.setCollectionId(m_collection.id());
        todo.setStartDateTime(QDateTime(QDate(2013, 9, 3), QTime(0,30,0)));
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
        QVERIFY(errorMap.isEmpty());
        QVERIFY(!items[0].id().isNull());

        m_itemRemovedTime = QDateTime();
        m_requestFinishedTime = QDateTime();

        QOrganizerItemRemoveRequest req;
        connect(&req, SIGNAL(stateChanged(QOrganizerAbstractRequest::State)),
                this, SLOT(requestFinished(QOrganizerAbstractRequest::State)));
        connect(m_engine, SIGNAL(itemsRemoved(QList<QOrganizerItemId>)),
                this, SLOT(itemRemoved()));
        req.setItem(items[0]);
        m_engine->startRequest(&req);
        m_engine->waitForRequestFinished(&req, -1);

        // check if the signal item removed was fired after the request finish
        QVERIFY(!m_itemRemovedTime.isValid());
        QVERIFY(m_requestFinishedTime.isValid());
        QVERIFY(m_requestFinishedTime > m_itemRemovedTime);
    }
};

const QString EventTest::defaultCollectionName = QStringLiteral("TEST_EVENT_COLLECTION");
const QString EventTest::defaultTaskCollectionName = QStringLiteral("TEST_EVENT_COLLECTION TASK LIST");
const QString EventTest::collectionTypePropertyName = QStringLiteral("collection-type");
const QString EventTest::taskListTypeName = QStringLiteral("Task List");


QTEST_MAIN(EventTest)

#include "event-test.moc"
