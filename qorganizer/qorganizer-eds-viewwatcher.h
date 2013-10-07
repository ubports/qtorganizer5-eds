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

#ifndef __QORGANIZER_EDS_VIEWWATCHER_H__
#define __QORGANIZER_EDS_VIEWWATCHER_H__

#include "qorganizer-eds-engine.h"

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QEventLoop>

#include <libecal/libecal.h>

class ViewWatcher : public QObject
{
    Q_OBJECT
public:
    ViewWatcher(QOrganizerEDSEngine *engine,
                const QtOrganizer::QOrganizerCollectionId &collectionId,
                QOrganizerEDSCollectionEngineId *edsId);
    virtual ~ViewWatcher();
    void append(ECalClientView *view, FetchRequestData *data);
    void clear();
    void wait();

Q_SIGNALS:
    void viewProcessed(ECalClientView *view, gpointer data);

private:
    QOrganizerEDSEngine *m_parent;
    GCancellable *m_cancellable;
    QOrganizerEDSCollectionEngineId *m_edsId;
    ECalClient *m_eClient;
    ECalClientView *m_eView;
    QEventLoop *m_eventLoop;

    static QString m_dateFilter;

    static void clientConnected(GObject *sourceObject, GAsyncResult *res, ViewWatcher *self);
    static void viewReady(GObject *sourceObject, GAsyncResult *res, ViewWatcher *self);

    static void onObjectsAdded(ECalClientView *view, GSList *objects, ViewWatcher *self);
    static void onObjectsRemoved(ECalClientView *view, GSList *objects, ViewWatcher *self);
    static void onObjectsModified(ECalClientView *view, GSList *objects, ViewWatcher *self);
};

#endif
