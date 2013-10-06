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

#include "qorganizer-eds-viewwatcher.h"
#include "qorganizer-eds-fetchrequestdata.h"
#include "qorganizer-eds-engineid.h"

#include <QtOrganizer/QOrganizerAbstractRequest>
#include <QtOrganizer/QOrganizerManagerEngine>
#include <QtOrganizer/QOrganizerItemId>

using namespace QtOrganizer;

QString ViewWatcher::m_dateFilter;

ViewWatcher::ViewWatcher(QOrganizerEDSEngine *engine,
                         const QOrganizerCollectionId &collectionId,
                         QOrganizerEDSCollectionEngineId *edsId)
    : m_parent(engine),
      m_edsId(edsId),
      m_eClient(0),
      m_eView(0),
      m_eventLoop(0)
{
    if (m_dateFilter.isEmpty()) {
        QDateTime startDate;
        startDate.setMSecsSinceEpoch(0);

        QDateTime endDate;
        endDate.setMSecsSinceEpoch(std::numeric_limits<qint64>::max());

        m_dateFilter =  QString("(occur-in-time-range? "
                                                     "(make-time \"%1\") (make-time \"%2\"))")
                                                     .arg(isodate_from_time_t(startDate.toTime_t()))
                                                     .arg(isodate_from_time_t(endDate.toTime_t()));

    }
    m_cancellable = g_cancellable_new();
    e_cal_client_connect(m_edsId->m_esource,
                         m_edsId->m_sourceType,
                         m_cancellable,
                         (GAsyncReadyCallback) ViewWatcher::clientConnected,
                         this);
}

ViewWatcher::~ViewWatcher()
{
    clear();
}

void ViewWatcher::clientConnected(GObject *sourceObject, GAsyncResult *res, ViewWatcher *self)
{
    Q_UNUSED(sourceObject);
    GError *gError = 0;
    self->m_eClient = E_CAL_CLIENT(e_cal_client_connect_finish(res, &gError));
    if (gError) {
        qWarning() << "Fail to connect with server ("
                   << e_source_get_display_name(self->m_edsId->m_esource) << "):"
                   << gError->message;
        g_error_free(gError);
        gError = 0;
        if (self->m_eventLoop) {
            self->m_eventLoop->quit();
        }
    } else {
        e_cal_client_get_view(self->m_eClient,
                              self->m_dateFilter.toUtf8().data(),
                              self->m_cancellable,
                              (GAsyncReadyCallback) ViewWatcher::viewReady,
                              self);
    }
}

void ViewWatcher::viewReady(GObject *sourceObject, GAsyncResult *res, ViewWatcher *self)
{
    Q_UNUSED(sourceObject);
    GError *gError = 0;
    ECalClientView *view = 0;
    e_cal_client_get_view_finish(self->m_eClient, res, &view, &gError);
    if (gError) {
        qWarning() << "Fail to open view ("
                   << e_source_get_display_name(self->m_edsId->m_esource) << "):"
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
                       << e_source_get_display_name(self->m_edsId->m_esource) << "):"
                       << gError->message;
            g_error_free(gError);
            gError = 0;
            if (self->m_eventLoop) {
                self->m_eventLoop->quit();
            }
        } else {
            qDebug() << "Listening for changes on ("
                     << e_source_get_display_name(self->m_edsId->m_esource) << ")";
        }
        g_clear_object(&self->m_cancellable);
    }

    if (self->m_eventLoop) {
        self->m_eventLoop->quit();
    }
}

void ViewWatcher::clear()
{
    if (m_cancellable) {
        g_cancellable_cancel(m_cancellable);
        wait();
        g_clear_object(&m_cancellable);
    }

    if (m_eView) {
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

void ViewWatcher::onObjectsAdded(ECalClientView *view,
                                 GSList *objects,
                                 ViewWatcher *self)
{
    Q_UNUSED(view);
    QList<QOrganizerItem> items;

    items = QOrganizerEDSEngine::parseEvents(self->m_edsId, objects, true);
    QOrganizerItemChangeSet changeSet;

    Q_FOREACH(QOrganizerItem item, items) {
        changeSet.insertAddedItem(item.id());
    }
    changeSet.emitSignals(self->m_parent);
}

void ViewWatcher::onObjectsRemoved(ECalClientView *view,
                                   GSList *objects,
                                   ViewWatcher *self)
{
    Q_UNUSED(view);
    QOrganizerItemChangeSet changeSet;

    for (GSList *l = objects; l; l = l->next) {
        ECalComponentId *id = static_cast<ECalComponentId*>(l->data);

        QOrganizerEDSEngineId *itemId = new QOrganizerEDSEngineId(self->m_edsId->toString(),
                                                                  QString::fromUtf8(id->uid));
        changeSet.insertRemovedItem(QOrganizerItemId(itemId));
    }
    changeSet.emitSignals(self->m_parent);
}

void ViewWatcher::onObjectsModified(ECalClientView *view,
                                    GSList *objects,
                                    ViewWatcher *self)
{
    QList<QOrganizerItem> items;

    items = QOrganizerEDSEngine::parseEvents(self->m_edsId, objects, true);
    QOrganizerItemChangeSet changeSet;

    Q_FOREACH(QOrganizerItem item, items) {
        changeSet.insertChangedItem(item.id());
    }
    changeSet.emitSignals(self->m_parent);
}
