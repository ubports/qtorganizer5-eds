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

    QOrganizerEDSEngine *m_engineWrite;
    QOrganizerEDSEngine *m_engineRead;

private Q_SLOTS:
    void init()
    {
        EDSBaseTest::init(0);
        m_engineWrite = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());
        m_engineRead = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());
    }

    void cleanup()
    {
        delete m_engineRead;
        m_engineRead = 0;
        EDSBaseTest::cleanup(m_engineWrite);
        m_engineWrite = 0;
    }

    void testCreateCollection()
    {
        QOrganizerCollection collection;
        QtOrganizer::QOrganizerManager::Error error;
        collection.setMetaData(QOrganizerCollection::KeyName, defaultCollectionName);

        QList<QOrganizerCollection> collections = m_engineRead->collections(&error);
        int initalCollectionCount = collections.count();

        QVERIFY(m_engineWrite->saveCollection(&collection, &error));
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(!collection.id().isNull());

        collections = m_engineWrite->collections(&error);
        QCOMPARE(collections.count(), initalCollectionCount + 1);

        // wait some time for the changes to propagate
        collections = m_engineRead->collections(&error);
        QCOMPARE(collections.count(), initalCollectionCount + 1);
    }

    void testRemoveCollection()
    {
        static QString removableCollectionName = defaultTaskCollectionName + QStringLiteral("_REMOVABLE");

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
        QOrganizerCollection collection;
        QtOrganizer::QOrganizerManager::Error error;
        collection.setMetaData(QOrganizerCollection::KeyName, defaultTaskCollectionName);
        collection.setExtendedMetaData(collectionTypePropertyName, taskListTypeName);

        QSignalSpy createdCollection(m_engineWrite, SIGNAL(collectionsAdded(QList<QOrganizerCollectionId>)));
        QVERIFY(m_engineWrite->saveCollection(&collection, &error));
        QCOMPARE(error, QOrganizerManager::NoError);
        QVERIFY(!collection.id().isNull());

        //verify signal
        QCOMPARE(createdCollection.count(), 1);
        QList<QVariant> args = createdCollection.takeFirst();
        QCOMPARE(args.count(), 1);

        QVERIFY(m_engineWrite->collections(&error).contains(collection));
        QVERIFY(m_engineRead->collections(&error).contains(collection));
    }

    void testCreateTask()
    {
        static QString displayLabelValue = QStringLiteral("Todo test");
        static QString descriptionValue = QStringLiteral("Todo description");

        QOrganizerCollection collection;
        QtOrganizer::QOrganizerManager::Error error;
        collection.setMetaData(QOrganizerCollection::KeyName, defaultTaskCollectionName + "2");
        collection.setExtendedMetaData(collectionTypePropertyName, taskListTypeName);

        QVERIFY(m_engineWrite->saveCollection(&collection, &error));
        QVERIFY(!collection.id().isNull());

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
        appendToRemove(items[0].id());

        //verify signal
        QCOMPARE(createdItem.count(), 1);
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

const QString CollectionTest::defaultCollectionName = QStringLiteral("TEST COLLECTION");
const QString CollectionTest::defaultTaskCollectionName = QStringLiteral("TEST COLLECTION TASK LIST");
const QString CollectionTest::collectionTypePropertyName = QStringLiteral("collection-type");
const QString CollectionTest::taskListTypeName = QStringLiteral("Task List");


QTEST_MAIN(CollectionTest)

#include "collections-test.moc"
