/*
 * Copyright 2013 Canonical Ltd.
 *
 * This file is part of canonical-pim-service.
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


#include "qorganizer-eds-engine.h"
#include "qorganizer-eds-fetchrequestdata.h"
#include "qorganizer-eds-fetchbyidrequestdata.h"
#include "qorganizer-eds-fetchocurrencedata.h"
#include "qorganizer-eds-saverequestdata.h"
#include "qorganizer-eds-removerequestdata.h"
#include "qorganizer-eds-removebyidrequestdata.h"
#include "qorganizer-eds-savecollectionrequestdata.h"
#include "qorganizer-eds-removecollectionrequestdata.h"
#include "qorganizer-eds-viewwatcher.h"
#include "qorganizer-eds-enginedata.h"
#include "qorganizer-eds-source-registry.h"
#include "qorganizer-eds-parseeventthread.h"

#include <QtCore/qdebug.h>
#include <QtCore/QPointer>
#include <QtCore/QTimeZone>

#include <QtOrganizer/QOrganizerEventAttendee>
#include <QtOrganizer/QOrganizerItemLocation>
#include <QtOrganizer/QOrganizerEventTime>
#include <QtOrganizer/QOrganizerItemFetchRequest>
#include <QtOrganizer/QOrganizerItemFetchByIdRequest>
#include <QtOrganizer/QOrganizerItemSaveRequest>
#include <QtOrganizer/QOrganizerItemRemoveRequest>
#include <QtOrganizer/QOrganizerItemRemoveByIdRequest>
#include <QtOrganizer/QOrganizerCollectionFetchRequest>
#include <QtOrganizer/QOrganizerCollectionSaveRequest>
#include <QtOrganizer/QOrganizerCollectionRemoveRequest>
#include <QtOrganizer/QOrganizerEvent>
#include <QtOrganizer/QOrganizerTodo>
#include <QtOrganizer/QOrganizerTodoTime>
#include <QtOrganizer/QOrganizerJournal>
#include <QtOrganizer/QOrganizerJournalTime>
#include <QtOrganizer/QOrganizerItemIdFetchRequest>
#include <QtOrganizer/QOrganizerItemIdFilter>
#include <QtOrganizer/QOrganizerItemReminder>
#include <QtOrganizer/QOrganizerItemAudibleReminder>
#include <QtOrganizer/QOrganizerItemVisualReminder>
#include <QtOrganizer/QOrganizerEventOccurrence>
#include <QtOrganizer/QOrganizerTodoOccurrence>
#include <QtOrganizer/QOrganizerItemParent>
#include <QtOrganizer/QOrganizerItemExtendedDetail>

#include <glib.h>
#include <libecal/libecal.h>
#include <libical/ical.h>

using namespace QtOrganizer;
QOrganizerEDSEngineData *QOrganizerEDSEngine::m_globalData = 0;

QOrganizerEDSEngine* QOrganizerEDSEngine::createEDSEngine(const QMap<QString, QString>& parameters)
{
    Q_UNUSED(parameters);
    if (!m_globalData) {
        m_globalData = new QOrganizerEDSEngineData();
        m_globalData->m_sourceRegistry = new SourceRegistry;
    }
    m_globalData->m_refCount.ref();
    return new QOrganizerEDSEngine(m_globalData);
}

QOrganizerEDSEngine::QOrganizerEDSEngine(QOrganizerEDSEngineData *data)
    : d(data)
{
    d->m_sharedEngines << this;

    Q_FOREACH(const QByteArray &sourceId, d->m_sourceRegistry->sourceIds()){
        onSourceAdded(sourceId);
    }
    QObject::connect(d->m_sourceRegistry, &SourceRegistry::sourceAdded,
                     this, &QOrganizerEDSEngine::onSourceAdded);
    QObject::connect(d->m_sourceRegistry, &SourceRegistry::sourceRemoved,
                     this, &QOrganizerEDSEngine::onSourceRemoved);
    QObject::connect(d->m_sourceRegistry, &SourceRegistry::sourceUpdated,
                     this, &QOrganizerEDSEngine::onSourceUpdated);
    d->m_sourceRegistry->load(managerUri());
}

QOrganizerEDSEngine::~QOrganizerEDSEngine()
{
    while(m_runningRequests.count()) {
        QOrganizerAbstractRequest *req = m_runningRequests.keys().first();
        req->cancel();
        QOrganizerEDSEngine::requestDestroyed(req);
    }

    d->m_sharedEngines.remove(this);
    if (!d->m_refCount.deref()) {
        delete d;
        m_globalData = 0;
    }
}

QString QOrganizerEDSEngine::managerName() const
{
    return QStringLiteral("eds");
}

void QOrganizerEDSEngine::itemsAsync(QOrganizerItemFetchRequest *req)
{
    FetchRequestData *data = new FetchRequestData(this,
                                                  d->m_sourceRegistry->sourceIds(),
                                                  req);
    // avoid query if the filter is invalid
    if (data->filterIsValid()) {
        itemsAsyncStart(data);
    } else {
        data->finish();
    }
}

void QOrganizerEDSEngine::itemsAsyncStart(FetchRequestData *data)
{
    // check if request was destroyed by the caller
    if (!data->isLive()) {
        releaseRequestData(data);
        return;
    }

    QByteArray sourceId = data->nextSourceId();
    if (!sourceId.isEmpty()) {
        EClient *client = data->parent()->d->m_sourceRegistry->client(sourceId);
        data->setClient(client);
        g_object_unref(client);

        if (data->hasDateInterval()) {
            e_cal_client_generate_instances(data->client(),
                                            data->startDate(),
                                            data->endDate(),
                                            data->cancellable(),
                                            (ECalRecurInstanceFn) QOrganizerEDSEngine::itemsAsyncListed,
                                            data,
                                            (GDestroyNotify) QOrganizerEDSEngine::itemsAsyncDone);
        } else {
            // if no date interval was set we return only the main events without recurrence
            e_cal_client_get_object_list_as_comps(E_CAL_CLIENT(client),
                                                  data->dateFilter().toUtf8().data(),
                                                  data->cancellable(),
                                                  (GAsyncReadyCallback) QOrganizerEDSEngine::itemsAsyncListedAsComps,
                                                  data);
        }
    } else {
        data->finish();
    }
}

void QOrganizerEDSEngine::itemsAsyncDone(FetchRequestData *data)
{
    if (data->isLive()) {
        data->compileCurrentIds();
        itemsAsyncFetchDeatachedItems(data);
    } else {
        releaseRequestData(data);
    }
}

void QOrganizerEDSEngine::itemsAsyncFetchDeatachedItems(FetchRequestData *data)
{
    QByteArray parentId = data->nextParentId();
    if (!parentId.isEmpty()) {
        e_cal_client_get_objects_for_uid(E_CAL_CLIENT(data->client()),
                                         parentId.data(),
                                         data->cancellable(),
                                         (GAsyncReadyCallback) QOrganizerEDSEngine::itemsAsyncListByIdListed,
                                         data);
    } else {
        itemsAsyncStart(data);
    }
}

void QOrganizerEDSEngine::itemsAsyncListByIdListed(GObject *source,
                                                   GAsyncResult *res,
                                                   FetchRequestData *data)
{
    Q_UNUSED(source);
    GError *gError = 0;
    GSList *events = 0;
    e_cal_client_get_objects_for_uid_finish(E_CAL_CLIENT(data->client()),
                                            res,
                                            &events,
                                            &gError);
    if (gError) {
        qWarning() << "Fail to list deatached events in calendar" << gError->message;
        g_error_free(gError);
        gError = 0;
        if (data->isLive()) {
            data->finish(QOrganizerManager::InvalidCollectionError);
        } else {
            releaseRequestData(data);
        }
        return;
    }

    for(GSList *e = events; e != NULL; e = e->next) {
        icalcomponent * ical = e_cal_component_get_icalcomponent(static_cast<ECalComponent*>(e->data));
        data->appendDeatachedResult(ical);
    }

    itemsAsyncFetchDeatachedItems(data);
}


gboolean QOrganizerEDSEngine::itemsAsyncListed(ECalComponent *comp,
                                               time_t instanceStart,
                                               time_t instanceEnd,
                                               FetchRequestData *data)
{
    Q_UNUSED(instanceStart);
    Q_UNUSED(instanceEnd);

    if (data->isLive()) {
        icalcomponent *icalComp = icalcomponent_new_clone(e_cal_component_get_icalcomponent(comp));
        if (icalComp) {
            data->appendResult(icalComp);
        }
        return TRUE;
    }
    return FALSE;
}

void QOrganizerEDSEngine::itemsAsyncListedAsComps(GObject *source,
                                                  GAsyncResult *res,
                                                  FetchRequestData *data)
{
    Q_UNUSED(source);
    GError *gError = 0;
    GSList *events = 0;
    e_cal_client_get_object_list_as_comps_finish(E_CAL_CLIENT(data->client()),
                                                 res,
                                                 &events,
                                                 &gError);
    if (gError) {
        qWarning() << "Fail to list events in calendar" << gError->message;
        g_error_free(gError);
        gError = 0;
        if (data->isLive()) {
            data->finish(QOrganizerManager::InvalidCollectionError);
        } else {
            releaseRequestData(data);
        }
        return;
    }

    // check if request was destroyed by the caller
    if (data->isLive()) {
        QOrganizerItemFetchRequest *req = data->request<QOrganizerItemFetchRequest>();
        if (req) {
            data->appendResults(data->parent()->parseEvents(data->sourceId(),
                                                            events,
                                                            false,
                                                            req->fetchHint().detailTypesHint()));
        }
        itemsAsyncStart(data);
    } else {
        releaseRequestData(data);
    }
}

void QOrganizerEDSEngine::itemsByIdAsync(QOrganizerItemFetchByIdRequest *req)
{
    FetchByIdRequestData *data = new FetchByIdRequestData(this, req);
    itemsByIdAsyncStart(data);
}

void QOrganizerEDSEngine::itemsByIdAsyncStart(FetchByIdRequestData *data)
{
    // check if request was destroyed by the caller
    if (!data->isLive()) {
        releaseRequestData(data);
        return;
    }

    QOrganizerItemId id = data->nextId();
    if (!id.isNull()) {
        QByteArray collectionId;
        QByteArray fullItemId = idToEds(id, &collectionId);
        QByteArray rId;
        QByteArray itemId = toComponentId(fullItemId, &rId);

        EClient *client = data->parent()->d->m_sourceRegistry->client(collectionId);
        if (client) {
            data->setClient(client);
            e_cal_client_get_object(data->client(),
                                    itemId.data(),
                                    rId.data(),
                                    data->cancellable(),
                                    (GAsyncReadyCallback) QOrganizerEDSEngine::itemsByIdAsyncListed,
                                    data);
            g_object_unref(client);
            return;
        }
    } else if (data->end()) {
        data->finish();
        return;
    }
    qWarning() << "Invalid item id" << id;
    data->appendResult(QOrganizerItem());
    itemsByIdAsyncStart(data);
}

void QOrganizerEDSEngine::itemsByIdAsyncListed(GObject *client,
                                               GAsyncResult *res,
                                               FetchByIdRequestData *data)
{
    Q_UNUSED(client);
    GError *gError = 0;
    icalcomponent *icalComp = 0;
    e_cal_client_get_object_finish(data->client(), res, &icalComp, &gError);
    if (gError) {
        qWarning() << "Fail to list events in calendar" << gError->message;
        g_error_free(gError);
        gError = 0;
        data->appendResult(QOrganizerItem());
    } else if (icalComp && data->isLive()) {
        GSList *events = g_slist_append(0, icalComp);
        QList<QOrganizerItem> items;
        QOrganizerItemFetchByIdRequest *req = data->request<QOrganizerItemFetchByIdRequest>();
        items = data->parent()->parseEvents(data->currentSourceId(),
                                            events,
                                            true,
                                            req->fetchHint().detailTypesHint());
        Q_ASSERT(items.size() == 1);
        data->appendResult(items[0]);
        g_slist_free_full(events, (GDestroyNotify) icalcomponent_free);
    }

    if (data->isLive()) {
        itemsByIdAsyncStart(data);
    } else {
        releaseRequestData(data);
    }
}

void QOrganizerEDSEngine::itemOcurrenceAsync(QOrganizerItemOccurrenceFetchRequest *req)
{
    FetchOcurrenceData *data = new FetchOcurrenceData(this, req);

    QByteArray rId;
    QByteArray edsItemId = idToEds(req->parentItem().id());
    QByteArray cId = toComponentId(edsItemId, &rId);

    EClient *client = data->parent()->d->m_sourceRegistry->client(req->parentItem().collectionId().localId());
    if (client) {
        data->setClient(client);
        e_cal_client_get_object(data->client(),
                                cId, rId,
                                data->cancellable(),
                                (GAsyncReadyCallback) QOrganizerEDSEngine::itemOcurrenceAsyncGetObjectDone,
                                data);
        g_object_unref(client);
    } else {
        qWarning() << "Fail to find collection:" << req->parentItem().collectionId();
        data->finish(QOrganizerManager::DoesNotExistError);
    }
}

void QOrganizerEDSEngine::itemOcurrenceAsyncGetObjectDone(GObject *source,
                                                          GAsyncResult *res,
                                                          FetchOcurrenceData *data)
{
    Q_UNUSED(source);
    GError *error = 0;
    icalcomponent *comp = 0;
    e_cal_client_get_object_finish(data->client(), res, &comp, &error);
    if (error) {
        qWarning() << "Fail to get object for id:" << data->request<QOrganizerItemOccurrenceFetchRequest>()->parentItem();
        g_error_free(error);
        if (data->isLive()) {
            data->finish(QOrganizerManager::DoesNotExistError);
        } else {
            releaseRequestData(data);
        }
        return;
    }

    if (data->isLive()) {
        e_cal_client_generate_instances_for_object(data->client(),
                                                   comp,
                                                   data->startDate(),
                                                   data->endDate(),
                                                   data->cancellable(),
                                                   (ECalRecurInstanceFn) QOrganizerEDSEngine::itemOcurrenceAsyncListed,
                                                   data,
                                                   (GDestroyNotify) QOrganizerEDSEngine::itemOcurrenceAsyncDone);
    } else {
        releaseRequestData(data);
    }
}

void QOrganizerEDSEngine::itemOcurrenceAsyncListed(ECalComponent *comp,
                                                   time_t instanceStart,
                                                   time_t instanceEnd,
                                                   FetchOcurrenceData *data)
{
    Q_UNUSED(instanceStart);
    Q_UNUSED(instanceEnd);

    // check if request was destroyed by the caller
    if (!data->isLive()) {
        releaseRequestData(data);
        return;
    }

    icalcomponent *icalComp = icalcomponent_new_clone(e_cal_component_get_icalcomponent(comp));
    if (icalComp) {
        data->appendResult(icalComp);
    }
}

void QOrganizerEDSEngine::itemOcurrenceAsyncDone(FetchOcurrenceData *data)
{
    if (data->isLive()) {
        data->finish();
    } else {
        releaseRequestData(data);
    }
}

QList<QOrganizerItem> QOrganizerEDSEngine::items(const QList<QOrganizerItemId> &itemIds,
                                                 const QOrganizerItemFetchHint &fetchHint,
                                                 QMap<int, QOrganizerManager::Error> *errorMap,
                                                 QOrganizerManager::Error *error)
{
    QOrganizerItemFetchByIdRequest *req = new QOrganizerItemFetchByIdRequest(this);
    req->setIds(itemIds);
    req->setFetchHint(fetchHint);

    startRequest(req);
    waitForRequestFinished(req, 0);

    if (error) {
        *error = req->error();
    }

    if (errorMap) {
        *errorMap = req->errorMap();
    }
    req->deleteLater();
    return req->items();
}

QList<QOrganizerItem> QOrganizerEDSEngine::items(const QOrganizerItemFilter &filter,
                                                 const QDateTime &startDateTime,
                                                 const QDateTime &endDateTime,
                                                 int maxCount,
                                                 const QList<QOrganizerItemSortOrder> &sortOrders,
                                                 const QOrganizerItemFetchHint &fetchHint,
                                                 QOrganizerManager::Error *error)
{
    QOrganizerItemFetchRequest *req = new QOrganizerItemFetchRequest(this);

    req->setFilter(filter);
    req->setStartDate(startDateTime);
    req->setEndDate(endDateTime);
    req->setMaxCount(maxCount);
    req->setSorting(sortOrders);
    req->setFetchHint(fetchHint);

    startRequest(req);
    waitForRequestFinished(req, 0);

    if (error) {
        *error = req->error();
    }

    req->deleteLater();
    return req->items();
}

QList<QOrganizerItemId> QOrganizerEDSEngine::itemIds(const QOrganizerItemFilter &filter,
                                                     const QDateTime &startDateTime,
                                                     const QDateTime &endDateTime,
                                                     const QList<QOrganizerItemSortOrder> &sortOrders,
                                                     QOrganizerManager::Error *error)
{
    qWarning() << Q_FUNC_INFO << "Not implemented";
    QList<QOrganizerItemId> items;
    if (error) {
        *error = QOrganizerManager::NotSupportedError;
    }
    return items;
}

QList<QOrganizerItem> QOrganizerEDSEngine::itemOccurrences(const QOrganizerItem &parentItem,
                                                           const QDateTime &startDateTime,
                                                           const QDateTime &endDateTime,
                                                           int maxCount,
                                                           const QOrganizerItemFetchHint &fetchHint,
                                                           QOrganizerManager::Error *error)
{
    QOrganizerItemOccurrenceFetchRequest *req = new QOrganizerItemOccurrenceFetchRequest(this);

    req->setParentItem(parentItem);
    req->setStartDate(startDateTime);
    req->setEndDate(endDateTime);
    req->setMaxOccurrences(maxCount);
    req->setFetchHint(fetchHint);

    startRequest(req);
    waitForRequestFinished(req, 0);

    if (error) {
        *error = req->error();
    }

    req->deleteLater();
    return req->itemOccurrences();
}

QList<QOrganizerItem> QOrganizerEDSEngine::itemsForExport(const QDateTime &startDateTime,
                                                                 const QDateTime &endDateTime,
                                                                 const QOrganizerItemFilter &filter,
                                                                 const QList<QOrganizerItemSortOrder> &sortOrders,
                                                                 const QOrganizerItemFetchHint &fetchHint,
                                                                 QOrganizerManager::Error *error)
{
    qWarning() << Q_FUNC_INFO << "Not implemented";
    if (error) {
        *error = QOrganizerManager::NotSupportedError;
    }
    return QList<QOrganizerItem>();

}

void QOrganizerEDSEngine::saveItemsAsync(QOrganizerItemSaveRequest *req)
{
    if (req->items().count() == 0) {
        QOrganizerManagerEngine::updateItemSaveRequest(req,
                                                       QList<QOrganizerItem>(),
                                                       QOrganizerManager::NoError,
                                                       QMap<int, QOrganizerManager::Error>(),
                                                       QOrganizerAbstractRequest::FinishedState);
        return;
    }
    SaveRequestData *data = new SaveRequestData(this, req);
    saveItemsAsyncStart(data);
}

void QOrganizerEDSEngine::saveItemsAsyncStart(SaveRequestData *data)
{
    // check if request was destroyed by the caller
    if (!data->isLive()) {
        releaseRequestData(data);
        return;
    }

    QByteArray sourceId = data->nextSourceId();

    if (sourceId.isNull() && data->end()) {
        data->finish();
        return;
    } else {
        bool createItems = true;
        QList<QOrganizerItem> items = data->takeItemsToCreate();
        if (items.isEmpty()) {
            createItems = false;
            items = data->takeItemsToUpdate();
        }

        if (items.isEmpty()) {
            saveItemsAsyncStart(data);
            return;
        }

        /* We have entered this code path because sourceId is not null;
         * however, it can still be empty: that's because the SaveRequestData
         * class returns an empty (but not null!) sourceId for those items
         * which don't have a collection set, and that therefore should be
         * stored into the default collection.
         * The next "if" condition checks exactly for this situation.
         */
        if (sourceId.isEmpty() && createItems) {
            sourceId = data->parent()->d->m_sourceRegistry->defaultCollection().id().localId();
        }

        EClient *client = data->parent()->d->m_sourceRegistry->client(sourceId);
        if (!client) {
            Q_FOREACH(const QOrganizerItem &i, items) {
                data->appendResult(i, QOrganizerManager::InvalidCollectionError);
            }
            saveItemsAsyncStart(data);
            return;
        }

        Q_ASSERT(client);
        data->setClient(client);
        g_object_unref(client);

        bool hasRecurrence = false;
        GSList *comps = parseItems(data->client(),
                                   items,
                                   &hasRecurrence);
        if (comps) {
            data->setWorkingItems(items);
            if (createItems) {
                e_cal_client_create_objects(data->client(),
                                            comps,
                                            data->cancellable(),
                                            (GAsyncReadyCallback) QOrganizerEDSEngine::saveItemsAsyncCreated,
                                            data);
            } else {
                //WORKAROUND: There is no api to say what kind of update we want in case of update recurrence
                // items (E_CAL_OBJ_MOD_ALL, E_CAL_OBJ_MOD_THIS, E_CAL_OBJ_MOD_THISNADPRIOR, E_CAL_OBJ_MOD_THIS_AND_FUTURE)
                // as temporary solution the user can use "update-mode" property in QOrganizerItemSaveRequest object,
                // if not was specified, we will try to guess based on the event list.
                // If the event list does not cotain any recurrence event we will use E_CAL_OBJ_MOD_ALL
                // If the event list cotains any recurrence event we will use E_CAL_OBJ_MOD_THIS
                // all other cases should be explicitly specified using "update-mode" property
                int updateMode = data->updateMode();
                if (updateMode == -1) {
                    updateMode = hasRecurrence ? E_CAL_OBJ_MOD_THIS : E_CAL_OBJ_MOD_ALL;
                }
                e_cal_client_modify_objects(data->client(),
                                            comps,
                                            static_cast<ECalObjModType>(updateMode),
                                            data->cancellable(),
                                            (GAsyncReadyCallback) QOrganizerEDSEngine::saveItemsAsyncModified,
                                            data);
            }
            g_slist_free_full(comps, (GDestroyNotify) icalcomponent_free);
        } else {
            qWarning() << "Fail to translate items";
        }
    }
}

