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

#ifndef QORGANIZER_EDS_ENGINE_H
#define QORGANIZER_EDS_ENGINE_H

#include <QExplicitlySharedDataPointer>

#include <QtOrganizer/QOrganizerCollectionId>
#include <QtOrganizer/QOrganizerItemId>
#include <QtOrganizer/QOrganizerItemFetchHint>
#include <QtOrganizer/QOrganizerManager>
#include <QtOrganizer/QOrganizerManagerEngine>
#include <QtOrganizer/QOrganizerItemSortOrder>
#include <QtOrganizer/QOrganizerItemFilter>
#include <QtOrganizer/QOrganizerItemChangeSet>
#include <QtOrganizer/QOrganizerCollectionId>
#include <QtOrganizer/QOrganizerItemReminder>
#include <QtOrganizer/QOrganizerItemOccurrenceFetchRequest>

#include <libecal/libecal.h>

class RequestData;
class FetchRequestData;
class FetchByIdRequestData;
class FetchOcurrenceData;
class SaveRequestData;
class RemoveRequestData;
class RemoveByIdRequestData;
class SaveCollectionRequestData;
class RemoveCollectionRequestData;
class ViewWatcher;
class QOrganizerEDSEngineData;

class QOrganizerEDSEngine : public QtOrganizer::QOrganizerManagerEngine
{
    Q_OBJECT

public:
    static QOrganizerEDSEngine *createEDSEngine(const QMap<QString, QString>& parameters);

    ~QOrganizerEDSEngine();

    // URI reporting
    QString managerName() const override;

    // items
    QList<QtOrganizer::QOrganizerItem> items(const QList<QtOrganizer::QOrganizerItemId> &itemIds,
                                              const QtOrganizer::QOrganizerItemFetchHint &fetchHint,
                                              QMap<int, QtOrganizer::QOrganizerManager::Error> *errorMap,
                                              QtOrganizer::QOrganizerManager::Error *error);

    QList<QtOrganizer::QOrganizerItem> items(const QtOrganizer::QOrganizerItemFilter &filter,
                                              const QDateTime &startDateTime,
                                              const QDateTime &endDateTime,
                                              int maxCount,
                                              const QList<QtOrganizer::QOrganizerItemSortOrder> &sortOrders,
                                              const QtOrganizer::QOrganizerItemFetchHint &fetchHint,
                                              QtOrganizer::QOrganizerManager::Error *error);


    QList<QtOrganizer::QOrganizerItemId> itemIds(const QtOrganizer::QOrganizerItemFilter &filter,
                                                  const QDateTime &startDateTime,
                                                  const QDateTime &endDateTime,
                                                  const QList<QtOrganizer::QOrganizerItemSortOrder> &sortOrders,
                                                  QtOrganizer::QOrganizerManager::Error *error);

    QList<QtOrganizer::QOrganizerItem> itemOccurrences(const QtOrganizer::QOrganizerItem &parentItem,
                                                        const QDateTime &startDateTime,
                                                        const QDateTime &endDateTime,
                                                        int maxCount,
                                                        const QtOrganizer::QOrganizerItemFetchHint &fetchHint,
                                                        QtOrganizer::QOrganizerManager::Error *error);

    QList<QtOrganizer::QOrganizerItem> itemsForExport(const QDateTime &startDateTime,
                                                       const QDateTime &endDateTime,
                                                       const QtOrganizer::QOrganizerItemFilter &filter,
                                                       const QList<QtOrganizer::QOrganizerItemSortOrder> &sortOrders,
                                                       const QtOrganizer::QOrganizerItemFetchHint &fetchHint,
                                                       QtOrganizer::QOrganizerManager::Error *error);

    bool saveItems(QList<QtOrganizer::QOrganizerItem> *items,
                   const QList<QtOrganizer::QOrganizerItemDetail::DetailType> &detailMask,
                   QMap<int, QtOrganizer::QOrganizerManager::Error> *errorMap,
                   QtOrganizer::QOrganizerManager::Error *error);

    bool removeItems(const QList<QtOrganizer::QOrganizerItemId> &itemIds,
                     QMap<int, QtOrganizer::QOrganizerManager::Error> *errorMap,
                     QtOrganizer::QOrganizerManager::Error *error);

