/*
 * Copyright 2013 Canonical Ltd.
 *
 * This file is part of ubuntu-pim-service.
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

#ifndef __QORGANIZER_EDS_SOURCEREGISTRY_H__
#define __QORGANIZER_EDS_SOURCEREGISTRY_H__

#include <QtCore/QObject>
#include <QtCore/QSettings>

#include <QtOrganizer/QOrganizerCollectionId>
#include <QtOrganizer/QOrganizerCollection>

#include <libecal/libecal.h>

#include "qorganizer-eds-collection-engineid.h"

#define COLLECTION_CALLENDAR_TYPE_METADATA  "collection-type"
#define COLLECTION_SELECTED_METADATA        "collection-selected"
#define COLLECTION_READONLY_METADATA        "collection-readonly"
#define COLLECTION_DEFAULT_METADATA         "collection-default"
#define COLLECTION_SYNC_READONLY_METADATA   "collection-sync-readonly"
#define COLLECTION_ACCOUNT_ID_METADATA      "collection-account-id"
#define COLLECTION_DATA_METADATA            "collection-metadata"

class SourceRegistry : public QObject
{
    Q_OBJECT
public:
    SourceRegistry(QObject *parent=0);
    ~SourceRegistry();

    ESourceRegistry *object() const;
    void load();
    QtOrganizer::QOrganizerCollection defaultCollection() const;
    void setDefaultCollection(QtOrganizer::QOrganizerCollection &collection);
    QtOrganizer::QOrganizerCollection collection(const QString &collectionId) const;
    QList<QtOrganizer::QOrganizerCollection> collections() const;
    QStringList collectionsIds() const;
    QList<QOrganizerEDSCollectionEngineId*> collectionsEngineIds() const;
    ESource *source(const QString &collectionId) const;
    QOrganizerEDSCollectionEngineId* collectionEngineId(const QString &collectionId) const;
    QtOrganizer::QOrganizerCollection collection(ESource *source) const;
    QtOrganizer::QOrganizerCollection insert(ESource *source);
    void remove(ESource *source);
    void remove(const QString &collectionId);
    EClient *client(const QString &collectionId);
    void clear();

    static QtOrganizer::QOrganizerCollection parseSource(ESource *source,
                                                         bool isDefault,
                                                         QOrganizerEDSCollectionEngineId **edsId);
    static ESource *newSourceFromCollection(const QtOrganizer::QOrganizerCollection &collection);

Q_SIGNALS:
    void sourceAdded(const QString &collectionId);
    void sourceRemoved(const QString &collectionId);
    void sourceUpdated(const QString &collectionId);

private:
    QSettings m_settings;
    ESourceRegistry *m_sourceRegistry;
    QtOrganizer::QOrganizerCollection m_defaultCollection;
    QMap<QString, EClient*> m_clients;
    QMap<QString, ESource*> m_sources;
    QMap<QString, QtOrganizer::QOrganizerCollection> m_collections;
    QMap<QString, QOrganizerEDSCollectionEngineId*> m_collectionsMap;

    // handler id
    int m_sourceAddedId;
    int m_sourceRemovedId;
    int m_sourceChangedId;
    int m_sourceEnabledId;
    int m_sourceDisabledId;
    int m_defaultSourceChangedId;

    QByteArray defaultCollectionId() const;
    QString findCollection(ESource *source) const;
    QtOrganizer::QOrganizerCollection registerSource(ESource *source, bool isDefault = false);
    void updateDefaultCollection(QtOrganizer::QOrganizerCollection *collection);
    static void updateCollection(QtOrganizer::QOrganizerCollection *collection,
                                 bool isDefault,
                                 ESource *source,
                                 EClient *client = 0);


    // glib callback
    static void onSourceAdded(ESourceRegistry *registry,
                              ESource *source,
                              SourceRegistry *self);
    static void onSourceChanged(ESourceRegistry *registry,
                                ESource *source,
                                SourceRegistry *self);
    static void onSourceRemoved(ESourceRegistry *registry,
                                ESource *source,
                                SourceRegistry *self);
    static void onDefaultCalendarChanged(ESourceRegistry *registry,
                                         GParamSpec *pspec,
                                         SourceRegistry *self);
};

#endif
