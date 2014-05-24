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

#include "qorganizer-eds-requestdata.h"

#include <QtCore/QDebug>
#include <QtCore/QCoreApplication>

#include <QtOrganizer/QOrganizerAbstractRequest>
#include <QtOrganizer/QOrganizerManagerEngine>

using namespace QtOrganizer;

RequestData::RequestData(QOrganizerEDSEngine *engine, QtOrganizer::QOrganizerAbstractRequest *req)
    : m_parent(engine),
      m_client(0),
      m_req(req)
{
    QOrganizerManagerEngine::updateRequestState(req, QOrganizerAbstractRequest::ActiveState);
    m_cancellable = g_cancellable_new();
    m_parent->m_runningRequests.insert(req, this);
}

RequestData::~RequestData()
{
    if (m_cancellable) {
        g_clear_object(&m_cancellable);
    }

    if (m_client) {
        g_clear_object(&m_client);
    }
}

GCancellable* RequestData::cancellable() const
{
    return m_cancellable;
}

bool RequestData::isLive() const
{
    return (!m_req.isNull() &&
            (m_req->state() == QOrganizerAbstractRequest::ActiveState));
}

ECalClient *RequestData::client() const
{
    return E_CAL_CLIENT(m_client);
}

QOrganizerEDSEngine *RequestData::parent() const
{
    return m_parent;
}

void RequestData::cancel()
{
    QMutexLocker locker(&m_canceling);
    if (m_cancellable) {
        gulong id = g_cancellable_connect(m_cancellable,
                                          (GCallback) RequestData::onCancelled,
                                          this, NULL);
        // wait the cancel
        wait();

        // cancel
        g_cancellable_cancel(m_cancellable);

        // done
        g_cancellable_disconnect(m_cancellable, id);
        m_cancellable = 0;
    }
}

bool RequestData::cancelled()
{
    if (!m_req.isNull()) {
        return isCanceling();
    }
    return false;
}

void RequestData::wait()
{
    QMutexLocker locker(&m_waiting);
    while(!m_finished) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    }
}

bool RequestData::isWaiting()
{
    bool result = true;
    if (m_waiting.tryLock()) {
        result = false;
        m_waiting.unlock();
    }
    return result;
}

bool RequestData::isCanceling()
{
    bool result = true;
    if (m_canceling.tryLock()) {
        result = false;
        m_canceling.unlock();
    }
    return result;
}

void RequestData::deleteLater()
{
    if (!m_waiting.tryLock()) {
        // wait still running
        return;
    }
    m_waiting.unlock();
    m_parent->m_runningRequests.remove(m_req);
    delete this;
}

void RequestData::finish(QOrganizerManager::Error error)
{
    Q_UNUSED(error);
    m_finished = true;
}

bool RequestData::finished() const
{
    return m_finished;
}

gboolean RequestData::destroy(RequestData *self)
{
    delete self;
    return 0;
}

void RequestData::onCancelled(GCancellable *cancellable, RequestData *self)
{
    Q_UNUSED(cancellable);
    self->m_waiting.unlock();
}

void RequestData::setClient(EClient *client)
{
    if (m_client == client) {
        return;
    }
    if (m_client) {
        g_clear_object(&m_client);
    }
    if (client) {
        m_client = client;
        g_object_ref(m_client);
    }
}
