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

class FetchRequestData : public RequestData
{
public:
    FetchRequestData(QOrganizerEDSEngine *engine, QtOrganizer::QOrganizerAbstractRequest *req);
    ~FetchRequestData();

    QOrganizerEDSCollectionEngineId* nextCollection();
    QOrganizerEDSCollectionEngineId* collection() const;

    void finish(QtOrganizer::QOrganizerManager::Error error = QtOrganizer::QOrganizerManager::NoError);
    void appendResults(QList<QtOrganizer::QOrganizerItem> results);
    QString dateFilter();

private:
    QList<QOrganizerEDSCollectionEngineId*> m_collections;
    QOrganizerEDSCollectionEngineId *m_current;
    QList<QtOrganizer::QOrganizerItem> m_results;
};

#endif
