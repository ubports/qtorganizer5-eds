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

#include "qorganizer-eds-savecollectionrequestdata.h"

#include <QtOrganizer/QOrganizerManagerEngine>
#include <QtOrganizer/QOrganizerCollectionSaveRequest>

using namespace QtOrganizer;

#define COLLECTION_CALLENDAR_TYPE_METADATA "collection-type"

SaveCollectionRequestData::SaveCollectionRequestData(QOrganizerEDSEngine *engine,
                                                     QtOrganizer::QOrganizerAbstractRequest *req)
    : RequestData(engine, req),
      m_sources(0),
      m_currentSource(0)
{
    parseCollections();
}

SaveCollectionRequestData::~SaveCollectionRequestData()
{
}


void SaveCollectionRequestData::finish(QtOrganizer::QOrganizerManager::Error error)
{
    qDebug() << "update request collections" << m_results;

    QOrganizerManagerEngine::updateCollectionSaveRequest(request<QOrganizerCollectionSaveRequest>(),
                                                         m_results,
                                                         error,
                                                         m_errorMap,
                                                         QOrganizerAbstractRequest::FinishedState);

    QList<QOrganizerCollectionId> added;
    Q_FOREACH(QOrganizerCollection col, m_results) {
        added.append(col.id());
    }
    Q_EMIT parent()->collectionsAdded(added);
}

void SaveCollectionRequestData::commit(QtOrganizer::QOrganizerManager::Error error)
{
    if (error != QOrganizerManager::NoError) {
        m_errorMap.insert(m_currentSource, error);
    } else {
        ESource *source = E_SOURCE(g_list_nth_data(m_sources, m_currentSource));
        QOrganizerEDSCollectionEngineId *edsId = 0;

        QOrganizerCollection collection = QOrganizerEDSEngine::parseSource(source, parent()->managerUri(), &edsId);
        parent()->registerCollection(collection, edsId);

        m_results.append(collection);
    }
    m_currentSource++;
}

GList *SaveCollectionRequestData::sources() const
{
    return m_sources;
}

QList<QOrganizerCollection> SaveCollectionRequestData::results() const
{
    return m_results;
}

void SaveCollectionRequestData::parseCollections()
{
    if (m_sources) {
        g_list_free_full(m_sources, g_object_unref);
        m_sources = 0;
    }

    m_errorMap.clear();
    int index = 0;
    Q_FOREACH(QOrganizerCollection collection, request<QOrganizerCollectionSaveRequest>()->collections()) {
        ESource *source = 0;
        if (collection.id().isNull()) {
            GError *gError = 0;
            source = e_source_new(0, 0, &gError);
            if (gError) {
                m_errorMap.insert(index, QOrganizerManager::UnspecifiedError);
                qWarning() << "Fail to create source:" << gError->message;
                g_error_free(gError);
            }
        } else {
            qDebug() << "Collection update not implemented";
            Q_ASSERT(FALSE);
        }

        QString name = collection.metaData(QOrganizerCollection::KeyName).toString();
        e_source_set_display_name(source, name.toUtf8().data());
        e_source_set_parent(source,  "local-stub");

        QVariant callendarType = collection.extendedMetaData(COLLECTION_CALLENDAR_TYPE_METADATA);
        ESourceBackend *extCalendar = 0;

        if (callendarType.toString() == E_SOURCE_EXTENSION_TASK_LIST) {
            extCalendar = E_SOURCE_BACKEND(e_source_get_extension(source, E_SOURCE_EXTENSION_TASK_LIST));
        } else if (callendarType.toString() == E_SOURCE_EXTENSION_MEMO_LIST) {
            extCalendar = E_SOURCE_BACKEND(e_source_get_extension(source, E_SOURCE_EXTENSION_MEMO_LIST));
        } else {
            extCalendar = E_SOURCE_BACKEND(e_source_get_extension(source, E_SOURCE_EXTENSION_CALENDAR));
        }

        if (extCalendar) {
            e_source_backend_set_backend_name(extCalendar, "local");
        } else {
            qWarning() << "Fail to get source callendar";
        }
        m_sources = g_list_append(m_sources, source);
        index++;
    }

    qDebug() << "Request with" << g_list_length(m_sources) << "sources";
}

ESource *SaveCollectionRequestData::begin() const
{
    return E_SOURCE(g_list_nth_data(m_sources, m_currentSource));
}