void QOrganizerEDSEngine::saveItemsAsyncModified(GObject *source_object,
                                                GAsyncResult *res,
                                                SaveRequestData *data)
{
    Q_UNUSED(source_object);

    GError *gError = 0;
    e_cal_client_modify_objects_finish(E_CAL_CLIENT(data->client()),
                                       res,
                                       &gError);

    if (gError) {
        qWarning() << "Fail to modify items" << gError->message;
        g_error_free(gError);
        gError = 0;
        if (data->isLive()) {
            Q_FOREACH(const QOrganizerItem &i, data->workingItems()) {
                data->appendResult(i, QOrganizerManager::UnspecifiedError);
            }
        }
    } else if (data->isLive()) {
        data->appendResults(data->workingItems());
    }

    if (data->isLive()) {
        saveItemsAsyncStart(data);
    } else {
        releaseRequestData(data);
    }
}

void QOrganizerEDSEngine::saveItemsAsyncCreated(GObject *source_object,
                                                GAsyncResult *res,
                                                SaveRequestData *data)
{
    Q_UNUSED(source_object);

    GError *gError = 0;
    GSList *uids = 0;
    e_cal_client_create_objects_finish(E_CAL_CLIENT(data->client()),
                                       res,
                                       &uids,
                                       &gError);
    if (gError) {
        qWarning() << "Fail to create items:" << (void*) data << gError->message;
        g_error_free(gError);
        gError = 0;

        if (data->isLive()) {
            Q_FOREACH(const QOrganizerItem &i, data->workingItems()) {
                data->appendResult(i, QOrganizerManager::UnspecifiedError);
            }
        }
    } else if (data->isLive()) {
        QByteArray currentSourceId = data->currentSourceId();
        if (currentSourceId.isEmpty()) {
            currentSourceId = data->parent()->defaultCollectionId().localId();
        }
        QList<QOrganizerItem> items = data->workingItems();
        QString managerUri = data->parent()->managerUri();
        for(uint i=0, iMax=g_slist_length(uids); i < iMax; i++) {
            QOrganizerItem &item = items[i];
            QByteArray uid(static_cast<const gchar*>(g_slist_nth_data(uids, i)));

            QOrganizerCollectionId collectionId(managerUri, currentSourceId);

            QString itemGuid =
                uid.contains(':') ? uid.mid(uid.lastIndexOf(':') + 1) : uid;
            QOrganizerItemId itemId = idFromEds(collectionId, uid);
            item.setId(itemId);
            item.setGuid(QString::fromUtf8(itemId.localId()));

            item.setCollectionId(collectionId);
        }
        g_slist_free_full(uids, g_free);
        data->appendResults(items);
    }

    // check if request was destroyed by the caller
    if (data->isLive()) {
        saveItemsAsyncStart(data);
    } else {
        releaseRequestData(data);
    }
}

bool QOrganizerEDSEngine::saveItems(QList<QtOrganizer::QOrganizerItem> *items,
                                    const QList<QtOrganizer::QOrganizerItemDetail::DetailType> &detailMask,
                                    QMap<int, QtOrganizer::QOrganizerManager::Error> *errorMap,
                                    QtOrganizer::QOrganizerManager::Error *error)

{
    QOrganizerItemSaveRequest *req = new QOrganizerItemSaveRequest(this);
    req->setItems(*items);
    req->setDetailMask(detailMask);

    startRequest(req);
    waitForRequestFinished(req, 0);

    *errorMap = req->errorMap();
    *error = req->error();
    *items = req->items();

    return (*error == QOrganizerManager::NoError);
}

void QOrganizerEDSEngine::removeItemsByIdAsync(QOrganizerItemRemoveByIdRequest *req)
{
    if (req->itemIds().count() == 0) {
        QOrganizerManagerEngine::updateItemRemoveByIdRequest(req,
                                                             QOrganizerManager::NoError,
                                                             QMap<int, QOrganizerManager::Error>(),
                                                             QOrganizerAbstractRequest::FinishedState);
        return;
    }

    RemoveByIdRequestData *data = new RemoveByIdRequestData(this, req);
    removeItemsByIdAsyncStart(data);
}

void QOrganizerEDSEngine::removeItemsByIdAsyncStart(RemoveByIdRequestData *data)
{
    // check if request was destroyed by the caller
    if (!data->isLive()) {
        releaseRequestData(data);
        return;
    }

    QByteArray collectionId = data->next();
    for(; !collectionId.isNull(); collectionId = data->next()) {
        EClient *client = data->parent()->d->m_sourceRegistry->client(collectionId);
        data->setClient(client);
        g_object_unref(client);
        GSList *ids = data->compIds();
        GError *gError = 0;
        e_cal_client_remove_objects_sync(data->client(), ids, E_CAL_OBJ_MOD_THIS, 0, 0);
        if (gError) {
            qWarning() << "Fail to remove Items" << gError->message;
            g_error_free(gError);
            gError = 0;
        }
        data->commit();
    }
    data->finish();
}

