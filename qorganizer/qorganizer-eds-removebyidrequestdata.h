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

#ifndef __QORGANIZER_EDS_REMOVEBYIDQUESTDATA_H__
#define __QORGANIZER_EDS_REMOVEBYIDQUESTDATA_H__

#include "qorganizer-eds-requestdata.h"

#include <glib.h>

class RemoveByIdRequestData : public RequestData
{
public:
    RemoveByIdRequestData(QOrganizerEDSEngine *engine, QtOrganizer::QOrganizerAbstractRequest *req);
    ~RemoveByIdRequestData();

    QtOrganizer::QOrganizerCollectionId collectionId() const;

    void finish(QtOrganizer::QOrganizerManager::Error error = QtOrganizer::QOrganizerManager::NoError);

    GSList *compIds() const;

    QtOrganizer::QOrganizerCollectionId next();
    void commit();
    virtual void cancel();

private:
    QHash<QtOrganizer::QOrganizerCollectionId, QSet<QtOrganizer::QOrganizerItemId> > m_pending;
    QtOrganizer::QOrganizerCollectionId m_currentCollectionId;
    QSet<QtOrganizer::QOrganizerItemId> m_currentIds;

    bool m_sessionStaterd;
    GSList *m_currentCompIds;

    void reset();
    void clear();
    GSList *parseIds(QSet<QtOrganizer::QOrganizerItemId> iids);
};

#endif
