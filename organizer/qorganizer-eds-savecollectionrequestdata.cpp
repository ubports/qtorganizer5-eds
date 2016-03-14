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
#include "qorganizer-eds-enginedata.h"
#include "qorganizer-eds-source-registry.h"

#include <QtCore/QDebug>

#include <QtOrganizer/QOrganizerManagerEngine>
#include <QtOrganizer/QOrganizerCollectionSaveRequest>
#include <QtOrganizer/QOrganizerCollectionChangeSet>

using namespace QtOrganizer;

SaveCollectionRequestData::SaveCollectionRequestData(QOrganizerEDSEngine *engine,
                                                     QtOrganizer::QOrganizerAbstractRequest *req)
    : RequestData(engine, req),
      m_currentSources(0),
      m_registry(0)
{
    parseCollections();
}

SaveCollectionRequestData::~SaveCollectionRequestData()
{
    if (m_registry) {
        g_object_unref(m_registry);
        m_registry = 0;
    }

    if (m_currentSources) {
        g_list_free_full(m_currentSources, g_object_unref);
        m_currentSources = 0;
    }
}

void SaveCollectionRequestData::finish(QtOrganizer::QOrganizerManager::Error error,
                                       QtOrganizer::QOrganizerAbstractRequest::State state)
{
    QOrganizerManagerEngine::updateCollectionSaveRequest(request<QOrganizerCollectionSaveRequest>(),
                                                         m_results.values(),
                                                         error,
                                                         m_errorMap,
                                                         state);


    // changes will be fired by source-registry
    m_changeSet.clearAll();

    RequestData::finish(error, state);
}

void SaveCollectionRequestData::commitSourceCreated()
{
    GList *i = g_list_first(m_currentSources);

    for(; i != 0; i = i->next) {
        ESource *source = E_SOURCE(i->data);
        SourceRegistry *registry = parent()->d->m_sourceRegistry;
        QOrganizerCollection collection =registry->insert(source);
        bool isDefault = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(source), "is-default"));
        if (isDefault) {
            SourceRegistry *registry = parent()->d->m_sourceRegistry;
            Q_ASSERT(registry);
            registry->setDefaultCollection(collection);
        }
        m_results.insert(m_sources.key(source), collection);
        m_changeSet.insertAddedCollection(collection.id());
    }
}

void SaveCollectionRequestData::commitSourceUpdated(ESource *source,
                                                    QOrganizerManager::Error error)
{
    int index = m_sourcesToUpdate.firstKey();
    m_sourcesToUpdate.remove(index);

    if (error == QOrganizerManager::NoError) {
        QOrganizerEDSCollectionEngineId *id;
        bool isDefault = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(source), "is-default"));
        QOrganizerCollection collection = SourceRegistry::parseSource(source, isDefault, &id);
        m_results.insert(index, collection);

        if (isDefault) {
            SourceRegistry *registry = parent()->d->m_sourceRegistry;
            Q_ASSERT(registry);
            registry->setDefaultCollection(collection);
        }

        m_changeSet.insertChangedCollection(collection.id());
    } else {
        m_errorMap.insert(index, error);
    }
}

ESource *SaveCollectionRequestData::nextSourceToUpdate()
{
    if (m_sourcesToUpdate.size() > 0) {
        return m_sourcesToUpdate.first();
    } else {
        return 0;
    }
}

bool SaveCollectionRequestData::prepareToCreate()
{
    Q_FOREACH(ESource *source, m_sourcesToCreate.values()) {
        m_currentSources = g_list_append(m_currentSources, source);
    }
    return (g_list_length(m_currentSources) > 0);
}

bool SaveCollectionRequestData::prepareToUpdate()
{
    return (m_sourcesToUpdate.size() > 0);
}

void SaveCollectionRequestData::setRegistry(ESourceRegistry *registry)
{
    if (m_registry) {
        g_object_unref(m_registry);
        m_registry = 0;
    }
    if (registry) {
        m_registry = registry;
        g_object_ref(m_registry);
    }
}

ESourceRegistry *SaveCollectionRequestData::registry() const
{
    return m_registry;
}

GList *SaveCollectionRequestData::sourcesToCreate() const
{
    return m_currentSources;
}

void SaveCollectionRequestData::parseCollections()
{
    m_sources.clear();
    m_errorMap.clear();

    int index = 0;
    Q_FOREACH(const QOrganizerCollection &collection, request<QOrganizerCollectionSaveRequest>()->collections()) {
        ESource *source = 0;
        bool isNew = true;
        if (collection.id().isNull()) {
            GError *gError = 0;
            source = e_source_new(0, 0, &gError);
            if (gError) {
                m_errorMap.insert(index, QOrganizerManager::UnspecifiedError);
                qWarning() << "Fail to create source:" << gError->message;
                g_error_free(gError);
                Q_ASSERT(false);
            }
            e_source_set_parent(source, "local-stub");
        } else {
            source = m_parent->d->m_sourceRegistry->source(collection.id().toString());
            isNew = false;
        }

        QVariant callendarType = collection.extendedMetaData(COLLECTION_CALLENDAR_TYPE_METADATA);
        ESourceBackend *extCalendar = 0;

        if (callendarType.toString() == E_SOURCE_EXTENSION_TASK_LIST) {
            extCalendar = E_SOURCE_BACKEND(e_source_get_extension(source, E_SOURCE_EXTENSION_TASK_LIST));
        } else if (callendarType.toString() == E_SOURCE_EXTENSION_MEMO_LIST) {
            extCalendar = E_SOURCE_BACKEND(e_source_get_extension(source, E_SOURCE_EXTENSION_MEMO_LIST));
        } else {
            extCalendar = E_SOURCE_BACKEND(e_source_get_extension(source, E_SOURCE_EXTENSION_CALENDAR));
        }


        if (source) {
            if (isNew) {
                if (extCalendar) {
                    e_source_backend_set_backend_name(extCalendar, "local");
                } else {
                    qWarning() << "Fail to get source callendar";
                }
            }

            // update name
            QString name = collection.metaData(QOrganizerCollection::KeyName).toString();
            e_source_set_display_name(source, name.toUtf8().constData());

            // update color
            QString color = collection.metaData(QOrganizerCollection::KeyColor).toString();
            e_source_selectable_set_color(E_SOURCE_SELECTABLE(extCalendar), color.toUtf8().constData());

            // update selected
            bool selected = collection.extendedMetaData(COLLECTION_SELECTED_METADATA).toBool();
            e_source_selectable_set_selected(E_SOURCE_SELECTABLE(extCalendar), selected);

            // default collection
            bool isDefault = collection.extendedMetaData(COLLECTION_DEFAULT_METADATA).toBool();
            g_object_set_data(G_OBJECT(source), "is-default", GINT_TO_POINTER(isDefault));

            m_sources.insert(index, source);
            if (isNew) {
                m_sourcesToCreate.insert(index, source);
            } else {
                m_sourcesToUpdate.insert(index, source);
            }
            index++;
        }
    }
}

