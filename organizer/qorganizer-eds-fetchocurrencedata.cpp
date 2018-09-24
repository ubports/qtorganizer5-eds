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

#include "qorganizer-eds-fetchocurrencedata.h"

#include <QtCore/QDebug>

#include <QtOrganizer/QOrganizerItemOccurrenceFetchRequest>
#include <QtOrganizer/QOrganizerItemCollectionFilter>

using namespace QtOrganizer;

FetchOcurrenceData::FetchOcurrenceData(QOrganizerEDSEngine *engine,
                                       QOrganizerAbstractRequest *req)
    : RequestData(engine, req),
      m_components(0)
{
}

FetchOcurrenceData::~FetchOcurrenceData()
{
    if (m_components) {
        g_slist_free_full(m_components, (GDestroyNotify)icalcomponent_free);
        m_components = 0;
    }
}

time_t FetchOcurrenceData::startDate() const
{
    QDateTime startDate = request<QOrganizerItemOccurrenceFetchRequest>()->startDate();
    if (!startDate.isValid()) {
        startDate = QDateTime::fromTime_t(0);
        qWarning() << "Start date is invalide using " << startDate;
    }
    return startDate.toTime_t();
}

time_t FetchOcurrenceData::endDate() const
{
    QDateTime endDate = request<QOrganizerItemOccurrenceFetchRequest>()->endDate();
    if (!endDate.isValid()) {
        QDate currentDate = QDate::currentDate();
        endDate.setTime(QTime(0, 0, 0));
        endDate.setDate(QDate(currentDate.year()+1, 1, 1));
        qWarning() << "End date is invalid using " << endDate;
    }
    return endDate.toTime_t();
}

void FetchOcurrenceData::finish(QOrganizerManager::Error error,
                                QtOrganizer::QOrganizerAbstractRequest::State state)
{
    QList<QtOrganizer::QOrganizerItem> results;

    if (m_components) {
        QOrganizerItemOccurrenceFetchRequest *req = request<QOrganizerItemOccurrenceFetchRequest>();
        QByteArray sourceId = req->parentItem().collectionId().localId();
        results = parent()->parseEvents(sourceId, m_components, true,
                                        req->fetchHint().detailTypesHint());
        g_slist_free_full(m_components, (GDestroyNotify)icalcomponent_free);
        m_components = 0;
    }

    QOrganizerManagerEngine::updateItemOccurrenceFetchRequest(request<QOrganizerItemOccurrenceFetchRequest>(),
                                                              results,
                                                              error,
                                                              state);

    RequestData::finish(error, state);
}

void FetchOcurrenceData::appendResult(icalcomponent *comp)
{
    m_components = g_slist_append(m_components, comp);
}
