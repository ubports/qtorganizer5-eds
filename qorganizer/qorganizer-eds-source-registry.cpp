#include "qorganizer-eds-source-registry.h"

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
    qDebug() << Q_FUNC_INFO << (void*) this;
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

    m_collections.clear();
    m_collectionsMap.clear();

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
    return m_collectionsMap[collectionId];
}

ESource *SourceRegistry::source(const QString &collectionId) const
{
    QOrganizerEDSCollectionEngineId *id = m_collectionsMap[collectionId];
    if (id) {
        return id->m_esource;
    } else {
        return 0;
    }
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
    if (!collectionId.isEmpty()) {
        m_collections.remove(collectionId);
        m_collectionsMap.remove(collectionId);
        Q_EMIT sourceRemoved(collectionId);
    }
}

void SourceRegistry::remove(const QString &collectionId)
{
    QOrganizerCollection collection = m_collections.take(collectionId);
    if (!collection.id().isNull()) {
        m_collectionsMap.remove(collectionId);
        Q_EMIT sourceRemoved(collectionId);
    }
}

QString SourceRegistry::findCollection(ESource *source) const
{
    QMap<QString, QOrganizerEDSCollectionEngineId*>::const_iterator i = m_collectionsMap.constBegin();
    while (i != m_collectionsMap.constEnd()) {
        if (e_source_equal(i.value()->m_esource, source)) {
            return i.key();
        }
        i++;
    }
    return QString();
}

QOrganizerCollection SourceRegistry::registerSource(ESource *source)
{
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

            qDebug() << "EMIT SOURCE ADDED" << collectionId;
            Q_EMIT sourceAdded(collectionId);
        } else {
            qDebug() << "Source already exists.";
        }
        return collection;
    }
    return QOrganizerCollection();
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
    qDebug() << Q_FUNC_INFO << (void*) self;
    Q_UNUSED(registry);
    self->insert(source);
}

void SourceRegistry::onSourceChanged(ESourceRegistry *registry,
                                     ESource *source,
                                     SourceRegistry *self)
{
#if 0
    qDebug() << Q_FUNC_INFO << (void*) self;
    Q_UNUSED(registry);
    QString collectionId = self->findCollection(source);
    if (!collectionId.isEmpty() && self->m_collections.contains(collectionId)) {
        QOrganizerCollection &collection = self->m_collections[collectionId];
        self->updateCollection(&collection, source);
        Q_EMIT self->sourceUpdated(collectionId);
    } else {
        qWarning() << "Source changed not found";
    }
#endif
}

void SourceRegistry::onSourceRemoved(ESourceRegistry *registry,
                                     ESource *source,
                                     SourceRegistry *self)
{
    qDebug() << Q_FUNC_INFO << (void*) self;
    Q_UNUSED(registry);
    self->remove(source);
}

void SourceRegistry::updateCollection(QOrganizerCollection *collection,
                                      ESource *source)
{
    qDebug() << Q_FUNC_INFO << (void*) this;
    //TODO get metadata (color, etc..)
    collection->setMetaData(QOrganizerCollection::KeyName,
                            QString::fromUtf8(e_source_get_display_name(source)));
}