    // collections
    QtOrganizer::QOrganizerCollectionId defaultCollectionId() const override;
    QtOrganizer::QOrganizerCollection collection(const QtOrganizer::QOrganizerCollectionId &collectionId,
                                                  QtOrganizer::QOrganizerManager::Error *error);
    QList<QtOrganizer::QOrganizerCollection> collections(QtOrganizer::QOrganizerManager::Error* error);
    bool saveCollection(QtOrganizer::QOrganizerCollection* collection, QtOrganizer::QOrganizerManager::Error* error);
    bool removeCollection(const QtOrganizer::QOrganizerCollectionId& collectionId, QtOrganizer::QOrganizerManager::Error* error);

    // Asynchronous Request Support
    virtual void requestDestroyed(QtOrganizer::QOrganizerAbstractRequest* req);
    virtual bool startRequest(QtOrganizer::QOrganizerAbstractRequest* req);
    virtual bool cancelRequest(QtOrganizer::QOrganizerAbstractRequest* req);
    virtual bool waitForRequestFinished(QtOrganizer::QOrganizerAbstractRequest* req, int msecs);

    // Capabilities reporting
    virtual QList<QtOrganizer::QOrganizerItemFilter::FilterType> supportedFilters() const;
    virtual QList<QtOrganizer::QOrganizerItemDetail::DetailType> supportedItemDetails(QtOrganizer::QOrganizerItemType::ItemType itemType) const;
    virtual QList<QtOrganizer::QOrganizerItemType::ItemType> supportedItemTypes() const;

    /* Map between EDS items (source + uid) and QtOrganizer items
     * (QOrganizerCollectionId + QOrganizerItemId):
     */
    static QtOrganizer::QOrganizerItemId
        idFromEds(const QtOrganizer::QOrganizerCollectionId &collectionId,
                  const gchar *uid);
    static QByteArray idToEds(const QtOrganizer::QOrganizerItemId &itemId,
                              QByteArray *sourceId = nullptr);

    // debug
    int runningRequestCount() const;

protected Q_SLOTS:
    void onSourceAdded(const QByteArray &sourceId);
    void onSourceRemoved(const QByteArray &sourceId);
    void onSourceUpdated(const QByteArray &sourceId);
    void onViewChanged(QtOrganizer::QOrganizerItemChangeSet *change);

protected:
    QOrganizerEDSEngine(QOrganizerEDSEngineData *data);

private:
    static QOrganizerEDSEngineData *m_globalData;
    QOrganizerEDSEngineData *d;
    QMap<QtOrganizer::QOrganizerAbstractRequest*, RequestData*> m_runningRequests;

    QList<QtOrganizer::QOrganizerItem> parseEvents(const QByteArray &sourceId, GSList *events, bool isIcalEvents, QList<QtOrganizer::QOrganizerItemDetail::DetailType> detailsHint);
    void parseEventsAsync(const QMap<QByteArray, GSList *> &events,
                          bool isIcalEvents,
                          QList<QtOrganizer::QOrganizerItemDetail::DetailType> detailsHint,
                          QObject *source,
                          const QByteArray &slot);
    static QList<QtOrganizer::QOrganizerItem> parseEvents(const QtOrganizer::QOrganizerCollectionId &collectionId, GSList *events, bool isIcalEvents, QList<QtOrganizer::QOrganizerItemDetail::DetailType> detailsHint);
    static GSList *parseItems(ECalClient *client, QList<QtOrganizer::QOrganizerItem> items, bool *hasRecurrence);

    // QOrganizerItem -> ECalComponent
    static void parseId(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp);
    static void parseSummary(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp);
    static void parseDescription(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp);
    static void parseComments(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp);
    static void parseTags(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp);
    static void parseReminders(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp);
    static void encodeAttachment(const QUrl &url, ECalComponentAlarm *alarm);
    static void parseVisualReminderAttachment(const QtOrganizer::QOrganizerItemDetail &detail, ECalComponentAlarm *alarm);
    static void parseAudibleReminderAttachment(const QtOrganizer::QOrganizerItemDetail &detail, ECalComponentAlarm *alarm);
    static void parseStartTime(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp);
    static void parseEndTime(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp);
    static void parseTodoStartTime(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp);
    static void parseRecurrence(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp);
    static void parseWeekRecurrence(const QtOrganizer::QOrganizerRecurrenceRule &qRule, struct icalrecurrencetype *rule);
    static void parseMonthRecurrence(const QtOrganizer::QOrganizerRecurrenceRule &qRule, struct icalrecurrencetype *rule);
    static void parseYearRecurrence(const QtOrganizer::QOrganizerRecurrenceRule &qRule, struct icalrecurrencetype *rule);
    static void parsePriority(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp);
    static void parseLocation(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp);
    static void parseDueDate(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp);
    static void parseProgress(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp);
    static void parseStatus(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp);
    static void parseAttendeeList(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp);
    static void parseExtendedDetails(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp);

