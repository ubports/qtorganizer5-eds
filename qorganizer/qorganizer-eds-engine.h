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

#include "qorganizer-eds-collection-engineid.h"

#include <QtOrganizer/QOrganizerItemId>
#include <QtOrganizer/QOrganizerItemFetchHint>
#include <QtOrganizer/QOrganizerManager>
#include <QtOrganizer/QOrganizerManagerEngine>
#include <QtOrganizer/QOrganizerItemSortOrder>
#include <QtOrganizer/QOrganizerItemFilter>
#include <QtOrganizer/QOrganizerItemChangeSet>
#include <QtOrganizer/QOrganizerCollectionId>
#include <QtOrganizer/QOrganizerItemReminder>

#include <libecal/libecal.h>

class FetchRequestData;
class SaveRequestData;
class RemoveRequestData;
class SaveCollectionRequestData;
class RemoveCollectionRequestData;

class QOrganizerEDSEngine : public QtOrganizer::QOrganizerManagerEngine
{
    Q_OBJECT

public:
    static QOrganizerEDSEngine *createEDSEngine(const QMap<QString, QString>& parameters);

    ~QOrganizerEDSEngine();

    // URI reporting
    QString managerName() const;
    QMap<QString, QString> managerParameters() const;


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
    QtOrganizer::QOrganizerCollection defaultCollection(QtOrganizer::QOrganizerManager::Error* error);
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

protected:
    QOrganizerEDSEngine();
/*
    // Implement "signal coalescing" for batch functions via change set
    virtual bool saveItem(QtOrganizer::QOrganizerItem* theOrganizerItem,
                          QtOrganizer::QOrganizerItemChangeSet& changeSet,
                          QtOrganizer::QOrganizerManager::Error* error);
    virtual bool removeItem(const QtOrganizer::QOrganizerItemId& organizeritemId,
                            QtOrganizer::QOrganizerItemChangeSet& changeSet,
                            QtOrganizer::QOrganizerManager::Error* error);
    virtual bool removeOccurrence(const QtOrganizer::QOrganizerItem& organizeritem,
                                  QtOrganizer::QOrganizerItemChangeSet& changeSet,
                                  QtOrganizer::QOrganizerManager::Error* error);
*/

private:
    QList<QtOrganizer::QOrganizerCollection> m_collections;
    QMap<QString, QOrganizerEDSCollectionEngineId*> m_collectionsMap;

    QtOrganizer::QOrganizerCollection m_defaultCollection;
    QList<FetchRequestData*> m_pendingFetchRequest;

    void loadCollections();
    void registerCollection(const QtOrganizer::QOrganizerCollection &collection, QOrganizerEDSCollectionEngineId *edsId);
    void unregisterCollection(const QtOrganizer::QOrganizerCollectionId &collectionId);

    ESource *findSource(const QtOrganizer::QOrganizerCollectionId &id) const;

    static QtOrganizer::QOrganizerCollection parseSource(ESource *source, const QString &managerUri);
    static QtOrganizer::QOrganizerCollection parseSource(ESource *source, const QString &managerUri, QOrganizerEDSCollectionEngineId **edsId);
    static QList<QtOrganizer::QOrganizerItem> parseEvents(QOrganizerEDSCollectionEngineId *collection, GSList *events);
    static GSList *parseItems(ECalClient *client, QList<QtOrganizer::QOrganizerItem> items);

    // QOrganizerItem -> ECalComponent
    static void parseSummary(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp);
    static void parseDescription(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp);
    static void parseComments(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp);
    static void parseTags(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp);
    static void parseReminders(const QtOrganizer::QOrganizerItem &item, ECalComponent *comp);
    static void encodeAttachment(QByteArray data, ECalComponentAlarm *alarm);
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

    // ECalComponent -> QOrganizerItem
    static void parseSummary(ECalComponent *comp, QtOrganizer::QOrganizerItem *item);
    static void parseDescription(ECalComponent *comp, QtOrganizer::QOrganizerItem *item);
    static void parseComments(ECalComponent *comp, QtOrganizer::QOrganizerItem *item);
    static void parseTags(ECalComponent *comp, QtOrganizer::QOrganizerItem *item);
    static void parseReminders(ECalComponent *comp, QtOrganizer::QOrganizerItem *item);
    static QByteArray dencodeAttachment(ECalComponentAlarm *alarm);
    static void parseAudibleReminderAttachment(ECalComponentAlarm *alarm, QtOrganizer::QOrganizerItemReminder *aDetail);
    static void parseVisualReminderAttachment(ECalComponentAlarm *alarm, QtOrganizer::QOrganizerItemReminder *aDetail);
    static void parseStartTime(ECalComponent *comp, QtOrganizer::QOrganizerItem *item);
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

