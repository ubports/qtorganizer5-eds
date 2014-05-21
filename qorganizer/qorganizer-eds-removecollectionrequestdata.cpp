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
#include "qorganizer-eds-enginedata.h"
#include "qorganizer-eds-source-registry.h"

#include <QtOrganizer/QOrganizerManagerEngine>
#include <QtOrganizer/QOrganizerCollectionRemoveRequest>
#include <QtOrganizer/QOrganizerCollectionChangeSet>

using namespace QtOrganizer;

RemoveCollectionRequestData::RemoveCollectionRequestData(QOrganizerEDSEngine *engine, QtOrganizer::QOrganizerAbstractRequest *req)
    : RequestData(engine, req),
      m_currentCollection(0),
      m_remoteDeletable(false)
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

    QList<QOrganizerCollectionId> removedIds = m_pendingCollections;
    Q_FOREACH(int index, m_errorMap.keys()) {
        removedIds.removeAt(index);
    }

//    QOrganizerCollectionChangeSet cs;
//    cs.insertRemovedCollections(removedIds);
//    emitChangeset(&cs);
    RequestData::finish(error);
}

void RemoveCollectionRequestData::commit(QtOrganizer::QOrganizerManager::Error error)
{
    if (error != QOrganizerManager::NoError) {
        m_errorMap.insert(m_currentCollection, error);
    } else {
        QOrganizerCollectionId cId = m_pendingCollections.at(m_currentCollection);
        parent()->d->m_sourceRegistry->remove(cId.toString());
    }

    m_currentCollection++;
    m_remoteDeletable = false;
}

bool RemoveCollectionRequestData::remoteDeletable() const
{
    return m_remoteDeletable;
}

void RemoveCollectionRequestData::setRemoteDeletable(bool deletable)
{
    m_remoteDeletable = deletable;
}

ESource *RemoveCollectionRequestData::begin()
{
    if (m_pendingCollections.count() > m_currentCollection) {
        QOrganizerCollectionId cId = m_pendingCollections.at(m_currentCollection);
        return parent()->d->m_sourceRegistry->source(cId.toString());
    } else {
        return 0;
    }
}
