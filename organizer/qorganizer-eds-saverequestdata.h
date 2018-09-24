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

#ifndef __QORGANIZER_EDS_SAVEREQUESTDATA_H__
#define __QORGANIZER_EDS_SAVEREQUESTDATA_H__

#include "qorganizer-eds-requestdata.h"
#include "qorganizer-eds-engine.h"

class SaveRequestData : public RequestData
{
public:
    SaveRequestData(QOrganizerEDSEngine *engine,
                    QtOrganizer::QOrganizerAbstractRequest *req);
    ~SaveRequestData();

    void finish(QtOrganizer::QOrganizerManager::Error error = QtOrganizer::QOrganizerManager::NoError,
                QtOrganizer::QOrganizerAbstractRequest::State state = QtOrganizer::QOrganizerAbstractRequest::FinishedState);


    QByteArray nextSourceId();
    QByteArray currentSourceId() const;
    QList<QtOrganizer::QOrganizerItem> takeItemsToCreate();
    QList<QtOrganizer::QOrganizerItem> takeItemsToUpdate();
    bool end() const;
    void setWorkingItems(QList<QtOrganizer::QOrganizerItem> items);
    QList<QtOrganizer::QOrganizerItem> workingItems() const;
    int updateMode() const;

    void appendResults(QList<QtOrganizer::QOrganizerItem> results);
    void appendResult(const QtOrganizer::QOrganizerItem &item, QtOrganizer::QOrganizerManager::Error error = QtOrganizer::QOrganizerManager::NoError);
private:
    QList<QtOrganizer::QOrganizerItem> m_result;
    QMap<int, QtOrganizer::QOrganizerManager::Error> m_erros;
    QMap<QByteArray, QList<QtOrganizer::QOrganizerItem> > m_items;
    QList<QtOrganizer::QOrganizerItem> m_currentItems;
    QList<QtOrganizer::QOrganizerItem> m_workingItems;
    QByteArray m_currentSourceId;
};

#endif
