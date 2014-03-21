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
#include "qorganizer-eds-engineid.h"

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
        e_cal_client_view_stop(m_eView, 0);
        QCoreApplication::processEvents();
        g_clear_object(&m_eView);
    }

    if (m_eClient) {
        g_clear_object(&m_eClient);
        QCoreApplication::processEvents();
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
        ECalComponent *comp;

        icalcomponent *clone = icalcomponent_new_clone(static_cast<icalcomponent*>(l->data));
        comp = e_cal_component_new_from_icalcomponent(clone);

        e_cal_component_get_uid(comp, &uid);
        QOrganizerEDSEngineId *itemId = new QOrganizerEDSEngineId(m_collectionId,
                                                                  QString::fromUtf8(uid));
        result << QOrganizerItemId(itemId);
    }
    return result;
}

void ViewWatcher::onObjectsAdded(ECalClientView *view,
                                 GSList *objects,
                                 ViewWatcher *self)
{
    Q_UNUSED(view);

    QOrganizerItemChangeSet changeSet;
    changeSet.insertAddedItems(self->parseItemIds(objects));
    self->m_engineData->emitSharedSignals(&changeSet);
}

void ViewWatcher::onObjectsRemoved(ECalClientView *view,
                                   GSList *objects,
                                   ViewWatcher *self)
{
    Q_UNUSED(view);
    QOrganizerItemChangeSet changeSet;

    for (GSList *l = objects; l; l = l->next) {
        ECalComponentId *id = static_cast<ECalComponentId*>(l->data);
        QOrganizerEDSEngineId *itemId = new QOrganizerEDSEngineId(self->m_collectionId,
                                                                  QString::fromUtf8(id->uid));
        changeSet.insertRemovedItem(QOrganizerItemId(itemId));
    }

    self->m_engineData->emitSharedSignals(&changeSet);
}

void ViewWatcher::onObjectsModified(ECalClientView *view,
                                    GSList *objects,
                                    ViewWatcher *self)
{
    Q_UNUSED(view);

    QOrganizerItemChangeSet changeSet;
    changeSet.insertChangedItems(self->parseItemIds(objects));

    self->m_engineData->emitSharedSignals(&changeSet);
}
