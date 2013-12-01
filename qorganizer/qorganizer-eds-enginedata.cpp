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

#include "qorganizer-eds-enginedata.h"
#include "qorganizer-eds-viewwatcher.h"
#include "qorganizer-eds-source-registry.h"

QOrganizerEDSEngineData::QOrganizerEDSEngineData()
    : QSharedData(),
      m_sourceRegistry(0)
{
}

QOrganizerEDSEngineData::QOrganizerEDSEngineData(const QOrganizerEDSEngineData& other)
    : QSharedData(other)
{
}

QOrganizerEDSEngineData::~QOrganizerEDSEngineData()
{
    qDeleteAll(m_viewWatchers);
    m_viewWatchers.clear();

    if (m_sourceRegistry) {
        delete m_sourceRegistry;
        m_sourceRegistry = 0;
    }
}

ViewWatcher* QOrganizerEDSEngineData::watch(const QString &collectionId)
{
    ViewWatcher *vw = m_viewWatchers[collectionId];
    if (!vw) {
        vw = new ViewWatcher(collectionId, this, m_sourceRegistry->client(collectionId));
        m_viewWatchers.insert(collectionId, vw);
    }
    return vw;
}

void QOrganizerEDSEngineData::unWatch(const QString &collectionId)
{
    ViewWatcher *viewW = m_viewWatchers.take(collectionId);
    if (viewW) {
        delete viewW;
    }
}
