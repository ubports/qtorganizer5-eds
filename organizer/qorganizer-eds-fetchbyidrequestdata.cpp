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

#include "qorganizer-eds-fetchbyidrequestdata.h"

#include <QtOrganizer/QOrganizerItemFetchByIdRequest>

using namespace QtOrganizer;

FetchByIdRequestData::FetchByIdRequestData(QOrganizerEDSEngine *engine,
                                           QOrganizerAbstractRequest *req)
    : RequestData(engine, req),
      m_current(-1)
{

}

FetchByIdRequestData::~FetchByIdRequestData()
{
}

QOrganizerItemId FetchByIdRequestData::nextId()
{
    QOrganizerItemId id;
    QList<QOrganizerItemId> ids = request<QOrganizerItemFetchByIdRequest>()->ids();
    m_current++;
    if (m_current < ids.count()) {
        id = ids[m_current];
    }
    return id;
}

QOrganizerItemId FetchByIdRequestData::currentId() const
{
    return request<QOrganizerItemFetchByIdRequest>()->ids()[m_current];
}

QByteArray FetchByIdRequestData::currentSourceId() const
{
    QOrganizerItemId itemId = currentId();
    if (!itemId.isNull()) {
        QByteArray sourceId;
        QOrganizerEDSEngine::idToEds(itemId, &sourceId);
        return sourceId;
    }
    return QByteArray();
}

bool FetchByIdRequestData::end() const
{
    QList<QOrganizerItemId> ids = request<QOrganizerItemFetchByIdRequest>()->ids();
    return (m_current >= ids.count());
}

void FetchByIdRequestData::finish(QOrganizerManager::Error error,
                                  QOrganizerAbstractRequest::State state)
{
    QOrganizerManagerEngine::updateItemFetchByIdRequest(request<QOrganizerItemFetchByIdRequest>(),
                                                        m_results,
                                                        error,
                                                        m_errors,
                                                        state);
    RequestData::finish(error, state);
}

int FetchByIdRequestData::appendResult(const QOrganizerItem &result)
{
    if (result.id().isNull()) {
        m_errors.insert(m_current, QOrganizerManager::DoesNotExistError);
    } else {
        m_results << result;
    }
    return m_results.length();
}
