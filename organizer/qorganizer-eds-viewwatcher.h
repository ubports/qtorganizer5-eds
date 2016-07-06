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
#include <QtCore/QTimer>

#include <libecal/libecal.h>

class QOrganizerEDSEngineData;

class ViewWatcher : public QObject
{
    Q_OBJECT
public:
    ViewWatcher(const QString &collectionId,
                QOrganizerEDSEngineData *data,
                EClient *client);
    virtual ~ViewWatcher();
    void clear();
    void wait();

private Q_SLOTS:
    void flush();

private:
    QString m_collectionId;
    QOrganizerEDSEngineData *m_engineData;
    GCancellable *m_cancellable;
    ECalClient *m_eClient;
    ECalClientView *m_eView;
    QEventLoop *m_eventLoop;
    QOrganizerItemChangeSet m_changeSet;
    QTimer m_dirty;

    QList<QtOrganizer::QOrganizerItemId> parseItemIds(GSList *objects);
    void notify();


    static void clientConnected(GObject *sourceObject, GAsyncResult *res, ViewWatcher *self);
    static void viewReady(GObject *sourceObject, GAsyncResult *res, ViewWatcher *self);

    static void onObjectsAdded(ECalClientView *view, GSList *objects, ViewWatcher *self);
    static void onObjectsRemoved(ECalClientView *view, GSList *objects, ViewWatcher *self);
    static void onObjectsModified(ECalClientView *view, GSList *objects, ViewWatcher *self);
};

#endif
