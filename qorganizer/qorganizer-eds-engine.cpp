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
#include "qorganizer-eds-saverequestdata.h"
#include "qorganizer-eds-removerequestdata.h"

#include <QtCore/qdebug.h>
#include <QtCore/QPointer>
#include <QtCore/qstringbuilder.h>
#include <QtCore/quuid.h>

#include <QtOrganizer/QOrganizerEventAttendee>
#include <QtOrganizer/QOrganizerItemLocation>
#include <QtOrganizer/QOrganizerEventTime>
#include <QtOrganizer/QOrganizerItemFetchRequest>
#include <QtOrganizer/QOrganizerItemSaveRequest>
#include <QtOrganizer/QOrganizerItemRemoveRequest>
#include <QtOrganizer/QOrganizerCollectionFetchRequest>
#include <QtOrganizer/QOrganizerEvent>
#include <QtOrganizer/QOrganizerTodo>
#include <QtOrganizer/QOrganizerTodoTime>
#include <QtOrganizer/QOrganizerJournal>
#include <QtOrganizer/QOrganizerJournalTime>

#include <glib.h>
#include <libecal/libecal.h>
#include <libical/ical.h>

using namespace QtOrganizer;

QOrganizerEDSEngine* QOrganizerEDSEngine::createEDSEngine(const QMap<QString, QString>& parameters)
{
    Q_UNUSED(parameters);
    return new QOrganizerEDSEngine();
}

QOrganizerEDSEngine::QOrganizerEDSEngine()
{
    qDebug() << Q_FUNC_INFO;
    loadCollections();
}

QOrganizerEDSEngine::~QOrganizerEDSEngine()
{
    qDebug() << Q_FUNC_INFO;
    m_collections.clear();
    m_collectionsMap.clear();
}

QString QOrganizerEDSEngine::managerName() const
{
    return QStringLiteral("eds");
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
    qDebug() << Q_FUNC_INFO;
    FetchRequestData *data = new FetchRequestData(this, req);
    m_pendingFetchRequest << data;
    itemsAsyncStart(data);
}

void QOrganizerEDSEngine::itemsAsyncStart(FetchRequestData *data)
{
    qDebug() << Q_FUNC_INFO;
    QOrganizerEDSCollectionEngineId *collection = data->nextCollection();
    if (collection) {
        e_cal_client_connect(collection->m_esource,
                             E_CAL_CLIENT_SOURCE_TYPE_EVENTS,
                             data->cancellable(),
                             (GAsyncReadyCallback) QOrganizerEDSEngine::itemsAsyncConnected,
                             data);
    } else {
        data->finish();
        delete data;
    }
}

void QOrganizerEDSEngine::itemsAsyncConnected(GObject *source_object,
                                              GAsyncResult *res,
                                              FetchRequestData *data)
{
    Q_UNUSED(source_object);
    qDebug() << Q_FUNC_INFO;
    GError *gError = 0;
    EClient *client = e_cal_client_connect_finish(res, &gError);

    if (gError) {
        qWarning() << "Fail to open calendar" << gError->message;
        g_error_free(gError);
        gError = 0;
        data->finish(QOrganizerManager::InvalidCollectionError);
        delete data;
    } else {
        data->setClient(client);
        e_cal_client_get_object_list_as_comps(E_CAL_CLIENT(client),
                                              data->dateFilter().toUtf8().data(),
                                              data->cancellable(),
                                              (GAsyncReadyCallback) QOrganizerEDSEngine::itemsAsyncListed,
                                              data);
    }
}

void QOrganizerEDSEngine::itemsAsyncListed(GObject *source_object,
                                           GAsyncResult *res,
                                           FetchRequestData *data)
{
    Q_UNUSED(source_object);
    qDebug() << Q_FUNC_INFO;
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
        data->finish(QOrganizerManager::InvalidCollectionError);
        delete data;
        return;
    } else {
        qDebug() << "Query size:" << g_slist_length(events);
        data->appendResults(parseEvents(data->collection(), events));
    }
    itemsAsyncStart(data);
}



