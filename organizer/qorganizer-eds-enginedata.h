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

#ifndef __QORGANIZER_EDS_ENGINEDATA_H__
#define __QORGANIZER_EDS_ENGINEDATA_H__

#include <QSharedData>
#include <QMap>

#include <QtOrganizer/QOrganizerAbstractRequest>
#include <QtOrganizer/QOrganizerManagerEngine>
#include <QtOrganizer/QOrganizerItemChangeSet>
#include <QtOrganizer/QOrganizerCollectionChangeSet>

class SourceRegistry;
class ViewWatcher;
class RequestData;

class QOrganizerEDSEngineData : public QSharedData
{
public:
    QOrganizerEDSEngineData();
    QOrganizerEDSEngineData(const QOrganizerEDSEngineData& other);
    ~QOrganizerEDSEngineData();

    template<class K>
    void emitSharedSignals(K* cs)
    {
        Q_FOREACH(QtOrganizer::QOrganizerManagerEngine* engine, m_sharedEngines) {
            cs->emitSignals(engine);
        }
    }

    ViewWatcher* watch(const QtOrganizer::QOrganizerCollectionId &collectionId);
    void unWatch(const QByteArray &sourceId);

    QAtomicInt m_refCount;
    SourceRegistry *m_sourceRegistry;
    QSet<QtOrganizer::QOrganizerManagerEngine*> m_sharedEngines;

private:
    QMap<QByteArray, ViewWatcher*> m_viewWatchers;

};

#endif
