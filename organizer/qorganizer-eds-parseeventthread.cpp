#include "qorganizer-eds-parseeventthread.h"
#include "qorganizer-eds-collection-engineid.h"
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

void QOrganizerParseEventThread::start(QMap<QOrganizerEDSCollectionEngineId *, GSList *> events,
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

    Q_FOREACH(QOrganizerEDSCollectionEngineId *id, m_events.keys()) {
        if (!m_source) {
            break;
        }
        result += QOrganizerEDSEngine::parseEvents(id, m_events.value(id), m_isIcalEvents, m_detailsHint);
    }

    if (m_source && m_slot.isValid()) {
        m_slot.invoke(m_source, Qt::QueuedConnection, Q_ARG(QList<QOrganizerItem>, result));
    }
}
