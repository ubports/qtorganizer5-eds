#include "qorganizer-eds-parseeventthread.h"
#include "qorganizer-eds-engine.h"

#include <QDebug>

QOrganizerParseEventThread::QOrganizerParseEventThread(QObject *source,
                                                       const QByteArray &slot,
                                                       QObject *parent)
    : QThread(parent),
      m_source(source)
{
    qRegisterMetaType<QList<QOrganizerItem> >();
    int slotIndex = source->metaObject()->indexOfSlot(slot.mid(1));
    if (slotIndex == -1) {
        qWarning() << "Invalid slot:" << slot << "for object" << m_source;
    } else {
        m_slot = source->metaObject()->method(slotIndex);
    }

    connect(this, SIGNAL(finished()), SLOT(deleteLater()));
}

QOrganizerParseEventThread::~QOrganizerParseEventThread()
{
    Q_FOREACH(GSList *components ,m_events.values()) {
        if (m_isIcalEvents) {
            g_slist_free_full(components, (GDestroyNotify)icalcomponent_free);
        } else {
            g_slist_free_full(components, (GDestroyNotify)g_object_unref);
        }
    }
    m_events.clear();
}

void QOrganizerParseEventThread::start(QMap<QOrganizerCollectionId, GSList *> events,
                                       bool isIcalEvents,
                                       QList<QOrganizerItemDetail::DetailType> detailsHint)
{
    m_events = events;
    m_isIcalEvents = isIcalEvents;
    m_detailsHint = detailsHint;
    QThread::start();
}

void QOrganizerParseEventThread::run()
{
    QList<QOrganizerItem> result;

    Q_FOREACH(const QOrganizerCollectionId &id, m_events.keys()) {
        if (!m_source) {
            break;
        }
        result += QOrganizerEDSEngine::parseEvents(id, m_events.value(id), m_isIcalEvents, m_detailsHint);
    }

    if (m_source && m_slot.isValid()) {
        m_slot.invoke(m_source, Qt::QueuedConnection, Q_ARG(QList<QOrganizerItem>, result));
    }
}
