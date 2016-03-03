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

//ugly hack but this allow us to test the engine without mock EDS
#define private public
#include "qorganizer-eds-engine.h"
#undef private

#include <QObject>
#include <QtTest>
#include <QDebug>

#include <QtOrganizer>

#include <libecal/libecal.h>

using namespace QtOrganizer;

class ParseEcalTest : public QObject
{
    Q_OBJECT
private:
    static const QString vEvent;
    QList<QOrganizerItem> m_itemsParsed;

public Q_SLOTS:
    void onEventAsyncParsed(QList<QOrganizerItem> items)
    {
        m_itemsParsed = items;
    }

private Q_SLOTS:
    void cleanup()
    {
        m_itemsParsed.clear();
    }

    void testParseStartTime()
    {
        QDateTime startTime = QDateTime::currentDateTime();
        QDateTime endTime(startTime);
        endTime = endTime.addDays(2);

        ECalComponent *comp = e_cal_component_new();
        e_cal_component_set_new_vtype(comp, E_CAL_COMPONENT_EVENT);

        ECalComponentDateTime dt;

        struct icaltimetype itt = icaltime_from_timet(startTime.toTime_t(), FALSE);
        dt.value = &itt;
        dt.tzid = "";
        e_cal_component_set_dtstart(comp, &dt);

        QOrganizerEvent item;
        QOrganizerEDSEngine::parseStartTime(comp, &item);
        QCOMPARE(item.startDateTime().toTime_t(), startTime.toTime_t());

        itt = icaltime_from_timet(endTime.toTime_t(), FALSE);
        dt.value = &itt;
        e_cal_component_set_dtend(comp, &dt);

        QOrganizerEDSEngine::parseEndTime(comp, &item);
        QCOMPARE(item.endDateTime().toTime_t(), endTime.toTime_t());
    }

    void testParseRemindersQOrganizerEvent2ECalComponent()
    {
        QOrganizerEvent event;
        QOrganizerItemAudibleReminder aReminder;

        // Check audible reminder
        aReminder.setRepetition(10, 30);
        aReminder.setSecondsBeforeStart(10);
        QCOMPARE(aReminder.secondsBeforeStart(), 10);
        event.saveDetail(&aReminder);

        ECalComponent *comp = e_cal_component_new();
        e_cal_component_set_new_vtype(comp, E_CAL_COMPONENT_EVENT);

        QOrganizerEDSEngine::parseReminders(event, comp);

        GList *aIds = e_cal_component_get_alarm_uids(comp);
        QCOMPARE(g_list_length(aIds), (guint) 1);

        ECalComponentAlarm *alarm = e_cal_component_get_alarm(comp, (const gchar*)aIds->data);
        QVERIFY(alarm);

        ECalComponentAlarmAction aAction;
        e_cal_component_alarm_get_action(alarm, &aAction);
        QCOMPARE(aAction, E_CAL_COMPONENT_ALARM_AUDIO);

        ECalComponentAlarmTrigger trigger;
        e_cal_component_alarm_get_trigger(alarm, &trigger);
        QCOMPARE(trigger.type, E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START);
        QCOMPARE(icaldurationtype_as_int(trigger.u.rel_duration) * -1, aReminder.secondsBeforeStart());

        ECalComponentAlarmRepeat aRepeat;
        e_cal_component_alarm_get_repeat(alarm, &aRepeat);
        QCOMPARE(aRepeat.repetitions, aReminder.repetitionCount());
        QCOMPARE(icaldurationtype_as_int(aRepeat.duration), aReminder.repetitionDelay());

        g_object_unref(comp);
    }

    void testParseRemindersECalComponent2QOrganizerEvent()
    {
        ECalComponent *comp = e_cal_component_new();
        e_cal_component_set_new_vtype(comp, E_CAL_COMPONENT_EVENT);

        ECalComponentAlarm *alarm = e_cal_component_alarm_new();
        e_cal_component_alarm_set_action(alarm, E_CAL_COMPONENT_ALARM_DISPLAY);

        ECalComponentAlarmTrigger trigger;
        trigger.type = E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START;
        trigger.u.rel_duration = icaldurationtype_from_int(-10);
        e_cal_component_alarm_set_trigger(alarm, trigger);

        ECalComponentAlarmRepeat aRepeat;
        aRepeat.repetitions = 5;
        aRepeat.duration = icaldurationtype_from_int(100);
        e_cal_component_alarm_set_repeat(alarm, aRepeat);

        e_cal_component_add_alarm(comp, alarm);
        e_cal_component_alarm_free(alarm);

        QOrganizerItem alarmItem;
        QOrganizerEDSEngine::parseReminders(comp, &alarmItem);

        QOrganizerItemVisualReminder vReminder = alarmItem.detail(QOrganizerItemDetail::TypeVisualReminder);
        QCOMPARE(vReminder.repetitionCount(), 5);
        QCOMPARE(vReminder.repetitionDelay(), 100);
        QCOMPARE(vReminder.secondsBeforeStart(), 10);

        g_object_unref(comp);
    }

