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

#include <QtCore>
#include <QtTest>

#include <libecal/libecal.h>

EDSBaseTest::EDSBaseTest()
{
    GError *error = 0;
    m_sourceRegistry = e_source_registry_new_sync(0, &error);
    if (error) {
        qWarning() << "Fail to create sourge registry:" << error->message;
        g_error_free(error);
        Q_ASSERT(false);
    }
}

EDSBaseTest::~EDSBaseTest()
{
    g_object_unref(m_sourceRegistry);
}

void EDSBaseTest::init()
{
    cleanup();
    // wait to flush DBUS calls
    QTest::qWait(1000);
}

void EDSBaseTest::cleanup()
{
    GError *error;
    gboolean status;
    GList *sources = e_source_registry_list_sources(m_sourceRegistry, 0);

    for(GList  *i = sources; i != 0; i = i->next) {
        ESource *source = E_SOURCE(i->data);
        error = 0;
        status = true;
        if (e_source_get_remote_deletable(source)) {
            status = e_source_remote_delete_sync(source, 0, &error);
            QTest::qWait(100);
        } else if (e_source_get_removable(source)) {
            status = e_source_remove_sync(source, 0, &error);
            QTest::qWait(100);
            // check if source was removed
            const gchar *uid = e_source_get_uid(source);
            Q_ASSERT(e_source_registry_ref_source(m_sourceRegistry, uid) == 0);
        }
        if (error) {
            qWarning() << "Fail to remove source:" << error->message;
            g_error_free(error);
            Q_ASSERT(false);
        }

        Q_ASSERT(status);
    }

    g_list_free_full(sources, g_object_unref);
    e_source_registry_debug_dump(m_sourceRegistry, 0);
}
