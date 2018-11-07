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

#include <evolution-data-server-ubuntu/e-source-ubuntu.h>

using namespace QtOrganizer;

SaveCollectionRequestData::SaveCollectionRequestData(QOrganizerEDSEngine *engine,
                                                     QtOrganizer::QOrganizerAbstractRequest *req)
    : RequestData(engine, req),
      m_currentSources(0),
      m_registry(0),
      m_finishWasCalled(false)
{
    parseCollections();
}

SaveCollectionRequestData::~SaveCollectionRequestData()
{
    QObject::disconnect(m_registryConnection);

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
    if (error == QtOrganizer::QOrganizerManager::NoError &&
        !m_sourcesToCreate.isEmpty()) {
        // Not all sources have been created yet; let's wait
        m_finishWasCalled = true;
        return;
    }

    QOrganizerManagerEngine::updateCollectionSaveRequest(request<QOrganizerCollectionSaveRequest>(),
                                                         m_results.values(),
                                                         error,
                                                         m_errorMap,
                                                         state);

    RequestData::finish(error, state);
}

void SaveCollectionRequestData::commitSourceUpdated(ESource *source,
                                                    QOrganizerManager::Error error)
{
    int index = m_sourcesToUpdate.firstKey();
    m_sourcesToUpdate.remove(index);

    if (error == QOrganizerManager::NoError) {
        bool isDefault = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(source), "is-default"));
        QOrganizerCollection collection = SourceRegistry::parseSource(parent()->managerUri(),
                                                                      source, isDefault);
        m_results.insert(index, collection);

        if (isDefault) {
            SourceRegistry *registry = parent()->d->m_sourceRegistry;
            Q_ASSERT(registry);
            registry->setDefaultCollection(collection);
        }
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

void SaveCollectionRequestData::onSourceCreated(ESource *newSource)
{
    bool createdByUs = false;
    for (auto i = m_sourcesToCreate.begin();
         i != m_sourcesToCreate.end();
         i++) {
        ESource *source = *i;
        if (e_source_equal(source, newSource)) {
            createdByUs = true;
            m_sourcesToCreate.erase(i);
            break;
        }
    }

    if (!createdByUs) return;

    SourceRegistry *registry = parent()->d->m_sourceRegistry;
    Q_ASSERT(registry);
    QOrganizerCollection collection = registry->collection(newSource);
    m_results.insert(m_sources.key(newSource), collection);

    if (m_sourcesToCreate.isEmpty() && m_finishWasCalled) {
        finish();
    }
}

void SaveCollectionRequestData::parseCollections()
{
    m_sources.clear();
    m_errorMap.clear();

    SourceRegistry *registry = m_parent->d->m_sourceRegistry;
    m_registryConnection =
        QObject::connect(registry, &SourceRegistry::sourceAdded,
                         [this,registry](const QByteArray &sourceId) {
            onSourceCreated(registry->source(sourceId));
        });

    int index = 0;
    Q_FOREACH(const QOrganizerCollection &collection, request<QOrganizerCollectionSaveRequest>()->collections()) {
        ESource *source = 0;
        bool isNew = collection.id().isNull();
        if (isNew) {
            source = SourceRegistry::newSourceFromCollection(collection);
        } else {
            source = registry->source(collection.id().localId());
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

            // Ubuntu extension
            QVariant accountId = collection.extendedMetaData(COLLECTION_ACCOUNT_ID_METADATA);
            QVariant readOnly = collection.extendedMetaData(COLLECTION_SYNC_READONLY_METADATA);
            QVariant metadata = collection.extendedMetaData(COLLECTION_DATA_METADATA);
            if (accountId.isValid() ||
                readOnly.isValid() ||
                metadata.isValid()) {

                ESourceUbuntu *extUbuntu = E_SOURCE_UBUNTU(e_source_get_extension(source, E_SOURCE_EXTENSION_UBUNTU));
                e_source_ubuntu_set_writable(extUbuntu, readOnly.isValid() ? !readOnly.toBool() : true);
                e_source_ubuntu_set_account_id(extUbuntu, accountId.toUInt());
                e_source_ubuntu_set_metadata(extUbuntu, metadata.toString().toUtf8().constData());
                e_source_ubuntu_set_autoremove(extUbuntu, TRUE);
            }

            m_sources.insert(index, source);
            if (isNew) {
                registry->expectSourceCreation(source);
                m_sourcesToCreate.insert(index, source);
            } else {
                m_sourcesToUpdate.insert(index, source);
            }
            index++;
        }
    }
}