void QOrganizerEDSEngine::removeItemsAsync(QOrganizerItemRemoveRequest *req)
{
    if (req->items().count() == 0) {
        QOrganizerManagerEngine::updateItemRemoveRequest(req,
                                                         QOrganizerManager::NoError,
                                                         QMap<int, QOrganizerManager::Error>(),
                                                         QOrganizerAbstractRequest::FinishedState);
        return;
    }

    RemoveRequestData *data = new RemoveRequestData(this, req);
    removeItemsAsyncStart(data);
}

void QOrganizerEDSEngine::removeItemsAsyncStart(RemoveRequestData *data)
{
    // check if request was destroyed by the caller
    if (!data->isLive()) {
        releaseRequestData(data);
        return;
    }

    QOrganizerCollectionId collection = data->next();
    for(; !collection.isNull(); collection = data->next()) {
        EClient *client = data->parent()->d->m_sourceRegistry->client(collection.localId());
        Q_ASSERT(client);
        data->setClient(client);
        g_object_unref(client);
        GSList *ids = data->compIds();
        GError *gError = 0;
        e_cal_client_remove_objects_sync(data->client(), ids, E_CAL_OBJ_MOD_THIS, 0, 0);
        if (gError) {
            qWarning() << "Fail to remove Items" << gError->message;
            g_error_free(gError);
            gError = 0;
        }
        data->commit();
    }
    data->finish();
}

bool QOrganizerEDSEngine::removeItems(const QList<QOrganizerItemId> &itemIds,
                                      QMap<int, QOrganizerManager::Error> *errorMap,
                                      QOrganizerManager::Error *error)
{
    QOrganizerItemRemoveByIdRequest *req = new QOrganizerItemRemoveByIdRequest(this);
    req->setItemIds(itemIds);
    startRequest(req);
    waitForRequestFinished(req, 0);

    if (errorMap) {
        *errorMap = req->errorMap();
    }
    if (error) {
        *error = req->error();
    }

    return (*error == QOrganizerManager::NoError);
}

QOrganizerCollectionId QOrganizerEDSEngine::defaultCollectionId() const
{
    return d->m_sourceRegistry->defaultCollection().id();
}

QOrganizerCollection QOrganizerEDSEngine::collection(const QOrganizerCollectionId& collectionId,
                                                     QOrganizerManager::Error* error)
{
    QOrganizerCollection collection = d->m_sourceRegistry->collection(collectionId.localId());
    if (collection.id().isNull() && error) {
        *error = QOrganizerManager::DoesNotExistError;
    }

    return collection;
}

QList<QOrganizerCollection> QOrganizerEDSEngine::collections(QOrganizerManager::Error* error)
{
    QOrganizerCollectionFetchRequest *req = new QOrganizerCollectionFetchRequest(this);

    startRequest(req);
    waitForRequestFinished(req, 0);

    if (error) {
        *error = req->error();
    }

    if (req->error() == QOrganizerManager::NoError) {
        return req->collections();
    } else {
        return QList<QOrganizerCollection>();
    }
}

bool QOrganizerEDSEngine::saveCollection(QOrganizerCollection* collection, QOrganizerManager::Error* error)
{
    QOrganizerCollectionSaveRequest *req = new QOrganizerCollectionSaveRequest(this);
    req->setCollection(*collection);

    startRequest(req);
    waitForRequestFinished(req, 0);

    *error = req->error();
    if ((*error == QOrganizerManager::NoError) &&
        (req->collections().count())) {
        *collection = req->collections()[0];
        return true;
    } else {
        return false;
    }
}

void QOrganizerEDSEngine::saveCollectionAsync(QOrganizerCollectionSaveRequest *req)
{
    if (req->collections().count() == 0) {
        QOrganizerManagerEngine::updateCollectionSaveRequest(req,
                                                             QList<QOrganizerCollection>(),
                                                             QOrganizerManager::NoError,
                                                             QMap<int, QOrganizerManager::Error>(),
                                                             QOrganizerAbstractRequest::FinishedState);
        return;
    }

    ESourceRegistry *registry = d->m_sourceRegistry->object();
    SaveCollectionRequestData *requestData = new SaveCollectionRequestData(this, req);
    requestData->setRegistry(registry);

    if (requestData->prepareToCreate()) {
        e_source_registry_create_sources(registry,
                                         requestData->sourcesToCreate(),
                                         requestData->cancellable(),
                                         (GAsyncReadyCallback) QOrganizerEDSEngine::saveCollectionAsyncCommited,
                                         requestData);
    } else {
        requestData->prepareToUpdate();
        g_idle_add((GSourceFunc) saveCollectionUpdateAsyncStart, requestData);
    }
}

void QOrganizerEDSEngine::saveCollectionAsyncCommited(ESourceRegistry *registry,
                                                      GAsyncResult *res,
                                                      SaveCollectionRequestData *data)
{
    GError *gError = 0;
    e_source_registry_create_sources_finish(registry, res, &gError);
    if (gError) {
        qWarning() << "Fail to create sources:" << gError->message;
        g_error_free(gError);
        if (data->isLive()) {
            data->finish(QOrganizerManager::InvalidCollectionError);
            return;
        }
    } else if (data->isLive()) {
        data->commitSourceCreated();
        data->prepareToUpdate();
        g_idle_add((GSourceFunc) saveCollectionUpdateAsyncStart, data);
    }
}


gboolean QOrganizerEDSEngine::saveCollectionUpdateAsyncStart(SaveCollectionRequestData *data)
{
    // check if request was destroyed by the caller
    if (!data->isLive()) {
        releaseRequestData(data);
        return FALSE;
    }

    ESource *source = data->nextSourceToUpdate();
    if (source) {
        e_source_write(source,
                       data->cancellable(),
                       (GAsyncReadyCallback) QOrganizerEDSEngine::saveCollectionUpdateAsynCommited,
                       data);
    } else {
        data->finish();
    }
    return FALSE;
}

void QOrganizerEDSEngine::saveCollectionUpdateAsynCommited(ESource *source,
                                                           GAsyncResult *res,
                                                           SaveCollectionRequestData *data)
{
    GError *gError = 0;

    e_source_write_finish(source, res, &gError);
    if (gError) {
        qWarning() << "Fail to update collection" << gError->message;
        g_error_free(gError);
        if (data->isLive()) {
            data->commitSourceUpdated(source, QOrganizerManager::InvalidCollectionError);
        }
    } else if (data->isLive()) {
        data->commitSourceUpdated(source);
    }

    if (data->isLive()) {
        g_idle_add((GSourceFunc) saveCollectionUpdateAsyncStart, data);
    } else {
        releaseRequestData(data);
    }
}

bool QOrganizerEDSEngine::removeCollection(const QOrganizerCollectionId& collectionId, QOrganizerManager::Error* error)
{
    QOrganizerCollectionRemoveRequest *req = new QOrganizerCollectionRemoveRequest(this);
    req->setCollectionId(collectionId);

    startRequest(req);
    waitForRequestFinished(req, 0);

    if (error) {
        *error = req->error();
    }

    return(req->error() == QOrganizerManager::NoError);
}

void QOrganizerEDSEngine::removeCollectionAsync(QtOrganizer::QOrganizerCollectionRemoveRequest *req)
{
    if (req->collectionIds().count() == 0) {
        QOrganizerManagerEngine::updateCollectionRemoveRequest(req,
                                                             QOrganizerManager::NoError,
                                                             QMap<int, QOrganizerManager::Error>(),
                                                             QOrganizerAbstractRequest::FinishedState);
        return;
    }

    RemoveCollectionRequestData *requestData = new RemoveCollectionRequestData(this, req);
    removeCollectionAsyncStart(0, 0, requestData);
}

void QOrganizerEDSEngine::removeCollectionAsyncStart(GObject *sourceObject,
                                                     GAsyncResult *res,
                                                     RemoveCollectionRequestData *data)
{
    // check if request was destroyed by the caller
    if (!data->isLive()) {
        releaseRequestData(data);
        return;
    }

    if (sourceObject && res) {
        GError *gError = 0;
        if (data->remoteDeletable()) {
            e_source_remote_delete_finish(E_SOURCE(sourceObject), res, &gError);
        } else {
            e_source_remove_finish(E_SOURCE(sourceObject), res, &gError);
        }
        if (gError) {
            qWarning() << "Fail to remove collection" << gError->message;
            g_error_free(gError);
            data->commit(QOrganizerManager::InvalidCollectionError);
        } else {
            data->commit();
        }
    }

    ESource *source = data->begin();
    if (source) {
        ESourceRegistry *registry = NULL;
        gboolean accountRemovable = e_source_get_removable(source);
        gboolean remoteDeletable = e_source_get_remote_deletable(source);

        if ((accountRemovable == FALSE) && (remoteDeletable == FALSE)) {
            qWarning() << "Account not removable will refetch source";
            // WORKAROUND: Sometimes EDS take longer to make a account removable with this we
            // force EDS to update sources infomation
            registry  = e_source_registry_new_sync(NULL, NULL);
            source = e_source_registry_ref_source(registry, e_source_get_uid(source));
            accountRemovable = e_source_get_removable(source);
            remoteDeletable = e_source_get_remote_deletable(source);
        }

        if (remoteDeletable == TRUE) {
            data->setRemoteDeletable(true);
            e_source_remote_delete(source, data->cancellable(),
                                   (GAsyncReadyCallback) QOrganizerEDSEngine::removeCollectionAsyncStart,
                                   data);
        } else if (accountRemovable == TRUE) {
            e_source_remove(source, data->cancellable(),
                            (GAsyncReadyCallback) QOrganizerEDSEngine::removeCollectionAsyncStart,
                            data);
        } else {
            qWarning() << "Source not removable" << e_source_get_uid(source);
            data->commit(QOrganizerManager::InvalidCollectionError);
            removeCollectionAsyncStart(0, 0, data);
        }

        if (registry) {
            g_object_unref(source);
            g_object_unref(registry);
        }
    } else {
        data->finish();
    }

}

void QOrganizerEDSEngine::releaseRequestData(RequestData *data)
{
    data->deleteLater();
}

void QOrganizerEDSEngine::requestDestroyed(QOrganizerAbstractRequest* req)
{
    RequestData *data = m_runningRequests.take(req);
    if (data) {
        data->cancel();
    }
}

bool QOrganizerEDSEngine::startRequest(QOrganizerAbstractRequest* req)
{
    if (!req)
        return false;

    switch (req->type())
    {
        case QOrganizerAbstractRequest::ItemFetchRequest:
            itemsAsync(qobject_cast<QOrganizerItemFetchRequest*>(req));
            break;
        case QOrganizerAbstractRequest::ItemFetchByIdRequest:
            itemsByIdAsync(qobject_cast<QOrganizerItemFetchByIdRequest*>(req));
            break;
        case QOrganizerAbstractRequest::ItemOccurrenceFetchRequest:
            itemOcurrenceAsync(qobject_cast<QOrganizerItemOccurrenceFetchRequest*>(req));
            break;
        case QOrganizerAbstractRequest::CollectionFetchRequest:
            QOrganizerManagerEngine::updateCollectionFetchRequest(qobject_cast<QOrganizerCollectionFetchRequest*>(req),
                                                                  d->m_sourceRegistry->collections(),
                                                                  QOrganizerManager::NoError,
                                                                  QOrganizerAbstractRequest::FinishedState);
            break;
        case QOrganizerAbstractRequest::ItemSaveRequest:
            saveItemsAsync(qobject_cast<QOrganizerItemSaveRequest*>(req));
            break;
        case QOrganizerAbstractRequest::ItemRemoveRequest:
            removeItemsAsync(qobject_cast<QOrganizerItemRemoveRequest*>(req));
            break;
        case QOrganizerAbstractRequest::ItemRemoveByIdRequest:
            removeItemsByIdAsync(qobject_cast<QOrganizerItemRemoveByIdRequest*>(req));
            break;
        case QOrganizerAbstractRequest::CollectionSaveRequest:
            saveCollectionAsync(qobject_cast<QOrganizerCollectionSaveRequest*>(req));
            break;
        case QOrganizerAbstractRequest::CollectionRemoveRequest:
            removeCollectionAsync(qobject_cast<QOrganizerCollectionRemoveRequest*>(req));
            break;
        default:
            updateRequestState(req, QOrganizerAbstractRequest::FinishedState);
            qWarning() << "No implemented request" << req->type();
            break;
    }

    return true;
}

bool QOrganizerEDSEngine::cancelRequest(QOrganizerAbstractRequest* req)
{
    RequestData *data = m_runningRequests.value(req);
    if (data) {
        data->cancel();
        return true;
    }
    qWarning() << "Request is not running" << (void*) req;
    return false;
}

bool QOrganizerEDSEngine::waitForRequestFinished(QOrganizerAbstractRequest* req, int msecs)
{
    Q_ASSERT(req);

    RequestData *data = m_runningRequests.value(req);
    if (data) {
        data->wait(msecs);
        // We can delete the operation already finished
        data->deleteLater();
    }

    return true;
}