    // ECalComponent -> QOrganizerItem
    static bool hasRecurrence(ECalComponent *comp);
    static void parseId(ECalComponent *comp, QtOrganizer::QOrganizerItem *item, const QtOrganizer::QOrganizerCollectionId &edsCollectionId);
    static void parseSummary(ECalComponent *comp, QtOrganizer::QOrganizerItem *item);
    static void parseDescription(ECalComponent *comp, QtOrganizer::QOrganizerItem *item);
    static void parseComments(ECalComponent *comp, QtOrganizer::QOrganizerItem *item);
    static void parseTags(ECalComponent *comp, QtOrganizer::QOrganizerItem *item);
    static void parseReminders(ECalComponent *comp, QtOrganizer::QOrganizerItem *item, QList<QtOrganizer::QOrganizerItemDetail::DetailType> detailsHint = QList<QtOrganizer::QOrganizerItemDetail::DetailType>());
    static QUrl dencodeAttachment(ECalComponentAlarm *alarm);
    static void parseAudibleReminderAttachment(ECalComponentAlarm *alarm, QtOrganizer::QOrganizerItemReminder *aDetail);
    static void parseVisualReminderAttachment(ECalComponentAlarm *alarm, QtOrganizer::QOrganizerItemReminder *aDetail);
    static void parseStartTime(ECalComponent *comp, QtOrganizer::QOrganizerItem *item);
    static void parseTodoStartTime(ECalComponent *comp, QtOrganizer::QOrganizerItem *item);
    static void parseEndTime(ECalComponent *comp, QtOrganizer::QOrganizerItem *item);
    static void parseRecurrence(ECalComponent *comp, QtOrganizer::QOrganizerItem *item);
    static void parseWeekRecurrence(struct icalrecurrencetype *rule, QtOrganizer::QOrganizerRecurrenceRule *qRule);
    static void parseMonthRecurrence(struct icalrecurrencetype *rule, QtOrganizer::QOrganizerRecurrenceRule *qRule);
    static void parseYearRecurrence(struct icalrecurrencetype *rule, QtOrganizer::QOrganizerRecurrenceRule *qRule);
    static void parsePriority(ECalComponent *comp, QtOrganizer::QOrganizerItem *item);
    static void parseLocation(ECalComponent *comp, QtOrganizer::QOrganizerItem *item);
    static void parseDueDate(ECalComponent *comp, QtOrganizer::QOrganizerItem *item);
    static void parseProgress(ECalComponent *comp, QtOrganizer::QOrganizerItem *item);
    static void parseStatus(ECalComponent *comp, QtOrganizer::QOrganizerItem *item);
    static void parseAttendeeList(ECalComponent *comp, QtOrganizer::QOrganizerItem *item);
    static void parseExtendedDetails(ECalComponent *comp, QtOrganizer::QOrganizerItem *item);

    static QDateTime fromIcalTime(struct icaltimetype value, const char *tzId);
    static icaltimetype fromQDateTime(const QDateTime &dateTime, bool allDay, QByteArray *tzId);

    static QtOrganizer::QOrganizerItem *parseEvent(ECalComponent *comp, QList<QtOrganizer::QOrganizerItemDetail::DetailType> detailsHint);
    static QtOrganizer::QOrganizerItem *parseToDo(ECalComponent *comp, QList<QtOrganizer::QOrganizerItemDetail::DetailType> detailsHint);
    static QtOrganizer::QOrganizerItem *parseJournal(ECalComponent *comp, QList<QtOrganizer::QOrganizerItemDetail::DetailType> detailsHint);

