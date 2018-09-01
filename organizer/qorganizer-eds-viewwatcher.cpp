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

#include "qorganizer-eds-enginedata.h"
#include "qorganizer-eds-viewwatcher.h"
#include "qorganizer-eds-fetchrequestdata.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>

#include <QtOrganizer/QOrganizerAbstractRequest>
#include <QtOrganizer/QOrganizerManagerEngine>
#include <QtOrganizer/QOrganizerItemId>

using namespace QtOrganizer;

ViewWatcher::ViewWatcher(const QString &collectionId,
                         QOrganizerEDSEngineData *data,
                         EClient *client)
    : m_collectionId(collectionId),
      m_engineData(data),
      m_eClient(E_CAL_CLIENT(client)),
      m_eView(0),
      m_eventLoop(0)
{
    g_object_ref(m_eClient);
    m_cancellable = g_cancellable_new();
    e_cal_client_get_view(m_eClient,
                          QStringLiteral("#t").toUtf8().constData(), // match all,
                          m_cancellable,
                          (GAsyncReadyCallback) ViewWatcher::viewReady,
                          this);
    wait();
    m_dirty.setSingleShot(true);
    connect(&m_dirty, SIGNAL(timeout()), SLOT(flush()));
}

ViewWatcher::~ViewWatcher()
{
    clear();
}

void ViewWatcher::viewReady(GObject *sourceObject, GAsyncResult *res, ViewWatcher *self)
{
    Q_UNUSED(sourceObject);
    GError *gError = 0;
    ECalClientView *view = 0;
    e_cal_client_get_view_finish(self->m_eClient, res, &view, &gError);
    if (gError) {
        qWarning() << "Fail to open view ("
                   << self->m_collectionId << "):"
                   << gError->message;
        g_error_free(gError);
        gError = 0;
    } else {
        self->m_eView = view;

        g_signal_connect(view,
                         "objects-added",
                         (GCallback) ViewWatcher::onObjectsAdded,
                         self);

        g_signal_connect(view,
                         "objects-removed",
                         (GCallback) ViewWatcher::onObjectsRemoved,
                         self);

        g_signal_connect(view,
                         "objects-modified",
                         (GCallback) ViewWatcher::onObjectsModified,
                         self);
        e_cal_client_view_set_flags(view, E_CAL_CLIENT_VIEW_FLAGS_NONE, NULL);
        e_cal_client_view_start(view, &gError);
        if (gError) {
            qWarning() << "Fail to start view ("
                       << self->m_collectionId << "):"
                       << gError->message;
            g_error_free(gError);
            gError = 0;
        }
    }
    g_clear_object(&self->m_cancellable);
    if (self->m_eventLoop) {
        self->m_eventLoop->quit();
    }
}

void ViewWatcher::clear()
{
    if (m_cancellable) {
        g_cancellable_cancel(m_cancellable);
        wait();
        Q_ASSERT(m_cancellable == 0);
    }

    if (m_eView) {
        GError *gErr = 0;
        e_cal_client_view_stop(m_eView, &gErr);
        if (gErr) {
            qWarning() << "Fail to stop view" << gErr->message;
            g_error_free(gErr);
        }
        g_clear_object(&m_eView);
    }

    if (m_eClient) {
        g_clear_object(&m_eClient);
    }
}

void ViewWatcher::wait()
{
    if (m_cancellable) {
        QEventLoop eventLoop;
        m_eventLoop = &eventLoop;
        eventLoop.exec();
        m_eventLoop = 0;
    }
}

QList<QOrganizerItemId> ViewWatcher::parseItemIds(GSList *objects)
{
    QList<QOrganizerItemId> result;

    for (GSList *l = objects; l; l = l->next) {
        const gchar *uid = 0;
        icalcomponent *icalcomp = static_cast<icalcomponent*>(l->data);
        icalproperty *prop = icalcomponent_get_first_property(icalcomp, ICAL_UID_PROPERTY);
        if (prop) {
            uid = icalproperty_get_uid(prop);
        } else {
            qWarning() << "Fail to parse component ID";
        }

        QOrganizerCollectionId collectionId =
            QOrganizerCollectionId::fromString(m_collectionId);
        QOrganizerItemId itemId(collectionId.managerUri(), uid);
        result << itemId;
    }
    return result;
}

void ViewWatcher::notify()
{
    m_dirty.start(500);
}

void ViewWatcher::flush()
{
    m_engineData->emitSharedSignals(&m_changeSet);
    m_changeSet.clearAll();
}

void ViewWatcher::onObjectsAdded(ECalClientView *view,
                                 GSList *objects,
                                 ViewWatcher *self)
{
    Q_UNUSED(view);
    self->m_changeSet.insertAddedItems(self->parseItemIds(objects));
    self->notify();
}

void ViewWatcher::onObjectsRemoved(ECalClientView *view,
                                   GSList *objects,
                                   ViewWatcher *self)
{
    Q_UNUSED(view);

    for (GSList *l = objects; l; l = l->next) {
        ECalComponentId *id = static_cast<ECalComponentId*>(l->data);
        QOrganizerCollectionId collectionId =
            QOrganizerCollectionId::fromString(self->m_collectionId);
        QOrganizerItemId itemId(collectionId.managerUri(), id->uid);
        self->m_changeSet.insertRemovedItem(itemId);
    }
    self->notify();
}

void ViewWatcher::onObjectsModified(ECalClientView *view,
                                    GSList *objects,
                                    ViewWatcher *self)
{
    Q_UNUSED(view);

    self->m_changeSet.insertChangedItems(self->parseItemIds(objects),
                                         QList<QOrganizerItemDetail::DetailType>());
    self->notify();
}
