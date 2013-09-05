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

private Q_SLOTS:
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
    }
};

QTEST_MAIN(ParseEcalTest)

#include "parseecal-test.moc"
