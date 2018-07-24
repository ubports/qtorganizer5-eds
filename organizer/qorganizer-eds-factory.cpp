#include "qorganizer-eds-factory.h"
#include "qorganizer-eds-engine.h"

#include <QtCore/QDebug>

using namespace QtOrganizer;

QOrganizerManagerEngine* QOrganizerEDSFactory::engine(const QMap<QString, QString>& parameters, QOrganizerManager::Error* error)
{
    Q_UNUSED(error);
    return QOrganizerEDSEngine::createEDSEngine(parameters);
}

QString QOrganizerEDSFactory::managerName() const
{
    return QString::fromLatin1("eds");
}
