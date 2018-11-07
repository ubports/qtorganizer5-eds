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

void SourceRegistry::load(const QString &managerUri)
{
    if (m_sourceRegistry) {
        Q_ASSERT(managerUri == m_managerUri);
        return;
    }

    clear();
    m_managerUri = managerUri;

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
                     (GCallback) SourceRegistry::onSourceEnabled,
                     this);
    m_sourceRemovedId = g_signal_connect(m_sourceRegistry,
                     "source-removed",
                     (GCallback) SourceRegistry::onSourceRemoved,
                     this);
    m_defaultSourceChangedId =  g_signal_connect(m_sourceRegistry,
                     "notify::default-calendar",
                      G_CALLBACK(SourceRegistry::onDefaultCalendarChanged),
                      this);

    QByteArray defaultId = defaultSourceId();
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
    QString edsId = QString::fromUtf8(m_defaultCollection.id().localId());
    m_settings.setValue(DEFAULT_COLLECTION_SETTINGS, edsId);
}

QOrganizerCollection SourceRegistry::collection(const QByteArray &sourceId) const
{
    return m_collections.value(sourceId);
}

QList<QOrganizerCollection> SourceRegistry::collections() const
{
    return m_collections.values();
}

QByteArrayList SourceRegistry::sourceIds() const
{
    return m_collections.keys();
}

ESource *SourceRegistry::source(const QByteArray &sourceId) const
{
    return m_sources[sourceId];
}

QOrganizerCollectionId SourceRegistry::collectionId(const QByteArray &sourceId) const
{
    return collection(sourceId).id();
}

QOrganizerCollection SourceRegistry::collection(ESource *source) const
{
    QByteArray sourceId = findSource(source);
    return m_collections[sourceId];
}

void SourceRegistry::expectSourceCreation(ESource *source)
{
    m_expectedNewSources.append((ESource*)g_object_ref(source));
}

void SourceRegistry::insert(ESource *source)
{
    bool isDefault = false;

    for (auto i = m_expectedNewSources.begin();
         i != m_expectedNewSources.end();
         i++) {
        ESource *expectedSource = *i;
        if (e_source_equal(expectedSource, source)) {
            /* SaveCollectionRequestData stored the "default" flag into the
             * source's GObject data */
            isDefault = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(expectedSource), "is-default"));

            m_expectedNewSources.erase(i);
            g_object_unref(expectedSource);
            break;
        }
    }

    QOrganizerCollection collection = registerSource(source, isDefault);
    if (isDefault) {
        setDefaultCollection(collection);
    }
}

void SourceRegistry::remove(ESource *source)
{
    QByteArray sourceId = findSource(source);
    remove(sourceId);
}

void SourceRegistry::remove(const QByteArray &sourceId)
{
    if (sourceId.isEmpty()) {
        return;
    }

    QOrganizerCollection collection = m_collections.take(sourceId);
    if (!collection.id().isNull()) {
        Q_EMIT sourceRemoved(sourceId);
        g_object_unref(m_sources.take(sourceId));
        EClient *client = m_clients.take(sourceId);
        if (client) {
            g_object_unref(client);
        }
    }

    // update default collection if necessary
    if (m_defaultCollection.id().localId() == sourceId) {
        m_defaultCollection = QOrganizerCollection();
        setDefaultCollection(m_collections.first());
    }
}

