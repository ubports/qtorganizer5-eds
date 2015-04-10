/*
 * Copyright 2015 Canonical Ltd.
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

#ifndef QORGANIZER_PARSE_EVENT_THREAD_H
#define QORGANIZER_PARSE_EVENT_THREAD_H

#include <QObject>
#include <QList>
#include <QPointer>
#include <QThread>
#include <QByteArray>
#include <QMetaMethod>

#include <QtOrganizer/QOrganizerItemDetail>
#include <QtOrganizer/QOrganizerItem>

#include <glib.h>

class QOrganizerEDSCollectionEngineId;

class QOrganizerParseEventThread : public QThread
{
    Q_OBJECT
public:
    QOrganizerParseEventThread(QObject *source,
                               const QByteArray &slot,
                               QObject *parent = 0);

    void start(QMap<QOrganizerEDSCollectionEngineId *, GSList *> events,
               bool isIcalEvents,
               QList<QtOrganizer::QOrganizerItemDetail::DetailType> detailsHint);

private:
    QPointer<QObject> m_source;
    QMetaMethod m_slot;

    // parse data
    QMap<QOrganizerEDSCollectionEngineId *, GSList *> m_events;
    bool m_isIcalEvents;
    QList<QtOrganizer::QOrganizerItemDetail::DetailType> m_detailsHint;

    // virtual
    void run();
};

#endif
