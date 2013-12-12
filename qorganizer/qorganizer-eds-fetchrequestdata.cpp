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

FetchRequestData::FetchRequestData(QOrganizerEDSEngine *engine,
                                   QStringList collections,
                                   QOrganizerAbstractRequest *req)
    : RequestData(engine, req),
      m_collections(collections)
{
}

FetchRequestData::~FetchRequestData()
{
}

QString FetchRequestData::nextCollection()
{
    m_current = "";
    setClient(0);
    if (m_collections.size()) {
        m_current = m_collections.takeFirst();
        return m_current;
    } else {
        return QString();
    }
}

QString FetchRequestData::collection() const
{
    return m_current;
}

time_t FetchRequestData::startDate() const
{
    QDateTime startDate = request<QOrganizerItemFetchRequest>()->startDate();
    if (startDate.isValid()) {
        return startDate.toTime_t();
    } else {
        return 0;
    }
}

time_t FetchRequestData::endDate() const
{
    QDateTime endDate = request<QOrganizerItemFetchRequest>()->endDate();
    if (endDate.isValid()) {
        return endDate.toTime_t();
    } else {
        return QDateTime::fromMSecsSinceEpoch(std::numeric_limits < quint32 >::max()).toTime_t();
    }
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
    Q_FOREACH(const QOrganizerItem &item, results) {
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
