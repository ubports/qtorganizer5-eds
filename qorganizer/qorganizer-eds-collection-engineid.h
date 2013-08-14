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

#ifndef __QORGANIZER_EDS_COLLECTION_ENGINEID_H__
#define __QORGANIZER_EDS_COLLECTION_ENGINEID_H__

#include <QtOrganizer/QOrganizerCollectionEngineId>

#include <libedataserver/libedataserver.h>

class QOrganizerEDSCollectionEngineId : public QtOrganizer::QOrganizerCollectionEngineId
{
public:
    QOrganizerEDSCollectionEngineId();
    ~QOrganizerEDSCollectionEngineId();
    QOrganizerEDSCollectionEngineId(const QOrganizerEDSCollectionEngineId& other);
    QOrganizerEDSCollectionEngineId(const QString& idString);

    bool isEqualTo(const QtOrganizer::QOrganizerCollectionEngineId* other) const;
    bool isLessThan(const QtOrganizer::QOrganizerCollectionEngineId* other) const;

    QString managerUri() const;
    QOrganizerEDSCollectionEngineId *clone() const;

    QString toString() const;

    uint hash() const;

#ifndef QT_NO_DEBUG_STREAM
    QDebug& debugStreamOut(QDebug& dbg) const;
#endif

private:
    QString m_collectionId;
    QString m_managerUri;
    ESource *m_esource;

    QOrganizerEDSCollectionEngineId(ESource *source, const QString &managerUri);

    friend class QOrganizerEDSEngine;
};



#endif
