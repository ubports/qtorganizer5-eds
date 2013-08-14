/*
 * Copyright 2013 Canonical Ltd.
 *
 * This file is part of canonical-pim-service.
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

#include "qorganizer-eds-engineid.h"

using namespace QtOrganizer;

QOrganizerEDSEngineId::QOrganizerEDSEngineId()
    : QOrganizerItemEngineId()
{
}

QOrganizerEDSEngineId::QOrganizerEDSEngineId(const QString &collectionId,
                                             const QString &id,
                                             const QString& managerUri)
    : QOrganizerItemEngineId(),
      m_managerUri(managerUri),
      m_collectionId(collectionId),
      m_itemId(id)
{
}

QOrganizerEDSEngineId::~QOrganizerEDSEngineId()
{
}

QOrganizerEDSEngineId::QOrganizerEDSEngineId(const QOrganizerEDSEngineId& other)
    : QOrganizerItemEngineId(),
      m_managerUri(other.m_managerUri),
      m_collectionId(other.m_collectionId),
      m_itemId(other.m_itemId)
{
}

QOrganizerEDSEngineId::QOrganizerEDSEngineId(const QString& idString)
    : QOrganizerItemEngineId()
{
    int temp = 0;
    int colonIndex = idString.indexOf(QStringLiteral(":"), 0);
    m_managerUri = idString.mid(temp, colonIndex);
    temp = colonIndex + 1;
    colonIndex = idString.indexOf(QStringLiteral(":"), temp);
    m_collectionId = idString.mid(temp, (colonIndex-temp));
    temp = colonIndex + 1;
    m_itemId = idString.mid(temp);
}

bool QOrganizerEDSEngineId::isEqualTo(const QOrganizerItemEngineId* other) const
{
    // note: we don't need to check the collectionId because itemIds in the memory
    // engine are unique regardless of which collection the item is in; also, we
    // don't need to check the managerUri, because this function is not called if
    // the managerUris don't match.
    if (m_itemId != static_cast<const QOrganizerEDSEngineId*>(other)->m_itemId)
        return false;
    return true;
}

bool QOrganizerEDSEngineId::isLessThan(const QOrganizerItemEngineId* other) const
{
    // order by collection, then by item in collection.
    const QOrganizerEDSEngineId* otherPtr = static_cast<const QOrganizerEDSEngineId*>(other);
    if (m_managerUri < otherPtr->m_managerUri)
        return true;
    if (m_collectionId < otherPtr->m_collectionId)
        return true;
    if (m_collectionId == otherPtr->m_collectionId)
        return (m_itemId < otherPtr->m_itemId);
    return false;
}

QString QOrganizerEDSEngineId::managerUri() const
{
    return m_managerUri;
}

QString QOrganizerEDSEngineId::toString() const
{
    return (m_collectionId + QLatin1Char(':') + m_itemId);
}

QOrganizerItemEngineId* QOrganizerEDSEngineId::clone() const
{
    return new QOrganizerEDSEngineId(m_collectionId, m_itemId, m_managerUri);
}

uint QOrganizerEDSEngineId::hash() const
{
    return qHash(m_itemId);
}

#ifndef QT_NO_DEBUG_STREAM
QDebug& QOrganizerEDSEngineId::debugStreamOut(QDebug& dbg) const
{
    dbg.nospace() << "QOrganizerItemMemoryEngineId(" << m_managerUri << ", " << m_collectionId << ", " << m_itemId << ")";
    return dbg.maybeSpace();
}

QString QOrganizerEDSEngineId::toComponentId(const QtOrganizer::QOrganizerItemId &itemId)
{
    return itemId.toString().split("&#58;").last();
}
#endif
