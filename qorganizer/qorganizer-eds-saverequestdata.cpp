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

#include "qorganizer-eds-saverequestdata.h"
#include "qorganizer-eds-enginedata.h"

#include <QtOrganizer/QOrganizerManagerEngine>
#include <QtOrganizer/QOrganizerItemSaveRequest>

using namespace QtOrganizer;

SaveRequestData::SaveRequestData(QOrganizerEDSEngine *engine,
                                 QtOrganizer::QOrganizerAbstractRequest *req,
                                 QOrganizerCollectionId collectionId)
    : RequestData(engine, req),
      m_collectionId(collectionId)
{
}

SaveRequestData::~SaveRequestData()
{
}

void SaveRequestData::finish(QtOrganizer::QOrganizerManager::Error error)
{
    QOrganizerManagerEngine::updateItemSaveRequest(request<QOrganizerItemSaveRequest>(),
                                                   m_result,
                                                   error,
                                                   QMap<int, QOrganizerManager::Error>(),
                                                   QOrganizerAbstractRequest::FinishedState);
    Q_FOREACH(QOrganizerItem item, m_result) {
        m_changeSet.insertAddedItem(item.id());
    }
    emitChangeset(&m_changeSet);
}

void SaveRequestData::appendResults(QList<QOrganizerItem> result)
{
    m_result += result;
}

QOrganizerCollectionId SaveRequestData::collectionId() const
{
    return m_collectionId;
}

bool SaveRequestData::isNew() const
{
    QList<QOrganizerItem> items = request<QOrganizerItemSaveRequest>()->items();
    if (items.count() > 0) {
        return items[0].id().isNull();
    }
    return false;
}
