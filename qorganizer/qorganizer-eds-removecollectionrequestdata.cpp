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

#include "qorganizer-eds-removecollectionrequestdata.h"
#include "qorganizer-eds-engineid.h"

#include <QtOrganizer/QOrganizerManagerEngine>
#include <QtOrganizer/QOrganizerCollectionRemoveRequest>

using namespace QtOrganizer;

RemoveCollectionRequestData::RemoveCollectionRequestData(QOrganizerEDSEngine *engine, QtOrganizer::QOrganizerAbstractRequest *req)
    : RequestData(engine, req),
      m_currentCollection(0)
{
    m_pendingCollections = request<QOrganizerCollectionRemoveRequest>()->collectionIds();
}

RemoveCollectionRequestData::~RemoveCollectionRequestData()
{
}

void RemoveCollectionRequestData::finish(QtOrganizer::QOrganizerManager::Error error)
{
    QOrganizerManagerEngine::updateCollectionRemoveRequest(request<QOrganizerCollectionRemoveRequest>(),
                                                           error,
                                                           m_errorMap,
                                                           QOrganizerAbstractRequest::FinishedState);

    // emit collection removed signal
    QList<QOrganizerCollectionId> removedIds = m_pendingCollections;
    Q_FOREACH(int index, m_errorMap.keys()) {
        removedIds.removeAt(index);
    }

    // remove source from engine
    Q_FOREACH(QOrganizerCollectionId id, removedIds) {
        parent()->unregisterCollection(id);
    }

    Q_EMIT parent()->collectionsRemoved(removedIds);
}

void RemoveCollectionRequestData::commit(QtOrganizer::QOrganizerManager::Error error)
{
    if (error != QOrganizerManager::NoError) {
        m_errorMap.insert(m_currentCollection, error);
    }

    m_currentCollection++;
}

ESource *RemoveCollectionRequestData::begin()
{
    if (m_pendingCollections.count() > m_currentCollection) {
        QOrganizerCollectionId cId = m_pendingCollections.at(m_currentCollection);
        return parent()->findSource(cId);
    } else {
        return 0;
    }
}