QList<QOrganizerItemDetail::DetailType> QOrganizerEDSEngine::supportedItemDetails(QOrganizerItemType::ItemType itemType) const
{
    QList<QOrganizerItemDetail::DetailType> supportedDetails;
    supportedDetails << QOrganizerItemDetail::TypeItemType
                     << QOrganizerItemDetail::TypeGuid
                     << QOrganizerItemDetail::TypeTimestamp
                     << QOrganizerItemDetail::TypeDisplayLabel
                     << QOrganizerItemDetail::TypeDescription
                     << QOrganizerItemDetail::TypeComment
                     << QOrganizerItemDetail::TypeTag
                     << QOrganizerItemDetail::TypeClassification
                     << QOrganizerItemDetail::TypeExtendedDetail;

    if (itemType == QOrganizerItemType::TypeEvent) {
        supportedDetails << QOrganizerItemDetail::TypeRecurrence
                         << QOrganizerItemDetail::TypeEventTime
                         << QOrganizerItemDetail::TypePriority
                         << QOrganizerItemDetail::TypeLocation
                         << QOrganizerItemDetail::TypeReminder
                         << QOrganizerItemDetail::TypeAudibleReminder
                         << QOrganizerItemDetail::TypeEmailReminder
                         << QOrganizerItemDetail::TypeVisualReminder;
    } else if (itemType == QOrganizerItemType::TypeTodo) {
        supportedDetails << QOrganizerItemDetail::TypeRecurrence
                         << QOrganizerItemDetail::TypeTodoTime
                         << QOrganizerItemDetail::TypePriority
                         << QOrganizerItemDetail::TypeTodoProgress
                         << QOrganizerItemDetail::TypeReminder
                         << QOrganizerItemDetail::TypeAudibleReminder
                         << QOrganizerItemDetail::TypeEmailReminder
                         << QOrganizerItemDetail::TypeVisualReminder;
    } else if (itemType == QOrganizerItemType::TypeEventOccurrence) {
        supportedDetails << QOrganizerItemDetail::TypeParent
                         << QOrganizerItemDetail::TypeEventTime
                         << QOrganizerItemDetail::TypePriority
                         << QOrganizerItemDetail::TypeLocation
                         << QOrganizerItemDetail::TypeReminder
                         << QOrganizerItemDetail::TypeAudibleReminder
                         << QOrganizerItemDetail::TypeEmailReminder
                         << QOrganizerItemDetail::TypeVisualReminder;
    } else if (itemType == QOrganizerItemType::TypeTodoOccurrence) {
        supportedDetails << QOrganizerItemDetail::TypeParent
                         << QOrganizerItemDetail::TypeTodoTime
                         << QOrganizerItemDetail::TypePriority
                         << QOrganizerItemDetail::TypeTodoProgress
                         << QOrganizerItemDetail::TypeReminder
                         << QOrganizerItemDetail::TypeAudibleReminder
                         << QOrganizerItemDetail::TypeEmailReminder
                         << QOrganizerItemDetail::TypeVisualReminder;
    } else if (itemType == QOrganizerItemType::TypeJournal) {
        supportedDetails << QOrganizerItemDetail::TypeJournalTime;
    } else if (itemType == QOrganizerItemType::TypeNote) {
        // nothing ;)
    } else {
        supportedDetails.clear();
    }

    return supportedDetails;
}

QList<QOrganizerItemFilter::FilterType> QOrganizerEDSEngine::supportedFilters() const
{
    QList<QOrganizerItemFilter::FilterType> supported;

    supported << QOrganizerItemFilter::InvalidFilter
              << QOrganizerItemFilter::DetailFilter
              << QOrganizerItemFilter::DetailFieldFilter
              << QOrganizerItemFilter::DetailRangeFilter
              << QOrganizerItemFilter::IntersectionFilter
              << QOrganizerItemFilter::UnionFilter
              << QOrganizerItemFilter::IdFilter
              << QOrganizerItemFilter::CollectionFilter
              << QOrganizerItemFilter::DefaultFilter;

    return supported;
}

QList<QOrganizerItemType::ItemType> QOrganizerEDSEngine::supportedItemTypes() const
{
    return QList<QOrganizerItemType::ItemType>() << QOrganizerItemType::TypeEvent
                         << QOrganizerItemType::TypeEventOccurrence
                         << QOrganizerItemType::TypeJournal
                         << QOrganizerItemType::TypeNote
                         << QOrganizerItemType::TypeTodo
                         << QOrganizerItemType::TypeTodoOccurrence;
}

int QOrganizerEDSEngine::runningRequestCount() const
{
    return m_runningRequests.count();
}

void QOrganizerEDSEngine::onSourceAdded(const QByteArray &sourceId)
{
    QOrganizerCollectionId id(managerUri(), sourceId);
    d->watch(id);

    Q_EMIT collectionsAdded(QList<QOrganizerCollectionId>() << id);

    QList<QPair<QOrganizerCollectionId, QOrganizerManager::Operation> > ops;
    ops << qMakePair(id, QOrganizerManager::Add);
    Q_EMIT collectionsModified(ops);
}

void QOrganizerEDSEngine::onSourceRemoved(const QByteArray &sourceId)
{
    d->unWatch(sourceId);
    QOrganizerCollectionId id(managerUri(), sourceId);

    Q_EMIT collectionsRemoved(QList<QOrganizerCollectionId>() << id);

    QList<QPair<QOrganizerCollectionId, QOrganizerManager::Operation> > ops;
    ops << qMakePair(id, QOrganizerManager::Remove);
    Q_EMIT collectionsModified(ops);
}

void QOrganizerEDSEngine::onSourceUpdated(const QByteArray &sourceId)
{
    QOrganizerCollectionId id(managerUri(), sourceId);
    Q_EMIT collectionsChanged(QList<QOrganizerCollectionId>() << id);

    QList<QPair<QOrganizerCollectionId, QOrganizerManager::Operation> > ops;
    ops << qMakePair(id, QOrganizerManager::Change);
    Q_EMIT collectionsModified(ops);
}

void QOrganizerEDSEngine::onViewChanged(QOrganizerItemChangeSet *change)
{
    change->emitSignals(this);
}

QDateTime QOrganizerEDSEngine::fromIcalTime(struct icaltimetype value, const char *tzId)
{
    uint tmTime;
    bool allDayEvent = icaltime_is_date(value);

    // check if ialtimetype contais a time and timezone
    if (!allDayEvent && tzId) {
        QByteArray tzLocationName;
        icaltimezone *timezone = icaltimezone_get_builtin_timezone_from_tzid(tzId);

        if (icaltime_is_utc(value)) {
            tzLocationName = "UTC";
        } else {
            // fallback: sometimes the tzId contains the location name
            if (!timezone) {
                static const char prefix[] = "/freeassociation.sourceforge.net/Tzfile/";
                if (strncmp(tzId, prefix, sizeof(prefix) - 1) == 0) {
                    tzId += sizeof(prefix) - 1;
                }
                timezone = icaltimezone_get_builtin_timezone(tzId);
            }
            tzLocationName = QByteArray(icaltimezone_get_location(timezone));
        }

        tmTime = icaltime_as_timet_with_zone(value, timezone);
        QTimeZone qTz(tzLocationName);
        return QDateTime::fromTime_t(tmTime, qTz);
    } else {
        tmTime = icaltime_as_timet(value);
        QDateTime t = QDateTime::fromTime_t(tmTime, Qt::UTC);
        // all day events will set as local time
        // floating time events will be set as UTC
        QDateTime tt;
        if (allDayEvent)
          tt = QDateTime(t.date(), QTime(0,0,0),
                         QTimeZone(QTimeZone::systemTimeZoneId()));
        else
          tt = QDateTime(t.date(), t.time(), Qt::UTC);
        return tt;
    }
}

icaltimetype QOrganizerEDSEngine::fromQDateTime(const QDateTime &dateTime,
                                                bool allDay,
                                                QByteArray *tzId)
{
    QDateTime finalDate(dateTime);
    QTimeZone tz;

    if (!allDay) {
        switch (finalDate.timeSpec()) {
        case Qt::UTC:
        case Qt::OffsetFromUTC:
            // convert date to UTC timezone
            tz = QTimeZone("UTC");
            finalDate = finalDate.toTimeZone(tz);
            break;
        case Qt::TimeZone:
            tz = finalDate.timeZone();
            if (!tz.isValid()) {
                // floating time
                finalDate = QDateTime(finalDate.date(), finalDate.time(), Qt::UTC);
            }
            break;
        case Qt::LocalTime:
            tz = QTimeZone(QTimeZone::systemTimeZoneId());
            finalDate = finalDate.toTimeZone(tz);
            break;
        default:
            break;
        }
    }

    if (tz.isValid()) {
        icaltimezone *timezone = 0;
        timezone = icaltimezone_get_builtin_timezone(tz.id().constData());
        *tzId = QByteArray(icaltimezone_get_tzid(timezone));
        return icaltime_from_timet_with_zone(finalDate.toTime_t(), allDay, timezone);
    } else {
        bool invalidTime = allDay || !finalDate.time().isValid();
        finalDate = QDateTime(finalDate.date(),
                              invalidTime ? QTime(0, 0, 0) : finalDate.time(),
                              Qt::UTC);

        *tzId = "";
        return icaltime_from_timet(finalDate.toTime_t(), allDay);
    }
}

void QOrganizerEDSEngine::parseStartTime(ECalComponent *comp, QOrganizerItem *item)
{
    ECalComponentDateTime *dt = g_new0(ECalComponentDateTime, 1);
    e_cal_component_get_dtstart(comp, dt);
    if (dt->value) {
        QOrganizerEventTime etr = item->detail(QOrganizerItemDetail::TypeEventTime);
        QDateTime qdtime = fromIcalTime(*dt->value, dt->tzid);
        if (qdtime.isValid())
            etr.setStartDateTime(qdtime);
        if (icaltime_is_date(*dt->value) != etr.isAllDay()) {
            etr.setAllDay(icaltime_is_date(*dt->value));
        }
        item->saveDetail(&etr);
    }
    e_cal_component_free_datetime(dt);
    g_free(dt);
}

void QOrganizerEDSEngine::parseTodoStartTime(ECalComponent *comp, QOrganizerItem *item)
{
    ECalComponentDateTime *dt = g_new0(ECalComponentDateTime, 1);
    e_cal_component_get_dtstart(comp, dt);
    if (dt->value) {
        QOrganizerTodoTime etr = item->detail(QOrganizerItemDetail::TypeTodoTime);
        QDateTime qdtime = fromIcalTime(*dt->value, dt->tzid);
        if (qdtime.isValid())
            etr.setStartDateTime(qdtime);
        if (icaltime_is_date(*dt->value) != etr.isAllDay()) {
            etr.setAllDay(icaltime_is_date(*dt->value));
        }
        item->saveDetail(&etr);
    }
    e_cal_component_free_datetime(dt);
    g_free(dt);
}

void QOrganizerEDSEngine::parseEndTime(ECalComponent *comp, QOrganizerItem *item)
{
    ECalComponentDateTime *dt = g_new0(ECalComponentDateTime, 1);
    e_cal_component_get_dtend(comp, dt);
    if (dt->value) {
        QOrganizerEventTime etr = item->detail(QOrganizerItemDetail::TypeEventTime);
        QDateTime qdtime = fromIcalTime(*dt->value, dt->tzid);
        if (qdtime.isValid())
            etr.setEndDateTime(qdtime);
        if (icaltime_is_date(*dt->value) != etr.isAllDay()) {
            etr.setAllDay(icaltime_is_date(*dt->value));
        }
        item->saveDetail(&etr);
    }
    e_cal_component_free_datetime(dt);
    g_free(dt);
}

void QOrganizerEDSEngine::parseWeekRecurrence(struct icalrecurrencetype *rule, QtOrganizer::QOrganizerRecurrenceRule *qRule)
{
    static QMap<icalrecurrencetype_weekday, Qt::DayOfWeek>  daysOfWeekMap;
    if (daysOfWeekMap.isEmpty()) {
        daysOfWeekMap.insert(ICAL_MONDAY_WEEKDAY, Qt::Monday);
        daysOfWeekMap.insert(ICAL_THURSDAY_WEEKDAY, Qt::Thursday);
        daysOfWeekMap.insert(ICAL_WEDNESDAY_WEEKDAY, Qt::Wednesday);
        daysOfWeekMap.insert(ICAL_TUESDAY_WEEKDAY, Qt::Tuesday);
        daysOfWeekMap.insert(ICAL_FRIDAY_WEEKDAY, Qt::Friday);
        daysOfWeekMap.insert(ICAL_SATURDAY_WEEKDAY, Qt::Saturday);
        daysOfWeekMap.insert(ICAL_SUNDAY_WEEKDAY, Qt::Sunday);
    }

    qRule->setFrequency(QOrganizerRecurrenceRule::Weekly);

    QSet<Qt::DayOfWeek> daysOfWeek;
    for (int d=0; d <= Qt::Sunday; d++) {
        short day = rule->by_day[d];
        if (day != ICAL_RECURRENCE_ARRAY_MAX) {
            daysOfWeek.insert(daysOfWeekMap[icalrecurrencetype_day_day_of_week(rule->by_day[d])]);
        }
    }

    qRule->setDaysOfWeek(daysOfWeek);
}

void QOrganizerEDSEngine::parseMonthRecurrence(struct icalrecurrencetype *rule, QtOrganizer::QOrganizerRecurrenceRule *qRule)
{
    qRule->setFrequency(QOrganizerRecurrenceRule::Monthly);

    QSet<int> daysOfMonth;
    for (int d=0; d < ICAL_BY_MONTHDAY_SIZE; d++) {
        short day = rule->by_month_day[d];
        if (day != ICAL_RECURRENCE_ARRAY_MAX) {
            daysOfMonth.insert(day);
        }
    }
    qRule->setDaysOfMonth(daysOfMonth);
}