EClient* SourceRegistry::client(const QByteArray &sourceId)
{
    if (sourceId.isEmpty()) {
        return 0;
    }

    EClient *client = m_clients.value(sourceId, 0);
    if (!client) {
        const auto i = m_sources.find(sourceId);
        if (i != m_sources.end()) {
            GError *gError = 0;

            ESource *source = i.value();
            ECalClientSourceType sourceType;
            if (e_source_has_extension(source, E_SOURCE_EXTENSION_CALENDAR)) {
                sourceType = E_CAL_CLIENT_SOURCE_TYPE_EVENTS;
            } else if (e_source_has_extension(source, E_SOURCE_EXTENSION_TASK_LIST)) {
                sourceType = E_CAL_CLIENT_SOURCE_TYPE_TASKS;
            } else if (e_source_has_extension(source, E_SOURCE_EXTENSION_MEMO_LIST)) {
                sourceType = E_CAL_CLIENT_SOURCE_TYPE_MEMOS;
            } else {
                qWarning() << "Source extension not supported";
                Q_ASSERT(false);
            }

            client = E_CAL_CLIENT_CONNECT_SYNC(source, sourceType, 0, &gError);
            if (gError) {
                qWarning() << "Fail to connect with client" << gError->message;
                g_error_free(gError);
            } else {
                // If the client is read only update the collection
                if (e_client_is_readonly(client)) {
                    QOrganizerCollection &c = m_collections[sourceId];
                    c.setExtendedMetaData(COLLECTION_READONLY_METADATA, true);
                    Q_EMIT sourceUpdated(sourceId);
                }
                m_clients.insert(sourceId, client);
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
    m_clients.clear();

    for (ESource *source: m_expectedNewSources) {
        g_object_unref(source);
    }
    m_expectedNewSources.clear();
}

QByteArray SourceRegistry::findSource(ESource *source) const
{
    QMap<QByteArray, ESource*>::ConstIterator i = m_sources.constBegin();
    while (i != m_sources.constEnd()) {
        if (e_source_equal(source, i.value())) {
            return i.key();
        }
        i++;
    }
    return QByteArray();
}

QOrganizerCollection SourceRegistry::registerSource(ESource *source, bool isDefault)
{
    QByteArray sourceId = findSource(source);
    if (sourceId.isEmpty()) {
        bool isEnabled = e_source_get_enabled(source);
        bool isCalendar = e_source_has_extension(source, E_SOURCE_EXTENSION_CALENDAR);
        bool isTaskList = e_source_has_extension(source, E_SOURCE_EXTENSION_TASK_LIST);
        bool isMemoList = e_source_has_extension(source, E_SOURCE_EXTENSION_MEMO_LIST);
        bool isAlarms = e_source_has_extension(source, E_SOURCE_EXTENSION_ALARMS);

        if ( isEnabled && (isCalendar || isTaskList || isMemoList || isAlarms)) {
            QOrganizerCollection collection = parseSource(m_managerUri, source, isDefault);
            sourceId = collection.id().localId();

            if (!m_sources.contains(sourceId)) {
                m_collections.insert(sourceId, collection);
                m_sources.insert(sourceId, source);
                g_object_ref(source);

                Q_EMIT sourceAdded(sourceId);
            } else {
                Q_ASSERT(false);
            }
            return collection;
        }
        return QOrganizerCollection();
    } else {
        return m_collections.value(sourceId);
    }

}

void SourceRegistry::updateDefaultCollection(QOrganizerCollection *collection)
{
    if (m_defaultCollection.id() != collection->id()) {
        QByteArray oldDefaultSourceId = m_defaultCollection.id().localId();

        collection->setExtendedMetaData(COLLECTION_DEFAULT_METADATA, true);
        m_defaultCollection = *collection;
        Q_EMIT sourceUpdated(m_defaultCollection.id().localId());

        if (m_collections.contains(oldDefaultSourceId)) {
            QOrganizerCollection &old = m_collections[oldDefaultSourceId];
            old.setExtendedMetaData(COLLECTION_DEFAULT_METADATA, false);
            Q_EMIT sourceUpdated(oldDefaultSourceId);
        }
    }
}

QOrganizerCollection SourceRegistry::parseSource(const QString &managerUri,
                                                 ESource *source,
                                                 bool isDefault)
{
    QByteArray sourceId(e_source_get_uid(source));
    QOrganizerCollectionId id(managerUri, sourceId);
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

    e_source_set_parent(source, "local-stub");
    return source;
}

QByteArray SourceRegistry::defaultSourceId() const
{
    QVariant id = m_settings.value(DEFAULT_COLLECTION_SETTINGS);
    if (id.isValid()) {
        return id.toString().toUtf8();
    }

    // fallback to eds default collection
    ESource *defaultCalendarSource = e_source_registry_ref_default_calendar(m_sourceRegistry);
    QByteArray eId(e_source_get_uid(defaultCalendarSource));
    g_object_unref(defaultCalendarSource);
    return eId;
}

void SourceRegistry::onSourceAdded(ESourceRegistry *registry,
                                   ESource *source,
                                   SourceRegistry *self)
{
    Q_UNUSED(registry);
    qDebug() << Q_FUNC_INFO << source;
    self->insert(source);
}

void SourceRegistry::onSourceEnabled(ESourceRegistry *registry,
                                     ESource *source,
                                     SourceRegistry *self)
{
    Q_UNUSED(registry);
    self->registerSource(source);
}

void SourceRegistry::onSourceChanged(ESourceRegistry *registry,
                                     ESource *source,
                                     SourceRegistry *self)
{
    Q_UNUSED(registry);
    QByteArray sourceId = self->findSource(source);
    if (!sourceId.isEmpty() && self->m_collections.contains(sourceId)) {
        QOrganizerCollection &collection = self->m_collections[sourceId];
        self->updateCollection(&collection,
                               self->m_defaultCollection.id() == collection.id(),
                               source, self->m_clients.value(sourceId));
        Q_EMIT self->sourceUpdated(sourceId);
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

    QByteArray sourceId = self->findSource(defaultCalendar);
    if (!sourceId.isEmpty()) {
        QOrganizerCollection &collection = self->m_collections[sourceId];
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
    /* For some reason the EDS we have in xenial (3.18.5) sets the "readable"
     * property to false immediately after the object is retrieved: this can be
     * verified by watching the "notify:readable" signal.
     * This might be the bug recently fixed with EDS commit
     * ff1af187f8bd608c85549aaa877cd03bcc54d7be, but in any case we cannot rely
     * on the value of this property for the time being.
    // the source and client need to be writable
    if (client) {
        writable = writable && !e_client_is_readonly(client);
    }
    */
    collection->setExtendedMetaData(COLLECTION_READONLY_METADATA, !writable);

    // default
    collection->setExtendedMetaData(COLLECTION_DEFAULT_METADATA, isDefault);

    // Ubuntu Extension
    ESourceUbuntu *extUbuntu = E_SOURCE_UBUNTU(e_source_get_extension(source, E_SOURCE_EXTENSION_UBUNTU));
    if (extUbuntu) {
        collection->setExtendedMetaData(COLLECTION_ACCOUNT_ID_METADATA, e_source_ubuntu_get_account_id(extUbuntu));
        writable = e_source_ubuntu_get_writable(extUbuntu) == TRUE;
        collection->setExtendedMetaData(COLLECTION_SYNC_READONLY_METADATA, !writable);
        if (!writable) {
            // Set account as read-only if not writable
            collection->setExtendedMetaData(COLLECTION_READONLY_METADATA, true);
        }
        const gchar *data = e_source_ubuntu_get_metadata(extUbuntu);
        collection->setExtendedMetaData(COLLECTION_DATA_METADATA, data ? QString::fromUtf8(data) : QString());
    }

}
