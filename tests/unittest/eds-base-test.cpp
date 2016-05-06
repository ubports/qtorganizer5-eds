/*
 * Copyright 2013 Canonical Ltd.
 *
 * This file is part of ubuntu-pim-service.
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

#include "config.h"
#include "eds-base-test.h"
#include "qorganizer-eds-engine.h"

#include <QtCore>
#include <QtTest>

#include <libecal/libecal.h>

using namespace QtOrganizer;

class GScopedPointerUnref
{
public:
    static inline void cleanup(void *pointer)
    {
        if (pointer) {
            g_clear_object(&pointer);
        }
    }
};

template<class KLASS>
class GScopedPointer : public QScopedPointer<KLASS, GScopedPointerUnref>
{
public:
    GScopedPointer(KLASS* obj = 0)
        : QScopedPointer<KLASS, GScopedPointerUnref>(obj)
    {}
};

EDSBaseTest::EDSBaseTest()
{
    qRegisterMetaType<QList<QOrganizerCollectionId> >();
    qRegisterMetaType<QList<QOrganizerItemId> >();
    qRegisterMetaType<QList<QOrganizerItem> >();
    QCoreApplication::addLibraryPath(QORGANIZER_DEV_PATH);
}

EDSBaseTest::~EDSBaseTest()
{
}

void EDSBaseTest::initTestCase()
{
    QTest::qWait(1000);
}


void EDSBaseTest::init()
{
}

void EDSBaseTest::cleanup()
{
    QTest::qWait(1000);
}

QString EDSBaseTest::getEventFromEvolution(const QOrganizerItemId &id,
                                           const QOrganizerCollectionId &collectionId)
{
    GError *error = 0;
    GScopedPointer<ESourceRegistry> sourceRegistry(e_source_registry_new_sync(0, &error));
    if (error) {
        qWarning() << "Fail to create source registry" << error->message;
        g_error_free(error);
        return QString();
    }
    GScopedPointer<ESource> calendar;
    if (collectionId.isNull()) {
        calendar.reset(e_source_registry_ref_default_calendar(sourceRegistry.data()));
    } else {
        calendar.reset(e_source_registry_ref_source(sourceRegistry.data(),
                                                    collectionId.toString().split(":").last().toUtf8().data()));
    }
    GScopedPointer<EClient> client(E_CAL_CLIENT_CONNECT_SYNC(calendar.data(),
                                                             E_CAL_CLIENT_SOURCE_TYPE_EVENTS,
                                                             0,
                                                             &error));
    if (error) {
        qWarning() << "Fail to connect to calendar" << error->message;
        g_error_free(error);
        return QString();
    }

    icalcomponent *obj = 0;
    QString uid = id.toString().split("/").last();
    QString ruid;

    // recurrence id
    if (uid.contains("#")) {
        QStringList ids = uid.split("#");
        uid = ids[0];
        ruid = ids[1];
    }

    e_cal_client_get_object_sync(reinterpret_cast<ECalClient*>(client.data()),
                                 uid.toUtf8().data(),
                                 ruid.toUtf8().data(),
                                 &obj, 0, &error);
    if (error) {
        qWarning() << "Fail to retrieve object:" << error->message;
        g_error_free(error);
    }

    QString result = QString::fromUtf8(icalcomponent_as_ical_string(obj));
    icalcomponent_free (obj);
    return result;
}

QString EDSBaseTest::uniqueCollectionName() const
{
    return QUuid::createUuid().toString();
}
