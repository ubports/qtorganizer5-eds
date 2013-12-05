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

#ifndef __QORGANIZER_EDS_REMOVEQUESTDATA_H__
#define __QORGANIZER_EDS_REMOVEQUESTDATA_H__

#include "qorganizer-eds-requestdata.h"

#include <glib.h>

class RemoveRequestData : public RequestData
{
public:
    RemoveRequestData(QOrganizerEDSEngine *engine, QtOrganizer::QOrganizerAbstractRequest *req);
    ~RemoveRequestData();

    QList<QtOrganizer::QOrganizerCollectionId> pendingCollections() const;
    QtOrganizer::QOrganizerCollectionId collectionId() const;

    void finish(QtOrganizer::QOrganizerManager::Error error = QtOrganizer::QOrganizerManager::NoError);

    GSList *compIds() const;

    QtOrganizer::QOrganizerCollectionId next();
    void commit();
    virtual void cancel();

private:
    QSet<QtOrganizer::QOrganizerCollectionId> m_pendingCollections;
    QList<QtOrganizer::QOrganizerItem> m_pendingItems;

    bool m_sessionStaterd;
    GSList *m_currentCompIds;
    QList<QtOrganizer::QOrganizerItemId> m_currentIds;
    QtOrganizer::QOrganizerCollectionId m_currentCollectionId;

    void clear();
    GSList* takeItemsIds(QtOrganizer::QOrganizerCollectionId collectionId);
};

#endif
