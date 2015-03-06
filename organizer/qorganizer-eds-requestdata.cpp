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
      m_finished(false),
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
    if (m_cancellable) {
        g_cancellable_cancel(m_cancellable);
    }

    if (isLive()) {
        finish(QOrganizerManager::UnspecifiedError,
               QOrganizerAbstractRequest::CanceledState);
    }
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

void RequestData::deleteLater()
{
    if (isWaiting()) {
        // still running
        return;
    }
    if (!m_parent.isNull()) {
        m_parent->m_runningRequests.remove(m_req);
    }
    delete this;
}

void RequestData::finish(QOrganizerManager::Error error,
                         QtOrganizer::QOrganizerAbstractRequest::State state)
{
    Q_UNUSED(error);
    Q_UNUSED(state);
    m_finished = true;
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
