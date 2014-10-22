#include "qorganizer-eds-source-registry.h"

#include <QtCore/QDebug>

using namespace QtOrganizer;

SourceRegistry::SourceRegistry(QObject *parent)
    : QObject(parent),
      m_sourceRegistry(0),
      m_sourceAddedId(0),
      m_sourceRemovedId(0),
      m_sourceChangedId(0),
      m_sourceEnabledId(0),
      m_sourceDisabledId(0)
{
}

SourceRegistry::~SourceRegistry()
{
    clear();

    if (m_sourceRegistry) {
        g_signal_handler_disconnect(m_sourceRegistry, m_sourceAddedId);
        g_signal_handler_disconnect(m_sourceRegistry, m_sourceRemovedId);
        g_signal_handler_disconnect(m_sourceRegistry, m_sourceChangedId);
        g_signal_handler_disconnect(m_sourceRegistry, m_sourceEnabledId);
        g_signal_handler_disconnect(m_sourceRegistry, m_sourceDisabledId);

        g_clear_object(&m_sourceRegistry);

    }
}

ESourceRegistry *SourceRegistry::object() const
{
    return m_sourceRegistry;
}

void SourceRegistry::load()
{
    if (m_sourceRegistry) {
        return;
    }

    clear();

    GError *error = 0;
    m_sourceRegistry = e_source_registry_new_sync(0, &error);
    if (error) {
        qWarning() << "Fail to create sourge registry:" << error->message;
        g_error_free(error);
        return;
    }

    m_sourceAddedId = g_signal_connect(m_sourceRegistry,
                     "source-added",
                     (GCallback) SourceRegistry::onSourceAdded,
                     this);
    m_sourceChangedId = g_signal_connect(m_sourceRegistry,
                     "source-changed",
                     (GCallback) SourceRegistry::onSourceChanged,
                     this);
    m_sourceDisabledId = g_signal_connect(m_sourceRegistry,
                     "source-disabled",
                     (GCallback) SourceRegistry::onSourceRemoved,
                     this);
    m_sourceEnabledId = g_signal_connect(m_sourceRegistry,
                     "source-enabled",
                     (GCallback) SourceRegistry::onSourceAdded,
                     this);
    m_sourceRemovedId = g_signal_connect(m_sourceRegistry,
                     "source-removed",
                     (GCallback) SourceRegistry::onSourceRemoved,
                     this);


    // We use calendar as default source, if you are trying to use other source type
    // you need to set the item source id manually
    ESource *defaultCalendarSource = e_source_registry_ref_default_calendar(m_sourceRegistry);

    GList *sources = e_source_registry_list_sources(m_sourceRegistry, 0);
    for(int i = 0, iMax = g_list_length(sources); i < iMax; i++) {
        ESource *source = E_SOURCE(g_list_nth_data(sources, i));

        QOrganizerCollection collection = registerSource(source);
        if (e_source_equal(defaultCalendarSource, source)) {
            m_defaultCollection = collection;
        }
    }

    g_list_free_full(sources, g_object_unref);

    if (defaultCalendarSource) {
        g_object_unref(defaultCalendarSource);
    }
}


QtOrganizer::QOrganizerCollection SourceRegistry::defaultCollection() const
{
    return m_defaultCollection;
}

QOrganizerCollection SourceRegistry::collection(const QString &collectionId) const
{
    return m_collections.value(collectionId);
}

QList<QOrganizerCollection> SourceRegistry::collections() const
{
    return m_collections.values();
}

QStringList SourceRegistry::collectionsIds() const
{
    return m_collections.keys();
}

QList<QOrganizerEDSCollectionEngineId *> SourceRegistry::collectionsEngineIds() const
{
    return m_collectionsMap.values();
}

QOrganizerEDSCollectionEngineId *SourceRegistry::collectionEngineId(const QString &collectionId) const
{
    return m_collectionsMap.value(collectionId, 0);
}

ESource *SourceRegistry::source(const QString &collectionId) const
{
    return m_sources[collectionId];
}

QOrganizerCollection SourceRegistry::collection(ESource *source) const
{
    QString collectionId = findCollection(source);
    return m_collections[collectionId];
}

QOrganizerCollection SourceRegistry::insert(ESource *source)
{
    return registerSource(source);
}

void SourceRegistry::remove(ESource *source)
{
    QString collectionId = findCollection(source);
    remove(collectionId);
}

void SourceRegistry::remove(const QString &collectionId)
{
    if (collectionId.isEmpty()) {
        return;
    }

    QOrganizerCollection collection = m_collections.take(collectionId);
    if (!collection.id().isNull()) {
        Q_EMIT sourceRemoved(collectionId);
        m_collectionsMap.remove(collectionId);
        g_object_unref(m_sources.take(collectionId));
        EClient *client = m_clients.take(collectionId);
        if (client) {
            g_object_unref(client);
        }
    }
}

EClient* SourceRegistry::client(const QString &collectionId)
{
    if (collectionId.isEmpty()) {
        return 0;
    }

    EClient *client = m_clients.value(collectionId, 0);
    if (!client) {
        QOrganizerEDSCollectionEngineId *eid = m_collectionsMap[collectionId];
        if (eid) {
            GError *gError = 0;
            client = e_cal_client_connect_sync(eid->m_esource, eid->m_sourceType, 0, &gError);
            if (gError) {
                qWarning() << "Fail to connect with client" << gError->message;
                g_error_free(gError);
            } else {
                // If the client is read only update the collection
                if (e_client_is_readonly(client)) {
                    QOrganizerCollection &c = m_collections[collectionId];
                    c.setExtendedMetaData(COLLECTION_READONLY_METADATA, true);
                }
                m_clients.insert(collectionId, client);
            }
        }
    }
    if (client) {
        g_object_ref(client);
    }
    return client;
}

