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
#include "qorganizer-eds-source-registry.h"
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

    bool containsCollection(const QList<QOrganizerCollection> &lst, const QOrganizerCollection &collection  )
    {
        bool found = false;
        Q_FOREACH(const QOrganizerCollection &col, lst) {
            if (col.id() == collection.id()) {
                found = true;
            }
        }
        return found;
    }

private Q_SLOTS:
    void initTestCase()
    {
        EDSBaseTest::init();
        m_engineWrite = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());
        m_engineRead = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());
    }

    void cleanupTestCase()
    {
        delete m_engineRead;
        delete m_engineWrite;
        m_engineRead = 0;
        m_engineWrite = 0;
        EDSBaseTest::cleanup();
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

        QSignalSpy createCollection(m_engineRead, SIGNAL(collectionsAdded(QList<QOrganizerCollectionId>)));
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
        QSignalSpy createdItem(m_engineRead, SIGNAL(itemsAdded(QList<QOrganizerItemId>)));
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
        QCOMPARE(createdItem.takeFirst().count(), 1);

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

    void testCreateCollection()
    {
        static const QString collectionName = uniqueCollectionName();

        QOrganizerCollection collection;
        QtOrganizer::QOrganizerManager::Error error;

        collection.setMetaData(QOrganizerCollection::KeyName, collectionName);
        collection.setMetaData(QOrganizerCollection::KeyColor, QStringLiteral("red"));

        QList<QOrganizerCollection> collections = m_engineRead->collections(&error);
        int initalCollectionCount = collections.count();

        QSignalSpy createCollection(m_engineRead, SIGNAL(collectionsAdded(QList<QOrganizerCollectionId>)));
        QVERIFY(m_engineWrite->saveCollection(&collection, &error));
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(!collection.id().isNull());
        QTRY_COMPARE(createCollection.count(), 1);

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

        QSignalSpy collectionCreated(m_engineRead, SIGNAL(collectionsAdded(QList<QOrganizerCollectionId>)));
        QVERIFY(m_engineWrite->saveCollection(&collection, &error));
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(!collection.id().isNull());
        QTRY_COMPARE(collectionCreated.count(), 1);

        // wait for the collection to became writable
        QTRY_COMPARE_WITH_TIMEOUT(collection.extendedMetaData(QStringLiteral(COLLECTION_READONLY_METADATA)).toBool(), true, 10000);

        // Check if the collection was stored correct
        QOrganizerCollection newCollection = m_engineRead->collection(collection.id(), &error);
        QCOMPARE(newCollection.metaData(QOrganizerCollection::KeyName).toString(), collectionName);
        QCOMPARE(newCollection.metaData(QOrganizerCollection::KeyColor).toString(), QStringLiteral("red"));
        QCOMPARE(newCollection.extendedMetaData(QStringLiteral("collection-selected")).toBool(), false);

        // wait collection to became writable
        QTRY_VERIFY_WITH_TIMEOUT(!m_engineRead->collection(newCollection.id(), 0).extendedMetaData("collection-readonly").toBool(), 5000);

        // update the collection
        QSignalSpy updateCollection(m_engineRead, SIGNAL(collectionsChanged(QList<QOrganizerCollectionId>)));
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

    void testCreateTaskList()
    {
        static const QString collectionName = uniqueCollectionName() + QStringLiteral("_TASKS") ;

        QOrganizerCollection collection;
        QtOrganizer::QOrganizerManager::Error error;
        collection.setMetaData(QOrganizerCollection::KeyName, collectionName);
        collection.setExtendedMetaData(collectionTypePropertyName, taskListTypeName);

        QSignalSpy createdCollection(m_engineRead, SIGNAL(collectionsAdded(QList<QOrganizerCollectionId>)));
        QVERIFY(m_engineWrite->saveCollection(&collection, &error));
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(!collection.id().isNull());

        //verify signal
        QTRY_COMPARE(createdCollection.count(), 1);
        QList<QVariant> args = createdCollection.takeFirst();
        QCOMPARE(args.count(), 1);

        QVERIFY(containsCollection(m_engineWrite->collections(&error), collection));
        QVERIFY(containsCollection(m_engineRead->collections(&error), collection));
    }

    void testRemoveCollection()
    {
        static QString removableCollectionName = uniqueCollectionName();

        // Create a collection
        QOrganizerCollection collection;
        QtOrganizer::QOrganizerManager::Error error;
        collection.setMetaData(QOrganizerCollection::KeyName, removableCollectionName);

        QList<QOrganizerCollection> collections = m_engineRead->collections(&error);
        int initalCollectionCount = collections.count();

        QSignalSpy createCollection(m_engineRead, SIGNAL(collectionsAdded(QList<QOrganizerCollectionId>)));
        QVERIFY(m_engineWrite->saveCollection(&collection, &error));
        QTRY_COMPARE(createCollection.count(), 1);

        // wait collection to became writable
        QTRY_VERIFY_WITH_TIMEOUT(!m_engineRead->collection(collection.id(), 0).extendedMetaData("collection-readonly").toBool(), 5000);

        // remove recent created collection
        QSignalSpy removeCollection(m_engineRead, SIGNAL(collectionsRemoved(QList<QOrganizerCollectionId>)));
        QVERIFY(m_engineWrite->removeCollection(collection.id(), &error));
        QTRY_COMPARE(removeCollection.count(), 1);

        collections = m_engineWrite->collections(&error);
        QCOMPARE(collections.count(), initalCollectionCount);
        QVERIFY(!containsCollection(collections, collection));

        collections = m_engineRead->collections(&error);
        QCOMPARE(collections.count(), initalCollectionCount);
        QVERIFY(!containsCollection(collections, collection));
    }

    void testReadOnlyCollection()
    {
        // check if the anniversaries collection is read-only
        static const QString anniversariesCollectionName = QStringLiteral("Birthdays & Anniversaries");
        QtOrganizer::QOrganizerManager::Error error;
        QList<QOrganizerCollection> collections = m_engineRead->collections(&error);
        Q_FOREACH(const QOrganizerCollection &col, collections) {
            if (col.metaData(QOrganizerCollection::KeyName) == anniversariesCollectionName) {
                QVERIFY(col.extendedMetaData("collection-readonly").toBool());
            }
        }
    }

    void testCreateNewDefaultCollection()
    {
        static QString newCollection = uniqueCollectionName();

        // Create a new default collection
        QOrganizerCollection collection;
        QtOrganizer::QOrganizerManager::Error error;
        collection.setMetaData(QOrganizerCollection::KeyName, newCollection);
        collection.setMetaData(QOrganizerCollection::KeyColor, QStringLiteral("red"));
        collection.setExtendedMetaData(QStringLiteral("collection-selected"), true);
        collection.setExtendedMetaData(QStringLiteral("collection-default"), true);

        QSignalSpy createdCollection(m_engineRead, SIGNAL(collectionsAdded(QList<QOrganizerCollectionId>)));
        QVERIFY(m_engineWrite->saveCollection(&collection, &error));
        // create collection
        QTRY_COMPARE(createdCollection.count(), 1);

        // wait collection to became the default one
        QTRY_COMPARE_WITH_TIMEOUT(m_engineRead->defaultCollection(0).id(), collection.id(), 5000);
    }

    void testUpdateDefaultCollection()
    {
        static QString newCollectionId = uniqueCollectionName();

        // store current default collection
        QOrganizerCollection defaultCollection = m_engineRead->defaultCollection(0);

        // Create a collection
        QOrganizerCollection collection;
        QtOrganizer::QOrganizerManager::Error error;
        collection.setMetaData(QOrganizerCollection::KeyName, newCollectionId);
        QSignalSpy createCollection(m_engineRead, SIGNAL(collectionsAdded(QList<QOrganizerCollectionId>)));
        QVERIFY(m_engineWrite->saveCollection(&collection, &error));
        QTRY_COMPARE(createCollection.count(), 1);
        // wait collection to became writable
        QTRY_VERIFY_WITH_TIMEOUT(!m_engineRead->collection(collection.id(), 0).extendedMetaData("collection-readonly").toBool(), 5000);

        // make sure that the new collection is not default
        QOrganizerCollection newCollection = m_engineRead->collection(collection.id(), 0);
        QCOMPARE(newCollection.extendedMetaData(QStringLiteral("collection-default")).toBool(), false);
        QVERIFY(newCollection.id() != defaultCollection.id());

        // mark new collection as default
        QSignalSpy changedCollection(m_engineRead, SIGNAL(collectionsChanged(QList<QOrganizerCollectionId>)));
        newCollection.setExtendedMetaData(QStringLiteral("collection-default"), true);
        QVERIFY(m_engineWrite->saveCollection(&newCollection, &error));
        // old default collection will change, and the new one
        QTRY_COMPARE(changedCollection.count() , 3);

        // wait collection to became the default one
        QTRY_COMPARE_WITH_TIMEOUT(m_engineRead->defaultCollection(0).id(), newCollection.id(), 5000);
    }
};

const QString CollectionTest::collectionTypePropertyName = QStringLiteral("collection-type");
const QString CollectionTest::taskListTypeName = QStringLiteral("Task List");


QTEST_MAIN(CollectionTest)

#include "collections-test.moc"
