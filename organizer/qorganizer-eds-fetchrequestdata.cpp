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

#include "qorganizer-eds-fetchrequestdata.h"

#include <QtCore/QDebug>

#include <QtOrganizer/QOrganizerItemFetchRequest>
#include <QtOrganizer/QOrganizerItemCollectionFilter>
#include <QtOrganizer/QOrganizerItemUnionFilter>
#include <QtOrganizer/QOrganizerItemIntersectionFilter>

using namespace QtOrganizer;

FetchRequestData::FetchRequestData(QOrganizerEDSEngine *engine,
                                   QStringList collections,
                                   QOrganizerAbstractRequest *req)
    : RequestData(engine, req),
      m_parseListener(0),
      m_currentComponents(0)
{
    // filter collections related with the query
    m_collections = filterCollections(collections);
}

FetchRequestData::~FetchRequestData()
{
    delete m_parseListener;

    Q_FOREACH(GSList *components, m_components.values()) {
        g_slist_free_full(components, (GDestroyNotify)icalcomponent_free);
    }
    m_components.clear();
}

QString FetchRequestData::nextCollection()
{
    if (m_currentComponents) {
        m_components.insert(m_current, m_currentComponents);
        m_currentComponents = 0;
    }
    m_current = "";
    setClient(0);
    if (m_collections.size()) {
        m_current = m_collections.takeFirst();
        return m_current;
    } else {
        return QString();
    }
}

QString FetchRequestData::nextParentId()
{
    QString nextId;
    if (!m_currentParentIds.isEmpty()) {
        nextId = m_currentParentIds.values().first();
        m_currentParentIds.remove(nextId);
    }
    return nextId;
}

QString FetchRequestData::collection() const
{
    return m_current;
}

time_t FetchRequestData::startDate() const
{
    QDateTime startDate = request<QOrganizerItemFetchRequest>()->startDate();
    if (!startDate.isValid()) {
        QDate currentDate = QDate::currentDate();
        startDate.setTime(QTime(0, 0, 0));
        startDate.setDate(QDate(currentDate.year(), 1, 1));
        qWarning() << "Start date is invalid using " << startDate;
    }
    return startDate.toTime_t();
}

time_t FetchRequestData::endDate() const
{
    QDateTime endDate = request<QOrganizerItemFetchRequest>()->endDate();
    if (!endDate.isValid()) {
        QDate currentDate = QDate::currentDate();
        endDate.setTime(QTime(0, 0, 0));
        endDate.setDate(QDate(currentDate.year()+1, 1, 1));
        qWarning() << "End date is invalid using " << endDate;
    }
    return endDate.toTime_t();
}

bool FetchRequestData::hasDateInterval() const
{
    if (!filterIsValid()) {
        return false;
    }

    QDateTime endDate = request<QOrganizerItemFetchRequest>()->endDate();
    QDateTime startDate = request<QOrganizerItemFetchRequest>()->startDate();

    return (endDate.isValid() && startDate.isValid());
}

bool FetchRequestData::filterIsValid() const
{
    return (request<QOrganizerItemFetchRequest>()->filter().type() != QOrganizerItemFilter::InvalidFilter);
}

void FetchRequestData::cancel()
{
    if (m_parseListener) {
        delete m_parseListener;
        m_parseListener = 0;
    }
    RequestData::cancel();
}

void FetchRequestData::compileCurrentIds()
{
    for(GSList *e = m_currentComponents; e != NULL; e = e->next) {
        icalcomponent *icalComp = static_cast<icalcomponent *>(e->data);
        if (e_cal_util_component_has_recurrences (icalComp)) {
            m_currentParentIds.insert(QString::fromUtf8(icalcomponent_get_uid(icalComp)));
        }
    }
}

void FetchRequestData::finish(QOrganizerManager::Error error,
                              QOrganizerAbstractRequest::State state)
{
    if (!m_components.isEmpty()) {
        m_parseListener = new FetchRequestDataParseListener(this,
                                                            error,
                                                            state);
        QOrganizerItemFetchRequest *req =  request<QOrganizerItemFetchRequest>();
        if (req) {
            parent()->parseEventsAsync(m_components,
                                       true,
                                       req->fetchHint().detailTypesHint(),
                                       m_parseListener,
                                       SLOT(onParseDone(QList<QtOrganizer::QOrganizerItem>)));

            return;
        }
    }
    finishContinue(error, state);
}

void FetchRequestData::finishContinue(QOrganizerManager::Error error,
                                      QOrganizerAbstractRequest::State state)
{
    if (m_parseListener) {
        m_parseListener->deleteLater();
        m_parseListener = 0;
    }

    Q_FOREACH(GSList *components, m_components.values()) {
        g_slist_free_full(components, (GDestroyNotify)icalcomponent_free);
    }
    m_components.clear();

    QOrganizerItemFetchRequest *req =  request<QOrganizerItemFetchRequest>();
    if (req) {
        QOrganizerManagerEngine::updateItemFetchRequest(req,
                                                        m_results,
                                                        error,
                                                        state);
    }

    // TODO: emit changeset???
    RequestData::finish(error, state);
}