    static ECalComponent *createDefaultComponent(ECalClient *client, icalcomponent_kind iKind, ECalComponentVType eType);
    static ECalComponent *parseEventItem(ECalClient *client, const QtOrganizer::QOrganizerItem &item);
    static ECalComponent *parseTodoItem(ECalClient *client, const QtOrganizer::QOrganizerItem &item);
    static ECalComponent *parseJournalItem(ECalClient *client, const QtOrganizer::QOrganizerItem &item);
    static QByteArray toComponentId(const QByteArray &itemId, QByteArray *rid);
    static ECalComponentId *ecalComponentId(const QtOrganizer::QOrganizerItemId &itemId);

    // glib callback
    void itemsAsync(QtOrganizer::QOrganizerItemFetchRequest *req);
    static void itemsAsyncStart(FetchRequestData *data);
    static gboolean itemsAsyncListed(ECalComponent *comp, time_t instanceStart, time_t instanceEnd, FetchRequestData *data);
    static void itemsAsyncDone(FetchRequestData *data);
    static void itemsAsyncListedAsComps(GObject *source, GAsyncResult *res, FetchRequestData *data);
    static void itemsAsyncFetchDeatachedItems(FetchRequestData *data);
    static void itemsAsyncListByIdListed(GObject *source, GAsyncResult *res, FetchRequestData *data);

    void itemsByIdAsync(QtOrganizer::QOrganizerItemFetchByIdRequest *req);
    static void itemsByIdAsyncStart(FetchByIdRequestData *data);
    static void itemsByIdAsyncListed(GObject *client, GAsyncResult *res, FetchByIdRequestData *data);

    void itemOcurrenceAsync(QtOrganizer::QOrganizerItemOccurrenceFetchRequest *req);
    static void itemOcurrenceAsyncGetObjectDone(GObject *source, GAsyncResult *res, FetchOcurrenceData *data);
    static void itemOcurrenceAsyncListed(ECalComponent *comp, time_t instanceStart, time_t instanceEnd, FetchOcurrenceData *data);
    static void itemOcurrenceAsyncDone(FetchOcurrenceData *data);

    void saveItemsAsync(QtOrganizer::QOrganizerItemSaveRequest *req);
    static void saveItemsAsyncStart(SaveRequestData *data);
    static void saveItemsAsyncCreated(GObject *source_object, GAsyncResult *res, SaveRequestData *data);
    static void saveItemsAsyncModified(GObject *source_object, GAsyncResult *res, SaveRequestData *data);

    void removeItemsByIdAsync(QtOrganizer::QOrganizerItemRemoveByIdRequest *req);
    static void removeItemsByIdAsyncStart(RemoveByIdRequestData *data);

    void removeItemsAsync(QtOrganizer::QOrganizerItemRemoveRequest *req);
    static void removeItemsAsyncStart(RemoveRequestData *data);

    void saveCollectionAsync(QtOrganizer::QOrganizerCollectionSaveRequest *req);
    static gboolean saveCollectionUpdateAsyncStart(SaveCollectionRequestData *data);
    static void saveCollectionAsyncCommited(ESourceRegistry *registry, GAsyncResult *res, SaveCollectionRequestData *data);
    static void saveCollectionUpdateAsynCommited(ESource *source, GAsyncResult *res, SaveCollectionRequestData *data);

    void removeCollectionAsync(QtOrganizer::QOrganizerCollectionRemoveRequest *req);
    static void removeCollectionAsyncStart(GObject *sourceObject, GAsyncResult *res, RemoveCollectionRequestData *data);

    static void releaseRequestData(RequestData *data);

    friend class RequestData;
    friend class SaveCollectionRequestData;
    friend class RemoveCollectionRequestData;
    friend class ViewWatcher;
    friend class FetchRequestData;
    friend class FetchOcurrenceData;
    friend class QOrganizerParseEventThread;
    friend class RemoveByIdRequestData;
    friend class RemoveRequestData;
};

//FIXME: Do we really need this, this looks wrong
using namespace QtOrganizer;
Q_DECLARE_METATYPE(QList<QOrganizerCollectionId>)
Q_DECLARE_METATYPE(QList<QOrganizerItemId>)
Q_DECLARE_METATYPE(QList<QOrganizerItem>)

#endif