    static QDateTime fromIcalTime(struct icaltimetype value);
    static QtOrganizer::QOrganizerItem *parseEvent(ECalComponent *comp);
    static QtOrganizer::QOrganizerItem *parseToDo(ECalComponent *comp);
    static QtOrganizer::QOrganizerItem *parseJournal(ECalComponent *comp);

    static ECalComponent *createDefaultComponent(ECalClient *client, icalcomponent_kind iKind, ECalComponentVType eType);
    static ECalComponent *parseEventItem(ECalClient *client, const QtOrganizer::QOrganizerItem &item);
    static ECalComponent *parseTodoItem(ECalClient *client, const QtOrganizer::QOrganizerItem &item);
    static ECalComponent *parseJournalItem(ECalClient *client, const QtOrganizer::QOrganizerItem &item);

    // glib callback
    void itemsAsync(QtOrganizer::QOrganizerItemFetchRequest *req);
    static void itemsAsyncStart(FetchRequestData *data);
    static void itemsAsyncConnected(GObject *source_object, GAsyncResult *res, FetchRequestData *data);
    static void itemsAsyncListed(GObject *source_object, GAsyncResult *res, FetchRequestData *data);

    void saveItemsAsync(QtOrganizer::QOrganizerItemSaveRequest *req);
    static void saveItemsAsyncConnected(GObject *source_object, GAsyncResult *res, SaveRequestData *data);
    static void saveItemsAsyncCreated(GObject *source_object, GAsyncResult *res, SaveRequestData *data);
    static void saveItemsAsyncModified(GObject *source_object, GAsyncResult *res, SaveRequestData *data);

    void removeItemsAsync(QtOrganizer::QOrganizerItemRemoveRequest *req);
    static void removeItemsAsyncStart(RemoveRequestData *data);
    static void removeItemsAsyncConnected(GObject *source_object, GAsyncResult *res, RemoveRequestData *data);
    static void removeItemsAsyncRemoved(GObject *source_object, GAsyncResult *res, RemoveRequestData *data);

    void saveCollectionAsync(QtOrganizer::QOrganizerCollectionSaveRequest *req);
    static void saveCollectionAsyncStart(ESourceRegistry *registry, SaveCollectionRequestData *data);
    static void saveCollectionAsyncCommited(GObject *source_object, GAsyncResult *res, SaveCollectionRequestData *data);

    void removeCollectionAsync(QtOrganizer::QOrganizerCollectionRemoveRequest *req);
    static void removeCollectionAsyncStart(GObject *source_object, GAsyncResult *res, RemoveCollectionRequestData *data);
/*
    QList<QtOrganizer::QOrganizerItem> internalItemOccurrences(const QtOrganizer::QOrganizerItem& parentItem,
                                                               const QDateTime& periodStart,
                                                               const QDateTime& periodEnd,
                                                               int maxCount,
                                                               bool includeExceptions,
                                                               bool sortItems,
                                                               QList<QDate> *exceptionDates,
                                                               QtOrganizer::QOrganizerManager::Error* error) const;

    QList<QtOrganizer::QOrganizerItem> internalItems(const QDateTime& startDate,
                                                     const QDateTime& endDate,
                                                     const QtOrganizer::QOrganizerItemFilter& filter,
                                                     const QList<QtOrganizer::QOrganizerItemSortOrder>& sortOrders,
                                                     const QtOrganizer::QOrganizerItemFetchHint& fetchHint,
                                                     QtOrganizer::QOrganizerManager::Error* error,
                                                     bool forExport) const;
    QtOrganizer::QOrganizerItem item(const QtOrganizer::QOrganizerItemId& organizeritemId) const;
    */
    friend class FetchRequestData;
    friend class SaveCollectionRequestData;
    friend class RemoveCollectionRequestData;
};

#endif