void FetchRequestData::appendResult(icalcomponent *comp)
{
    m_currentComponents = g_slist_append(m_currentComponents, comp);
}

void FetchRequestData::appendDeatachedResult(icalcomponent *comp)
{
    const gchar *uid;
    struct icaltimetype rid;

    uid = icalcomponent_get_uid(comp);
    rid = icalcomponent_get_recurrenceid(comp);

    QString collectionIdString =
        QString::fromUtf8(QOrganizerCollectionId::fromString(m_current).localId().toHex());

    for(GSList *e=m_currentComponents; e != NULL; e = e->next) {
        icalcomponent *ical = static_cast<icalcomponent *>(e->data);
        if ((g_strcmp0(uid, icalcomponent_get_uid(ical)) == 0) &&
            (icaltime_compare(rid, icalcomponent_get_recurrenceid(ical)) == 0)) {

            // replace instance event
            icalcomponent_free (ical);
            e->data = icalcomponent_new_clone(comp);
            QString itemId = QString("%1/%2#%3")
                    .arg(collectionIdString)
                    .arg(QString::fromUtf8(uid))
                    .arg(QString::fromUtf8(icaltime_as_ical_string(rid)));
            m_deatachedIds.append(itemId);
            break;
        }
    }
}

int FetchRequestData::appendResults(QList<QOrganizerItem> results)
{
    int count = 0;
    QOrganizerItemFetchRequest *req = request<QOrganizerItemFetchRequest>();
    if (!req) {
        return 0;
    }
    QOrganizerItemFilter filter = req->filter();
    QList<QOrganizerItemSortOrder> sorting = req->sorting();

    Q_FOREACH(QOrganizerItem item, results) {
        if (QOrganizerManagerEngine::testFilter(filter, item)) {
            QOrganizerManagerEngine::addSorted(&m_results, item, sorting);
            count++;
        }
    }
    return count;
}

QString FetchRequestData::dateFilter()
{
    QOrganizerItemFetchRequest *r = request<QOrganizerItemFetchRequest>();
    if (r->filter().type() == QOrganizerItemFilter::InvalidFilter) {
        qWarning("Query for events with invalid filter type");
        return QStringLiteral("");
    }

    QDateTime startDate = r->startDate();
    QDateTime endDate = r->endDate();

    if (!startDate.isValid() ||
        !endDate.isValid()) {
        return QStringLiteral("#t"); // match all
    }

    gchar *startDateStr = isodate_from_time_t(startDate.toTime_t());
    gchar *endDateStr = isodate_from_time_t(endDate.toTime_t());

    QString query = QString("(occur-in-time-range? "
                            "(make-time \"%1\") (make-time \"%2\"))")
            .arg(startDateStr)
            .arg(endDateStr);

    g_free(startDateStr);
    g_free(endDateStr);

    return query;
}

QStringList FetchRequestData::filterCollections(const QStringList &collections) const
{
    QStringList result;
    if (filterIsValid()) {
        QOrganizerItemFilter f = request<QOrganizerItemFetchRequest>()->filter();
        QStringList cFilters = collectionsFromFilter(f);
        if (cFilters.contains("*") || cFilters.isEmpty()) {
            result = collections;
        } else {
            Q_FOREACH(const QString &f, collections) {
                if (cFilters.contains(f)) {
                    result << f;
                }
            }
        }
    }
    return result;
}

QStringList FetchRequestData::collectionsFromFilter(const QOrganizerItemFilter &f) const
{
    QStringList result;

    switch(f.type()) {
    case QOrganizerItemFilter::CollectionFilter:
    {
        QOrganizerItemCollectionFilter cf = static_cast<QOrganizerItemCollectionFilter>(f);
        Q_FOREACH(const QOrganizerCollectionId &id, cf.collectionIds()) {
            result << id.toString();
        }
        break;
    }
    case QOrganizerItemFilter::IntersectionFilter:
    {
        QOrganizerItemIntersectionFilter intersec = static_cast<QOrganizerItemIntersectionFilter>(f);
        Q_FOREACH(const QOrganizerItemFilter &f, intersec.filters()) {
            result << collectionsFromFilter(f);
        }
        break;
    }
    case QOrganizerItemFilter::UnionFilter:
        // TODO: better handle union filters, for now they will consider all collections
        result << "*";
        break;
    default:
        break;
    }

    return result;
}

FetchRequestDataParseListener::FetchRequestDataParseListener(FetchRequestData *data,
                                                             QOrganizerManager::Error error,
                                                             QOrganizerAbstractRequest::State state)
    : QObject(0),
      m_data(data),
      m_error(error),
      m_state(state)
{
}

void FetchRequestDataParseListener::onParseDone(QList<QOrganizerItem> results)
{
    m_data->appendResults(results);
    m_data->finishContinue(m_error, m_state);
}
