#include "qorganizer-eds-source-registry.h"
#include "config.h"

#include <QtCore/QDebug>
#include <evolution-data-server-ubuntu/e-source-ubuntu.h>

using namespace QtOrganizer;

static const QString DEFAULT_COLLECTION_SETTINGS("qtpim/default-colection");

SourceRegistry::SourceRegistry(QObject *parent)
    : QObject(parent),
      m_sourceRegistry(0),
      m_sourceAddedId(0),
      m_sourceRemovedId(0),
      m_sourceChangedId(0),
      m_sourceEnabledId(0),
      m_sourceDisabledId(0),
      m_defaultSourceChangedId(0)
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
        g_signal_handler_disconnect(m_sourceRegistry, m_defaultSourceChangedId);

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
    m_defaultSourceChangedId =  g_signal_connect(m_sourceRegistry,
                     "notify::default-calendar",
                      G_CALLBACK(SourceRegistry::onDefaultCalendarChanged),
                      this);

    QByteArray defaultId = defaultCollectionId();
    GList *sources = e_source_registry_list_sources(m_sourceRegistry, 0);
    bool foundDefault = false;
    for(int i = 0, iMax = g_list_length(sources); i < iMax; i++) {
        ESource *source = E_SOURCE(g_list_nth_data(sources, i));
        bool isDefault = (g_strcmp0(defaultId.constData(), e_source_get_uid(source)) == 0);
        QOrganizerCollection collection = registerSource(source, isDefault);

        if (isDefault) {
            foundDefault = true;
            m_defaultCollection = collection;
        }
    }

    if (!foundDefault) {
        //fallback to first collection
        m_defaultCollection = m_collections.first();
    }

    g_list_free_full(sources, g_object_unref);
}

QtOrganizer::QOrganizerCollection SourceRegistry::defaultCollection() const
{
    return m_defaultCollection;
}

