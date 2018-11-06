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

#include "qorganizer-eds-removerequestdata.h"
#include "qorganizer-eds-enginedata.h"

#include <QtOrganizer/QOrganizerManagerEngine>
#include <QtOrganizer/QOrganizerItemRemoveRequest>

using namespace QtOrganizer;

RemoveRequestData::RemoveRequestData(QOrganizerEDSEngine *engine, QtOrganizer::QOrganizerAbstractRequest *req)
    :RequestData(engine, req),
      m_sessionStaterd(0),
      m_currentCompIds(0)
{
    m_pendingItems = request<QOrganizerItemRemoveRequest>()->items();
    Q_FOREACH(const QOrganizerItem &item, m_pendingItems) {
        m_pendingCollections.insert(item.collectionId());
    }
}

RemoveRequestData::~RemoveRequestData()
{
}

GSList *RemoveRequestData::takeItemsIds(QOrganizerCollectionId collectionId)
{
    GSList *ids = 0;
    QList<QtOrganizer::QOrganizerItem> items = m_pendingItems;

    Q_FOREACH(const QOrganizerItem &item, items) {
        if (item.collectionId() == collectionId) {
            m_currentIds.append(item.id());

            ECalComponentId *id = QOrganizerEDSEngine::ecalComponentId(item.id());
            if (id) {
                ids = g_slist_append(ids, id);
            }
            m_pendingItems.removeAll(item);
        }
    }

    return ids;
}

void RemoveRequestData::finish(QOrganizerManager::Error error,
                               QOrganizerAbstractRequest::State state)
{
    e_client_refresh_sync(m_client, 0, 0);
    QOrganizerManagerEngine::updateItemRemoveRequest(request<QOrganizerItemRemoveRequest>(),
                                                     error,
                                                     QMap<int, QOrganizerManager::Error>(),
                                                     state);

    //The signal will be fired by the view watcher. Check ViewWatcher::onObjectsRemoved
    //emitChangeset(&m_changeSet);
    RequestData::finish(error, state);
}

GSList *RemoveRequestData::compIds() const
{
    return m_currentCompIds;
}

void RemoveRequestData::commit()
{
    Q_ASSERT(m_sessionStaterd);
    QOrganizerManagerEngine::updateItemRemoveRequest(request<QOrganizerItemRemoveRequest>(),
                                                     QtOrganizer::QOrganizerManager::NoError,
                                                     QMap<int, QOrganizerManager::Error>(),
                                                     QOrganizerAbstractRequest::ActiveState);
    reset();
}

QOrganizerCollectionId RemoveRequestData::next()
{
    Q_ASSERT(!m_sessionStaterd);
    if (m_pendingCollections.count() > 0) {
        m_sessionStaterd = true;
        QSet<QtOrganizer::QOrganizerCollectionId>::const_iterator i = m_pendingCollections.constBegin();
        m_currentCollectionId = *i;
        m_pendingCollections.remove(*i);
        m_currentCompIds = takeItemsIds(m_currentCollectionId);
        return m_currentCollectionId;
    }
    return QOrganizerCollectionId();
}

void RemoveRequestData::cancel()
{
    Q_ASSERT(m_sessionStaterd);
    RequestData::cancel();
    clear();
}

void RemoveRequestData::reset()
{
    m_currentCollectionId = QOrganizerCollectionId();
    m_currentIds.clear();

    if (m_currentCompIds) {
        g_slist_free_full(m_currentCompIds, (GDestroyNotify)e_cal_component_free_id);
        m_currentCompIds = 0;
    }
    m_sessionStaterd = false;
}

void RemoveRequestData::clear()
{
    reset();
    setClient(0);
}
