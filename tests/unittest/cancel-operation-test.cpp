/*
 * Copyright 2013 Canonical Ltd.
 *
 * This file is part of qtorganizer5-eds.
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


#include <QObject>
#include <QtTest>
#include <QDebug>

#include <QtOrganizer>

#include "qorganizer-eds-engine.h"
#include "qorganizer-eds-requestdata.h"
#include "eds-base-test.h"


using namespace QtOrganizer;

class CancelOperationTest : public QObject, public EDSBaseTest
{
    Q_OBJECT
private:
    QOrganizerEDSEngine *m_engine;
    QOrganizerCollection m_collection;

private Q_SLOTS:
    void init()
    {
        EDSBaseTest::init();
        QMap<QString, QString> parameters;
        parameters.insert("sleepMode", "true");
        m_engine = QOrganizerEDSEngine::createEDSEngine(parameters);

        QtOrganizer::QOrganizerManager::Error error;
        m_collection = QOrganizerCollection();
        m_collection.setMetaData(QOrganizerCollection::KeyName, uniqueCollectionName());

        bool saveResult = m_engine->saveCollection(&m_collection, &error);
        QVERIFY(saveResult);
        QCOMPARE(error, QtOrganizer::QOrganizerManager::NoError);
    }

    void cleanup()
    {
        QTRY_COMPARE(RequestData::instanceCount(), 0);
        delete m_engine;
        m_engine = 0;
        EDSBaseTest::cleanup();
    }

    void cancelOperationAfterStart()
    {
        QOrganizerEvent event;
        event.setStartDateTime(QDateTime::currentDateTime());
        event.setDisplayLabel("displayLabelValue");
        event.setDescription("descriptionValue");
        event.setCollectionId(m_collection.id());

        QSignalSpy createdItem(m_engine, SIGNAL(itemsAdded(QList<QOrganizerItemId>)));

        // Try cancel a create item operation
        QOrganizerItemSaveRequest req;
        req.setItem(event);
        m_engine->startRequest(&req);
        QCOMPARE(req.state(), QOrganizerAbstractRequest::ActiveState);
        m_engine->cancelRequest(&req);
        QCOMPARE(req.state(), QOrganizerAbstractRequest::CanceledState);
        QTRY_COMPARE(createdItem.count(), 0);

        QTRY_COMPARE(m_engine->runningRequestCount(), 0);
    }

    void deleteManagerBeforeRequestFinish()
    {
        QOrganizerEvent event;
        event.setStartDateTime(QDateTime::currentDateTime());
        event.setDisplayLabel("displayLabelValue");
        event.setDescription("descriptionValue");
        event.setCollectionId(m_collection.id());

        QOrganizerEDSEngine *engine = QOrganizerEDSEngine::createEDSEngine(QMap<QString, QString>());

        // Start a request
        QOrganizerItemSaveRequest req;
        req.setItem(event);
        engine->startRequest(&req);
        QCOMPARE(req.state(), QOrganizerAbstractRequest::ActiveState);

        // delete engine
        delete engine;
    }

    void cancelBeforeStart()
    {
        QOrganizerEvent event;
        event.setStartDateTime(QDateTime::currentDateTime());
        event.setDisplayLabel("displayLabelValue");
        event.setDescription("descriptionValue");
        event.setCollectionId(m_collection.id());

        // Cancel before start
        QOrganizerItemSaveRequest req;
        req.setItem(event);
        m_engine->cancelRequest(&req);

        QTRY_COMPARE(m_engine->runningRequestCount(), 0);
    }

    void destroyRequestAfterStart()
    {
        QOrganizerEvent event;
        event.setStartDateTime(QDateTime::currentDateTime());
        event.setDisplayLabel("displayLabelValue");
        event.setDescription("descriptionValue");
        event.setCollectionId(m_collection.id());

        QOrganizerManager *mgr = new QOrganizerManager("eds");
        QOrganizerItemSaveRequest *req = new QOrganizerItemSaveRequest;
        req->setManager(mgr);
        req->setItem(event);
        req->start();
        req->deleteLater();

        delete mgr;
    }

    void startMultipleRequests()
    {
        QList<QOrganizerCollection> collections = m_engine->collections(0);

        QList<QOrganizerItemSaveRequest*> requests;
        for(int i=0; i < 100; i++) {
            QOrganizerEvent event;
            event.setStartDateTime(QDateTime::currentDateTime());
            event.setDisplayLabel(QString("displayLabelValue_%1").arg(i));
            event.setDescription(QString("descriptionValue_%2").arg(i));
            event.setCollectionId(m_collection.id());

            QOrganizerItemSaveRequest *req = new QOrganizerItemSaveRequest;
            req->setItem(event);
            m_engine->startRequest(req);
            QCOMPARE(req->state(), QOrganizerAbstractRequest::ActiveState);
            requests << req;
        }

        Q_FOREACH(QOrganizerItemSaveRequest *r, requests) {
            m_engine->cancelRequest(r);
            QCOMPARE(r->state(), QOrganizerAbstractRequest::CanceledState);
        }

        QTRY_COMPARE(m_engine->runningRequestCount(), 0);
    }
};

QTEST_MAIN(CancelOperationTest)

#include "cancel-operation-test.moc"
