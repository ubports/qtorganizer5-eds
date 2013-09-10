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


using namespace QtOrganizer;

class EventTest : public QObject
{
    Q_OBJECT
private:
    static const QString defaultCollectionName;
    static const QString defaultTaskCollectionName;
    static const QString collectionTypePropertyName;
    static const QString taskListTypeName;

private Q_SLOTS:
    void testCreateEventWithReminder()
    {
        static QString displayLabelValue = QStringLiteral("Todo test");
        static QString descriptionValue = QStringLiteral("Todo description");

        QOrganizerEDSEngine *engine = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());

        QOrganizerCollection collection;
        QtOrganizer::QOrganizerManager::Error error;
        collection.setMetaData(QOrganizerCollection::KeyName, defaultCollectionName);
        collection.setExtendedMetaData(collectionTypePropertyName, taskListTypeName);

        engine->saveCollection(&collection, &error);

        QOrganizerTodo todo;
        todo.setCollectionId(collection.id());
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

        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        QList<QOrganizerItem> items;
        items << todo;
        bool saveResult = engine->saveItems(&items,
                                            QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                            &errorMap,
                                            &error);
        QVERIFY(saveResult);

        // delete engine to make sure the new engine will be loaded from scratch
        delete engine;

        engine = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());

        QOrganizerItemSortOrder sort;
        QOrganizerItemFetchHint hint;
        QOrganizerItemIdFilter filter;

        QList<QOrganizerItemId> ids;
        ids << items[0].id();
        filter.setIds(ids);

        items = engine->items(filter,
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
};

const QString EventTest::defaultCollectionName = QStringLiteral("TEST_EVENT_COLLECTION");
const QString EventTest::defaultTaskCollectionName = QStringLiteral("TEST_EVENT_COLLECTION TASK LIST");
const QString EventTest::collectionTypePropertyName = QStringLiteral("collection-type");
const QString EventTest::taskListTypeName = QStringLiteral("Task List");


QTEST_MAIN(EventTest)

#include "event-test.moc"
