#include "qorganizer-eds-factory.h"
#include "qorganizer-eds-collection-engineid.h"
#include "qorganizer-eds-engineid.h"
#include "qorganizer-eds-engine.h"

using namespace QtOrganizer;

QOrganizerManagerEngine* QOrganizerEDSFactory::engine(const QMap<QString, QString>& parameters, QOrganizerManager::Error* error)
{
    qDebug() << Q_FUNC_INFO << "eds";
    Q_UNUSED(error);

    return QOrganizerEDSEngine::createEDSEngine(parameters);
}

QOrganizerItemEngineId* QOrganizerEDSFactory::createItemEngineId(const QMap<QString, QString>& parameters, const QString& idString) const
{
    qDebug() << Q_FUNC_INFO;
    Q_UNUSED(parameters);
    return new QOrganizerEDSEngineId(idString);
}

QOrganizerCollectionEngineId* QOrganizerEDSFactory::createCollectionEngineId(const QMap<QString, QString>& parameters, const QString& idString) const
{
    qDebug() << Q_FUNC_INFO;
    Q_UNUSED(parameters);
    return new QOrganizerEDSCollectionEngineId(idString);
}

QString QOrganizerEDSFactory::managerName() const
{
    qDebug() << Q_FUNC_INFO;
    return QString::fromLatin1("eds");
}