void QOrganizerEDSEngine::parseYearRecurrence(struct icalrecurrencetype *rule, QtOrganizer::QOrganizerRecurrenceRule *qRule)
{
    qRule->setFrequency(QOrganizerRecurrenceRule::Yearly);

    QSet<int> daysOfYear;
    for (int d=0; d < ICAL_BY_YEARDAY_SIZE; d++) {
        short day = rule->by_year_day[d];
        if (day != ICAL_RECURRENCE_ARRAY_MAX) {
            daysOfYear.insert(day);
        }
    }
    qRule->setDaysOfYear(daysOfYear);

    QSet<QOrganizerRecurrenceRule::Month> monthOfYear;
    for (int d=0; d < ICAL_BY_MONTH_SIZE; d++) {
        short month = rule->by_month[d];
        if (month != ICAL_RECURRENCE_ARRAY_MAX) {
            monthOfYear.insert(static_cast<QOrganizerRecurrenceRule::Month>(month));
        }
    }
    qRule->setMonthsOfYear(monthOfYear);
}

void QOrganizerEDSEngine::parseRecurrence(ECalComponent *comp, QOrganizerItem *item)
{
    // recurence
    if (e_cal_component_has_rdates(comp)) {
        QSet<QDate> dates;
        GSList *periodList = 0;
        e_cal_component_get_rdate_list(comp, &periodList);
        for(GSList *i = periodList; i != 0; i = i->next) {
            ECalComponentPeriod *period = (ECalComponentPeriod*) i->data;
            //TODO: get timezone info
            QDateTime dt = fromIcalTime(period->start, 0);
            if (dt.isValid())
                dates.insert(dt.date());
            //TODO: period.end, period.duration
        }
        e_cal_component_free_period_list(periodList);

        QOrganizerItemRecurrence rec = item->detail(QOrganizerItemDetail::TypeRecurrence);
        rec.setRecurrenceDates(dates);
        item->saveDetail(&rec);
    }

    if (e_cal_component_has_exdates(comp)) {
        QSet<QDate> dates;
        GSList *exdateList = 0;

        e_cal_component_get_exdate_list(comp, &exdateList);
        for(GSList *i = exdateList; i != 0; i = i->next) {
            ECalComponentDateTime* dateTime = (ECalComponentDateTime*) i->data;
            QDateTime dt = fromIcalTime(*dateTime->value, dateTime->tzid);
            if (dt.isValid())
                dates.insert(dt.date());
        }
        e_cal_component_free_exdate_list(exdateList);

        QOrganizerItemRecurrence irec = item->detail(QOrganizerItemDetail::TypeRecurrence);
        irec.setExceptionDates(dates);
        item->saveDetail(&irec);
    }

    // rules
    GSList *ruleList = 0;
    e_cal_component_get_rrule_list(comp, &ruleList);
    if (ruleList) {
        QSet<QOrganizerRecurrenceRule> qRules;

        for(GSList *i = ruleList; i != 0; i = i->next) {
            struct icalrecurrencetype *rule = (struct icalrecurrencetype*) i->data;
            QOrganizerRecurrenceRule qRule;
            switch (rule->freq) {
                case ICAL_SECONDLY_RECURRENCE:
                case ICAL_MINUTELY_RECURRENCE:
                case ICAL_HOURLY_RECURRENCE:
                    qWarning() << "Recurrence frequency not supported";
                    break;
                case ICAL_DAILY_RECURRENCE:
                    qRule.setFrequency(QOrganizerRecurrenceRule::Daily);
                    break;
                case ICAL_WEEKLY_RECURRENCE:
                    parseWeekRecurrence(rule, &qRule);
                    break;
                case ICAL_MONTHLY_RECURRENCE:
                    parseMonthRecurrence(rule, &qRule);
                    break;
                case ICAL_YEARLY_RECURRENCE:
                    parseYearRecurrence(rule, &qRule);
                    break;
                case ICAL_NO_RECURRENCE:
                    break;
            }

            if (icaltime_is_date(rule->until)) {
                QDate dt = QDate::fromString(icaltime_as_ical_string(rule->until), "yyyyMMdd");
                if (dt.isValid()) {
                    qRule.setLimit(dt);
                }
            } else if (rule->count > 0) {
                qRule.setLimit(rule->count);
            }

            qRule.setInterval(rule->interval);

            QSet<int> positions;
            for (int d=0; d < ICAL_BY_SETPOS_SIZE; d++) {
                short day = rule->by_set_pos[d];
                if (day != ICAL_RECURRENCE_ARRAY_MAX) {
                    positions.insert(day);
                }
            }
            qRule.setPositions(positions);

            qRules << qRule;
        }

        if (!qRules.isEmpty()) {
            QOrganizerItemRecurrence irec = item->detail(QOrganizerItemDetail::TypeRecurrence);
            irec.setRecurrenceRules(qRules);
            item->saveDetail(&irec);
        }

        e_cal_component_free_recur_list(ruleList);
    }
    // TODO: exeptions rules
}

void QOrganizerEDSEngine::parsePriority(ECalComponent *comp, QOrganizerItem *item)
{
    gint *priority = 0;
    e_cal_component_get_priority(comp, &priority);
    if (priority) {
        QOrganizerItemPriority iPriority = item->detail(QOrganizerItemDetail::TypePriority);
        if ((*priority >= QOrganizerItemPriority::UnknownPriority) &&
            (*priority <= QOrganizerItemPriority::LowPriority)) {
            iPriority.setPriority((QOrganizerItemPriority::Priority) *priority);
        } else {
            iPriority.setPriority(QOrganizerItemPriority::UnknownPriority);
        }
        e_cal_component_free_priority(priority);
        item->saveDetail(&iPriority);
    }
}

void QOrganizerEDSEngine::parseLocation(ECalComponent *comp, QOrganizerItem *item)
{
    const gchar *location;
    e_cal_component_get_location(comp, &location);
    if (location) {
        QOrganizerItemLocation ld = item->detail(QOrganizerItemDetail::TypeLocation);
        ld.setLabel(QString::fromUtf8(location));
        item->saveDetail(&ld);
    }
}

void QOrganizerEDSEngine::parseDueDate(ECalComponent *comp, QOrganizerItem *item)
{
    ECalComponentDateTime due;
    e_cal_component_get_due(comp, &due);
    if (due.value) {
        QOrganizerTodoTime ttr = item->detail(QOrganizerItemDetail::TypeTodoTime);
        QDateTime qdtime = fromIcalTime(*due.value, due.tzid);
        if (qdtime.isValid())
            ttr.setDueDateTime(qdtime);
        if (icaltime_is_date(*due.value) != ttr.isAllDay()) {
            ttr.setAllDay(icaltime_is_date(*due.value));
        }
        item->saveDetail(&ttr);
    }
    e_cal_component_free_datetime(&due);
}

void QOrganizerEDSEngine::parseProgress(ECalComponent *comp, QOrganizerItem *item)
{
    gint percentage = e_cal_component_get_percent_as_int(comp);
    if (percentage > 0 && percentage <= 100) {
        QOrganizerTodoProgress tp = item->detail(QOrganizerItemDetail::TypeTodoProgress);
        tp.setPercentageComplete(percentage);
        item->saveDetail(&tp);
    }
}

void QOrganizerEDSEngine::parseStatus(ECalComponent *comp, QOrganizerItem *item)
{
    icalproperty_status status;
    e_cal_component_get_status(comp, &status);

    QOrganizerTodoProgress tp;
    switch(status) {
        case ICAL_STATUS_NONE:
            tp.setStatus(QOrganizerTodoProgress::StatusNotStarted);
            break;
        case ICAL_STATUS_INPROCESS:
            tp.setStatus(QOrganizerTodoProgress::StatusInProgress);
            break;
        case ICAL_STATUS_COMPLETED:
            tp.setStatus(QOrganizerTodoProgress::StatusComplete);
            break;
        case ICAL_STATUS_CANCELLED:
        default:
            //TODO: not supported
            break;
    }
    item->saveDetail(&tp);
}

void QOrganizerEDSEngine::parseAttendeeList(ECalComponent *comp, QOrganizerItem *item)
{
    GSList *attendeeList = 0;
    e_cal_component_get_attendee_list(comp, &attendeeList);
    for (GSList *attendeeIter=attendeeList; attendeeIter != 0; attendeeIter = attendeeIter->next) {
        ECalComponentAttendee *attendee = static_cast<ECalComponentAttendee *>(attendeeIter->data);
        QOrganizerEventAttendee qAttendee;

        qAttendee.setAttendeeId(QString::fromUtf8(attendee->member));
        qAttendee.setName(QString::fromUtf8(attendee->cn));
        qAttendee.setEmailAddress(QString::fromUtf8(attendee->value));

        switch(attendee->role) {
        case ICAL_ROLE_REQPARTICIPANT:
            qAttendee.setParticipationRole(QOrganizerEventAttendee::RoleRequiredParticipant);
            break;
        case ICAL_ROLE_OPTPARTICIPANT:
            qAttendee.setParticipationRole(QOrganizerEventAttendee::RoleOptionalParticipant);
            break;
        case ICAL_ROLE_CHAIR:
            qAttendee.setParticipationRole(QOrganizerEventAttendee::RoleChairperson);
            break;
        case ICAL_ROLE_X:
            qAttendee.setParticipationRole(QOrganizerEventAttendee::RoleHost);
            break;
        case ICAL_ROLE_NONE:
        default:
            qAttendee.setParticipationRole(QOrganizerEventAttendee::RoleNonParticipant);
            break;
        }

        switch(attendee->status) {
        case ICAL_PARTSTAT_ACCEPTED:
            qAttendee.setParticipationStatus(QOrganizerEventAttendee::StatusAccepted);
            break;
        case ICAL_PARTSTAT_DECLINED:
            qAttendee.setParticipationStatus(QOrganizerEventAttendee::StatusDeclined);
            break;
        case ICAL_PARTSTAT_TENTATIVE:
            qAttendee.setParticipationStatus(QOrganizerEventAttendee::StatusTentative);
            break;
        case ICAL_PARTSTAT_DELEGATED:
            qAttendee.setParticipationStatus(QOrganizerEventAttendee::StatusDelegated);
            break;
        case ICAL_PARTSTAT_COMPLETED:
            qAttendee.setParticipationStatus(QOrganizerEventAttendee::StatusCompleted);
            break;
        case ICAL_PARTSTAT_INPROCESS:
            qAttendee.setParticipationStatus(QOrganizerEventAttendee::StatusInProcess);
            break;
        case ICAL_PARTSTAT_NEEDSACTION:
        case ICAL_PARTSTAT_NONE:
        default:
            qAttendee.setParticipationStatus(QOrganizerEventAttendee::StatusUnknown);
            break;

        }
        item->saveDetail(&qAttendee);
    }
    e_cal_component_free_attendee_list(attendeeList);
}

void QOrganizerEDSEngine::parseExtendedDetails(ECalComponent *comp, QOrganizerItem *item)
{
    icalcomponent *icalcomp = e_cal_component_get_icalcomponent(comp);
    for (icalproperty *prop = icalcomponent_get_first_property(icalcomp, ICAL_X_PROPERTY);
         prop != NULL;
         prop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY)) {

        QOrganizerItemExtendedDetail ex;
        ex.setName(QString::fromUtf8(icalproperty_get_x_name(prop)));
        ex.setData(QByteArray(icalproperty_get_x(prop)));
        item->saveDetail(&ex);
    }
}

QOrganizerItem *QOrganizerEDSEngine::parseEvent(ECalComponent *comp,
                                                QList<QOrganizerItemDetail::DetailType> detailsHint)
{
    QOrganizerItem *event;
    if (hasRecurrence(comp)) {
        event = new QOrganizerEventOccurrence();
    } else {
        event = new QOrganizerEvent();
    }
    if (detailsHint.isEmpty() ||
        detailsHint.contains(QOrganizerItemDetail::TypeEventTime)) {
        parseStartTime(comp, event);
        parseEndTime(comp, event);
    }

    if (detailsHint.isEmpty() ||
        detailsHint.contains(QOrganizerItemDetail::TypeRecurrence)) {
        parseRecurrence(comp, event);
    }

    if (detailsHint.isEmpty() ||
        detailsHint.contains(QOrganizerItemDetail::TypePriority)) {
        parsePriority(comp, event);
    }

    if (detailsHint.isEmpty() ||
        detailsHint.contains(QOrganizerItemDetail::TypeLocation)) {
        parseLocation(comp, event);
    }
    return event;
}

QOrganizerItem *QOrganizerEDSEngine::parseToDo(ECalComponent *comp,
                                               QList<QOrganizerItemDetail::DetailType> detailsHint)
{
    QOrganizerItem *todo;
    if (hasRecurrence(comp)) {
        todo = new QOrganizerTodoOccurrence();
    } else {
        todo = new QOrganizerTodo();
    }

    if (detailsHint.isEmpty() ||
        detailsHint.contains(QOrganizerItemDetail::TypeTodoTime)) {
        parseTodoStartTime(comp, todo);
        parseDueDate(comp, todo);
    }

    if (detailsHint.isEmpty() ||
        detailsHint.contains(QOrganizerItemDetail::TypeRecurrence)) {
        parseRecurrence(comp, todo);
    }

    if (detailsHint.isEmpty() ||
        detailsHint.contains(QOrganizerItemDetail::TypePriority)) {
        parsePriority(comp, todo);
    }

    if (detailsHint.isEmpty() ||
        detailsHint.contains(QOrganizerItemDetail::TypeTodoProgress)) {
        parseProgress(comp, todo);
        parseStatus(comp, todo);
    }

    return todo;
}