QList<QOrganizerItem> QOrganizerEDSEngine::items(const QList<QOrganizerItemId> &itemIds,
                                                 const QOrganizerItemFetchHint &fetchHint,
                                                 QMap<int, QOrganizerManager::Error> *errorMap,
                                                 QOrganizerManager::Error *error)
{
    qDebug() << Q_FUNC_INFO;
    Q_UNUSED(fetchHint)
    Q_UNUSED(itemIds);
    Q_UNUSED(errorMap);

    QList<QOrganizerItem> items;
    return items;
}

QList<QOrganizerItem> QOrganizerEDSEngine::items(const QOrganizerItemFilter &filter,
                                                 const QDateTime &startDateTime,
                                                 const QDateTime &endDateTime,
                                                 int maxCount,
                                                 const QList<QOrganizerItemSortOrder> &sortOrders,
                                                 const QOrganizerItemFetchHint &fetchHint,
                                                 QOrganizerManager::Error *error)
{
    qDebug() << Q_FUNC_INFO;
    QList<QOrganizerItem> list;
    return list;
}



QList<QOrganizerItemId> QOrganizerEDSEngine::itemIds(const QOrganizerItemFilter &filter,
                                                     const QDateTime &startDateTime,
                                                     const QDateTime &endDateTime,
                                                     const QList<QOrganizerItemSortOrder> &sortOrders,
                                                     QOrganizerManager::Error *error)
{
    qDebug() << Q_FUNC_INFO;
    QList<QOrganizerItemId> items;
    return items;
}

QList<QOrganizerItem> QOrganizerEDSEngine::itemOccurrences(const QOrganizerItem &parentItem,
                                                                  const QDateTime &startDateTime,
                                                                  const QDateTime &endDateTime, int maxCount,
                                                                  const QOrganizerItemFetchHint &fetchHint,
                                                                  QOrganizerManager::Error *error)
{
    qDebug() << Q_FUNC_INFO;
    Q_UNUSED(fetchHint);
    return QList<QOrganizerItem>();
}

QList<QOrganizerItem> QOrganizerEDSEngine::itemsForExport(const QDateTime &startDateTime,
                                                                 const QDateTime &endDateTime,
                                                                 const QOrganizerItemFilter &filter,
                                                                 const QList<QOrganizerItemSortOrder> &sortOrders,
                                                                 const QOrganizerItemFetchHint &fetchHint,
                                                                 QOrganizerManager::Error *error)
{
    qDebug() << Q_FUNC_INFO;
    return QList<QOrganizerItem>();
}

void QOrganizerEDSEngine::saveItemsAsync(QOrganizerItemSaveRequest *req)
{
    qDebug() << Q_FUNC_INFO;
    if (req->items().count() == 0) {
        QOrganizerManagerEngine::updateItemSaveRequest(req,
                                                       QList<QOrganizerItem>(),
                                                       QOrganizerManager::NoError,
                                                       QMap<int, QOrganizerManager::Error>(),
                                                       QOrganizerAbstractRequest::FinishedState);
        return;
    }

    //TODO: support multiple items
    Q_ASSERT(req->items().count() <= 1);

    QOrganizerItem item = req->items().at(0);
    QOrganizerEDSCollectionEngineId *collectionEngineId = 0;
    QOrganizerCollectionId collectionId = item.collectionId();

    if (collectionId.isNull()) {
        collectionId = m_defaultCollection.id();
    }

    Q_ASSERT(!collectionId.isNull());
    collectionEngineId = m_collectionsMap[collectionId.toString()];

    SaveRequestData *data = new SaveRequestData(this, req, collectionId);
    e_cal_client_connect(collectionEngineId->m_esource,
                         E_CAL_CLIENT_SOURCE_TYPE_EVENTS,
                         data->cancellable(),
                         (GAsyncReadyCallback) QOrganizerEDSEngine::saveItemsAsyncConnected,
                         data);
}

