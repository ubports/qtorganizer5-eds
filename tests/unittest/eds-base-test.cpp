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


#include "eds-base-test.h"
#include <QtCore>
#include <QtTest>
#include <libecal/libecal.h>


void EDSBaseTest::clear()
{
    GError *error = 0;
    ESourceRegistry *registry = e_source_registry_new_sync(0, &error);
    if (error) {
        qWarning() << "Failt to get registry" << error->message;
    }
    Q_ASSERT(error == 0);
    GList *sources = e_source_registry_list_sources(registry, 0);
    for(int i=0, iMax=g_list_length(sources); i < iMax; i++) {
        ESource *source = E_SOURCE(g_list_nth_data(sources, i));
        if (e_source_get_removable(source)) {
            e_source_remove_sync(source, 0, &error);
            if (error) {
                qWarning() << "Failt to remove source" << error->message;
            }
            Q_ASSERT(error == 0);
        }
        g_object_unref(source);
    }
    g_object_unref(registry);
    QTest::qSleep(500);
}
