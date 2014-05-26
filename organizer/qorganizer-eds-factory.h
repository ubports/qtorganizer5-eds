/*
 * Copyright 2013 Canonical Ltd.
 *
 * This file is part of contact-service-app.
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

#ifndef __QORGANIZER_EDS_FACTORY_H__
#define __QORGANIZER_EDS_FACTORY_H__

#include <qorganizermanagerenginefactory.h>
#include <qorganizermanagerengine.h>
#include <qorganizeritemengineid.h>
#include <qorganizercollectionengineid.h>

class QOrganizerEDSFactory : public QtOrganizer::QOrganizerManagerEngineFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QOrganizerManagerEngineFactoryInterface" FILE "eds.json")

public:
    QtOrganizer::QOrganizerManagerEngine* engine(const QMap<QString, QString>& parameters, QtOrganizer::QOrganizerManager::Error*);
    QtOrganizer::QOrganizerItemEngineId* createItemEngineId(const QMap<QString, QString>& parameters, const QString& idString) const;
    QtOrganizer::QOrganizerCollectionEngineId* createCollectionEngineId(const QMap<QString, QString>& parameters, const QString& idString) const;
    QString managerName() const;
};

#endif