void SourceRegistry::setDefaultCollection(QtOrganizer::QOrganizerCollection &collection)
{
    if (m_defaultCollection.id() == collection.id())
        return;

    updateDefaultCollection(&collection);
    QString edsId = m_defaultCollection.id().toString().split(":").last();
    m_settings.setValue(DEFAULT_COLLECTION_SETTINGS, edsId);
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

    // update default collection if necessary
    if (m_defaultCollection.id().toString() == collectionId) {
        m_defaultCollection = QOrganizerCollection();
        setDefaultCollection(m_collections.first());
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

            client = E_CAL_CLIENT_CONNECT_SYNC(eid->m_esource, eid->m_sourceType, 0, &gError);
            if (gError) {
                qWarning() << "Fail to connect with client" << gError->message;
                g_error_free(gError);
            } else {
                // If the client is read only update the collection
                if (e_client_is_readonly(client)) {
                    QOrganizerCollection &c = m_collections[collectionId];
                    c.setExtendedMetaData(COLLECTION_READONLY_METADATA, true);
                    Q_EMIT sourceUpdated(collectionId);
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

QOrganizerCollection SourceRegistry::registerSource(ESource *source, bool isDefault)
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
            QOrganizerCollection collection = parseSource(source, isDefault, &edsId);
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

void SourceRegistry::updateDefaultCollection(QOrganizerCollection *collection)
{
    if (m_defaultCollection.id() != collection->id()) {
        QString oldDefaultCollectionId = m_defaultCollection.id().toString();

        collection->setExtendedMetaData(COLLECTION_DEFAULT_METADATA, true);
        m_defaultCollection = *collection;
        Q_EMIT sourceUpdated(m_defaultCollection.id().toString());

        if (m_collections.contains(oldDefaultCollectionId)) {
            QOrganizerCollection &old = m_collections[oldDefaultCollectionId];
            old.setExtendedMetaData(COLLECTION_DEFAULT_METADATA, false);
            Q_EMIT sourceUpdated(oldDefaultCollectionId);
        }
    }
}

QOrganizerCollection SourceRegistry::parseSource(ESource *source,
                                                 bool isDefault,
                                                 QOrganizerEDSCollectionEngineId **edsId)
{
    *edsId = new QOrganizerEDSCollectionEngineId(source);
    QOrganizerCollectionId id(*edsId);
    QOrganizerCollection collection;

    // id
    collection.setId(id);

    updateCollection(&collection, isDefault, source);
    return collection;
}

ESource *SourceRegistry::newSourceFromCollection(const QtOrganizer::QOrganizerCollection &collection)
{
    if (!collection.id().isNull()) {
        qWarning() << "Fail to create source from collection: Collection already has id.";
        return NULL;
    }

    GError *gError = 0;
    ESource *source = e_source_new(NULL, NULL, &gError);

    if (gError) {
        qWarning() << "Fail to create source:" << gError->message;
        g_error_free(gError);
        return NULL;
    }

    bool ok = false;
    int accountId = collection.extendedMetaData(COLLECTION_ACCOUNT_ID_METADATA).toInt(&ok);
    if (!ok) {
        accountId = 0;
    }
    bool syncReadOnly = collection.extendedMetaData(COLLECTION_SYNC_READONLY_METADATA).toBool();

    e_source_set_parent(source, "local-stub");
    ESourceUbuntu *extUbuntu = E_SOURCE_UBUNTU(e_source_get_extension(source, E_SOURCE_EXTENSION_UBUNTU));
    Q_ASSERT(extUbuntu);
    e_source_ubuntu_set_account_id(extUbuntu, accountId);
    e_source_ubuntu_set_writable(extUbuntu, syncReadOnly);
    e_source_ubuntu_set_autoremove(extUbuntu, TRUE);

    return source;
}

QByteArray SourceRegistry::defaultCollectionId() const
{
    QVariant id = m_settings.value(DEFAULT_COLLECTION_SETTINGS);
    if (id.isValid()) {
        return id.toString().toUtf8();
    }

    // fallback to eds default collection
    ESource *defaultCalendarSource = e_source_registry_ref_default_calendar(m_sourceRegistry);
    QString eId = QString::fromUtf8(e_source_get_uid(defaultCalendarSource));
    g_object_unref(defaultCalendarSource);
    return eId.toUtf8();
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
        self->updateCollection(&collection,
                               self->m_defaultCollection.id() == collection.id(),
                               source, self->m_clients.value(collectionId));
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

void SourceRegistry::onDefaultCalendarChanged(ESourceRegistry *registry,
                                              GParamSpec *pspec,
                                              SourceRegistry *self)
{
    Q_UNUSED(registry);
    Q_UNUSED(pspec);

    if (self->m_settings.value(DEFAULT_COLLECTION_SETTINGS).isValid()) {
        // we are using client confinguration
        return;
    }

    ESource *defaultCalendar = e_source_registry_ref_default_calendar(self->m_sourceRegistry);
    if (!defaultCalendar)
        return;

    QString collectionId = self->findCollection(defaultCalendar);
    if (!collectionId.isEmpty()) {
        QOrganizerCollection &collection = self->m_collections[collectionId];
        self->updateDefaultCollection(&collection);
    }
    g_object_unref(defaultCalendar);
}

void SourceRegistry::updateCollection(QOrganizerCollection *collection,
                                      bool isDefault,
                                      ESource *source,
                                      EClient *client)
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
    // the source and client need to be writable
    if (client) {
        writable = writable && !e_client_is_readonly(client);
    }
    collection->setExtendedMetaData(COLLECTION_READONLY_METADATA, !writable);

    // default
    collection->setExtendedMetaData(COLLECTION_DEFAULT_METADATA, isDefault);

    // Ubuntu Extension
    if (e_source_has_extension(source, E_SOURCE_EXTENSION_UBUNTU)) {
        ESourceUbuntu *extUbuntu = E_SOURCE_UBUNTU(e_source_get_extension(source, E_SOURCE_EXTENSION_UBUNTU));

        collection->setExtendedMetaData(COLLECTION_ACCOUNT_ID_METADATA, e_source_ubuntu_get_account_id(extUbuntu));
        bool syncWritable = e_source_ubuntu_get_writable(extUbuntu);

        collection->setExtendedMetaData(COLLECTION_SYNC_READONLY_METADATA, !syncWritable);
        if (!syncWritable) {
            // Set account as read-only if not sync writable
            collection->setExtendedMetaData(COLLECTION_READONLY_METADATA, syncWritable);
        }
    }
}
