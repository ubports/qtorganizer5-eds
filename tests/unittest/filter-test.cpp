/*
 * Copyright 2015 Canonical Ltd.
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

class FilterTest : public QObject, public EDSBaseTest
{
    Q_OBJECT
private:
    QOrganizerEDSEngine *m_engine;

    void createCollection(QOrganizerCollection **collection)
    {
        QtOrganizer::QOrganizerManager::Error error;
        *collection = new QOrganizerCollection();
        (*collection)->setMetaData(QOrganizerCollection::KeyName,
                                   uniqueCollectionName());

        QSignalSpy createdCollection(m_engine,
                                     SIGNAL(collectionsAdded(QList<QOrganizerCollectionId>)));
        bool saveResult = m_engine->saveCollection(*collection, &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QtOrganizer::QOrganizerManager::NoError);
        QTRY_COMPARE(createdCollection.count(), 1);
    }

private Q_SLOTS:
    void initTestCase()
    {
        EDSBaseTest::init();
        m_engine = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());
    }

    void cleanupTestCase()
    {
        delete m_engine;
        EDSBaseTest::cleanup();
    }

    // test functions
    void testFilterEventByCollectionId()
    {
        static QString displayLabelValue = QStringLiteral("Todo test %1 - %2");
        static QString descriptionValue = QStringLiteral("Todo description %1 - %2");

        QList<QOrganizerItem> items;
        QDateTime currentDate = QDateTime::currentDateTime();

        // create items on default collection
        for(int i=0; i < 10; i++) {
            QOrganizerEvent ev;
            ev.setStartDateTime(currentDate);
            ev.setEndDateTime(currentDate.addDays(1));
            ev.setDisplayLabel(displayLabelValue.arg(i).arg("default"));
            ev.setDescription(descriptionValue.arg(i).arg("default"));
            items << ev;
        }

        // create items on new collection
        QOrganizerCollection *collection;
        createCollection(&collection);
        for(int i=0; i < 10; i++) {
            QOrganizerEvent ev;
            ev.setCollectionId(collection->id());
            ev.setStartDateTime(currentDate);
            ev.setEndDateTime(currentDate.addDays(1));
            ev.setDisplayLabel(displayLabelValue.arg(i).arg("new"));
            ev.setDescription(descriptionValue.arg(i).arg("new"));
            items << ev;
        }

        // save all items
        QtOrganizer::QOrganizerManager::Error error;
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        bool saveResult = m_engine->saveItems(&items,
                                              QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                              &errorMap,
                                              &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QtOrganizer::QOrganizerManager::NoError);

        QOrganizerItemSortOrder sort;
        QOrganizerItemFetchHint hint;
        QOrganizerItemCollectionFilter filter;

        // filter items from default collection
        filter.setCollectionId(m_engine->defaultCollection(0).id());
        items = m_engine->items(filter,
                      QDateTime(),
                      QDateTime(),
                      100,
                      sort,
                      hint,
                      &error);
        QCOMPARE(items.count(), 10);
        // check returned items
        Q_FOREACH(const QOrganizerItem &i, items) {
            QVERIFY(static_cast<QOrganizerEvent>(i).displayLabel().endsWith("default"));
        }

        // filter items from new collection
        filter.setCollectionId(collection->id());
        items = m_engine->items(filter,
                      QDateTime(),
                      QDateTime(),
                      100,
                      sort,
                      hint,
                      &error);
        QCOMPARE(items.count(), 10);
        // check returned items
        Q_FOREACH(const QOrganizerItem &i, items) {
            QVERIFY(static_cast<QOrganizerEvent>(i).displayLabel().endsWith("new"));
        }

        // filter items from both collections
        QSet<QOrganizerCollectionId> ids;
        ids << m_engine->defaultCollection(0).id()
            << collection->id();

        filter.setCollectionIds(ids);
        items = m_engine->items(filter,
                      QDateTime(),
                      QDateTime(),
                      100,
                      sort,
                      hint,
                      &error);
        QCOMPARE(items.count(), 20);

        // filter using union filter
        // filter events from new collection or ends with 'default'
        QOrganizerItemUnionFilter uFilter;
        filter.setCollectionId(collection->id());

        QOrganizerItemDetailFieldFilter dFilter;
        dFilter.setDetail(QOrganizerItemDetail::TypeDescription,
                          QOrganizerItemDescription::FieldDescription);
        dFilter.setMatchFlags(QOrganizerItemFilter::MatchEndsWith);
        dFilter.setValue("default");
        uFilter.append(filter);
        uFilter.append(dFilter);

        qDebug() << "Fetch union>>>>>>>>>>>>>>>";
        items = m_engine->items(uFilter,
                      QDateTime(),
                      QDateTime(),
                      100,
                      sort,
                      hint,
                      &error);
        QCOMPARE(items.count(), 20);

        // filter using intersection filter
        // filter events from new collection and ends with 'new'
        QOrganizerItemIntersectionFilter iFilter;
        dFilter.setValue("new");
        iFilter.append(filter);
        iFilter.append(dFilter);

        items = m_engine->items(iFilter,
                      QDateTime(),
                      QDateTime(),
                      100,
                      sort,
                      hint,
                      &error);
        QCOMPARE(items.count(), 10);
        // check returned items
        Q_FOREACH(const QOrganizerItem &i, items) {
            QVERIFY(static_cast<QOrganizerEvent>(i).displayLabel().endsWith("new"));
        }

        delete collection;
    }
};

QTEST_MAIN(FilterTest)

#include "filter-test.moc"
