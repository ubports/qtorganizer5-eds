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

#ifndef __QORGANIZER_EDS_FETCHREQUESTDATA_H__
#define __QORGANIZER_EDS_FETCHREQUESTDATA_H__

#include "qorganizer-eds-requestdata.h"
#include <glib.h>

class FetchRequestDataParseListener;

class FetchRequestData : public RequestData
{
public:
    FetchRequestData(QOrganizerEDSEngine *engine,
                     const QByteArrayList &sourceIds,
                     QtOrganizer::QOrganizerAbstractRequest *req);
    ~FetchRequestData();

    QByteArray nextSourceId();
    QByteArray nextParentId();
    QByteArray sourceId() const;
    time_t startDate() const;
    time_t endDate() const;
    bool hasDateInterval() const;
    bool filterIsValid() const;
    void cancel();
    void compileCurrentIds();

    void finish(QtOrganizer::QOrganizerManager::Error error = QtOrganizer::QOrganizerManager::NoError,
                QtOrganizer::QOrganizerAbstractRequest::State state = QtOrganizer::QOrganizerAbstractRequest::FinishedState);
    void appendResult(icalcomponent *comp);
    void appendDeatachedResult(icalcomponent *comp);
    int appendResults(QList<QtOrganizer::QOrganizerItem> results);
    QString dateFilter();

private:
    FetchRequestDataParseListener *m_parseListener;
    QMap<QByteArray, GSList*> m_components;
    QByteArrayList m_sourceIds;
    QSet<QByteArray> m_currentParentIds;
    QByteArrayList m_deatachedIds;
    QByteArray m_current;
    GSList* m_currentComponents;
    QList<QtOrganizer::QOrganizerItem> m_results;

    QByteArrayList filterSourceIds(const QByteArrayList &collections) const;
    QByteArrayList sourceIdsFromFilter(const QtOrganizer::QOrganizerItemFilter &f) const;
    void finishContinue(QtOrganizer::QOrganizerManager::Error error,
                        QtOrganizer::QOrganizerAbstractRequest::State state);

    friend class FetchRequestDataParseListener;
};

class FetchRequestDataParseListener : public QObject
{
    Q_OBJECT
public:
    FetchRequestDataParseListener(FetchRequestData *data,
                                  QtOrganizer::QOrganizerManager::Error error,
                                  QtOrganizer::QOrganizerAbstractRequest::State state);

private Q_SLOTS:
    void onParseDone(QList<QtOrganizer::QOrganizerItem> results);

private:
    FetchRequestData *m_data;
    QtOrganizer::QOrganizerManager::Error m_error;
    QtOrganizer::QOrganizerAbstractRequest::State m_state;
};

#endif
