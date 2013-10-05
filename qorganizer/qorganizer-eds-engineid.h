/*
 * Copyright 2013 Canonical Ltd.
 *
 * This file is part of canonical-pim-service
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

#ifndef __QORGANIZER_EDS_ENGINEID_H__
#define __QORGANIZER_EDS_ENGINEID_H__

#include "qorganizer-eds-collection-engineid.h"

#include <QtOrganizer/QOrganizerItemEngineId>

#include <libecal/libecal.h>

class QOrganizerEDSEngineId : public QtOrganizer::QOrganizerItemEngineId
{
public:
    QOrganizerEDSEngineId();
    QOrganizerEDSEngineId(const QString& collectionId,
                          const QString& id);
    ~QOrganizerEDSEngineId();
    QOrganizerEDSEngineId(const QOrganizerEDSEngineId& other);
    QOrganizerEDSEngineId(const QString& idString);

    bool isEqualTo(const QtOrganizer::QOrganizerItemEngineId* other) const;
    bool isLessThan(const QtOrganizer::QOrganizerItemEngineId* other) const;

    QString managerUri() const;
    QtOrganizer::QOrganizerItemEngineId* clone() const;

    QString toString() const;
    uint hash() const;

#ifndef QT_NO_DEBUG_STREAM
    QDebug& debugStreamOut(QDebug& dbg) const;
#endif

    static QString managerUriStatic();
    static QString managerNameStatic();
    static QString toComponentId(const QtOrganizer::QOrganizerItemId &itemId);
    static QString toComponentId(const QString &itemId);

private:
    QString m_collectionId;
    QString m_itemId;
    friend class QOrganizerEDSEngine;
};

#endif