void QOrganizerEDSEngine::saveItemsAsyncConnected(GObject *source_object,
                                                  GAsyncResult *res,
                                                  SaveRequestData *data)
{
    Q_UNUSED(source_object);

    GError *gError = 0;
    EClient *client = e_cal_client_connect_finish(res, &gError);
    if (gError) {
        qWarning() << "Fail to open calendar" << gError->message;
        g_error_free(gError);
        gError = 0;
        data->finish(QOrganizerManager::InvalidCollectionError);
        delete data;
    } else {
        data->setClient(client);

        GSList *comps = parseItems(data->request<QOrganizerItemSaveRequest>()->items());
        if (comps) {
            if (data->isNew()) {
                e_cal_client_create_objects(E_CAL_CLIENT(client),
                                            comps,
                                            data->cancellable(),
                                            (GAsyncReadyCallback) QOrganizerEDSEngine::saveItemsAsyncCreated,
                                            data);
            } else {
                e_cal_client_modify_objects(E_CAL_CLIENT(client),
                                            comps,
                                            E_CAL_OBJ_MOD_ALL,
                                            data->cancellable(),
                                            (GAsyncReadyCallback) QOrganizerEDSEngine::saveItemsAsyncModified,
                                            data);
            }
            g_slist_free_full(comps, (GDestroyNotify) icalcomponent_free);
        } else {
            qWarning() << "Fail to translate items";
            data->finish(QOrganizerManager::BadArgumentError);
            delete data;
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
        data->finish(QOrganizerManager::InvalidDetailError);
        delete data;
    } else {
        data->appendResults(data->request<QOrganizerItemSaveRequest>()->items());
        data->finish();
        delete data;
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
        qWarning() << "Fail to create items" << gError->message;
        g_error_free(gError);
        gError = 0;
        data->finish(QOrganizerManager::InvalidDetailError);
        delete data;
    } else {
        QList<QOrganizerItem> items = data->request<QOrganizerItemSaveRequest>()->items();
        for(uint i=0, iMax=g_slist_length(uids); i < iMax; i++) {
            QOrganizerItem &item = items[i];
            const gchar *uid = static_cast<const gchar*>(g_slist_nth_data(uids, i));
            qDebug() << "Item created:" << uid;

            item.setCollectionId(data->collectionId());
            QOrganizerEDSEngineId *eid = new QOrganizerEDSEngineId(data->collectionId().toString(),
                                                                   QString::fromUtf8(uid),
                                                                   data->parent()->managerUri());
            item.setId(QOrganizerItemId(eid));
            item.setGuid(QString::fromUtf8(uid));
        }
        g_slist_free_full(uids, g_free);
        data->appendResults(items);
        data->finish();
        delete data;
    }
}

bool QOrganizerEDSEngine::saveItems(QList<QtOrganizer::QOrganizerItem> *items,
                                    const QList<QtOrganizer::QOrganizerItemDetail::DetailType> &detailMask,
                                    QMap<int, QtOrganizer::QOrganizerManager::Error> *errorMap,
                                    QtOrganizer::QOrganizerManager::Error *error)

{
    qDebug() << Q_FUNC_INFO;
    *error = QOrganizerManager::NoError;
    return true;
}

void QOrganizerEDSEngine::removeItemsAsync(QOrganizerItemRemoveRequest *req)
{
    qDebug() << Q_FUNC_INFO;
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
    qDebug() << Q_FUNC_INFO;
    QOrganizerCollectionId collection = data->begin();
    if (!collection.isNull()) {
        QOrganizerEDSCollectionEngineId *collectionEngineId = data->parent()->m_collectionsMap[collection.toString()];
        qDebug() << "REmove items from sourcE: " << e_source_get_display_name(collectionEngineId->m_esource);
        e_cal_client_connect(collectionEngineId->m_esource,
                             E_CAL_CLIENT_SOURCE_TYPE_EVENTS,
                             data->cancellable(),
                             (GAsyncReadyCallback) QOrganizerEDSEngine::removeItemsAsyncConnected,
                             data);
    } else {
        qWarning() << "Item source is null";
        data->finish();
        delete data;
    }
}

void QOrganizerEDSEngine::removeItemsAsyncConnected(GObject *source_object,
                                                    GAsyncResult *res,
                                                    RemoveRequestData *data)
{
    Q_UNUSED(source_object);

    GError *gError = 0;
    EClient *client = e_cal_client_connect_finish(res, &gError);
    if (gError) {
        qWarning() << "Fail to open calendar" << gError->message;
        g_error_free(gError);
        gError = 0;
        data->finish(QOrganizerManager::InvalidCollectionError);
        delete data;
    } else {
        data->setClient(client);
        GSList *ids = data->compIds();
        qDebug() << "Remove item size: " << g_slist_length(ids);
        e_cal_client_remove_objects(E_CAL_CLIENT(client),
                                    ids,
                                    E_CAL_OBJ_MOD_ALL,
                                    data->cancellable(),
                                    (GAsyncReadyCallback) QOrganizerEDSEngine::removeItemsAsyncRemoved,
                                    data);
    }
}

void QOrganizerEDSEngine::removeItemsAsyncRemoved(GObject *source_object,
                                                  GAsyncResult *res,
                                                  RemoveRequestData *data)
{
    Q_UNUSED(source_object);

    GError *gError = 0;
    e_cal_client_remove_objects_finish(E_CAL_CLIENT(data->client()), res, &gError);
    if (gError) {
        qWarning() << "Fail to remove Items" << gError->message;
        g_error_free(gError);
        gError = 0;
        data->finish(QOrganizerManager::InvalidCollectionError);
        delete data;
    } else {
        qDebug() << "Item removed";
        data->commit();
        removeItemsAsyncStart(data);
    }
}

bool QOrganizerEDSEngine::removeItems(const QList<QOrganizerItemId> &itemIds,
                                      QMap<int, QOrganizerManager::Error> *errorMap,
                                      QOrganizerManager::Error *error)
{
    qDebug() << Q_FUNC_INFO;
    *error = QOrganizerManager::NoError;
    return true;
}

QOrganizerCollection QOrganizerEDSEngine::defaultCollection(QOrganizerManager::Error* error)
{
    qDebug() << Q_FUNC_INFO;
    *error = QOrganizerManager::NoError;
    return m_defaultCollection;
}

QOrganizerCollection QOrganizerEDSEngine::collection(const QOrganizerCollectionId& collectionId, QOrganizerManager::Error* error)
{
    qDebug() << Q_FUNC_INFO;
    *error = QOrganizerManager::DoesNotExistError;
    return QOrganizerCollection();
}

QList<QOrganizerCollection> QOrganizerEDSEngine::collections(QOrganizerManager::Error* error)
{
    qDebug() << Q_FUNC_INFO;
    *error = QOrganizerManager::NoError;
    return m_collections;
}

bool QOrganizerEDSEngine::saveCollection(QOrganizerCollection* collection, QOrganizerManager::Error* error)
{
    qDebug() << Q_FUNC_INFO;
    *error = QOrganizerManager::NoError;
    return true;

}

bool QOrganizerEDSEngine::removeCollection(const QOrganizerCollectionId& collectionId, QOrganizerManager::Error* error)
{
    qDebug() << Q_FUNC_INFO;
    *error = QOrganizerManager::NoError;
    return true;
}

void QOrganizerEDSEngine::requestDestroyed(QOrganizerAbstractRequest* req)
{
    qDebug() << Q_FUNC_INFO;
    Q_UNUSED(req);
}

bool QOrganizerEDSEngine::startRequest(QOrganizerAbstractRequest* req)
{
    qDebug() << Q_FUNC_INFO;

    if (!req)
        return false;


    switch (req->type())
    {
        case QOrganizerAbstractRequest::ItemFetchRequest:
            itemsAsync(qobject_cast<QOrganizerItemFetchRequest*>(req));
            break;
        case QOrganizerAbstractRequest::CollectionFetchRequest:
            QOrganizerManagerEngine::updateCollectionFetchRequest(qobject_cast<QOrganizerCollectionFetchRequest*>(req),
                                                                  m_collections,
                                                                  QOrganizerManager::NoError,
                                                                  QOrganizerAbstractRequest::FinishedState);
            break;
        case QOrganizerAbstractRequest::ItemSaveRequest:
            saveItemsAsync(qobject_cast<QOrganizerItemSaveRequest*>(req));
            break;
        case QOrganizerAbstractRequest::ItemRemoveRequest:
            removeItemsAsync(qobject_cast<QOrganizerItemRemoveRequest*>(req));
            break;
        default:
            qDebug() << "No implemented request" << req->type();
            break;
    }

    return true;
}

bool QOrganizerEDSEngine::cancelRequest(QOrganizerAbstractRequest* req)
{
    qDebug() << Q_FUNC_INFO;
    Q_UNUSED(req); // we can't cancel since we complete immediately
    return false;
}

bool QOrganizerEDSEngine::waitForRequestFinished(QOrganizerAbstractRequest* req, int msecs)
{
    qDebug() << Q_FUNC_INFO;
    // in our implementation, we always complete any operation we start.
    Q_UNUSED(msecs);
    Q_UNUSED(req);

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

void QOrganizerEDSEngine::loadCollections()
{
    m_collections.clear();
    m_collectionsMap.clear();

    GError *error = 0;
    ESourceRegistry *registry = e_source_registry_new_sync(0, &error);
    if (error) {
        qWarning() << "Fail to create sourge registry:" << error->message;
        g_error_free(error);
        return;
    }

    ESource *defaultSource = e_source_registry_ref_default_address_book(registry);
    GList *sources = e_source_registry_list_sources(registry, E_SOURCE_EXTENSION_CALENDAR);
    for(int i=0, iMax=g_list_length(sources); i < iMax; i++) {
        ESource *source = E_SOURCE(g_list_nth_data(sources, i));
        QOrganizerCollection collection;
        QOrganizerEDSCollectionEngineId *edsId = new QOrganizerEDSCollectionEngineId(source, managerUri());
        QOrganizerCollectionId id(edsId);
        collection.setId(id);
        collection.setMetaData(QOrganizerCollection::KeyName,
                               QString::fromUtf8(e_source_get_display_name(source)));
        //TODO get metadata (color, etc..)
        m_collections << collection;
        m_collectionsMap.insert(id.toString(), edsId);

        if (e_source_compare_by_display_name(source, defaultSource) == 0) {
            m_defaultCollection = collection;
        }
    }

    g_list_free_full (sources, g_object_unref);
    g_object_unref(registry);
    if (defaultSource) {
        g_object_unref(defaultSource);
    }
    qDebug() << m_collections.count() << "Collection loaded";
}

QDateTime QOrganizerEDSEngine::fromIcalTime(struct icaltimetype value)
{
    struct tm tmTime = icaltimetype_to_tm(&value);
    g_date_time_new_from_unix_local(mktime(&tmTime));
    return QDateTime::fromTime_t(mktime(&tmTime));
}

void QOrganizerEDSEngine::parseStartTime(ECalComponent *comp, QOrganizerItem *item)
{
    ECalComponentDateTime dt;
    e_cal_component_get_dtstart(comp, &dt);
    if (dt.value) {
        QOrganizerEventTime etr = item->detail(QOrganizerItemDetail::TypeEventTime);
        etr.setStartDateTime(fromIcalTime(*dt.value));
        item->saveDetail(&etr);
    }
    e_cal_component_free_datetime(&dt);
}

void QOrganizerEDSEngine::parseEndTime(ECalComponent *comp, QOrganizerItem *item)
{
    ECalComponentDateTime dt;
    e_cal_component_get_dtend(comp, &dt);
    if (dt.value) {
        QOrganizerEventTime etr = item->detail(QOrganizerItemDetail::TypeEventTime);
        etr.setEndDateTime(fromIcalTime(*dt.value));
        item->saveDetail(&etr);
    }
    e_cal_component_free_datetime(&dt);
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
            QDateTime dt = fromIcalTime(period->start);
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
            QDateTime dt = fromIcalTime(*dateTime->value);
            dates.insert(dt.date());
        }
        e_cal_component_free_exdate_list(exdateList);

        QOrganizerItemRecurrence irec = item->detail(QOrganizerItemDetail::TypeRecurrence);
        irec.setExceptionDates(dates);
        item->saveDetail(&irec);
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
        ttr.setDueDateTime(fromIcalTime(*due.value));
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

QOrganizerItem *QOrganizerEDSEngine::parseEvent(ECalComponent *comp)
{
    QOrganizerEvent *event = new QOrganizerEvent();
    parseStartTime(comp, event);
    parseEndTime(comp, event);
    parseRecurrence(comp, event);
    parsePriority(comp, event);
    parseLocation(comp, event);
    return event;
}

QOrganizerItem *QOrganizerEDSEngine::parseToDo(ECalComponent *comp)
{
    QOrganizerTodo *todo = new QOrganizerTodo();
    parseStartTime(comp, todo);
    parseDueDate(comp, todo);
    parseRecurrence(comp, todo);
    parsePriority(comp, todo);
    parseProgress(comp, todo);
    parseStatus(comp, todo);

    //TODO: finishedDateTime
    return todo;
}

QOrganizerItem *QOrganizerEDSEngine::parseJournal(ECalComponent *comp)
{
    QOrganizerJournal *journal = new QOrganizerJournal();

    ECalComponentDateTime dt;
    e_cal_component_get_dtstart(comp, &dt);
    if (dt.value) {
        QOrganizerJournalTime jtime;
        jtime.setEntryDateTime(fromIcalTime(*dt.value));
        journal->saveDetail(&jtime);
    }
    e_cal_component_free_datetime(&dt);

    return journal;
}

QList<QOrganizerItem> QOrganizerEDSEngine::parseEvents(QOrganizerEDSCollectionEngineId *collection, GSList *events)
{
    QList<QOrganizerItem> items;
    for(int i=0, iMax=g_slist_length(events); i < iMax; i++) {
        QOrganizerItem *item;
        ECalComponent *comp = E_CAL_COMPONENT(g_slist_nth_data(events, i));


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

        //id
        const gchar *uid = 0;
        e_cal_component_get_uid(comp, &uid);
        QOrganizerCollectionId cId = QOrganizerCollectionId(collection);

        QOrganizerEDSEngineId *eid = new QOrganizerEDSEngineId(collection->m_collectionId,
                                                               QString::fromUtf8(uid),
                                                               collection->managerUri());
        item->setId(QOrganizerItemId(eid));
        item->setCollectionId(cId);
        qDebug() << ">>>>>>>>>>>>>>>>>>Loaded item id: " << item->id().toString();

        //summary
        ECalComponentText summary;
        e_cal_component_get_summary(comp, &summary);
        if (summary.value) {
            item->setDisplayLabel(QString::fromUtf8(summary.value));
        }

        //comments
        GSList *comments = 0;
        e_cal_component_get_comment_list(comp, &comments);
        for(int ci=0, ciMax=g_slist_length(comments); ci < ciMax; ci++) {
            ECalComponentText *txt = static_cast<ECalComponentText*>(g_slist_nth_data(comments, ci));
            item->addComment(QString::fromUtf8(txt->value));
        }
        e_cal_component_free_text_list(comments);

        //tags
        GSList *categories = 0;
        e_cal_component_get_categories_list(comp, &categories);
        for(int ci=0, ciMax=g_slist_length(comments); ci < ciMax; ci++) {
            item->addTag(QString::fromUtf8(static_cast<gchar*>(g_slist_nth_data(categories, ci))));
        }
        e_cal_component_free_categories_list(categories);


//        //Attendee
//        GList *attendeeList = 0;
//        e_cal_component_get_attendee_list(comp, &attendeeList);
//        for(int ci=0, ciMax=g_list_length(attendeeList); ci < ciMax; ci++) {
//            ECalComponentAttendee *attendee = static_cast<ECalComponentAttendee *>(g_list_nth_data(attendeeList, ci));
//            QOrganizerEventAttendee qAttendee;
//            qAttendee.setAttendeeId(QString::fromUtf8(attendee->member));
//            qAttendee.setEmailAddress(QString::fromUtf8(attendee->member));
//            qAttendee.setName(QString::fromUtf8(attendee->member));
//            //TODO: check
//            qAttendee.setParticipationRole(QOrganizerEventAttendee::RoleRequiredParticipant);
//            qAttendee.setParticipationStatus(QOrganizerEventAttendee::StatusAccepted);

//            item.saveDetail(&qAttendee);
//        }

        items << *item;
        delete item;
    }
    return items;
}

void QOrganizerEDSEngine::parseStartTime(const QOrganizerItem &item, ECalComponent *comp)
{
    QOrganizerEventTime etr = item.detail(QOrganizerItemDetail::TypeEventTime);
    if (!etr.isEmpty()) {
        ECalComponentDateTime dt;
        struct icaltimetype itt = icaltime_from_timet(etr.startDateTime().toTime_t(), FALSE);
        dt.value = &itt;
        e_cal_component_set_dtstart(comp, &dt);
    }
}

void QOrganizerEDSEngine::parseEndTime(const QOrganizerItem &item, ECalComponent *comp)
{
    QOrganizerEventTime etr = item.detail(QOrganizerItemDetail::TypeEventTime);
    if (!etr.isEmpty()) {
        ECalComponentDateTime dt;
        struct icaltimetype itt = icaltime_from_timet(etr.endDateTime().toTime_t(), FALSE);
        dt.value = &itt;
        e_cal_component_set_dtend(comp, &dt);;
    }
}

void QOrganizerEDSEngine::parseRecurrence(const QOrganizerItem &item, ECalComponent *comp)
{
    QOrganizerItemRecurrence rec = item.detail(QOrganizerItemDetail::TypeRecurrence);
    if (!rec.isEmpty()) {
        GSList *periodList = 0;
        Q_FOREACH(QDate dt, rec.recurrenceDates()) {
            ECalComponentPeriod *period = g_new0(ECalComponentPeriod, 1);
            period->start = icaltime_from_timet(QDateTime(dt).toTime_t(), TRUE);
            periodList = g_slist_append(periodList, period);
            //TODO: period.end, period.duration
        }
        e_cal_component_set_rdate_list(comp, periodList);
        e_cal_component_free_period_list(periodList);

        GSList *exdateList = 0;
        Q_FOREACH(QDate dt, rec.exceptionDates()) {
            ECalComponentDateTime dateTime;
            struct icaltimetype itt = icaltime_from_timet(QDateTime(dt).toTime_t(), TRUE);
            dateTime.value = &itt;
            exdateList = g_slist_append(exdateList, &dateTime);
        }
        e_cal_component_set_exdate_list(comp, exdateList);
        e_cal_component_free_exdate_list(exdateList);
    }
    // TODO: exeptions rules
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
        ECalComponentDateTime due;
        struct icaltimetype itt = icaltime_from_timet(ttr.dueDateTime().toTime_t(), FALSE);
        due.value = &itt;
        e_cal_component_set_due(comp, &due);
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

ECalComponent *QOrganizerEDSEngine::parseEventItem(const QOrganizerItem &item)
{
    ECalComponent *comp = e_cal_component_new();
    e_cal_component_set_new_vtype(comp, E_CAL_COMPONENT_EVENT);

    parseStartTime(item, comp);
    parseEndTime(item, comp);
    parseRecurrence(item, comp);
    parsePriority(item, comp);
    parseLocation(item, comp);
    return comp;

}

ECalComponent *QOrganizerEDSEngine::parseTodoItem(const QOrganizerItem &item)
{
    ECalComponent *comp = e_cal_component_new();
    e_cal_component_set_new_vtype(comp, E_CAL_COMPONENT_TODO);

    parseStartTime(item, comp);
    parseDueDate(item, comp);
    parseRecurrence(item, comp);
    parsePriority(item, comp);
    parseProgress(item, comp);
    parseStatus(item, comp);

    return comp;
}

ECalComponent *QOrganizerEDSEngine::parseJournalItem(const QOrganizerItem &item)
{
    ECalComponent *comp = e_cal_component_new();
    e_cal_component_set_new_vtype(comp, E_CAL_COMPONENT_JOURNAL);

    QOrganizerJournalTime jtime = item.detail(QOrganizerItemDetail::TypeJournalTime);
    if (!jtime.isEmpty()) {
        ECalComponentDateTime *dt = g_new0(ECalComponentDateTime, 1);
        *dt->value = icaltime_from_timet(jtime.entryDateTime().toTime_t(), FALSE);
        e_cal_component_set_dtstart(comp, dt);
        e_cal_component_free_datetime(dt);
    }

    return comp;
}

GSList *QOrganizerEDSEngine::parseItems(QList<QOrganizerItem> items)
{
    GSList *comps = 0;

    Q_FOREACH(QOrganizerItem item, items) {
        ECalComponent *comp = 0;

        switch(item.type()) {
            case QOrganizerItemType::TypeEvent:
                comp = parseEventItem(item);
                break;
            case QOrganizerItemType::TypeTodo:
                comp = parseTodoItem(item);
                break;
            case QOrganizerItemType::TypeJournal:
                comp = parseJournalItem(item);
                break;
            case QOrganizerItemType::TypeEventOccurrence:
                qWarning() << "Component TypeEventOccurrence not supported;";
                continue;
            case QOrganizerItemType::TypeTodoOccurrence:
                qWarning() << "Component TypeTodoOccurrence not supported;";
                continue;
            case QOrganizerItemType::TypeNote:
                qWarning() << "Component TypeNote not supported;";
            case QOrganizerItemType::TypeUndefined:
                continue;
        }

        // id
        if (!item.id().isNull()) {
           QOrganizerItemId id = item.id();
           QString cId = QOrganizerEDSEngineId::toComponentId(id);
           e_cal_component_set_uid(comp, cId.toUtf8().data());
        }

        //summary
        if (!item.displayLabel().isEmpty()) {
            ECalComponentText txt;
            txt.altrep = "";
            txt.value = item.displayLabel().toUtf8().data();
            e_cal_component_set_summary(comp, &txt);
        }

        //comments
        GSList *comments = 0;
        Q_FOREACH(QString comment, item.comments()) {
            ECalComponentText *txt = g_new0(ECalComponentText, 1);
            txt->value = comment.toUtf8().data();
            comments = g_slist_append(comments, txt);
        }
        e_cal_component_set_comment_list(comp, comments);
        e_cal_component_free_text_list(comments);

        //tags
        GSList *categories = 0;
        Q_FOREACH(QString tag, item.tags()) {
            ECalComponentText *txt = g_new0(ECalComponentText, 1);
            txt->value = tag.toUtf8().data();
            categories = g_slist_append(categories, txt);
        }
        e_cal_component_set_categories_list(comp, categories);
        e_cal_component_free_text_list(categories);

        comps = g_slist_append(comps,
                               icalcomponent_new_clone(e_cal_component_get_icalcomponent(comp)));
        g_object_unref(comp);
    }

    return comps;
}

