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

#ifndef __QORGANIZER_EDS_REQUESTDATA_H__
#define __QORGANIZER_EDS_REQUESTDATA_H__

#include "qorganizer-eds-engine.h"
#include "qorganizer-eds-enginedata.h"

#include <QtCore/QPointer>
#include <QtCore/QMutex>
#include <QtCore/QEventLoop>

#include <QtOrganizer/QOrganizerAbstractRequest>
#include <QtOrganizer/QOrganizerManager>
#include <QtOrganizer/QOrganizerItemChangeSet>

class RequestData
{
public:
    RequestData(QOrganizerEDSEngine *engine, QtOrganizer::QOrganizerAbstractRequest *req);
    GCancellable* cancellable() const;
    bool isLive() const;
    void setClient(EClient *client);
    ECalClient *client() const;
    QOrganizerEDSEngine *parent() const;
    virtual void cancel();
    void deleteLater();
    virtual void finish(QtOrganizer::QOrganizerManager::Error error, QtOrganizer::QOrganizerAbstractRequest::State state) = 0;
    void wait(int msec=0);
    bool isWaiting();

    template<class T>
    T* request() const {
        return qobject_cast<T*>(m_req.data());
    }

    template<class T>
    void emitChangeset(T *cs) {
        if (!m_parent.isNull()) {
             m_parent->d->emitSharedSignals<T>(cs);
        }
    }

protected:
    QPointer<QOrganizerEDSEngine> m_parent;
    EClient *m_client;
    QtOrganizer::QOrganizerItemChangeSet m_changeSet;
    QMutex m_waiting;
    bool m_finished;

    virtual ~RequestData();

private:
    QPointer<QtOrganizer::QOrganizerAbstractRequest> m_req;
    GCancellable *m_cancellable;
};

#endif