void SourceRegistry::clear()
{
    Q_FOREACH(ESource *source, m_sources.values()) {
        g_object_unref(source);
    }

    Q_FOREACH(EClient *client, m_clients.values()) {
        g_object_unref(client);
    }

    m_sources.clear();
    m_collections.clear();
    m_collectionsMap.clear();
    m_clients.clear();
}

QString SourceRegistry::findCollection(ESource *source) const
{
    QMap<QString, ESource*>::ConstIterator i = m_sources.constBegin();
    while (i != m_sources.constEnd()) {
        if (e_source_equal(source, i.value())) {
            return i.key();
        }
        i++;
    }
    return QString();
}

QOrganizerCollection SourceRegistry::registerSource(ESource *source)
{
    QString collectionId = findCollection(source);
    if (collectionId.isEmpty()) {
        bool isEnabled = e_source_get_enabled(source);
        bool isCalendar = e_source_has_extension(source, E_SOURCE_EXTENSION_CALENDAR);
        bool isTaskList = e_source_has_extension(source, E_SOURCE_EXTENSION_TASK_LIST);
        bool isMemoList = e_source_has_extension(source, E_SOURCE_EXTENSION_MEMO_LIST);
        bool isAlarms = e_source_has_extension(source, E_SOURCE_EXTENSION_ALARMS);

        if ( isEnabled && (isCalendar || isTaskList || isMemoList || isAlarms)) {
            QOrganizerEDSCollectionEngineId *edsId = 0;
            QOrganizerCollection collection = parseSource(source, &edsId);
            QString collectionId = collection.id().toString();

            if (!m_collectionsMap.contains(collectionId)) {
                m_collections.insert(collectionId, collection);
                m_collectionsMap.insert(collectionId, edsId);
                m_sources.insert(collectionId, source);
                g_object_ref(source);

                Q_EMIT sourceAdded(collectionId);
            } else {
                Q_ASSERT(false);
            }
            return collection;
        }
        return QOrganizerCollection();
    } else {
        return m_collections.value(collectionId);
    }

}

QOrganizerCollection SourceRegistry::parseSource(ESource *source,
                                                 QOrganizerEDSCollectionEngineId **edsId)
{
    *edsId = new QOrganizerEDSCollectionEngineId(source);
    QOrganizerCollectionId id(*edsId);
    QOrganizerCollection collection;

    collection.setId(id);
    updateCollection(&collection, source);
    return collection;
}

void SourceRegistry::onSourceAdded(ESourceRegistry *registry,
                                   ESource *source,
                                   SourceRegistry *self)
{
    Q_UNUSED(registry);
    self->insert(source);
}

void SourceRegistry::onSourceChanged(ESourceRegistry *registry,
                                     ESource *source,
                                     SourceRegistry *self)
{
    Q_UNUSED(registry);
    QString collectionId = self->findCollection(source);
    if (!collectionId.isEmpty() && self->m_collections.contains(collectionId)) {
        QOrganizerCollection &collection = self->m_collections[collectionId];
        self->updateCollection(&collection, source);
        Q_EMIT self->sourceUpdated(collectionId);
    } else {
        qWarning() << "Source changed not found";
    }
}

void SourceRegistry::onSourceRemoved(ESourceRegistry *registry,
                                     ESource *source,
                                     SourceRegistry *self)
{
    Q_UNUSED(registry);
    self->remove(source);
}

void SourceRegistry::updateCollection(QOrganizerCollection *collection,
                                      ESource *source)
{
    // name
    collection->setMetaData(QOrganizerCollection::KeyName,
                            QString::fromUtf8(e_source_get_display_name(source)));

    // extension
    ESourceBackend *extCalendar;
    if (e_source_has_extension(source, E_SOURCE_EXTENSION_TASK_LIST)) {
        extCalendar = E_SOURCE_BACKEND(e_source_get_extension(source, E_SOURCE_EXTENSION_TASK_LIST));
        collection->setExtendedMetaData(COLLECTION_CALLENDAR_TYPE_METADATA, E_SOURCE_EXTENSION_TASK_LIST);
    } else if (e_source_has_extension(source, E_SOURCE_EXTENSION_MEMO_LIST)) {
        extCalendar = E_SOURCE_BACKEND(e_source_get_extension(source, E_SOURCE_EXTENSION_MEMO_LIST));
        collection->setExtendedMetaData(COLLECTION_CALLENDAR_TYPE_METADATA, E_SOURCE_EXTENSION_MEMO_LIST);
    } else {
        extCalendar = E_SOURCE_BACKEND(e_source_get_extension(source, E_SOURCE_EXTENSION_CALENDAR));
        collection->setExtendedMetaData(COLLECTION_CALLENDAR_TYPE_METADATA, E_SOURCE_EXTENSION_CALENDAR);
    }

    // color
    const gchar *color = e_source_selectable_get_color(E_SOURCE_SELECTABLE(extCalendar));
    collection->setMetaData(QOrganizerCollection::KeyColor, QString::fromUtf8(color));

    // selected
    bool selected = (e_source_selectable_get_selected(E_SOURCE_SELECTABLE(extCalendar)) == TRUE);
    collection->setExtendedMetaData(COLLECTION_SELECTED_METADATA, selected);

    // writable
    bool writable = e_source_get_writable(source);
    collection->setExtendedMetaData(COLLECTION_READONLY_METADATA, !writable);
}
