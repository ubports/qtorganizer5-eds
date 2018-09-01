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

#include "qorganizer-eds-removebyidrequestdata.h"
#include "qorganizer-eds-enginedata.h"

#include <QtOrganizer/QOrganizerManagerEngine>
#include <QtOrganizer/QOrganizerItemRemoveByIdRequest>

using namespace QtOrganizer;

RemoveByIdRequestData::RemoveByIdRequestData(QOrganizerEDSEngine *engine, QtOrganizer::QOrganizerAbstractRequest *req)
    : RequestData(engine, req),
      m_sessionStaterd(0),
      m_currentCompIds(0)
{
    Q_FOREACH(const QOrganizerItemId &id, request<QOrganizerItemRemoveByIdRequest>()->itemIds()) {
        QString strId = id.toString();
        QString collectionId;
        if (strId.contains("/")) {
            collectionId = strId.split("/").first();
            QSet<QOrganizerItemId> ids = m_pending.value(collectionId);
            ids << id;
            m_pending.insert(collectionId, ids);
        }
    }
}

RemoveByIdRequestData::~RemoveByIdRequestData()
{
}

void RemoveByIdRequestData::finish(QtOrganizer::QOrganizerManager::Error error,
                                   QtOrganizer::QOrganizerAbstractRequest::State state)
{
    e_client_refresh_sync(m_client, 0, 0);
    QOrganizerManagerEngine::updateItemRemoveByIdRequest(request<QOrganizerItemRemoveByIdRequest>(),
                                                         error,
                                                         QMap<int, QOrganizerManager::Error>(),
                                                         state);
    //The signal will be fired by the view watcher. Check ViewWatcher::onObjectsRemoved
    //emitChangeset(&m_changeSet);

    RequestData::finish(error, state);
}

GSList *RemoveByIdRequestData::compIds() const
{
    return m_currentCompIds;
}

void RemoveByIdRequestData::commit()
{
    Q_ASSERT(m_sessionStaterd);
    QOrganizerManagerEngine::updateItemRemoveByIdRequest(request<QOrganizerItemRemoveByIdRequest>(),
                                                         QtOrganizer::QOrganizerManager::NoError,
                                                         QMap<int, QOrganizerManager::Error>(),
                                                         QOrganizerAbstractRequest::ActiveState);
    reset();
}

GSList *RemoveByIdRequestData::parseIds(QSet<QOrganizerItemId> iids)
{
    GSList *ids = 0;
    Q_FOREACH(const QOrganizerItemId &iid, iids) {
        ECalComponentId *id = QOrganizerEDSEngine::ecalComponentId(iid);
        if (id) {
            ids = g_slist_append(ids, id);
        }
    }

    return ids;
}

QString RemoveByIdRequestData::next()
{
    Q_ASSERT(!m_sessionStaterd);

    if (m_pending.count() > 0) {
        m_sessionStaterd = true;
        m_currentCollectionId = m_pending.keys().first();
        m_currentIds = m_pending[m_currentCollectionId];
        m_currentCompIds = parseIds(m_currentIds);
        m_pending.remove(m_currentCollectionId);
        return m_currentCollectionId;
    }
    return QString(QString::null);
}


void RemoveByIdRequestData::reset()
{
    m_currentIds.clear();
    m_currentCollectionId = QString(QString::null);
    if (m_currentCompIds) {
        g_slist_free_full(m_currentCompIds, (GDestroyNotify)e_cal_component_free_id);
        m_currentCompIds = 0;
    }
    m_sessionStaterd = false;
}

void RemoveByIdRequestData::cancel()
{
    Q_ASSERT(m_sessionStaterd);
    RequestData::cancel();
    clear();
}

void RemoveByIdRequestData::clear()
{
    reset();
    m_pending.clear();
    setClient(0);
}

QString RemoveByIdRequestData::collectionId() const
{
    return m_currentCollectionId;
}
