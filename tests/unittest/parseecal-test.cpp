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
};

QTEST_MAIN(ParseEcalTest)

#include "parseecal-test.moc"