QOrganizerItem *QOrganizerEDSEngine::parseJournal(ECalComponent *comp,
                                                  QList<QOrganizerItemDetail::DetailType> detailsHint)
{
    QOrganizerJournal *journal = new QOrganizerJournal();

    if (detailsHint.isEmpty() ||
        detailsHint.contains(QOrganizerItemDetail::TypeJournalTime)) {
        ECalComponentDateTime dt;
        e_cal_component_get_dtstart(comp, &dt);
        if (dt.value) {
            QDateTime qdtime = fromIcalTime(*dt.value, dt.tzid);
            if (qdtime.isValid()) {
              QOrganizerJournalTime jtime;
              jtime.setEntryDateTime(qdtime);
              journal->saveDetail(&jtime);
            }
        }
        e_cal_component_free_datetime(&dt);
    }

    return journal;
}

QByteArray QOrganizerEDSEngine::toComponentId(const QByteArray &itemId, QByteArray *rid)
{
    QList<QByteArray> ids = itemId.split('/').last().split('#');
    if (ids.size() == 2) {
        *rid = ids[1];
    }
    return ids[0];
}

ECalComponentId *QOrganizerEDSEngine::ecalComponentId(const QOrganizerItemId &itemId)
{
    QByteArray edsItemId = idToEds(itemId);
    QStringList ids = QString::fromUtf8(edsItemId).split("#");

    QString cId = ids[0];
    QString rId = (ids.size() == 2) ? ids[1] : QString();


    ECalComponentId *id = g_new0(ECalComponentId, 1);
    id->uid = g_strdup(cId.toUtf8().data());
    if (rId.isEmpty()) {
        id->rid = NULL;
    } else {
        id->rid = g_strdup(rId.toUtf8().data());
    }

    return id;
}

void QOrganizerEDSEngine::parseSummary(ECalComponent *comp, QtOrganizer::QOrganizerItem *item)
{
    ECalComponentText summary;
    e_cal_component_get_summary(comp, &summary);
    if (summary.value) {
        item->setDisplayLabel(QString::fromUtf8(summary.value));
    }
}

void QOrganizerEDSEngine::parseDescription(ECalComponent *comp, QtOrganizer::QOrganizerItem *item)
{

    GSList *descriptions = 0;
    e_cal_component_get_description_list(comp, &descriptions);

    QStringList itemDescription;

    for(GSList *descList = descriptions; descList != 0; descList = descList->next) {
        ECalComponentText *description = static_cast<ECalComponentText*>(descList->data);
        if (description && description->value) {
            itemDescription.append(QString::fromUtf8(description->value));
        }
    }

    item->setDescription(itemDescription.join("\n"));
    e_cal_component_free_text_list(descriptions);
}

void QOrganizerEDSEngine::parseComments(ECalComponent *comp, QtOrganizer::QOrganizerItem *item)
{
    GSList *comments = 0;
    e_cal_component_get_comment_list(comp, &comments);
    for(int ci=0, ciMax=g_slist_length(comments); ci < ciMax; ci++) {
        ECalComponentText *txt = static_cast<ECalComponentText*>(g_slist_nth_data(comments, ci));
        item->addComment(QString::fromUtf8(txt->value));
    }
    e_cal_component_free_text_list(comments);
}

void QOrganizerEDSEngine::parseTags(ECalComponent *comp, QtOrganizer::QOrganizerItem *item)
{
    GSList *categories = 0;
    e_cal_component_get_categories_list(comp, &categories);
    for(GSList *tag=categories; tag != 0; tag = tag->next) {
        item->addTag(QString::fromUtf8(static_cast<gchar*>(tag->data)));
    }
    e_cal_component_free_categories_list(categories);
}

QUrl QOrganizerEDSEngine::dencodeAttachment(ECalComponentAlarm *alarm)
{
    QUrl attachment;

    icalattach *attach = 0;
    e_cal_component_alarm_get_attach(alarm, &attach);
    if (attach) {
        if (icalattach_get_is_url(attach)) {
            const gchar *url = icalattach_get_url(attach);
            attachment = QUrl(QString::fromUtf8(url));
        }
        icalattach_unref(attach);
    }

    return attachment;
}

void QOrganizerEDSEngine::parseVisualReminderAttachment(ECalComponentAlarm *alarm, QOrganizerItemReminder *aDetail)
{
    QUrl attach = dencodeAttachment(alarm);
    if (attach.isValid()) {
        aDetail->setValue(QOrganizerItemVisualReminder::FieldDataUrl, attach);
    }

    ECalComponentText txt;
    e_cal_component_alarm_get_description(alarm, &txt);
    aDetail->setValue(QOrganizerItemVisualReminder::FieldMessage, QString::fromUtf8(txt.value));
}

void QOrganizerEDSEngine::parseAudibleReminderAttachment(ECalComponentAlarm *alarm, QOrganizerItemReminder *aDetail)
{
    QUrl attach = dencodeAttachment(alarm);
    if (attach.isValid()) {
        aDetail->setValue(QOrganizerItemAudibleReminder::FieldDataUrl, attach);
    }
}

void QOrganizerEDSEngine::parseReminders(ECalComponent *comp,
                                         QtOrganizer::QOrganizerItem *item,
                                         QList<QtOrganizer::QOrganizerItemDetail::DetailType> detailsHint)
{
    GList *alarms = e_cal_component_get_alarm_uids(comp);
    for(GList *a = alarms; a != 0; a = a->next) {
        QOrganizerItemReminder *aDetail = 0;

        QSharedPointer<ECalComponentAlarm> alarm(e_cal_component_get_alarm(comp, static_cast<const gchar*>(a->data)),
                                                 e_cal_component_alarm_free);
        if (!alarm) {
            continue;
        }
        ECalComponentAlarmAction aAction;

        e_cal_component_alarm_get_action(alarm.data(), &aAction);
        switch(aAction)
        {
            case E_CAL_COMPONENT_ALARM_DISPLAY:
                if (!detailsHint.isEmpty() &&
                    !detailsHint.contains(QOrganizerItemDetail::TypeReminder) &&
                    !detailsHint.contains(QOrganizerItemDetail::TypeVisualReminder)) {
                    continue;
                }
                aDetail = new QOrganizerItemVisualReminder();
                parseVisualReminderAttachment(alarm.data(), aDetail);
                break;
            case E_CAL_COMPONENT_ALARM_AUDIO:
                if (!detailsHint.isEmpty() &&
                    !detailsHint.contains(QOrganizerItemDetail::TypeReminder) &&
                    !detailsHint.contains(QOrganizerItemDetail::TypeAudibleReminder)) {
                    continue;
                }

            // use audio as fallback
            default:
                aDetail = new QOrganizerItemAudibleReminder();
                parseAudibleReminderAttachment(alarm.data(), aDetail);
                break;
        }

        ECalComponentAlarmTrigger trigger;
        e_cal_component_alarm_get_trigger(alarm.data(), &trigger);
        int relSecs = 0;
        bool fail = false;
        if (trigger.type == E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START) {

            relSecs = - icaldurationtype_as_int(trigger.u.rel_duration);
            if (relSecs < 0) {
                //WORKAROUND: Print warning only once, avoid flood application output
                static bool relativeStartwarningPrinted = false;
                relSecs = 0;
                if (!relativeStartwarningPrinted) {
                    fail = true;
                    relativeStartwarningPrinted = true;
                    qWarning() << "QOrganizer does not support triggers after event start";
                }
            }
        } else if (trigger.type != E_CAL_COMPONENT_ALARM_TRIGGER_NONE) {
            fail = true;
            //WORKAROUND: Print warning only once, avoid flood application output
            static bool warningPrinted = false;
            if (!warningPrinted) {
                qWarning() << "QOrganizer only supports triggers relative to event start.:" << trigger.type;
                warningPrinted = true;
            }
        }

        if (!fail) {
            aDetail->setSecondsBeforeStart(relSecs);
            ECalComponentAlarmRepeat aRepeat;
            e_cal_component_alarm_get_repeat(alarm.data(), &aRepeat);
            aDetail->setRepetition(aRepeat.repetitions, icaldurationtype_as_int(aRepeat.duration));
            item->saveDetail(aDetail);
        }
        delete aDetail;
    }
    cal_obj_uid_list_free(alarms);
}

void QOrganizerEDSEngine::parseEventsAsync(const QMap<QByteArray, GSList *> &events,
                                           bool isIcalEvents,
                                           QList<QOrganizerItemDetail::DetailType> detailsHint,
                                           QObject *source,
                                           const QByteArray &slot)
{
    QMap<QOrganizerCollectionId, GSList*> request;
    Q_FOREACH(const QByteArray &sourceId, events.keys()) {
        QOrganizerCollectionId collection = d->m_sourceRegistry->collectionId(sourceId);
        if (isIcalEvents) {
            request.insert(collection,
                           g_slist_copy_deep(events.value(sourceId),
                                             (GCopyFunc) icalcomponent_new_clone, NULL));
        } else {
            request.insert(collection,
                           g_slist_copy_deep(events.value(sourceId),
                                             (GCopyFunc) g_object_ref, NULL));
        }
    }

    // the thread will destroy itself when done
    QOrganizerParseEventThread *thread = new QOrganizerParseEventThread(source, slot);
    thread->start(request, isIcalEvents, detailsHint);
}

QList<QOrganizerItem> QOrganizerEDSEngine::parseEvents(const QOrganizerCollectionId &collectionId, GSList *events, bool isIcalEvents, QList<QOrganizerItemDetail::DetailType> detailsHint)
{
    QList<QOrganizerItem> items;
    for (GSList *l = events; l; l = l->next) {
        QOrganizerItem *item;
        ECalComponent *comp;
        if (isIcalEvents) {
            icalcomponent *clone = icalcomponent_new_clone(static_cast<icalcomponent*>(l->data));
            if (clone && icalcomponent_is_valid(clone)) {
                comp = e_cal_component_new_from_icalcomponent(clone);
            } else {
                qWarning() << "Fail to parse event";
                continue;
            }
        } else {
            comp = E_CAL_COMPONENT(l->data);
        }

        //type
        ECalComponentVType vType = e_cal_component_get_vtype(comp);
        switch(vType) {
            case E_CAL_COMPONENT_EVENT:
                item = parseEvent(comp, detailsHint);
                break;
            case E_CAL_COMPONENT_TODO:
                item = parseToDo(comp, detailsHint);
                break;
            case E_CAL_COMPONENT_JOURNAL:
                item = parseJournal(comp, detailsHint);
                break;
            case E_CAL_COMPONENT_FREEBUSY:
                qWarning() << "Component FREEBUSY not supported;";
                continue;
            case E_CAL_COMPONENT_TIMEZONE:
                qWarning() << "Component TIMEZONE not supported;";
            case E_CAL_COMPONENT_NO_TYPE:
                continue;
        }
        // id is mandatory
        parseId(comp, item, collectionId);

        if (detailsHint.isEmpty() ||
            detailsHint.contains(QOrganizerItemDetail::TypeDescription)) {
            parseDescription(comp, item);
        }

        if (detailsHint.isEmpty() ||
            detailsHint.contains(QOrganizerItemDetail::TypeDisplayLabel)) {
            parseSummary(comp, item);
        }

        if (detailsHint.isEmpty() ||
            detailsHint.contains(QOrganizerItemDetail::TypeComment)) {
            parseComments(comp, item);
        }

        if (detailsHint.isEmpty() ||
            detailsHint.contains(QOrganizerItemDetail::TypeTag)) {
            parseTags(comp, item);
        }

        if (detailsHint.isEmpty() ||
            detailsHint.contains(QOrganizerItemDetail::TypeReminder) ||
            detailsHint.contains(QOrganizerItemDetail::TypeVisualReminder) ||
            detailsHint.contains(QOrganizerItemDetail::TypeAudibleReminder) ||
            detailsHint.contains(QOrganizerItemDetail::TypeEmailReminder)) {
            parseReminders(comp, item, detailsHint);
        }

        if (detailsHint.isEmpty() ||
            detailsHint.contains(QOrganizerItemDetail::TypeEventAttendee)) {
            parseAttendeeList(comp, item);
        }

        if (detailsHint.isEmpty() ||
            detailsHint.contains(QOrganizerItemDetail::TypeExtendedDetail)) {
            parseExtendedDetails(comp, item);
        }

        items << *item;
        delete item;

        if (isIcalEvents) {
            g_object_unref(comp);
        }
    }
    return items;
}

QList<QOrganizerItem> QOrganizerEDSEngine::parseEvents(const QByteArray &sourceId,
                                                       GSList *events,
                                                       bool isIcalEvents,
                                                       QList<QOrganizerItemDetail::DetailType> detailsHint)
{
    QOrganizerCollectionId collection(managerUri(), sourceId);
    return parseEvents(collection, events, isIcalEvents, detailsHint);
}

void QOrganizerEDSEngine::parseStartTime(const QOrganizerItem &item, ECalComponent *comp)
{
    QOrganizerEventTime etr = item.detail(QOrganizerItemDetail::TypeEventTime);
    if (!etr.isEmpty()) {
        QByteArray tzId;
        struct icaltimetype ict = fromQDateTime(etr.startDateTime(), etr.isAllDay(), &tzId);
        ECalComponentDateTime dt;
        dt.tzid = tzId.isEmpty() ? NULL : tzId.constData();
        dt.value = &ict;
        e_cal_component_set_dtstart(comp, &dt);
    }
}

