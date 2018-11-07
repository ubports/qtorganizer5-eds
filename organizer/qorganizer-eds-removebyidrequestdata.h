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

    QByteArray sourceId() const;

    void finish(QtOrganizer::QOrganizerManager::Error error = QtOrganizer::QOrganizerManager::NoError,
                QtOrganizer::QOrganizerAbstractRequest::State state = QtOrganizer::QOrganizerAbstractRequest::FinishedState);

    GSList *compIds() const;

    QByteArray next();
    void commit();
    void cancel() override;

private:
    // Map source IDs to sets of items
    QHash<QByteArray, QSet<QtOrganizer::QOrganizerItemId> > m_pending;
    QSet<QtOrganizer::QOrganizerItemId> m_currentIds;
    QByteArray m_currentSourceId;

    bool m_sessionStaterd;
    GSList *m_currentCompIds;

    void reset();
    void clear();
    GSList *parseIds(QSet<QtOrganizer::QOrganizerItemId> iids);
};

#endif
