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

#include <QtOrganizer/QOrganizerAbstractRequest>
#include <QtOrganizer/QOrganizerManagerEngine>

using namespace QtOrganizer;

RequestData::RequestData(QOrganizerEDSEngine *engine, QtOrganizer::QOrganizerAbstractRequest *req)
    : m_parent(engine),
      m_req(req),
      m_client(0)
{
    QOrganizerManagerEngine::updateRequestState(req, QOrganizerAbstractRequest::ActiveState);
    m_cancellable = g_cancellable_new();
    m_parent->m_runningRequests.insert(req, this);
}

RequestData::~RequestData()
{
    Q_ASSERT(m_req->state() != QOrganizerAbstractRequest::ActiveState);
    if (m_cancellable) {
        g_clear_object(&m_cancellable);
    }

    if (m_client) {
        g_clear_object(&m_client);
    }

    m_parent->m_runningRequests.remove(m_req);
}

GCancellable* RequestData::cancellable() const
{
    g_cancellable_reset(m_cancellable);
    //g_object_ref(m_cancellable);
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
    QOrganizerManagerEngine::updateRequestState(m_req, QOrganizerAbstractRequest::CanceledState);
    if (m_cancellable) {
        g_cancellable_cancel(m_cancellable);
        m_parent->waitForRequestFinished(m_req, 0);
        g_object_unref(m_cancellable);
        m_cancellable = 0;
    }
}

bool RequestData::cancelled() const
{
    if (!m_req.isNull()) {
        return (m_req->state() == QOrganizerAbstractRequest::CanceledState);
    }
    return false;
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