void QOrganizerEDSEngine::parseEndTime(const QOrganizerItem &item, ECalComponent *comp)
{
    QOrganizerEventTime etr = item.detail(QOrganizerItemDetail::TypeEventTime);
    if (!etr.isEmpty()) {
        QDateTime eventEndDateTime = etr.endDateTime();
        if (etr.startDateTime() > eventEndDateTime) {
            eventEndDateTime = etr.startDateTime();
        }

        if (etr.isAllDay() &&
            (eventEndDateTime.date() == etr.startDateTime().date())) {
            eventEndDateTime = etr.startDateTime().addDays(1);
        }

        QByteArray tzId;
        struct icaltimetype ict = fromQDateTime(eventEndDateTime, etr.isAllDay(), &tzId);
        ECalComponentDateTime dt;
        dt.tzid = tzId.isEmpty() ? NULL : tzId.constData();
        dt.value = &ict;
        e_cal_component_set_dtend(comp, &dt);
    }
}

void QOrganizerEDSEngine::parseTodoStartTime(const QOrganizerItem &item, ECalComponent *comp)
{
    QOrganizerTodoTime etr = item.detail(QOrganizerItemDetail::TypeTodoTime);
    if (!etr.isEmpty() && !etr.startDateTime().isNull()) {
        QByteArray tzId;
        struct icaltimetype ict = fromQDateTime(etr.startDateTime(), etr.isAllDay(), &tzId);
        ECalComponentDateTime dt;
        dt.tzid = tzId.isEmpty() ? NULL : tzId.constData();
        dt.value = &ict;
        e_cal_component_set_dtstart(comp, &dt);
    }
}

void QOrganizerEDSEngine::parseWeekRecurrence(const QOrganizerRecurrenceRule &qRule, struct icalrecurrencetype *rule)
{
    static QMap<Qt::DayOfWeek, icalrecurrencetype_weekday>  daysOfWeekMap;
    if (daysOfWeekMap.isEmpty()) {
        daysOfWeekMap.insert(Qt::Monday, ICAL_MONDAY_WEEKDAY);
        daysOfWeekMap.insert(Qt::Thursday, ICAL_THURSDAY_WEEKDAY);
        daysOfWeekMap.insert(Qt::Wednesday, ICAL_WEDNESDAY_WEEKDAY);
        daysOfWeekMap.insert(Qt::Tuesday, ICAL_TUESDAY_WEEKDAY);
        daysOfWeekMap.insert(Qt::Friday, ICAL_FRIDAY_WEEKDAY);
        daysOfWeekMap.insert(Qt::Saturday, ICAL_SATURDAY_WEEKDAY);
        daysOfWeekMap.insert(Qt::Sunday, ICAL_SUNDAY_WEEKDAY);
    }

    QList<Qt::DayOfWeek> daysOfWeek = qRule.daysOfWeek().toList();
    int c = 0;

    rule->freq = ICAL_WEEKLY_RECURRENCE;
    for(int d=Qt::Monday; d <= Qt::Sunday; d++) {
        if (daysOfWeek.contains(static_cast<Qt::DayOfWeek>(d))) {
            rule->by_day[c++] = daysOfWeekMap[static_cast<Qt::DayOfWeek>(d)];
        }
    }
    for (int d = c; d < ICAL_BY_DAY_SIZE; d++) {
        rule->by_day[d] = ICAL_RECURRENCE_ARRAY_MAX;
    }
}

void QOrganizerEDSEngine::parseMonthRecurrence(const QOrganizerRecurrenceRule &qRule, struct icalrecurrencetype *rule)
{
    rule->freq = ICAL_MONTHLY_RECURRENCE;

    int c = 0;
    Q_FOREACH(int daysOfMonth, qRule.daysOfMonth()) {
        rule->by_month_day[c++] = daysOfMonth;
    }
    for (int d = c; d < ICAL_BY_MONTHDAY_SIZE; d++) {
        rule->by_month_day[d] = ICAL_RECURRENCE_ARRAY_MAX;
    }
}

void QOrganizerEDSEngine::parseYearRecurrence(const QOrganizerRecurrenceRule &qRule, struct icalrecurrencetype *rule)
{
    rule->freq = ICAL_YEARLY_RECURRENCE;

    QList<int> daysOfYear = qRule.daysOfYear().toList();
    int c = 0;
    for (int d=1; d < ICAL_BY_YEARDAY_SIZE; d++) {
        if (daysOfYear.contains(d)) {
            rule->by_year_day[c++] = d;
        }
    }
    for (int d = c; d < ICAL_BY_YEARDAY_SIZE; d++) {
        rule->by_year_day[d] = ICAL_RECURRENCE_ARRAY_MAX;
    }

    c = 0;
    QList<QOrganizerRecurrenceRule::Month> monthOfYear = qRule.monthsOfYear().toList();
    for (int d=1; d < ICAL_BY_MONTH_SIZE; d++) {
        if (monthOfYear.contains(static_cast<QOrganizerRecurrenceRule::Month>(d))) {
            rule->by_month[c++] = d;
        }
    }
    for (int d = c; d < ICAL_BY_YEARDAY_SIZE; d++) {
        rule->by_month[d] = ICAL_RECURRENCE_ARRAY_MAX;
    }
}



void QOrganizerEDSEngine::parseRecurrence(const QOrganizerItem &item, ECalComponent *comp)
{
    QOrganizerItemRecurrence rec = item.detail(QOrganizerItemDetail::TypeRecurrence);
    if (!rec.isEmpty()) {
        GSList *periodList = NULL;
        Q_FOREACH(const QDate &dt, rec.recurrenceDates()) {
            ECalComponentPeriod *period = g_new0(ECalComponentPeriod, 1);
            period->start = icaltime_from_timet(QDateTime(dt).toTime_t(), FALSE);
            periodList = g_slist_append(periodList, period);
            //TODO: period.end, period.duration
        }
        e_cal_component_set_rdate_list(comp, periodList);
        e_cal_component_free_period_list(periodList);

        GSList *exdateList = 0;
        Q_FOREACH(const QDate &dt, rec.exceptionDates()) {
            ECalComponentDateTime *dateTime = g_new0(ECalComponentDateTime, 1);
            struct icaltimetype *itt = g_new0(struct icaltimetype, 1);
            *itt = icaltime_from_timet(QDateTime(dt).toTime_t(), FALSE);
            dateTime->value = itt;
            exdateList = g_slist_append(exdateList, dateTime);
        }
        e_cal_component_set_exdate_list(comp, exdateList);
        e_cal_component_free_exdate_list(exdateList);

        GSList *ruleList = 0;
        Q_FOREACH(const QOrganizerRecurrenceRule &qRule, rec.recurrenceRules()) {
            struct icalrecurrencetype *rule = g_new0(struct icalrecurrencetype, 1);
            icalrecurrencetype_clear(rule);
            switch(qRule.frequency()) {
                case QOrganizerRecurrenceRule::Daily:
                    rule->freq = ICAL_DAILY_RECURRENCE;
                    break;
                case QOrganizerRecurrenceRule::Weekly:
                    parseWeekRecurrence(qRule, rule);
                    break;
                case QOrganizerRecurrenceRule::Monthly:
                    parseMonthRecurrence(qRule, rule);
                    break;
                case QOrganizerRecurrenceRule::Yearly:
                    parseYearRecurrence(qRule, rule);
                    break;
                case QOrganizerRecurrenceRule::Invalid:
                    rule->freq = ICAL_NO_RECURRENCE;
                    break;
            }

            switch (qRule.limitType()) {
            case QOrganizerRecurrenceRule::DateLimit:
                if (qRule.limitDate().isValid()) {
                    rule->until = icaltime_null_time();
                    rule->until.year = qRule.limitDate().year();
                    rule->until.month = qRule.limitDate().month();
                    rule->until.day = qRule.limitDate().day();
                    rule->until.is_date = 1;
                }
                break;
            case QOrganizerRecurrenceRule::CountLimit:
                if (qRule.limitCount() > 0) {
                    rule->count = qRule.limitCount();
                }
                break;
            case QOrganizerRecurrenceRule::NoLimit:
            default:
                rule->count = 0;
            }

            QSet<int> positions = qRule.positions();
            for (int d=1; d < ICAL_BY_SETPOS_SIZE; d++) {
                if (positions.contains(d)) {
                    rule->by_set_pos[d] = d;
                } else {
                    rule->by_set_pos[d] = ICAL_RECURRENCE_ARRAY_MAX;
                }
            }

            rule->interval = qRule.interval();
            ruleList = g_slist_append(ruleList, rule);
        }
        e_cal_component_set_rrule_list(comp, ruleList);
        g_slist_free_full(ruleList, g_free);
    }
}

void QOrganizerEDSEngine::parsePriority(const QOrganizerItem &item, ECalComponent *comp)
{
    QOrganizerItemPriority priority = item.detail(QOrganizerItemDetail::TypePriority);
    if (!priority.isEmpty()) {
        gint iPriority = (gint) priority.priority();
        e_cal_component_set_priority(comp, &iPriority);
    }
}

void QOrganizerEDSEngine::parseLocation(const QOrganizerItem &item, ECalComponent *comp)
{
    QOrganizerItemLocation ld = item.detail(QOrganizerItemDetail::TypeLocation);
    if (!ld.isEmpty()) {
        e_cal_component_set_location(comp, ld.label().toUtf8().data());
    }
}

void QOrganizerEDSEngine::parseDueDate(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp)
{
    QOrganizerTodoTime ttr = item.detail(QOrganizerItemDetail::TypeTodoTime);
    if (!ttr.isEmpty() && !ttr.dueDateTime().isNull()) {
        QDateTime dueDateTime = ttr.dueDateTime();
        if (ttr.startDateTime() > dueDateTime) {
            dueDateTime = ttr.startDateTime();
        }

        if (ttr.isAllDay() &&
            (dueDateTime.date() == ttr.startDateTime().date())) {
            dueDateTime = ttr.startDateTime().addDays(1);
        }

        QByteArray tzId;
        struct icaltimetype ict = fromQDateTime(dueDateTime, ttr.isAllDay(), &tzId);
        ECalComponentDateTime dt;
        dt.tzid = tzId.isEmpty() ? NULL : tzId.constData();
        dt.value = &ict;
        e_cal_component_set_due(comp, &dt);;
    }
}

void QOrganizerEDSEngine::parseProgress(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp)
{
    QOrganizerTodoProgress tp = item.detail(QOrganizerItemDetail::TypeTodoProgress);
    if (!tp.isEmpty() && (tp.percentageComplete() > 0)) {
        e_cal_component_set_percent_as_int(comp, tp.percentageComplete());
    }
}

void QOrganizerEDSEngine::parseStatus(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp)
{
    QOrganizerTodoProgress tp = item.detail(QOrganizerItemDetail::TypeTodoProgress);
    if (!tp.isEmpty()) {
        switch(tp.status()) {
            case QOrganizerTodoProgress::StatusNotStarted:
                e_cal_component_set_status(comp, ICAL_STATUS_NONE);
                break;
            case QOrganizerTodoProgress::StatusInProgress:
                e_cal_component_set_status(comp, ICAL_STATUS_INPROCESS);
                break;
            case QOrganizerTodoProgress::StatusComplete:
                e_cal_component_set_status(comp, ICAL_STATUS_COMPLETED);
                break;
            default:
                e_cal_component_set_status(comp, ICAL_STATUS_CANCELLED);
                break;
        }
    }
}

void QOrganizerEDSEngine::parseAttendeeList(const QOrganizerItem &item, ECalComponent *comp)
{
    GSList *attendeeList = 0;

    Q_FOREACH(const QOrganizerEventAttendee &attendee, item.details(QOrganizerItemDetail::TypeEventAttendee)) {
        ECalComponentAttendee *calAttendee = g_new0(ECalComponentAttendee, 1);

        calAttendee->member = g_strdup(attendee.attendeeId().toUtf8().constData());
        calAttendee->cn = g_strdup(attendee.name().toUtf8().constData());
        calAttendee->value = g_strdup(attendee.emailAddress().toUtf8().constData());

        switch(attendee.participationRole()) {
        case QOrganizerEventAttendee::RoleRequiredParticipant:
            calAttendee->role = ICAL_ROLE_REQPARTICIPANT;
            break;
        case QOrganizerEventAttendee::RoleOptionalParticipant:
            calAttendee->role = ICAL_ROLE_OPTPARTICIPANT;
            break;
        case QOrganizerEventAttendee::RoleChairperson:
            calAttendee->role = ICAL_ROLE_CHAIR;
            break;
        case QOrganizerEventAttendee::RoleHost:
            calAttendee->role = ICAL_ROLE_X;
            break;
        default:
            calAttendee->role = ICAL_ROLE_NONE;
        }

        switch(attendee.participationStatus()) {
        case QOrganizerEventAttendee::StatusAccepted:
            calAttendee->status = ICAL_PARTSTAT_ACCEPTED;
            break;
        case QOrganizerEventAttendee::StatusDeclined:
            calAttendee->status = ICAL_PARTSTAT_DECLINED;
            break;
        case QOrganizerEventAttendee::StatusTentative:
            calAttendee->status = ICAL_PARTSTAT_TENTATIVE;
            break;
        case QOrganizerEventAttendee::StatusDelegated:
            calAttendee->status = ICAL_PARTSTAT_DELEGATED;
            break;
        case QOrganizerEventAttendee::StatusInProcess:
            calAttendee->status = ICAL_PARTSTAT_INPROCESS;
            break;
        case QOrganizerEventAttendee::StatusCompleted:
            calAttendee->status = ICAL_PARTSTAT_COMPLETED;
            break;
        case QOrganizerEventAttendee::StatusUnknown:
        default:
            calAttendee->status = ICAL_PARTSTAT_NONE;
            break;
        }
        attendeeList = g_slist_append(attendeeList, calAttendee);
    }
    e_cal_component_set_attendee_list(comp, attendeeList);
    e_cal_component_free_attendee_list(attendeeList);
}

