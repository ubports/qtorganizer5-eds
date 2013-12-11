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

class RecurrenceTest : public QObject, public EDSBaseTest
{
    Q_OBJECT
private:
    static const QString defaultCollectionName;

    QOrganizerEDSEngine *m_engine;
    QOrganizerCollection m_collection;

private Q_SLOTS:
    void init()
    {
        EDSBaseTest::init(0);

        m_engine = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());

        QtOrganizer::QOrganizerManager::Error error;
        m_collection = QOrganizerCollection();
        m_collection.setMetaData(QOrganizerCollection::KeyName, defaultCollectionName);

        bool saveResult = m_engine->saveCollection(&m_collection, &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QtOrganizer::QOrganizerManager::NoError);
    }

    void cleanup()
    {
        EDSBaseTest::cleanup(m_engine);
    }

    void testCreateWeeklyEvent()
    {
        static QString displayLabelValue = QStringLiteral("Weekly test");
        static QString descriptionValue = QStringLiteral("Weekly description");

        QOrganizerEvent ev;
        ev.setCollectionId(m_collection.id());
        ev.setStartDateTime(QDateTime(QDate(2013, 12, 2), QTime(0,0,0)));
        ev.setEndDateTime(QDateTime(QDate(2013, 12, 2), QTime(0,30,0)));
        ev.setDisplayLabel(displayLabelValue);
        ev.setDescription(descriptionValue);

        QOrganizerRecurrenceRule rule;
        rule.setFrequency(QOrganizerRecurrenceRule::Weekly);
        rule.setDaysOfWeek(QSet<Qt::DayOfWeek>() << Qt::Monday);
        rule.setLimit(QDate(2013, 12, 31));
        ev.setRecurrenceRule(rule);

        QtOrganizer::QOrganizerManager::Error error;
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        QList<QOrganizerItem> items;
        items << ev;
        bool saveResult = m_engine->saveItems(&items,
                                              QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                              &errorMap,
                                              &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QtOrganizer::QOrganizerManager::NoError);

        // append new item to be removed after the test
        appendToRemove(items[0].id());

        QOrganizerItemSortOrder sort;
        QOrganizerItemFetchHint hint;
        QOrganizerItemFilter filter;
        items = m_engine->items(filter,
                      QDateTime(QDate(2013, 11, 30), QTime(0,0,0)),
                      QDateTime(QDate(2014, 1, 1), QTime(0,0,0)),
                      100,
                      sort,
                      hint,
                      &error);
        QCOMPARE(items.count(), 5);

        QList<QDateTime> expectedDates;
        expectedDates << QDateTime(QDate(2013, 12, 2), QTime(0,0,0))
                      << QDateTime(QDate(2013, 12, 9), QTime(0,0,0))
                      << QDateTime(QDate(2013, 12, 16), QTime(0,0,0))
                      << QDateTime(QDate(2013, 12, 23), QTime(0,0,0))
                      << QDateTime(QDate(2013, 12, 30), QTime(0,0,0));


        for(int i=0; i < 5; i++) {
            QOrganizerEventTime time = items[i].detail(QOrganizerItemDetail::TypeEventTime);
            QCOMPARE(time.startDateTime(), expectedDates[i]);
        }
    }

    void testCreateMonthlyEvent()
    {
        static QString displayLabelValue = QStringLiteral("Monthly test");
        static QString descriptionValue = QStringLiteral("Monthly description");

        QOrganizerEvent ev;
        ev.setCollectionId(m_collection.id());
        ev.setStartDateTime(QDateTime(QDate(2013, 1, 1), QTime(0,0,0)));
        ev.setEndDateTime(QDateTime(QDate(2013, 1, 1), QTime(0,30,0)));
        ev.setDisplayLabel(displayLabelValue);
        ev.setDescription(descriptionValue);

        QOrganizerRecurrenceRule rule;
        rule.setFrequency(QOrganizerRecurrenceRule::Monthly);
        rule.setDaysOfMonth(QSet<int>() << 1 << 5);
        rule.setLimit(QDate(2013, 12, 31));
        ev.setRecurrenceRule(rule);

        QtOrganizer::QOrganizerManager::Error error;
        QMap<int, QtOrganizer::QOrganizerManager::Error> errorMap;
        QList<QOrganizerItem> items;
        items << ev;
        bool saveResult = m_engine->saveItems(&items,
                                              QList<QtOrganizer::QOrganizerItemDetail::DetailType>(),
                                              &errorMap,
                                              &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QtOrganizer::QOrganizerManager::NoError);

        // append new item to be removed after the test
        appendToRemove(items[0].id());

        QOrganizerItemSortOrder sort;
        QOrganizerItemFetchHint hint;
        QOrganizerItemFilter filter;
        items = m_engine->items(filter,
                      QDateTime(QDate(2013, 1, 1), QTime(0,0,0)),
                      QDateTime(QDate(2014, 1, 1), QTime(0,0,0)),
                      100,
                      sort,
                      hint,
                      &error);
        QCOMPARE(items.count(), 24);
        for(int i=0; i < 12; i++) {
            QOrganizerEventTime time = items[i*2].detail(QOrganizerItemDetail::TypeEventTime);
            QCOMPARE(time.startDateTime(), QDateTime(QDate(2013, i+1, 1), QTime(0,0,0)));

            time = items[(i*2)+1].detail(QOrganizerItemDetail::TypeEventTime);
            QCOMPARE(time.startDateTime(), QDateTime(QDate(2013, i+1, 5), QTime(0,0,0)));
        }
    }
};

const QString RecurrenceTest::defaultCollectionName = QStringLiteral("TEST_RECURRENCE_EVENT_COLLECTION");

QTEST_MAIN(RecurrenceTest)

#include "recurrence-test.moc"
