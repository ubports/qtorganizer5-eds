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
#include "qorganizer-eds-engineid.h"
#include "qorganizer-eds-source-registry.h"

#include <QtCore/QDebug>
#include <QtCore/QUuid>

#include <QtOrganizer/QOrganizerCollection>

QOrganizerEDSCollectionEngineId::QOrganizerEDSCollectionEngineId(ESource *source)
    : m_esource(source)
{
    g_object_ref(m_esource);
    m_collectionId = QString::fromUtf8(e_source_get_uid(m_esource));
    if (e_source_has_extension(m_esource, E_SOURCE_EXTENSION_CALENDAR)) {
        m_sourceType = E_CAL_CLIENT_SOURCE_TYPE_EVENTS;
    } else if (e_source_has_extension(m_esource, E_SOURCE_EXTENSION_TASK_LIST)) {
        m_sourceType = E_CAL_CLIENT_SOURCE_TYPE_TASKS;
    } else if (e_source_has_extension(m_esource, E_SOURCE_EXTENSION_MEMO_LIST)) {
        m_sourceType = E_CAL_CLIENT_SOURCE_TYPE_MEMOS;
    } else {
        qWarning() << "Source extension not supported";
        Q_ASSERT(false);
    }
}

QOrganizerEDSCollectionEngineId::QOrganizerEDSCollectionEngineId()
    : QOrganizerCollectionEngineId(),
      m_esource(0)
{
}

QOrganizerEDSCollectionEngineId::QOrganizerEDSCollectionEngineId(const QOrganizerEDSCollectionEngineId& other)
    : QOrganizerCollectionEngineId(),
      m_collectionId(other.m_collectionId),
      m_esource(other.m_esource),
      m_sourceType(other.m_sourceType)
{
    if (m_esource) {
        g_object_ref(m_esource);
    }
}

QOrganizerEDSCollectionEngineId::QOrganizerEDSCollectionEngineId(const QString& idString)
    : QOrganizerCollectionEngineId(),
      m_esource(0)
{
    // separate engine id part, if full id given
    m_collectionId = idString.contains(":") ? idString.mid(idString.lastIndexOf(":")+1) : idString;
}

QOrganizerEDSCollectionEngineId::~QOrganizerEDSCollectionEngineId()
{
    if (m_esource) {
        g_clear_object(&m_esource);
    }
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
    if (m_collectionId < otherPtr->m_collectionId)
        return true;
    return false;
}

QString QOrganizerEDSCollectionEngineId::managerUri() const
{
    return QOrganizerEDSEngineId::managerUriStatic();
}

// collections created by qorganizer will contain this format:
// <display-name-base-64>_<uuid-base-64>[_<account-id>]
int QOrganizerEDSCollectionEngineId::accountId() const
{
    QByteArray decodedId = QByteArray::fromBase64(m_collectionId.toUtf8());
    QList<QByteArray> idFields = decodedId.split(':');
    if (idFields.size() == 3) {
        bool ok = false;
        int id = idFields.last().toInt(&ok);
        if (ok)
            return id;
    }
    return -1;
}

QString QOrganizerEDSCollectionEngineId::genSourceId(const QtOrganizer::QOrganizerCollection &collection)
{
    QByteArray id;
    QUuid uuid = QUuid::createUuid();

    id = collection.metaData(QtOrganizer::QOrganizerCollection::KeyName).toString().toUtf8() +
         ":" + uuid.toByteArray();

    bool ok = false;
    int collectionId = collection.extendedMetaData(COLLECTION_ACCOUNT_ID_METADATA).toInt(&ok);
    if (ok) {
        id += QString(":%1").arg(collectionId);
    }

    return QString(id.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString QOrganizerEDSCollectionEngineId::toString() const
{
    return m_collectionId;
}

QOrganizerEDSCollectionEngineId* QOrganizerEDSCollectionEngineId::clone() const
{
    return new QOrganizerEDSCollectionEngineId(m_esource);
}

uint QOrganizerEDSCollectionEngineId::hash() const
{
    return qHash(m_collectionId);
}

#ifndef QT_NO_DEBUG_STREAM
QDebug& QOrganizerEDSCollectionEngineId::debugStreamOut(QDebug& dbg) const
{
    dbg.nospace() << "QOrganizerEDSCollectionEngineId(" << managerUri() << "," <<  QByteArray::fromBase64(m_collectionId.toUtf8()) << ")";
    return dbg.maybeSpace();
}


#endif

