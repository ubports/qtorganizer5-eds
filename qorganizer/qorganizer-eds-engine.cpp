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
#include "qorganizer-eds-engineid.h"
#include "qorganizer-eds-collection-engineid.h"
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

#include <QtCore/qdebug.h>
#include <QtCore/QPointer>
#include <QtCore/QCoreApplication>
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

    Q_FOREACH(const QString &collectionId, d->m_sourceRegistry->collectionsIds()){
        onSourceAdded(collectionId);
    }
    connect(d->m_sourceRegistry, SIGNAL(sourceAdded(QString)), SLOT(onSourceAdded(QString)));
    connect(d->m_sourceRegistry, SIGNAL(sourceRemoved(QString)), SLOT(onSourceRemoved(QString)));
    d->m_sourceRegistry->load();
}

QOrganizerEDSEngine::~QOrganizerEDSEngine()
{
    Q_FOREACH(QOrganizerAbstractRequest *req, m_runningRequests.keys()) {
        req->cancel();
    }

    Q_ASSERT(m_runningRequests.count() == 0);
    d->m_sharedEngines.remove(this);
    if (!d->m_refCount.deref()) {
        delete d;
        m_globalData = 0;
    }
}

QString QOrganizerEDSEngine::managerName() const
{
    return QOrganizerEDSEngineId::managerNameStatic();
}

/*! \reimp
*/
QMap<QString, QString> QOrganizerEDSEngine::managerParameters() const
{
    QMap<QString, QString> params;
    return params;
}

void QOrganizerEDSEngine::itemsAsync(QOrganizerItemFetchRequest *req)
{
    FetchRequestData *data = new FetchRequestData(this,
                                                  d->m_sourceRegistry->collectionsIds(),
                                                  req);
    itemsAsyncStart(data);
}

void QOrganizerEDSEngine::itemsAsyncStart(FetchRequestData *data)
{
    // check if request was destroyed by the caller
    if (!data->isLive()) {
        releaseRequestData(data);
        return;
    }

    QString collection = data->nextCollection();
    if (!collection.isEmpty()) {
        EClient *client = data->parent()->d->m_sourceRegistry->client(collection);
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
        delete data;
    }
}

void QOrganizerEDSEngine::itemsAsyncDone(FetchRequestData *data)
{
    if (data->isLive()) {
        itemsAsyncStart(data);
    } else {
        releaseRequestData(data);
    }
}

