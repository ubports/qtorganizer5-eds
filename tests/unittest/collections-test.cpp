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

class CollectionTest : public QObject, public EDSBaseTest
{
    Q_OBJECT
private:
    static const QString defaultCollectionName;
    static const QString defaultTaskCollectionName;
    static const QString collectionTypePropertyName;
    static const QString taskListTypeName;
    QOrganizerEDSEngine *m_engine;

private Q_SLOTS:
    void init()
    {
        clear();
        m_engine = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());
    }

    void cleanup()
    {
        delete m_engine;
        m_engine = 0;
    }

    void testCreateCollection()
    {
        QOrganizerCollection collection;
        QtOrganizer::QOrganizerManager::Error error;
        collection.setMetaData(QOrganizerCollection::KeyName, defaultCollectionName);

        QVERIFY(m_engine->saveCollection(&collection, &error));
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(!collection.id().isNull());
    }

    void testCreateTaskList()
    {
        m_engine = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());

        QOrganizerCollection collection;
        QtOrganizer::QOrganizerManager::Error error;
        collection.setMetaData(QOrganizerCollection::KeyName, defaultTaskCollectionName);
        collection.setExtendedMetaData(collectionTypePropertyName, taskListTypeName);

        QSignalSpy createdCollection(m_engine, SIGNAL(collectionsAdded(QList<QOrganizerCollectionId>)));
        QVERIFY(m_engine->saveCollection(&collection, &error));
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(!collection.id().isNull());

        //verify signal
        QCOMPARE(createdCollection.count(), 1);
        QList<QVariant> args = createdCollection.takeFirst();
        QCOMPARE(args.count(), 1);

        QVERIFY(m_engine->collections(&error).contains(collection));
        delete m_engine;
        QTest::qSleep(1000);

        // recreate and check if the new collection is listed
        m_engine = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());
        QVERIFY(m_engine->collections(&error).contains(collection));
    }

    void testCreateTask()
    {
        static QString displayLabelValue = QStringLiteral("Todo test");
        static QString descriptionValue = QStringLiteral("Todo description");

        m_engine = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());

        QOrganizerCollection collection;
        QtOrganizer::QOrganizerManager::Error error;
        collection.setMetaData(QOrganizerCollection::KeyName, defaultTaskCollectionName + "2");
        collection.setExtendedMetaData(collectionTypePropertyName, taskListTypeName);

        QVERIFY(m_engine->saveCollection(&collection, &error));

        QOrganizerTodo todo;
        todo.setCollectionId(collection.id());
        todo.setStartDateTime(QDateTime(QDate(2013, 9, 3), QTime(0,30,0)));
        todo.setDisplayLabel(displayLabelValue);
        todo.setDescription(descriptionValue);

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
        QVERIFY(errorMap.isEmpty());
        QVERIFY(!items[0].id().isNull());

        //verify signal
        QCOMPARE(createdItem.count(), 1);
        QList<QVariant> args = createdItem.takeFirst();
        QCOMPARE(args.count(), 1);

        // check if the item is listead inside the correct collection
        QOrganizerItemSortOrder sort;
        QOrganizerItemFetchHint hint;
        QOrganizerItemCollectionFilter filter;

        filter.setCollectionId(collection.id());

        items = m_engine->items(filter,
                      QDateTime(),
                      QDateTime(),
                      10,
                      sort,
                      hint,
                      &error);

        QCOMPARE(items.count(), 1);
        QOrganizerTodo result = static_cast<QOrganizerTodo>(items[0]);
        todo = items[0];
        QCOMPARE(result.id(), todo.id());
        QCOMPARE(result.startDateTime(), todo.startDateTime());
        QCOMPARE(result.displayLabel(), todo.displayLabel());
        QCOMPARE(result.description(), todo.description());

        // check if the item is listead by id
        QList<QOrganizerItemId> ids;
        ids << todo.id();

        items = m_engine->items(ids, hint, &errorMap, &error);
        QCOMPARE(items.count(), 1);
        result = static_cast<QOrganizerTodo>(items[0]);
        todo = items[0];
        QCOMPARE(result.id(), todo.id());
        QCOMPARE(result.startDateTime(), todo.startDateTime());
        QCOMPARE(result.displayLabel(), todo.displayLabel());
        QCOMPARE(result.description(), todo.description());
    }

    void testRemoveCollection()
    {
        static QString removableCollectionName = defaultTaskCollectionName + QStringLiteral("_REMOVABLE");

        QOrganizerEDSEngine *engine = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());

        // Create a collection
        QOrganizerCollection collection;
        QtOrganizer::QOrganizerManager::Error error;
        collection.setMetaData(QOrganizerCollection::KeyName, removableCollectionName);
        QVERIFY(engine->saveCollection(&collection, &error));

        delete engine;
        QTest::qSleep(1000);

        engine = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());
        // remove recent created collection
        QVERIFY(engine->removeCollection(collection.id(), &error));

        // Check if collection is not listed anymore
        QTest::qSleep(1000);
        QList<QOrganizerCollection> collections = engine->collections(&error);
        QVERIFY(!collections.contains(collection));
    }
};

const QString CollectionTest::defaultCollectionName = QStringLiteral("TEST COLLECTION");
const QString CollectionTest::defaultTaskCollectionName = QStringLiteral("TEST COLLECTION TASK LIST");
const QString CollectionTest::collectionTypePropertyName = QStringLiteral("collection-type");
const QString CollectionTest::taskListTypeName = QStringLiteral("Task List");


QTEST_MAIN(CollectionTest)

#include "collections-test.moc"
