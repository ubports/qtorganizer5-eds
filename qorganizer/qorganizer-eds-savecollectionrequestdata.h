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

#ifndef __QORGANIZER_EDS_SAVECOLLECTIONREQUESTDATA_H__
#define __QORGANIZER_EDS_SAVECOLLECTIONREQUESTDATA_H__

#include "qorganizer-eds-requestdata.h"

class SaveCollectionRequestData : public RequestData
{
public:
    SaveCollectionRequestData(QOrganizerEDSEngine *engine,
                              QtOrganizer::QOrganizerAbstractRequest *req);
    ~SaveCollectionRequestData();

    void finish(QtOrganizer::QOrganizerManager::Error error = QtOrganizer::QOrganizerManager::NoError);
    bool isNew(int index) const;

    GList *sources() const;
    QList<QtOrganizer::QOrganizerCollection> results() const;
    void commit(QtOrganizer::QOrganizerManager::Error error = QtOrganizer::QOrganizerManager::NoError);

private:
    QMap<int, QtOrganizer::QOrganizerManager::Error> m_errorMap;
    QList<QtOrganizer::QOrganizerCollection> m_results;
    GList *m_sources;

    void parseCollections();

};

#endif
