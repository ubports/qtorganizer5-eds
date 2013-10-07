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
    m_cancellable = g_cancellable_new();
}

RequestData::~RequestData()
{
    if (m_cancellable) {
        g_cancellable_cancel(m_cancellable);
        g_object_unref(m_cancellable);
    }


    if (m_client) {
        g_object_unref(m_client);
    }
}

GCancellable* RequestData::cancellable() const
{
    return m_cancellable;
}

bool RequestData::isLive() const
{
    return !m_req.isNull();
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
        g_object_unref(m_cancellable);
        m_cancellable = 0;
    }
    QOrganizerManagerEngine::updateRequestState(m_req, QOrganizerAbstractRequest::CanceledState);
}

void RequestData::setClient(EClient *client)
{
    if (m_client == client) {
        return;
    }

    if (m_client) {
        g_object_unref(m_client);
        m_client = 0;
    }

    if (client) {
        m_client = client;
        g_object_ref(m_client);
    }
}
