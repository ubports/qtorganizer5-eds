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

class CollectionTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testCreateCollection()
    {
        QOrganizerEDSEngine *engine = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());

        QOrganizerCollection collection;
        QtOrganizer::QOrganizerManager::Error error;
        collection.setMetaData(QOrganizerCollection::KeyName, "TEST COLLECTION");

        QVERIFY(engine->saveCollection(&collection, &error));
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(!collection.id().isNull());

    }

    void testCreateTaskList()
    {
        QOrganizerEDSEngine *engine = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());

        QOrganizerCollection collection;
        QtOrganizer::QOrganizerManager::Error error;
        collection.setMetaData(QOrganizerCollection::KeyName, "TEST COLLECTION TASK LIST");
        collection.setExtendedMetaData("collection-type", "Task List");

        QSignalSpy createdCollection(engine, SIGNAL(collectionsAdded(QList<QOrganizerCollectionId>)));
        QVERIFY(engine->saveCollection(&collection, &error));
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(!collection.id().isNull());

        //verify signal
        QCOMPARE(createdCollection.count(), 1);
        QList<QVariant> args = createdCollection.takeFirst();
        QCOMPARE(args.count(), 1);

        QVERIFY(engine->collections(&error).contains(collection));
        delete engine;

        // recreate and check if the new collection is listed
        engine = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());
        QVERIFY(engine->collections(&error).contains(collection));
    }

    void testCreateTask()
    {
        QOrganizerEDSEngine *engine = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());

        QOrganizerCollection collection;
        QtOrganizer::QOrganizerManager::Error error;
        collection.setMetaData(QOrganizerCollection::KeyName, "TEST COLLECTION TASK LIST 2");
        collection.setExtendedMetaData("collection-type", "Task List");

        QOrganizerTodo todo;
        todo.setCollectionId(collection.id());
        todo.setStartDateTime(QDateTime(QDate(2013, 9, 3), QTime(0,30,0)));
        todo.setDisplayLabel("Todo test ");
        todo.setDescription("Todo description");

        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        QList<QOrganizerItem> items;
        QSignalSpy createdItem(engine, SIGNAL(itemsAdded(QList<QOrganizerItemId>)));
        items << todo;
        bool saveResult = engine->saveItems(&items,
                                            QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                            &errorMap,
                                            &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(errorMap.isEmpty());
        QVERIFY(!items[0].id().isNull());

        //verify signal
        QCOMPARE(createdItem.count(), 1);
        QList<QVariant> args = createdItem.takeFirst();
        QCOMPARE(args.count(), 1);

        delete engine;
    }
};

QTEST_MAIN(CollectionTest)

#include "collections-test.moc"
