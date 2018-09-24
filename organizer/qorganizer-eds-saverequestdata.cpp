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

#define UPDATE_MODE_PROPRETY        "update-mode"

using namespace QtOrganizer;

SaveRequestData::SaveRequestData(QOrganizerEDSEngine *engine,
                                 QtOrganizer::QOrganizerAbstractRequest *req)
    : RequestData(engine, req)
{
    // map items by source
    Q_FOREACH(const QOrganizerItem &i, request<QOrganizerItemSaveRequest>()->items()) {
        QByteArray sourceId = i.collectionId().localId();
        QList<QOrganizerItem> li = m_items[sourceId];
        li << i;
        m_items.insert(sourceId, li);
    }
}

SaveRequestData::~SaveRequestData()
{
}

void SaveRequestData::finish(QtOrganizer::QOrganizerManager::Error error,
                             QtOrganizer::QOrganizerAbstractRequest::State state)
{
    e_client_refresh_sync(m_client, 0, 0);
    QOrganizerManagerEngine::updateItemSaveRequest(request<QOrganizerItemSaveRequest>(),
                                                   m_result,
                                                   error,
                                                   m_erros,
                                                   state);
    // Change will be fired by the viewwatcher
    RequestData::finish(error, state);
}

void SaveRequestData::appendResults(QList<QOrganizerItem> result)
{
    m_result += result;
}

QByteArray SaveRequestData::nextSourceId()
{
    if (m_items.isEmpty()) {
        m_currentSourceId = QByteArray();
        m_currentItems.clear();
    } else {
        m_currentSourceId = m_items.keys().first();
        m_currentItems = m_items.take(m_currentSourceId);
    }
    m_workingItems.clear();
    return m_currentSourceId;
}

QByteArray SaveRequestData::currentSourceId() const
{
    return m_currentSourceId;
}

QList<QOrganizerItem> SaveRequestData::takeItemsToCreate()
{
    QList<QOrganizerItem> result;

    Q_FOREACH(const QOrganizerItem &i, m_currentItems) {
        if (i.id().isNull()) {
            result << i;
            m_currentItems.removeAll(i);
        }
    }
    return result;
}

QList<QOrganizerItem> SaveRequestData::takeItemsToUpdate()
{
    QList<QOrganizerItem> result;

    Q_FOREACH(const QOrganizerItem &i, m_currentItems) {
        if (!i.id().isNull()) {
            result << i;
            m_currentItems.removeAll(i);
        }
    }
    return result;
}

bool SaveRequestData::end() const
{
    return m_items.isEmpty();
}

void SaveRequestData::appendResult(const QOrganizerItem &item, QOrganizerManager::Error error)
{
    if (error != QOrganizerManager::NoError) {
        int index = request<QOrganizerItemSaveRequest>()->items().indexOf(item);
        if (index != -1) {
            m_erros.insert(index, error);
        }
    } else {
        m_result << item;
    }
}

void SaveRequestData::setWorkingItems(QList<QOrganizerItem> items)
{
    m_workingItems = items;
}

QList<QOrganizerItem> SaveRequestData::workingItems() const
{
    return m_workingItems;
}

int SaveRequestData::updateMode() const
{
    // due the lack of API we will use the QObject proprety "update-mode" to allow specify wich kind of
    // update the developer want
    QOrganizerItemSaveRequest *req = request<QOrganizerItemSaveRequest>();
    QVariant updateMode = req->property(UPDATE_MODE_PROPRETY);
    if (updateMode.isValid()) {
        return updateMode.toInt();
    } else {
        return -1;
    }
}
