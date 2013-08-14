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

#include "qorganizer-eds-collection-engineid.h"

QOrganizerEDSCollectionEngineId::QOrganizerEDSCollectionEngineId(ESource *source,
                                                                 const QString &managerUri)
    : m_managerUri(managerUri), m_esource(source)
{
    g_object_ref(m_esource);
    m_collectionId = QString::fromUtf8(e_source_get_uid(m_esource));
}

QOrganizerEDSCollectionEngineId::QOrganizerEDSCollectionEngineId()
    : QOrganizerCollectionEngineId()
{
}

QOrganizerEDSCollectionEngineId::QOrganizerEDSCollectionEngineId(const QOrganizerEDSCollectionEngineId& other)
    : QOrganizerCollectionEngineId(), m_collectionId(other.m_collectionId)
{
}

QOrganizerEDSCollectionEngineId::QOrganizerEDSCollectionEngineId(const QString& idString)
    : QOrganizerCollectionEngineId()
{
    int colonIndex = idString.indexOf(QStringLiteral(":"));
    m_managerUri = idString.mid(0, colonIndex).toUInt();
    m_collectionId = idString.mid(colonIndex+1);
}

QOrganizerEDSCollectionEngineId::~QOrganizerEDSCollectionEngineId()
{
    g_object_unref(m_esource);
}

bool QOrganizerEDSCollectionEngineId::isEqualTo(const QOrganizerCollectionEngineId* other) const
{
    // note: we don't need to check the managerUri because this function is not called
    // if the managerUris are different.
    if (m_collectionId != static_cast<const QOrganizerEDSCollectionEngineId*>(other)->m_collectionId)
        return false;
    return true;
}

bool QOrganizerEDSCollectionEngineId::isLessThan(const QOrganizerCollectionEngineId* other) const
{
    // order by collection, then by item in collection.
    const QOrganizerEDSCollectionEngineId* otherPtr = static_cast<const QOrganizerEDSCollectionEngineId*>(other);
    if (m_managerUri < otherPtr->m_managerUri)
        return true;
    if (m_collectionId < otherPtr->m_collectionId)
        return true;
    return false;
}

QString QOrganizerEDSCollectionEngineId::managerUri() const
{
    return m_managerUri;
}

QString QOrganizerEDSCollectionEngineId::toString() const
{
    return (m_managerUri + QLatin1Char(':') + m_collectionId);
}

QOrganizerEDSCollectionEngineId* QOrganizerEDSCollectionEngineId::clone() const
{
    return new QOrganizerEDSCollectionEngineId(m_esource, m_managerUri);
}

uint QOrganizerEDSCollectionEngineId::hash() const
{
    return qHash(m_collectionId);
}

#ifndef QT_NO_DEBUG_STREAM
QDebug& QOrganizerEDSCollectionEngineId::debugStreamOut(QDebug& dbg) const
{
    dbg.nospace() << "QOrganizerEDSCollectionEngineId(" << m_managerUri << "," << m_collectionId << ")";
    return dbg.maybeSpace();
}


#endif