    void testParseRecurenceQOrganizerEvent2ECalComponent()
    {
        // by date
        QOrganizerEvent event;
        QOrganizerItemRecurrence rec;

        QList<QDate> rDates;
        rDates << QDate(2010, 1, 20)
               << QDate(2011, 2, 21)
               << QDate(2012, 3, 22);

        rec.setRecurrenceDates(rDates.toSet());

        QList<QDate> rExeptDates;
        rExeptDates << QDate(2013, 4, 23)
                    << QDate(2014, 5, 24)
                    << QDate(2015, 6, 25);
        rec.setExceptionDates(rExeptDates.toSet());

        QOrganizerRecurrenceRule dailyRule;
        QList<QOrganizerRecurrenceRule> rrules;

        dailyRule.setFrequency(QOrganizerRecurrenceRule::Daily);
        dailyRule.setLimit(1000);
        rrules << dailyRule;

        QOrganizerRecurrenceRule weeklyRule;
        weeklyRule.setFrequency(QOrganizerRecurrenceRule::Weekly);
        weeklyRule.setLimit(1001);
        QList<Qt::DayOfWeek> daysOfWeek;
        daysOfWeek << Qt::Monday
                   << Qt::Tuesday
                   << Qt::Wednesday
                   << Qt::Thursday
                   << Qt::Friday;
        weeklyRule.setDaysOfWeek(daysOfWeek.toSet());
        weeklyRule.setFirstDayOfWeek(Qt::Sunday);
        rrules << weeklyRule;

        QOrganizerRecurrenceRule monthlyRule;
        monthlyRule.setFrequency(QOrganizerRecurrenceRule::Monthly);
        monthlyRule.setLimit(1002);

        QList<int> daysOfMonth;
        daysOfMonth << 1
                    << 15
                    << 30;
        monthlyRule.setDaysOfMonth(daysOfMonth.toSet());
        rrules << monthlyRule;

        QOrganizerRecurrenceRule yearlyRule;
        yearlyRule.setFrequency(QOrganizerRecurrenceRule::Yearly);
        yearlyRule.setLimit(1003);
        QList<int> daysOfYear;
        daysOfYear << 1
                    << 10
                    << 20
                    << 50
                    << 300;
        yearlyRule.setDaysOfYear(daysOfYear.toSet());


        QList<QOrganizerRecurrenceRule::Month> monthsOfYear;
        monthsOfYear << QOrganizerRecurrenceRule::January
                     << QOrganizerRecurrenceRule::March
                     << QOrganizerRecurrenceRule::December;
        yearlyRule.setMonthsOfYear(monthsOfYear.toSet());
        rrules << yearlyRule;

        rec.setRecurrenceRules(rrules.toSet());

        // save recurrence
        event.saveDetail(&rec);

        ECalComponent *comp = e_cal_component_new();
        e_cal_component_set_new_vtype(comp, E_CAL_COMPONENT_EVENT);
        QOrganizerEDSEngine::parseRecurrence(event, comp);

        // recurrence dates
        GSList *periodList = 0;
        e_cal_component_get_rdate_list(comp, &periodList);

        QCOMPARE(g_slist_length(periodList), (guint)3);

        for(GSList *pIter = periodList; pIter != 0; pIter = pIter->next) {
            ECalComponentPeriod *period = static_cast<ECalComponentPeriod*>(pIter->data);
            QDate periodDate = QDateTime::fromTime_t(icaltime_as_timet(period->start)).date();

            QVERIFY(rDates.contains(periodDate));
        }
        e_cal_component_free_period_list(periodList);

        // exception dates
        GSList *exDateList = 0;
        e_cal_component_get_exdate_list(comp, &exDateList);
        for(GSList *pIter = exDateList; pIter != 0; pIter = pIter->next) {
            ECalComponentDateTime *exDate = static_cast<ECalComponentDateTime*>(pIter->data);
            QDate exDateValue = QDateTime::fromTime_t(icaltime_as_timet(*exDate->value)).date();
            QVERIFY(rExeptDates.contains(exDateValue));
        }
        e_cal_component_free_exdate_list(exDateList);


        // rules
        GSList *recurList = 0;
        e_cal_component_get_rrule_list(comp, &recurList);
        QCOMPARE(g_slist_length(recurList), (guint) rrules.count());

        for(GSList *recurListIter = recurList; recurListIter != 0; recurListIter = recurListIter->next) {
            struct icalrecurrencetype *rule = static_cast<struct icalrecurrencetype*>(recurListIter->data);

            switch(rule->freq)
            {
            case ICAL_DAILY_RECURRENCE:
                QCOMPARE(rule->count, dailyRule.limitCount());
                break;
            case ICAL_WEEKLY_RECURRENCE:
                QCOMPARE(rule->count, weeklyRule.limitCount());
                for (int d = Qt::Monday; d <= Qt::Sunday; d++) {
                    if (daysOfWeek.contains(static_cast<Qt::DayOfWeek>(d))) {
                        QVERIFY(rule->by_day[(d-1)] != ICAL_RECURRENCE_ARRAY_MAX);
                    } else {
                        QVERIFY(rule->by_day[d-1] == ICAL_RECURRENCE_ARRAY_MAX);
                    }
                }
                break;
            case ICAL_MONTHLY_RECURRENCE:
            {
                QCOMPARE(rule->count, monthlyRule.limitCount());

                QList<int> ruleDays;
                for (int d=0; d < ICAL_BY_MONTHDAY_SIZE; d++) {
                    if (rule->by_month_day[d] != ICAL_RECURRENCE_ARRAY_MAX) {
                        ruleDays << rule->by_month_day[d];
                    }
                }
                QCOMPARE(ruleDays.count(), daysOfMonth.count());
                Q_FOREACH(int day, ruleDays) {
                    QVERIFY(daysOfMonth.contains(day));
                }
                break;
            }
            case ICAL_YEARLY_RECURRENCE:
            {
                QCOMPARE(rule->count, yearlyRule.limitCount());
                QList<int> ruleDays;

                for (int d=0; d < ICAL_BY_YEARDAY_SIZE; d++) {
                    if (rule->by_year_day[d] != ICAL_RECURRENCE_ARRAY_MAX) {

                        ruleDays << rule->by_year_day[d];
                    }
                }
                QCOMPARE(ruleDays.count(), daysOfYear.count());
                Q_FOREACH(int day, ruleDays) {
                    QVERIFY(daysOfYear.contains(day));
                }

                QList<int> ruleMonths;
                for (int d=0; d < ICAL_BY_MONTH_SIZE; d++) {
                    if (rule->by_month[d] != ICAL_RECURRENCE_ARRAY_MAX) {
                        ruleMonths << rule->by_month[d];
                    }
                }
                QCOMPARE(ruleMonths.count(), monthsOfYear.count());
                Q_FOREACH(int month, ruleMonths) {
                    QVERIFY(monthsOfYear.contains(static_cast<QOrganizerRecurrenceRule::Month>(month)));
                }
            }
            default:
                break;
            }
        }

        // invert
        QOrganizerEvent event2;
        QOrganizerEDSEngine::parseRecurrence(comp, &event2);

        QCOMPARE(event2.recurrenceDates(), event.recurrenceDates());
        QCOMPARE(event2.exceptionDates(), event.exceptionDates());

        QList<QOrganizerRecurrenceRule> rrules2 = event2.recurrenceRules().toList();
        QCOMPARE(rrules2.count(), rrules.count());

        Q_FOREACH(const QOrganizerRecurrenceRule &rule2, rrules2) {
            switch(rule2.frequency()) {
            case QOrganizerRecurrenceRule::Daily:
                QCOMPARE(rule2.limitCount(), dailyRule.limitCount());
                break;
            case QOrganizerRecurrenceRule::Weekly:
            {
                QCOMPARE(rule2.limitCount(), weeklyRule.limitCount());
                QList<Qt::DayOfWeek> daysOfWeek2 = rule2.daysOfWeek().toList();
                qSort(daysOfWeek2);
                QCOMPARE(daysOfWeek2, daysOfWeek);
                break;
            }
            case QOrganizerRecurrenceRule::Monthly:
            {
                QCOMPARE(rule2.limitCount(), monthlyRule.limitCount());
                QList<int> daysOfMonth2 = rule2.daysOfMonth().toList();
                qSort(daysOfMonth2);
                QCOMPARE(daysOfMonth2, daysOfMonth);
                break;
            }
            case QOrganizerRecurrenceRule::Yearly:
            {
                QCOMPARE(rule2.limitCount(), yearlyRule.limitCount());
                QList<int> daysOfYear2 = rule2.daysOfYear().toList();
                qSort(daysOfYear2);
                QCOMPARE(daysOfYear2, daysOfYear);

                QList<QOrganizerRecurrenceRule::Month> monthsOfYear2 = rule2.monthsOfYear().toList();
                qSort(monthsOfYear2);
                QCOMPARE(monthsOfYear2, monthsOfYear);
                break;
            }
            default:
                QVERIFY(false);
            }
        }

        g_object_unref(comp);
    }

