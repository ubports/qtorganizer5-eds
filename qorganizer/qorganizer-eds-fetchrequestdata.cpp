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

#include <QtOrganizer/QOrganizerItemFetchRequest>

using namespace QtOrganizer;

FetchRequestData::FetchRequestData(QOrganizerEDSEngine *engine, QOrganizerAbstractRequest *req)
    : RequestData(engine, req),
      m_collections(engine->m_collectionsMap.values())
{
}

FetchRequestData::~FetchRequestData()
{
}

QOrganizerEDSCollectionEngineId* FetchRequestData::nextCollection()
{
    m_current = 0;
    setClient(0);
    if (m_collections.size()) {
        m_current = m_collections.takeFirst();
        return m_current;
    } else {
        return 0;
    }
}

QOrganizerEDSCollectionEngineId* FetchRequestData::collection() const
{
    return m_current;
}

void FetchRequestData::finish(QOrganizerManager::Error error)
{
    QOrganizerManagerEngine::updateItemFetchRequest(request<QOrganizerItemFetchRequest>(),
                                                    m_results,
                                                    error,
                                                    QOrganizerAbstractRequest::FinishedState);
    // TODO: emit changeset???
}

int FetchRequestData::appendResults(QList<QOrganizerItem> results)
{
    int count = 0;
    QOrganizerItemFetchRequest *req = request<QOrganizerItemFetchRequest>();
    Q_FOREACH(QOrganizerItem item, results) {
        if (QOrganizerManagerEngine::testFilter(req->filter(), item)) {
            m_results << item;
            count++;
        }
    }
    return count;
}

QString FetchRequestData::dateFilter()
{
    QDateTime startDate = request<QOrganizerItemFetchRequest>()->startDate();
    QDateTime endDate = request<QOrganizerItemFetchRequest>()->endDate();

    if (!startDate.isValid() ||
        !endDate.isValid()) {
        return QStringLiteral("#t"); // match all
    }


    QString query = QString("(occur-in-time-range? "
                            "(make-time \"%1\") (make-time \"%2\"))")
            .arg(isodate_from_time_t(startDate.toTime_t()))
            .arg(isodate_from_time_t(endDate.toTime_t()));

    return query;
}