void QOrganizerEDSEngine::itemsAsyncListed(ECalComponent *comp,
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
    }
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
        }
        delete data;
        return;
    }

    // check if request was destroyed by the caller
    if (data->isLive()) {
        data->appendResults(data->parent()->parseEvents(data->collection(), events, false));
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

    QString id = data->nextId();
    if (!id.isEmpty()) {
        QStringList ids = id.split("/");
        if (ids.length() == 2) {
            Q_ASSERT(ids.length() == 2);
            QString collectionId = ids[0];
            QString rId;
            QString itemId = QOrganizerEDSEngineId::toComponentId(ids[1], &rId);

            EClient *client = data->parent()->d->m_sourceRegistry->client(collectionId);
            if (client) {
                data->setClient(client);
                e_cal_client_get_object(data->client(),
                                        itemId.toUtf8().data(),
                                        rId.toUtf8().data(),
                                        data->cancellable(),
                                        (GAsyncReadyCallback) QOrganizerEDSEngine::itemsByIdAsyncListed,
                                        data);
                g_object_unref(client);
                return;
            }
        }
    } else if (data->end()) {
        data->finish();
        delete data;
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
        QList<QOrganizerItem> items = data->parent()->parseEvents(data->currentCollectionId(), events, true);
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

    QString rId;
    QString cId = QOrganizerEDSEngineId::toComponentId(req->parentItem().id(), &rId);

    EClient *client = data->parent()->d->m_sourceRegistry->client(req->parentItem().collectionId().toString());
    if (client) {
        data->setClient(client);
        e_cal_client_get_object(data->client(),
                                cId.toUtf8(), rId.toUtf8(),
                                data->cancellable(),
                                (GAsyncReadyCallback) QOrganizerEDSEngine::itemOcurrenceAsyncGetObjectDone,
                                data);
        g_object_unref(client);
    } else {
        qWarning() << "Fail to find collection:" << req->parentItem().collectionId();
        data->finish(QOrganizerManager::DoesNotExistError);
        delete data;
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
        }
        delete data;
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
    }
    releaseRequestData(data);
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

    QString collectionId = data->nextCollection();

    if (collectionId.isNull() && data->end()) {
        data->finish();
        releaseRequestData(data);
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

        if (collectionId.isEmpty() && createItems) {
            collectionId = data->parent()->d->m_sourceRegistry->defaultCollection().id().toString();
        }

        EClient *client = data->parent()->d->m_sourceRegistry->client(collectionId);
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
        qWarning() << "Fail to create items:" << gError->message;
        g_error_free(gError);
        gError = 0;

        if (data->isLive()) {
            Q_FOREACH(const QOrganizerItem &i, data->workingItems()) {
                data->appendResult(i, QOrganizerManager::UnspecifiedError);
            }
        }
    } else if (data->isLive()) {
        QString currentCollectionId = data->currentCollection();
        if (currentCollectionId.isEmpty()) {
            currentCollectionId = data->parent()->defaultCollection(0).id().toString();
        }
        QList<QOrganizerItem> items = data->workingItems();
        for(uint i=0, iMax=g_slist_length(uids); i < iMax; i++) {
            QOrganizerItem &item = items[i];
            const gchar *uid = static_cast<const gchar*>(g_slist_nth_data(uids, i));

            QOrganizerEDSEngineId *eid = new QOrganizerEDSEngineId(currentCollectionId,
                                                                   QString::fromUtf8(uid));
            item.setId(QOrganizerItemId(eid));
            item.setGuid(QString::fromUtf8(uid));

            QOrganizerEDSCollectionEngineId *edsCollectionId = new QOrganizerEDSCollectionEngineId(currentCollectionId);
            item.setCollectionId(QOrganizerCollectionId(edsCollectionId));
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

    QString collectionId = data->next();
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
    releaseRequestData(data);
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
        EClient *client = data->parent()->d->m_sourceRegistry->client(collection.toString());
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
    releaseRequestData(data);
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

QOrganizerCollection QOrganizerEDSEngine::defaultCollection(QOrganizerManager::Error* error)
{
    qWarning() << Q_FUNC_INFO << "Not implemented";
    if (error) {
        *error = QOrganizerManager::NoError;
    }
    return d->m_sourceRegistry->defaultCollection();
}

QOrganizerCollection QOrganizerEDSEngine::collection(const QOrganizerCollectionId& collectionId,
                                                     QOrganizerManager::Error* error)
{
    QOrganizerCollection collection = d->m_sourceRegistry->collection(collectionId.toString());
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

    *error = req->error();

    if (*error == QOrganizerManager::NoError) {
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
        saveCollectionUpdateAsyncStart(requestData);
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
            releaseRequestData(data);
            return;
        }
    } else if (data->isLive()) {
        data->commitSourceCreated();
        data->prepareToUpdate();
        saveCollectionUpdateAsyncStart(data);
    }
}

void QOrganizerEDSEngine::saveCollectionUpdateAsyncStart(SaveCollectionRequestData *data)
{
    // check if request was destroyed by the caller
    if (!data->isLive()) {
        releaseRequestData(data);
        return;
    }

    ESource *source = data->nextSourceToUpdate();
    if (source) {
        e_source_write(source,
                       data->cancellable(),
                       (GAsyncReadyCallback) QOrganizerEDSEngine::saveCollectionUpdateAsynCommited,
                       data);
    } else {
        data->finish();
        releaseRequestData(data);
    }
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
        saveCollectionUpdateAsyncStart(data);
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

    *error = req->error();
    return(*error == QOrganizerManager::NoError);
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
        if (e_source_get_remote_deletable(source)) {
            data->setRemoteDeletable(true);
            e_source_remote_delete(source, data->cancellable(),
                                   (GAsyncReadyCallback) QOrganizerEDSEngine::removeCollectionAsyncStart,
                                   data);
        } else if (e_source_get_removable(source)) {
            e_source_remove(source, data->cancellable(),
                            (GAsyncReadyCallback) QOrganizerEDSEngine::removeCollectionAsyncStart,
                            data);
        } else {
            qWarning() << "Source not removable";
            data->commit(QOrganizerManager::InvalidCollectionError);
        }
    } else {
        data->finish();
        releaseRequestData(data);
    }
}

void QOrganizerEDSEngine::releaseRequestData(RequestData *data)
{
    if (data->cancelled()) {
        // if request was cancelled data will be destroyed later
        data->continueCancel();
    } else {
        delete data;
    }
}

void QOrganizerEDSEngine::requestDestroyed(QOrganizerAbstractRequest* req)
{
    RequestData *data = m_runningRequests.take(req);
    if (data) {
        data->cancel();
        delete data;
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
    return false;
}

bool QOrganizerEDSEngine::waitForRequestFinished(QOrganizerAbstractRequest* req, int msecs)
{
    Q_ASSERT(req);
    Q_UNUSED(msecs);

    QPointer<QOrganizerAbstractRequest> r(req);

    while(r && (r->state() == QOrganizerAbstractRequest::ActiveState)) {
        QCoreApplication::processEvents();
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

void QOrganizerEDSEngine::onSourceAdded(const QString &collectionId)
{
    d->watch(collectionId);
}

void QOrganizerEDSEngine::onSourceRemoved(const QString &collectionId)
{
    d->unWatch(collectionId);
}

void QOrganizerEDSEngine::onViewChanged(QOrganizerItemChangeSet *change)
{
    change->emitSignals(this);
}

QDateTime QOrganizerEDSEngine::fromIcalTime(struct icaltimetype value, const char *tzId)
{
    uint tmTime;

    if (tzId) {
        QByteArray tzName(tzId);
        // keep only the timezone name.
        tzName = tzName.replace("/freeassociation.sourceforge.net/Tzfile/", "");
        const icaltimezone *timezone = const_cast<const icaltimezone *>(icaltimezone_get_builtin_timezone(tzName.constData()));
        tmTime = icaltime_as_timet_with_zone(value, timezone);
        return QDateTime::fromTime_t(tmTime, QTimeZone(tzId));
    } else {
        tmTime = icaltime_as_timet(value);
        return QDateTime::fromTime_t(tmTime);
    }
}

icaltimetype QOrganizerEDSEngine::fromQDateTime(const QDateTime &dateTime,
                                                bool allDay,
                                                QByteArray *tzId)
{

    if (dateTime.timeSpec() == Qt::TimeZone) {
        const icaltimezone *timezone = 0;
        *tzId = dateTime.timeZone().id();
        timezone = const_cast<const icaltimezone *>(icaltimezone_get_builtin_timezone(tzId->constData()));
        return icaltime_from_timet_with_zone(dateTime.toTime_t(), allDay, timezone);
    } else {
        return icaltime_from_timet(dateTime.toTime_t(), allDay);
    }

}

void QOrganizerEDSEngine::parseStartTime(ECalComponent *comp, QOrganizerItem *item)
{
    ECalComponentDateTime *dt = g_new0(ECalComponentDateTime, 1);
    e_cal_component_get_dtstart(comp, dt);
    if (dt->value) {
        QOrganizerEventTime etr = item->detail(QOrganizerItemDetail::TypeEventTime);
        etr.setStartDateTime(fromIcalTime(*dt->value, dt->tzid));
        if (icaltime_is_date(*dt->value) != etr.isAllDay()) {
            etr.setAllDay(icaltime_is_date(*dt->value));
        }
        item->saveDetail(&etr);
    }
    e_cal_component_free_datetime(dt);
}

void QOrganizerEDSEngine::parseTodoStartTime(ECalComponent *comp, QOrganizerItem *item)
{
    ECalComponentDateTime *dt = g_new0(ECalComponentDateTime, 1);
    e_cal_component_get_dtstart(comp, dt);
    if (dt->value) {
        QOrganizerTodoTime etr = item->detail(QOrganizerItemDetail::TypeTodoTime);
        etr.setStartDateTime(fromIcalTime(*dt->value, dt->tzid));
        if (icaltime_is_date(*dt->value) != etr.isAllDay()) {
            etr.setAllDay(icaltime_is_date(*dt->value));
        }
        item->saveDetail(&etr);
    }
    e_cal_component_free_datetime(dt);
}

void QOrganizerEDSEngine::parseEndTime(ECalComponent *comp, QOrganizerItem *item)
{
    ECalComponentDateTime *dt = g_new0(ECalComponentDateTime, 1);
    e_cal_component_get_dtend(comp, dt);
    if (dt->value) {
        QOrganizerEventTime etr = item->detail(QOrganizerItemDetail::TypeEventTime);
        etr.setEndDateTime(fromIcalTime(*dt->value, dt->tzid));
        if (icaltime_is_date(*dt->value) != etr.isAllDay()) {
            etr.setAllDay(icaltime_is_date(*dt->value));
        }
        item->saveDetail(&etr);
    }
    e_cal_component_free_datetime(dt);
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
            } else {
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
        ttr.setDueDateTime(fromIcalTime(*due.value, due.tzid));
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
    if (percentage >= 0 && percentage <= 100) {
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

        qAttendee.setName(QString::fromUtf8(attendee->cn));
        qAttendee.setEmailAddress(QString::fromUtf8(attendee->member));

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

QOrganizerItem *QOrganizerEDSEngine::parseEvent(ECalComponent *comp)
{
    QOrganizerItem *event;
    if (hasRecurrence(comp)) {
        event = new QOrganizerEventOccurrence();
    } else {
        event = new QOrganizerEvent();
    }
    parseStartTime(comp, event);
    parseEndTime(comp, event);
    parseRecurrence(comp, event);
    parsePriority(comp, event);
    parseLocation(comp, event);
    return event;
}

QOrganizerItem *QOrganizerEDSEngine::parseToDo(ECalComponent *comp)
{
    QOrganizerItem *todo;
    if (hasRecurrence(comp)) {
        todo = new QOrganizerTodoOccurrence();
    } else {
        todo = new QOrganizerTodo();
    }
    parseTodoStartTime(comp, todo);
    parseDueDate(comp, todo);
    parseRecurrence(comp, todo);
    parsePriority(comp, todo);
    parseProgress(comp, todo);
    parseStatus(comp, todo);
    return todo;
}

QOrganizerItem *QOrganizerEDSEngine::parseJournal(ECalComponent *comp)
{
    QOrganizerJournal *journal = new QOrganizerJournal();

    ECalComponentDateTime dt;
    e_cal_component_get_dtstart(comp, &dt);
    if (dt.value) {
        QOrganizerJournalTime jtime;
        jtime.setEntryDateTime(fromIcalTime(*dt.value, dt.tzid));
        journal->saveDetail(&jtime);
    }
    e_cal_component_free_datetime(&dt);

    return journal;
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

void QOrganizerEDSEngine::parseReminders(ECalComponent *comp, QtOrganizer::QOrganizerItem *item)
{
    GList *alarms = e_cal_component_get_alarm_uids(comp);
    for(GList *a = alarms; a != 0; a = a->next) {
        QOrganizerItemReminder *aDetail = 0;

        ECalComponentAlarm *alarm = e_cal_component_get_alarm(comp, static_cast<const gchar*>(a->data));
        if (!alarm) {
            continue;
        }
        ECalComponentAlarmAction aAction;

        e_cal_component_alarm_get_action(alarm, &aAction);
        switch(aAction)
        {
            case E_CAL_COMPONENT_ALARM_DISPLAY:
                aDetail = new QOrganizerItemVisualReminder();
                parseVisualReminderAttachment(alarm, aDetail);
                break;
            case E_CAL_COMPONENT_ALARM_AUDIO:
            // use audio as fallback
            default:
                aDetail = new QOrganizerItemAudibleReminder();
                parseAudibleReminderAttachment(alarm, aDetail);
                break;
        }

        ECalComponentAlarmTrigger trigger;
        e_cal_component_alarm_get_trigger(alarm, &trigger);
        if (trigger.type == E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START) {
            aDetail->setSecondsBeforeStart(icaldurationtype_as_int(trigger.u.rel_duration) * -1);
        }

        ECalComponentAlarmRepeat aRepeat;
        e_cal_component_alarm_get_repeat(alarm, &aRepeat);
        aDetail->setRepetition(aRepeat.repetitions, icaldurationtype_as_int(aRepeat.duration));

        item->saveDetail(aDetail);
        delete aDetail;
    }
}

QList<QOrganizerItem> QOrganizerEDSEngine::parseEvents(const QString &collectionId,
                                                       GSList *events,
                                                       bool isIcalEvents)
{
    QOrganizerEDSCollectionEngineId *collection = d->m_sourceRegistry->collectionEngineId(collectionId);
    QList<QOrganizerItem> items;
    for (GSList *l = events; l; l = l->next) {
        QOrganizerItem *item;
        ECalComponent *comp;
        if (isIcalEvents) {
            icalcomponent *clone = icalcomponent_new_clone(static_cast<icalcomponent*>(l->data));
            comp = e_cal_component_new_from_icalcomponent(clone);
        } else {
            comp = E_CAL_COMPONENT(l->data);
        }

        //type
        ECalComponentVType vType = e_cal_component_get_vtype(comp);
        switch(vType) {
            case E_CAL_COMPONENT_EVENT:
                item = parseEvent(comp);
                break;
            case E_CAL_COMPONENT_TODO:
                item = parseToDo(comp);
                break;
            case E_CAL_COMPONENT_JOURNAL:
                item = parseJournal(comp);
                break;
            case E_CAL_COMPONENT_FREEBUSY:
                qWarning() << "Component FREEBUSY not supported;";
                continue;
            case E_CAL_COMPONENT_TIMEZONE:
                qWarning() << "Component TIMEZONE not supported;";
            case E_CAL_COMPONENT_NO_TYPE:
                continue;
        }
        parseId(comp, item, collection);
        parseDescription(comp, item);
        parseSummary(comp, item);
        parseComments(comp, item);
        parseTags(comp, item);
        parseReminders(comp, item);
        parseAttendeeList(comp, item);

        items << *item;
        delete item;

        if (isIcalEvents) {
            g_object_unref(comp);
        }
    }
    return items;
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
        QByteArray tzId;
        struct icaltimetype ict = fromQDateTime(etr.endDateTime(), etr.isAllDay(), &tzId);
        ECalComponentDateTime dt;
        dt.tzid = tzId.isEmpty() ? NULL : tzId.constData();
        dt.value = &ict;
        e_cal_component_set_dtend(comp, &dt);
    }
}

void QOrganizerEDSEngine::parseTodoStartTime(const QOrganizerItem &item, ECalComponent *comp)
{
    QOrganizerTodoTime etr = item.detail(QOrganizerItemDetail::TypeTodoTime);
    if (!etr.isEmpty()) {
        QByteArray tzId;
        struct icaltimetype ict = fromQDateTime(etr.startDateTime(), etr.isAllDay(), &tzId);
        ECalComponentDateTime dt;
        dt.tzid = tzId.isEmpty() ? NULL : tzId.constData();
        dt.value = &ict;
        e_cal_component_set_dtstart(comp, &dt);;
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

    QList<int> daysOfMonth = qRule.daysOfMonth().toList();
    int c = 0;
    for (int d=1; d < ICAL_BY_MONTHDAY_SIZE; d++) {
        if (daysOfMonth.contains(d)) {
            rule->by_month_day[c++] = d;
        }
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
        GSList *periodList = 0;
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

            if (qRule.limitDate().isValid()) {
                rule->until = icaltime_from_timet(QDateTime(qRule.limitDate()).toTime_t(), TRUE);
                rule->count = ICAL_RECURRENCE_ARRAY_MAX;
            } else {
                rule->count = qRule.limitCount();
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
    if (!ttr.isEmpty()) {
        QByteArray tzId;
        struct icaltimetype ict = fromQDateTime(ttr.dueDateTime(), ttr.isAllDay(), &tzId);
        ECalComponentDateTime dt;
        dt.tzid = tzId.isEmpty() ? NULL : tzId.constData();
        dt.value = &ict;
        e_cal_component_set_due(comp, &dt);;
    }
}

void QOrganizerEDSEngine::parseProgress(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp)
{
    QOrganizerTodoProgress tp = item.detail(QOrganizerItemDetail::TypeTodoProgress);
    if (!tp.isEmpty()) {
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

        calAttendee->cn = g_strdup(attendee.name().toUtf8().constData());
        calAttendee->value = g_strconcat("MAILTO:", attendee.emailAddress().toUtf8().constData(), NULL);
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
                                  QOrganizerEDSCollectionEngineId *edsCollectionId)
{
    ECalComponentId *id = e_cal_component_get_id(comp);
    QOrganizerEDSEngineId *edsParentId = 0;
    QOrganizerEDSEngineId *edsId = QOrganizerEDSEngineId::fromComponentId(edsCollectionId->m_collectionId, id, &edsParentId);

    item->setId(QOrganizerItemId(edsId));
    item->setGuid(QString::fromUtf8(id->uid));

    if (edsParentId) {
        QOrganizerItemParent itemParent = item->detail(QOrganizerItemDetail::TypeParent);
        itemParent.setParentId(QOrganizerItemId(edsParentId));
        item->saveDetail(&itemParent);
    }

    QOrganizerCollectionId cId = QOrganizerCollectionId(edsCollectionId);
    item->setCollectionId(cId);
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
        ECalComponentText *txt = g_new0(ECalComponentText, 1);
        txt->value = str.constData();
        categories = g_slist_append(categories, txt);
        // keep str alive until the property gets updated
        tagList << str;
    }

    if (categories) {
        e_cal_component_set_categories_list(comp, categories);
        e_cal_component_free_text_list(categories);
    }
}

void QOrganizerEDSEngine::encodeAttachment(const QUrl &url, ECalComponentAlarm *alarm)
{
    icalattach *attach = icalattach_new_from_url(url.toString().toUtf8());
    e_cal_component_alarm_set_attach(alarm, attach);
    icalattach_unref(attach);
}

void QOrganizerEDSEngine::parseVisualReminderAttachment(const QOrganizerItemDetail &detail, ECalComponentAlarm *alarm)
{
    ECalComponentText txt;
    QByteArray str = detail.value(QOrganizerItemVisualReminder::FieldMessage).toString().toUtf8();
    txt.altrep = 0;
    txt.value = str.constData();
    e_cal_component_alarm_set_description(alarm, &txt);
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

        if (reminder->secondsBeforeStart() > 0) {
            ECalComponentAlarmTrigger trigger;
            trigger.type = E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START;
            trigger.u.rel_duration = icaldurationtype_from_int(- reminder->secondsBeforeStart());
            e_cal_component_alarm_set_trigger(alarm, trigger);
        }

        ECalComponentAlarmRepeat aRepeat;
        // TODO: check if this is really necessary
        aRepeat.repetitions = reminder->repetitionCount(); //qMax(reminder->repetitionCount(), 1);
        aRepeat.duration = icaldurationtype_from_int(reminder->repetitionDelay());
        e_cal_component_alarm_set_repeat(alarm, aRepeat);

        e_cal_component_add_alarm(comp, alarm);
        e_cal_component_alarm_free(alarm);
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
        QString rId;
        QString cId = QOrganizerEDSEngineId::toComponentId(itemId, &rId);

        e_cal_component_set_uid(comp, cId.toUtf8().data());

        if (!rId.isEmpty()) {
            ECalComponentRange *recur_id;
            struct icaltimetype tt = icaltime_from_string(rId.toUtf8().data());

            recur_id = g_new0 (ECalComponentRange, 1);
            recur_id->type = E_CAL_COMPONENT_RANGE_SINGLE;
            recur_id->datetime.value = &tt;
            e_cal_component_set_recurid(comp, recur_id);
            g_free (recur_id);
        }
    }
}