    void testAsyncParse()
    {
        qRegisterMetaType<QList<QOrganizerItem> >();
        QOrganizerEDSEngine *engine = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());
        QVERIFY(engine);
        icalcomponent *ical = icalcomponent_new_from_string(vEvent.toUtf8().data());
        QVERIFY(ical);
        QVERIFY(icalcomponent_is_valid(ical));

        QList<QOrganizerItemDetail::DetailType> detailsHint;
        GSList *events = g_slist_append(0, ical);
        QMap<QString, GSList*> eventMap;
        eventMap.insert(engine->defaultCollection(0).id().toString(), events);
        engine->parseEventsAsync(eventMap, true, detailsHint, this, SLOT(onEventAsyncParsed(QList<QOrganizerItem>)));

        QTRY_COMPARE(m_itemsParsed.size(), 1);
        QOrganizerEvent ev = m_itemsParsed.at(0);
        QDateTime eventTime(QDate(2015,04, 8), QTime(19, 0, 0), QTimeZone("America/Recife"));
        QCOMPARE(ev.startDateTime(), eventTime);
        QCOMPARE(ev.endDateTime(), eventTime.addSecs(30 * 60));
        QCOMPARE(ev.displayLabel(), QStringLiteral("one minute after start"));
        QCOMPARE(ev.description(), QStringLiteral("event to parse"));