void QOrganizerEDSEngine::parseExtendedDetails(const QOrganizerItem &item, ECalComponent *comp)
{
    icalcomponent *icalcomp = e_cal_component_get_icalcomponent(comp);
    Q_FOREACH(const QOrganizerItemExtendedDetail &ex, item.details(QOrganizerItemDetail::TypeExtendedDetail)) {
        // We only support QByteArray.
        // We could use QStream serialization but it will make it impossible to read it from glib side, for example indicators.
        QByteArray data = ex.data().toByteArray();
        if (data.isEmpty()) {
            qWarning() << "Invalid value for property" << ex.name()
                       <<". EDS only supports QByteArray values for extended properties";
            continue;
        }

        icalproperty *xProp = icalproperty_new_x(data.constData());
        icalproperty_set_x_name(xProp, ex.name().toUtf8().constData());
        icalcomponent_add_property(icalcomp, xProp);
    }
}

bool QOrganizerEDSEngine::hasRecurrence(ECalComponent *comp)
{
    char *rid = e_cal_component_get_recurid_as_string(comp);
    bool result = (rid && strcmp(rid, "0"));
    if (rid) {
        free(rid);
    }
    return result;
}

void QOrganizerEDSEngine::parseId(ECalComponent *comp,
                                  QOrganizerItem *item,
                                  const QOrganizerCollectionId &collectionId)
{
    ECalComponentId *id = e_cal_component_get_id(comp);

    if (collectionId.isNull()) {
        qWarning() << "Parse Id with null collection";
        return;
    }

    QByteArray iId(id->uid);
    QByteArray rId(id->rid);
    if (!rId.isEmpty()) {
        iId += "#" + rId;
    }

    QByteArray itemGuid =
        iId.contains(':') ? iId.mid(iId.lastIndexOf(':') + 1) : iId;
    QOrganizerItemId itemId = idFromEds(collectionId, itemGuid);
    item->setId(itemId);
    item->setGuid(QString::fromUtf8(itemId.localId()));

    if (!rId.isEmpty()) {
        QOrganizerItemParent itemParent = item->detail(QOrganizerItemDetail::TypeParent);
        QOrganizerItemId parentId(idFromEds(collectionId, QByteArray(id->uid)));
        itemParent.setParentId(parentId);
        item->saveDetail(&itemParent);
    }

    item->setCollectionId(collectionId);
    e_cal_component_free_id(id);
}

ECalComponent *QOrganizerEDSEngine::createDefaultComponent(ECalClient *client,
                                                           icalcomponent_kind iKind,
                                                           ECalComponentVType eType)
{
    ECalComponent *comp;
    icalcomponent *icalcomp = 0;

    if (client && !e_cal_client_get_default_object_sync(client, &icalcomp, NULL, NULL)) {
        icalcomp = icalcomponent_new(iKind);
    }

    comp = e_cal_component_new();
    if (icalcomp && !e_cal_component_set_icalcomponent(comp, icalcomp)) {
        icalcomponent_free(icalcomp);
    }

    e_cal_component_set_new_vtype(comp, eType);

    return comp;
}

ECalComponent *QOrganizerEDSEngine::parseEventItem(ECalClient *client, const QOrganizerItem &item)
{
    ECalComponent *comp = createDefaultComponent(client, ICAL_VEVENT_COMPONENT, E_CAL_COMPONENT_EVENT);

    parseStartTime(item, comp);
    parseEndTime(item, comp);
    parseRecurrence(item, comp);
    parsePriority(item, comp);
    parseLocation(item, comp);
    return comp;

}

ECalComponent *QOrganizerEDSEngine::parseTodoItem(ECalClient *client, const QOrganizerItem &item)
{
    ECalComponent *comp = createDefaultComponent(client, ICAL_VTODO_COMPONENT, E_CAL_COMPONENT_TODO);

    parseTodoStartTime(item, comp);
    parseDueDate(item, comp);
    parseRecurrence(item, comp);
    parsePriority(item, comp);
    parseProgress(item, comp);
    parseStatus(item, comp);

    return comp;
}

ECalComponent *QOrganizerEDSEngine::parseJournalItem(ECalClient *client, const QOrganizerItem &item)
{
    ECalComponent *comp = createDefaultComponent(client, ICAL_VJOURNAL_COMPONENT, E_CAL_COMPONENT_JOURNAL);

    QOrganizerJournalTime jtime = item.detail(QOrganizerItemDetail::TypeJournalTime);
    if (!jtime.isEmpty()) {
        QByteArray tzId;
        struct icaltimetype ict = fromQDateTime(jtime.entryDateTime(), false, &tzId);
        ECalComponentDateTime dt;
        dt.tzid = tzId.isEmpty() ? NULL : tzId.constData();
        dt.value = &ict;
        e_cal_component_set_dtstart(comp, &dt);
    }

    return comp;
}

void QOrganizerEDSEngine::parseSummary(const QOrganizerItem &item, ECalComponent *comp)
{
    //summary
    if (!item.displayLabel().isEmpty()) {
        ECalComponentText txt;
        QByteArray str = item.displayLabel().toUtf8();
        txt.altrep = 0;
        txt.value = str.constData();
        e_cal_component_set_summary(comp, &txt);
    }
}

void QOrganizerEDSEngine::parseDescription(const QOrganizerItem &item, ECalComponent *comp)
{
    //description
    if (!item.description().isEmpty()) {
        GSList *descriptions = 0;
        QList<QByteArray> descList;

        Q_FOREACH(const QString &desc, item.description().split("\n")) {
            QByteArray str = desc.toUtf8();
            ECalComponentText *txt = g_new0(ECalComponentText, 1);
            txt->value = str.constData();
            descriptions = g_slist_append(descriptions, txt);
            // keep str alive until the property gets updated
            descList << str;
        }

        e_cal_component_set_description_list(comp, descriptions);
        e_cal_component_free_text_list(descriptions);
    }
}

void QOrganizerEDSEngine::parseComments(const QOrganizerItem &item, ECalComponent *comp)
{
    //comments
    GSList *comments = 0;
    QList<QByteArray> commentList;

    Q_FOREACH(const QString &comment, item.comments()) {
        QByteArray str = comment.toUtf8();
        ECalComponentText *txt = g_new0(ECalComponentText, 1);
        txt->value = str.constData();
        comments = g_slist_append(comments, txt);
        // keep str alive until the property gets updated
        commentList << str;
    }

    if (comments) {
        e_cal_component_set_comment_list(comp, comments);
        e_cal_component_free_text_list(comments);
    }
}

void QOrganizerEDSEngine::parseTags(const QOrganizerItem &item, ECalComponent *comp)
{
    //tags
    GSList *categories = 0;
    QList<QByteArray> tagList;

    Q_FOREACH(const QString &tag, item.tags()) {
        QByteArray str = tag.toUtf8();
        categories = g_slist_append(categories, str.data());
        // keep str alive until the property gets updated
        tagList << str;
    }

    if (categories) {
        e_cal_component_set_categories_list(comp, categories);
        g_slist_free(categories);
    }
}

void QOrganizerEDSEngine::encodeAttachment(const QUrl &url, ECalComponentAlarm *alarm)
{
    if (!url.isEmpty()) {
        icalattach *attach = icalattach_new_from_url(url.toString().toUtf8());
        e_cal_component_alarm_set_attach(alarm, attach);
        icalattach_unref(attach);
    }
}

void QOrganizerEDSEngine::parseVisualReminderAttachment(const QOrganizerItemDetail &detail, ECalComponentAlarm *alarm)
{
    ECalComponentText txt;
    QByteArray str = detail.value(QOrganizerItemVisualReminder::FieldMessage).toString().toUtf8();
    if (!str.isEmpty()) {
        txt.altrep = 0;
        txt.value = str.constData();
        e_cal_component_alarm_set_description(alarm, &txt);
    }
    encodeAttachment(detail.value(QOrganizerItemVisualReminder::FieldDataUrl).toUrl(), alarm);
}

void QOrganizerEDSEngine::parseAudibleReminderAttachment(const QOrganizerItemDetail &detail, ECalComponentAlarm *alarm)
{
    encodeAttachment(detail.value(QOrganizerItemAudibleReminder::FieldDataUrl).toUrl(), alarm);
}

void QOrganizerEDSEngine::parseReminders(const QOrganizerItem &item, ECalComponent *comp)
{
    //reminders
    QList<QOrganizerItemDetail> reminders = item.details(QOrganizerItemDetail::TypeAudibleReminder);
    reminders += item.details(QOrganizerItemDetail::TypeVisualReminder);

    Q_FOREACH(const QOrganizerItemDetail &detail, reminders) {
        const QOrganizerItemReminder *reminder = static_cast<const QOrganizerItemReminder*>(&detail);
        ECalComponentAlarm *alarm = e_cal_component_alarm_new();
        switch(reminder->type())
        {
            case QOrganizerItemReminder::TypeVisualReminder:
                e_cal_component_alarm_set_action(alarm, E_CAL_COMPONENT_ALARM_DISPLAY);
                parseVisualReminderAttachment(detail, alarm);
                break;
            case QOrganizerItemReminder::TypeAudibleReminder:
            default:
                // use audio as fallback
                e_cal_component_alarm_set_action(alarm, E_CAL_COMPONENT_ALARM_AUDIO);
                parseAudibleReminderAttachment(detail, alarm);
                break;
        }

        ECalComponentAlarmTrigger trigger;
        trigger.type = E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START;
        trigger.u.rel_duration = icaldurationtype_from_int(- reminder->secondsBeforeStart());
        e_cal_component_alarm_set_trigger(alarm, trigger);

        ECalComponentAlarmRepeat aRepeat;
        // TODO: check if this is really necessary
        aRepeat.repetitions = reminder->repetitionCount(); //qMax(reminder->repetitionCount(), 1);
        aRepeat.duration = icaldurationtype_from_int(reminder->repetitionDelay());
        e_cal_component_alarm_set_repeat(alarm, aRepeat);

        e_cal_component_add_alarm(comp, alarm);
        e_cal_component_alarm_free(alarm);
    }
}

QOrganizerItemId
QOrganizerEDSEngine::idFromEds(const QOrganizerCollectionId &collectionId,
                               const gchar *uid)
{
    return QOrganizerItemId(collectionId.managerUri(),
                            collectionId.localId() + '/' + QByteArray(uid));
}

QByteArray QOrganizerEDSEngine::idToEds(const QOrganizerItemId &itemId,
                                        QByteArray *sourceId)
{
    QList<QByteArray> ids = itemId.localId().split('/');
    if (ids.length() == 2) {
        if (sourceId) *sourceId = ids[0];
        return ids[1];
    } else {
        if (sourceId) *sourceId = QByteArray();
        return QByteArray();
    }
}

GSList *QOrganizerEDSEngine::parseItems(ECalClient *client,
                                        QList<QOrganizerItem> items,
                                        bool *hasRecurrence)
{
    GSList *comps = 0;

    Q_FOREACH(const QOrganizerItem &item, items) {
        ECalComponent *comp = 0;

        *hasRecurrence = ((item.type() == QOrganizerItemType::TypeTodoOccurrence) ||
                          (item.type() == QOrganizerItemType::TypeEventOccurrence));

        switch(item.type()) {
            case QOrganizerItemType::TypeEvent:
            case QOrganizerItemType::TypeEventOccurrence:
                comp = parseEventItem(client, item);
                break;
            case QOrganizerItemType::TypeTodo:
            case QOrganizerItemType::TypeTodoOccurrence:
                comp = parseTodoItem(client, item);
                break;
            case QOrganizerItemType::TypeJournal:
                comp = parseJournalItem(client, item);
                break;
            case QOrganizerItemType::TypeNote:
                qWarning() << "Component TypeNote not supported;";
            case QOrganizerItemType::TypeUndefined:
                continue;
        }
        parseId(item, comp);
        parseSummary(item, comp);
        parseDescription(item, comp);
        parseComments(item, comp);
        parseTags(item, comp);
        parseReminders(item, comp);
        parseAttendeeList(item, comp);
        parseExtendedDetails(item, comp);

        if (!item.id().isNull()) {
            e_cal_component_commit_sequence(comp);
        } else {
            e_cal_component_abort_sequence(comp);
        }

        comps = g_slist_append(comps,
                               icalcomponent_new_clone(e_cal_component_get_icalcomponent(comp)));

        g_object_unref(comp);
    }

    return comps;
}

void QOrganizerEDSEngine::parseId(const QOrganizerItem &item, ECalComponent *comp)
{
    QOrganizerItemId itemId = item.id();
    if (!itemId.isNull()) {
        QByteArray fullItemId = idToEds(itemId);
        QByteArray rId;
        QByteArray cId = toComponentId(fullItemId, &rId);

        e_cal_component_set_uid(comp, cId.data());

        if (!rId.isEmpty()) {
            ECalComponentRange recur_id;

            // use component tz on recurrence id
            ECalComponentDateTime dt;
            e_cal_component_get_dtstart(comp, &dt);
            dt.value = g_new0(icaltimetype, 1);
            *dt.value = icaltime_from_string(rId.data());

            recur_id.type = E_CAL_COMPONENT_RANGE_SINGLE;
            recur_id.datetime = dt;
            e_cal_component_set_recurid(comp, &recur_id);
            e_cal_component_free_datetime(&dt);
        }
    }
}
