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

#include <QtCore/QStringList>
#include <QtCore/QDebug>

using namespace QtOrganizer;

QOrganizerEDSEngineId::QOrganizerEDSEngineId()
    : QOrganizerItemEngineId()
{
}

QOrganizerEDSEngineId::QOrganizerEDSEngineId(const QString &collectionId,
                                             const QString &id)
    : QOrganizerItemEngineId()
{
    if (!collectionId.isNull() && !collectionId.isEmpty()) {
        m_collectionId = collectionId.contains(":") ? collectionId.mid(collectionId.lastIndexOf(":")+1) : collectionId;
    }

    if (!id.isNull() && !id.isEmpty()) {
        m_itemId = id.contains(":") ? id.mid(id.lastIndexOf(":")+1) : id;
    }
}

QOrganizerEDSEngineId::~QOrganizerEDSEngineId()
{
}

QOrganizerEDSEngineId::QOrganizerEDSEngineId(const QOrganizerEDSEngineId& other)
    : QOrganizerItemEngineId(),
      m_collectionId(other.m_collectionId),
      m_itemId(other.m_itemId)
{
}

QOrganizerEDSEngineId::QOrganizerEDSEngineId(const QString& idString)
    : QOrganizerItemEngineId()
{
    QString edsIdPart = idString.contains(":") ? idString.mid(idString.lastIndexOf(":")+1) : idString;
    QStringList idParts = edsIdPart.split("/");
    Q_ASSERT(idParts.count() == 2);
    m_collectionId = idParts.first();
    m_itemId = idParts.last();
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
    if (m_collectionId < otherPtr->m_collectionId)
        return true;
    if (m_collectionId == otherPtr->m_collectionId)
        return (m_itemId < otherPtr->m_itemId);
    return false;
}

QString QOrganizerEDSEngineId::managerUri() const
{
    return managerUriStatic();
}

QString QOrganizerEDSEngineId::toString() const
{
    return QString("%1/%2").arg(m_collectionId).arg(m_itemId);
}

QOrganizerItemEngineId* QOrganizerEDSEngineId::clone() const
{
    return new QOrganizerEDSEngineId(m_collectionId, m_itemId);
}

uint QOrganizerEDSEngineId::hash() const
{
    return qHash(m_itemId);
}

#ifndef QT_NO_DEBUG_STREAM
QDebug& QOrganizerEDSEngineId::debugStreamOut(QDebug& dbg) const
{
    dbg.nospace() << "QOrganizerEDSEngineId(" << managerNameStatic() << ", " << m_collectionId << ", " << m_itemId << ")";
    return dbg.maybeSpace();
}
#endif

QString QOrganizerEDSEngineId::managerNameStatic()
{
    return QStringLiteral("eds");
}

QString QOrganizerEDSEngineId::managerUriStatic()
{
    return QStringLiteral("qtorganizer:eds:");
}

QString QOrganizerEDSEngineId::toComponentId(const QtOrganizer::QOrganizerItemId &itemId, QString *rid)
{
    return toComponentId(itemId.toString(), rid);
}

QString QOrganizerEDSEngineId::toComponentId(const QString &itemId, QString *rid)
{
    QStringList ids = itemId.split("/").last().split("#");
    if (ids.size() == 2) {
        *rid = ids[1];
    }
    return ids[0];
}

ECalComponentId *QOrganizerEDSEngineId::toComponentIdObject(const QOrganizerItemId &itemId)
{
    QString rId;
    QString cId = toComponentId(itemId, &rId);

    ECalComponentId *id = g_new0(ECalComponentId, 1);
    id->uid = g_strdup(cId.toUtf8().data());
    if (rId.isEmpty()) {
        id->rid = NULL;
    } else {
        id->rid = g_strdup(rId.toUtf8().data());
    }

    return id;
}

QOrganizerEDSEngineId *QOrganizerEDSEngineId::fromComponentId(const QString &cId,
                                                              ECalComponentId *id,
                                                              QOrganizerEDSEngineId **parentId)
{
    QString iId = QString::fromUtf8(id->uid);
    QString rId = QString::fromUtf8(id->rid);

    if(!rId.isEmpty()) {
        *parentId = new QOrganizerEDSEngineId(cId, iId);
        iId += "#" + rId;
    }

    return new QOrganizerEDSEngineId(cId, iId);
}