        QOrganizerRecurrenceRule rrule = ev.recurrenceRule();
        QCOMPARE(rrule.frequency(), QOrganizerRecurrenceRule::Daily);
        QCOMPARE(rrule.limitType(), QOrganizerRecurrenceRule::NoLimit);

        QList<QDate> except = ev.exceptionDates().toList();
        qSort(except);
        QCOMPARE(except.size(), 2);
        QCOMPARE(except.at(0), QDate(2015, 4, 9));
        QCOMPARE(except.at(1), QDate(2015, 5, 1));

        QOrganizerItemVisualReminder vreminder = ev.detail(QOrganizerItemDetail::TypeVisualReminder);
        QCOMPARE(vreminder.secondsBeforeStart(), 60);
        QCOMPARE(vreminder.message(), QStringLiteral("alarm to parse"));

        g_slist_free_full(events, (GDestroyNotify)icalcomponent_free);
        delete engine;
    }
};

const QString ParseEcalTest::vEvent = QStringLiteral(""
"BEGIN:VEVENT\r\n"
"UID:20150408T215243Z-19265-1000-5926-24@renato-ubuntu\r\n"
"DTSTAMP:20150408T214536Z\r\n"
"DTSTART;TZID=/freeassociation.sourceforge.net/Tzfile/America/Recife:\r\n"
" 20150408T190000\r\n"
"DTEND;TZID=/freeassociation.sourceforge.net/Tzfile/America/Recife:\r\n"
" 20150408T193000\r\n"
"TRANSP:OPAQUE\r\n"
"SEQUENCE:6\r\n"
"SUMMARY:one minute after start\r\n"
"DESCRIPTION:event to parse\r\n"
"CLASS:PUBLIC\r\n"
"CREATED:20150408T215308Z\r\n"
"LAST-MODIFIED:20150409T200807Z\r\n"
"RRULE:FREQ=DAILY\r\n"
"EXDATE;VALUE=DATE:20150501\r\n"
"EXDATE;VALUE=DATE:20150409\r\n"
"BEGIN:VALARM\r\n"
"X-EVOLUTION-ALARM-UID:20150408T215549Z-19265-1000-5926-38@renato-ubuntu\r\n"
"ACTION:DISPLAY\r\n"
"TRIGGER;VALUE=DURATION;RELATED=START:-PT1M\r\n"
"DESCRIPTION:alarm to parse\r\n"
"END:VALARM\r\n"
"END:VEVENT\r\n"
"");


QTEST_MAIN(ParseEcalTest)

#include "parseecal-test.moc"
