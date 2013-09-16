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
#include "qorganizer-eds-engineid.h"

#include <QtOrganizer/QOrganizerManagerEngine>
#include <QtOrganizer/QOrganizerItemRemoveRequest>

using namespace QtOrganizer;

RemoveRequestData::RemoveRequestData(QOrganizerEDSEngine *engine, QtOrganizer::QOrganizerAbstractRequest *req)
    :RequestData(engine, req),
      m_sessionStaterd(0),
      m_currentCompIds(0)
{
    m_pendingItems = request<QOrganizerItemRemoveRequest>()->items();
    Q_FOREACH(QOrganizerItem item, m_pendingItems) {
        m_pendingCollections.insert(item.collectionId());
    }
}

RemoveRequestData::~RemoveRequestData()
{
}

QList<QtOrganizer::QOrganizerCollectionId> RemoveRequestData::pendingCollections() const
{
    return m_pendingCollections.toList();
}

GSList *RemoveRequestData::takeItemsIds(QOrganizerCollectionId collectionId)
{
    GSList *ids = 0;
    QList<QtOrganizer::QOrganizerItem> items = m_pendingItems;

    Q_FOREACH(QOrganizerItem item, items) {
        if (item.collectionId() == collectionId) {
            m_currentIds.append(item.id());

            ECalComponentId *id = g_new0(ECalComponentId, 1);

            id->uid = g_strdup(QOrganizerEDSEngineId::toComponentId(item.id()).toUtf8().data());
            ids = g_slist_append(ids, id);

            m_pendingItems.removeAll(item);
        }
    }

    return ids;
}

void RemoveRequestData::finish(QtOrganizer::QOrganizerManager::Error error)
{
    QOrganizerManagerEngine::updateItemRemoveRequest(request<QOrganizerItemRemoveRequest>(),
                                                     error,
                                                     QMap<int, QOrganizerManager::Error>(),
                                                     QOrganizerAbstractRequest::FinishedState);

    m_changeSet.emitSignals(m_parent);
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

    m_changeSet.insertRemovedItems(m_currentIds);
    m_currentCollectionId = QOrganizerCollectionId();
    clear();
}

QOrganizerCollectionId RemoveRequestData::begin()
{
    Q_ASSERT(!m_sessionStaterd);
    if (m_pendingCollections.count() > 0) {
        m_sessionStaterd = true;
        QSet<QtOrganizer::QOrganizerCollectionId>::const_iterator i = m_pendingCollections.constBegin();
        m_pendingCollections.remove(*i);
        m_currentCollectionId = *i;
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

void RemoveRequestData::clear()
{
    m_changeSet.clearAll();
    m_currentIds.clear();

    setClient(0);
    if (m_currentCompIds) {
        g_slist_free_full(m_currentCompIds, (GDestroyNotify)e_cal_component_free_id);
        m_currentCompIds = 0;
    }
    m_sessionStaterd = false;
}

QOrganizerCollectionId RemoveRequestData::collectionId() const
{
    return m_currentCollectionId;
}
