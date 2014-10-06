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
    static const QString collectionTypePropertyName;
    static const QString taskListTypeName;

    QOrganizerEDSEngine *m_engineWrite;
    QOrganizerEDSEngine *m_engineRead;

private Q_SLOTS:
    void init()
    {
        EDSBaseTest::init();
        m_engineWrite = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());
        m_engineRead = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());
    }

    void cleanup()
    {
        delete m_engineRead;
        delete m_engineWrite;
        m_engineRead = 0;
        m_engineWrite = 0;
        EDSBaseTest::cleanup();
    }

    void testCreateCollection()
    {
        static const QString collectionName = uniqueCollectionName();

        QOrganizerCollection collection;
        QtOrganizer::QOrganizerManager::Error error;

        collection.setMetaData(QOrganizerCollection::KeyName, collectionName);
        collection.setMetaData(QOrganizerCollection::KeyColor, QStringLiteral("red"));

        QList<QOrganizerCollection> collections = m_engineRead->collections(&error);
        int initalCollectionCount = collections.count();

        QVERIFY(m_engineWrite->saveCollection(&collection, &error));
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(!collection.id().isNull());

        collections = m_engineWrite->collections(&error);
        QCOMPARE(collections.count(), initalCollectionCount + 1);

        collections = m_engineRead->collections(&error);
        QCOMPARE(collections.count(), initalCollectionCount + 1);

        // Check if data was correct saved
        QOrganizerCollection newCollection = m_engineRead->collection(collection.id(), &error);
        QCOMPARE(error, QOrganizerManager::NoError);
        QCOMPARE(newCollection.metaData(QOrganizerCollection::KeyName).toString(), collectionName);
        QCOMPARE(newCollection.metaData(QOrganizerCollection::KeyColor).toString(), QStringLiteral("red"));
        QCOMPARE(newCollection.extendedMetaData("collection-type").toString(), QStringLiteral("Calendar"));
        QCOMPARE(newCollection.extendedMetaData("collection-selected").toBool(), false);
    }

    void testUpdateCollection()
    {
        static const QString collectionName = uniqueCollectionName();

        QOrganizerCollection collection;
        QtOrganizer::QOrganizerManager::Error error;

        collection.setMetaData(QOrganizerCollection::KeyName, collectionName);
        collection.setMetaData(QOrganizerCollection::KeyColor, QStringLiteral("red"));
        collection.setExtendedMetaData(QStringLiteral("collection-selected"), false);

        QVERIFY(m_engineWrite->saveCollection(&collection, &error));
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(!collection.id().isNull());

        // Check if the collection was stored correct
        QOrganizerCollection newCollection = m_engineRead->collection(collection.id(), &error);
        QCOMPARE(newCollection.metaData(QOrganizerCollection::KeyName).toString(), collectionName);
        QCOMPARE(newCollection.metaData(QOrganizerCollection::KeyColor).toString(), QStringLiteral("red"));
        QCOMPARE(newCollection.extendedMetaData(QStringLiteral("collection-selected")).toBool(), false);


        // update the collection
        QSignalSpy updateCollection(m_engineWrite, SIGNAL(collectionsChanged(QList<QOrganizerCollectionId>)));
        collection.setMetaData(QOrganizerCollection::KeyColor, "blue");
        collection.setExtendedMetaData("collection-selected", true);
        QVERIFY(m_engineWrite->saveCollection(&collection, &error));
        QCOMPARE(error, QOrganizerManager::NoError);

        QTRY_VERIFY(updateCollection.count() > 0);
        QList<QVariant> args = updateCollection.takeFirst();
        QCOMPARE(args.count(), 1);
        QCOMPARE(args[0].value<QList<QOrganizerCollectionId> >().at(0).toString(), collection.id().toString());


        // Check if the collection was updated correct
        newCollection = m_engineRead->collection(collection.id(), &error);
        QCOMPARE(error, QOrganizerManager::NoError);
        QCOMPARE(newCollection.metaData(QOrganizerCollection::KeyName).toString(), collectionName);
        QCOMPARE(newCollection.metaData(QOrganizerCollection::KeyColor).toString(), QStringLiteral("blue"));
        QCOMPARE(newCollection.extendedMetaData("collection-selected").toBool(), true);
    }

    void testRemoveCollection()
    {
        static QString removableCollectionName = uniqueCollectionName() + QStringLiteral("_REMOVABLE");

        // Create a collection
        QOrganizerCollection collection;
        QtOrganizer::QOrganizerManager::Error error;
        collection.setMetaData(QOrganizerCollection::KeyName, removableCollectionName);

        QList<QOrganizerCollection> collections = m_engineRead->collections(&error);
        int initalCollectionCount = collections.count();

        QVERIFY(m_engineWrite->saveCollection(&collection, &error));

        // remove recent created collection
        QVERIFY(m_engineWrite->removeCollection(collection.id(), &error));

        collections = m_engineWrite->collections(&error);
        QCOMPARE(collections.count(), initalCollectionCount);
        QVERIFY(!collections.contains(collection));

        collections = m_engineRead->collections(&error);
        QCOMPARE(collections.count(), initalCollectionCount);
        QVERIFY(!collections.contains(collection));
    }

   void testCreateTaskList()
    {
        static const QString collectionName = uniqueCollectionName() + QStringLiteral("_TASKS") ;

        QOrganizerCollection collection;
        QtOrganizer::QOrganizerManager::Error error;
        collection.setMetaData(QOrganizerCollection::KeyName, collectionName);
        collection.setExtendedMetaData(collectionTypePropertyName, taskListTypeName);

        QSignalSpy createdCollection(m_engineWrite, SIGNAL(collectionsAdded(QList<QOrganizerCollectionId>)));
        QVERIFY(m_engineWrite->saveCollection(&collection, &error));
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(!collection.id().isNull());

        //verify signal
        QTRY_COMPARE(createdCollection.count(), 1);
        QList<QVariant> args = createdCollection.takeFirst();
        QCOMPARE(args.count(), 1);

        QVERIFY(m_engineWrite->collections(&error).contains(collection));
        QVERIFY(m_engineRead->collections(&error).contains(collection));
    }

    void testCreateTask()
    {
        static const QString collectionName = uniqueCollectionName();
        static QString displayLabelValue = QStringLiteral("Todo test");
        static QString descriptionValue = QStringLiteral("Todo description");

        QOrganizerCollection collection;
        QtOrganizer::QOrganizerManager::Error error;
        collection.setMetaData(QOrganizerCollection::KeyName, collectionName);
        collection.setExtendedMetaData(collectionTypePropertyName, taskListTypeName);

        QSignalSpy createCollection(m_engineWrite, SIGNAL(collectionsAdded(QList<QOrganizerCollectionId>)));
        QVERIFY(m_engineWrite->saveCollection(&collection, &error));
        QVERIFY(!collection.id().isNull());
        QTRY_COMPARE(createCollection.count(), 1);

        QOrganizerTodo todo;
        todo.setCollectionId(collection.id());
        todo.setStartDateTime(QDateTime::currentDateTime());
        todo.setDisplayLabel(displayLabelValue);
        todo.setDescription(descriptionValue);

        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        QList<QOrganizerItem> items;
        QSignalSpy createdItem(m_engineWrite, SIGNAL(itemsAdded(QList<QOrganizerItemId>)));
        items << todo;
        bool saveResult = m_engineWrite->saveItems(&items,
                                            QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                            &errorMap,
                                            &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(errorMap.isEmpty());
        QVERIFY(!items[0].id().isNull());

        //verify signal
        QTRY_COMPARE(createdItem.count(), 1);
        QList<QVariant> args = createdItem.takeFirst();
        QCOMPARE(args.count(), 1);

        // check if the item is listead inside the correct collection
        QOrganizerItemSortOrder sort;
        QOrganizerItemFetchHint hint;
        QOrganizerItemCollectionFilter filter;

        filter.setCollectionId(collection.id());

        items = m_engineRead->items(filter,
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

        items = m_engineRead->items(ids, hint, &errorMap, &error);
        QCOMPARE(items.count(), 1);
        result = static_cast<QOrganizerTodo>(items[0]);
        todo = items[0];
        QCOMPARE(result.id(), todo.id());
        QCOMPARE(result.startDateTime(), todo.startDateTime());
        QCOMPARE(result.displayLabel(), todo.displayLabel());
        QCOMPARE(result.description(), todo.description());
    }
};

const QString CollectionTest::collectionTypePropertyName = QStringLiteral("collection-type");
const QString CollectionTest::taskListTypeName = QStringLiteral("Task List");


QTEST_MAIN(CollectionTest)

#include "collections-test.moc"
