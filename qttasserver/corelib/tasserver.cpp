/*************************************************************************** 
** 
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies). 
** All rights reserved. 
** Contact: Nokia Corporation (testabilitydriver@nokia.com) 
** 
** This file is part of Testability Driver Qt Agent
** 
** If you have questions regarding the use of this file, please contact 
** Nokia at testabilitydriver@nokia.com . 
** 
** This library is free software; you can redistribute it and/or 
** modify it under the terms of the GNU Lesser General Public 
** License version 2.1 as published by the Free Software Foundation 
** and appearing in the file LICENSE.LGPL included in the packaging 
** of this file. 
** 
****************************************************************************/ 
 
                 
                 
#include <QDebug>
#include <QTcpSocket>

#include <tasqtdatamodel.h>
#include <tascommandparser.h>
#include <taslogger.h>
#include <tasconstants.h>
#include <tascoreutils.h>
#include <version.h>

#if defined(TAS_USELOCALSOCKET)
#include <QLocalSocket>
#endif
#include "tasserverservicemanager.h"

#include "tasserver.h"
#include "closeappservice.h"
#include "startappservice.h"
#include "fixtureservice.h"
#include "killservice.h"
#include "registerservice.h"
#include "shellcommandservice.h"
#include "uistateservice.h"
#include "listappsservice.h"
#include "confservice.h"
#include "uicommandservice.h"
#include "webkitcommandservice.h"
#include "foregroundservice.h"
#include "systeminfoservice.h"
#include "resourceloggingservice.h"

/*!
  \class TasServer
  \brief TasTcpServer acts as the service layer for the tas plugins and pc side test framework     
    
  TasServer uses the TasTcpServer to listen to pc side service requests. These requests 
  are passed to registered tas plugins using a local socket connection.
    
  TasPlugins reqister to this component and responed to the service requests by sending the
  current ui state as response.    

*/



/*!
  Constructs a new TasServer with \a parent.
*/
TasServer::TasServer(QObject *parent)
    : QObject(parent) 
{ 
    TasLogger::logger()->setLogFile("qttasserver.log");
    //TasLogger::logger()->disableLogger();
    TasLogger::logger()->setLevel(DEBUG);

    TasLogger::logger()->debug("Logger created");
    
    mClientManager = TasClientManager::instance();    

    TasLogger::logger()->debug("Creating services...");
    mServiceManager = new TasServerServiceManager(this);
    mServiceManager->registerCommand(new UiStateService());
    mServiceManager->registerCommand(new CloseAppService());
    mServiceManager->registerCommand(new StartAppService());
    mServiceManager->registerCommand(new FixtureService());
    mServiceManager->registerCommand(new ShellCommandService());
    mServiceManager->registerCommand(new KillService());
    mServiceManager->registerCommand(new ListAppsService());
    mServiceManager->registerCommand(new ConfService());
    mServiceManager->registerCommand(new RegisterService());
    mServiceManager->registerCommand(new UiCommandService());
    mServiceManager->registerCommand(new WebkitCommandService());
    mServiceManager->registerCommand(new ForegroundService());
    mServiceManager->registerCommand(new SystemInfoService());
    mServiceManager->registerCommand(new ResourceLoggingService());    

    TasLogger::logger()->debug("Creating TCP server...");
    //ownership of serviceManager is transferred
    mTcpServer = new TasTcpServer(QT_SERVER_PORT_OUT, *mServiceManager,this);

#if defined(TAS_USELOCALSOCKET)            
     mLocalServer = new TasLocalServer(LOCAL_SERVER_NAME, *mServiceManager, this);
#else
     mInternalTcpServer = new TasTcpServer(QT_SERVER_PORT, *mServiceManager, this);

#ifdef Q_OS_SYMBIAN
     //     connect(mClientManager, SIGNAL(allClientsRemoved()), mInternalTcpServer, SLOT(restartServer()));
#endif

#endif
}

TasServer::~TasServer()
{ 
    delete mTcpServer;

#if defined(TAS_USELOCALSOCKET)
     delete mLocalServer;
#else
     delete mInternalTcpServer;
#endif     
    TasClientManager::deleteInstance();
    delete mServiceManager;
}

const QString& TasServer::getServerVersion()
{
    return TAS_VERSION;
}

/*!
  Closes the server and performs cleanup.
*/
void TasServer::closeServer()
{
    mClientManager->removeAllClients();
    TasLogger::logger()->debug("TasServer::closeServer");    
    mTcpServer->close();

#if defined(TAS_USELOCALSOCKET)            
    mLocalServer->close();
#else
    mInternalTcpServer->close();
#endif
    TasLogger::logger()->info("TasServer::closeServer done");   
}

void TasServer::killAllStartedProcesses()
{
    mClientManager->removeAllClients();
    mTcpServer->close();
    startServer();
    
}

/*!
  Starts the server and initializes all other components used for
  communication. 
  Returns true is the starting all components succeeded false otherwise.
  In false situations the server will not function properly. 
*/
bool TasServer::startServer()
{    

    //first start the tcp server for listeting tdriver messages
    if (!mTcpServer->start()){
        return false;
    }
            
#if defined(TAS_USELOCALSOCKET)        
    //second start the local server for listening to plugin message
    if(!mLocalServer->start()){
        mTcpServer->close();
        return false;
    }   
    TasLogger::logger()->info("TasServer::startServer listening " + mLocalServer->fullServerName());   
    QFile handle(mLocalServer->fullServerName());
    if (handle.exists()) {
        // Allow everything on the socket
        if (!handle.setPermissions(QFile::ReadOwner	|
                                   QFile::WriteOwner	|
                                   QFile::ExeOwner	  |
                                   QFile::ReadUser	  |
                                   QFile::WriteUser	|
                                   QFile::ExeUser	  |
                                   QFile::ReadGroup	|
                                   QFile::WriteGroup	|
                                   QFile::ExeGroup	  |
                                   QFile::ReadOther	|
                                   QFile::WriteOther	|
                                   QFile::ExeOther)) {
            TasLogger::logger()->warning("TasServer::startServer failed to set global write for local socket");
        }
    }
    
#else
    //start second tcp server for listeting plugin reqister messages
    if (!mInternalTcpServer->start()){
        mTcpServer->close();
        return false;
    }
#endif
    return true;
}

/*!
 Returns the port that server is listening. 
*/
int TasServer::getServerPort()
{
    return mTcpServer->serverPort();
}

/*!
  Returns the address that server is listening. 
*/
QString TasServer::getServerAddress()
{
    return mTcpServer->serverAddress().toString();
}

const QString& TasServer::getErrorMessage()
{
    return errorMessage;
}

TasTcpServer::TasTcpServer(int port, TasServerServiceManager& serviceManager, QObject * parent)
    :QTcpServer(parent), mServiceManager(serviceManager)
{
    mPort = port;
    mConnectionCount = 0;
}

TasTcpServer::~TasTcpServer()
{
}

void TasTcpServer::incomingConnection ( int socketDescriptor )
{
    TasLogger::logger()->debug("TasTcpServer::incomingConnection");
    mConnectionCount++;
    QTcpSocket* tcpSocket = new QTcpSocket(this);
    if (!tcpSocket->setSocketDescriptor(socketDescriptor)) {
        TasLogger::logger()->error("TasTcpServer::incomingConnection" + tcpSocket->errorString());
        delete tcpSocket;
    }
    else{
        TasLogger::logger()->info("TasTcpServer::incomingConnection number " +QString::number(mConnectionCount) 
                                   + " for server " + QString::number(mPort));
        TasServerSocket *socket = new TasServerSocket(*tcpSocket, this);   
        socket->setIdentification(QString::number(mPort));
        connect(tcpSocket, SIGNAL(disconnected()), socket, SLOT(disconnected()));            
        connect(tcpSocket, SIGNAL(disconnected()), tcpSocket, SLOT(deleteLater()));            
        
        socket->setRequestHandler(&mServiceManager);
        socket->setResponseHandler(&mServiceManager);
    }                              
}

bool TasTcpServer::start()
{
    bool started = false;
    for(int i = 0; i < 5; i++){
        if(listen(QHostAddress::Any, mPort)){
            started = true;
            break;
        }
        TasCoreUtils::wait(500);
    }
    if(!started){
        TasLogger::logger()->error("TasTcpServer::start failed to listen to port: "+QString::number(mPort)+". Reason: " + errorString());
    }
    else{
        TasLogger::logger()->info("TasTcpServer::start listening port: " + QString::number(mPort));
    }
    return started;
}

void TasTcpServer::restartServer()
{
    TasLogger::logger()->debug("TasTcpServer::restartServer");
    close();
    start();
}

void TasTcpServer::socketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    QObject* obj = sender();
    QString errorMsg;
    if(obj){
        QTcpSocket* socket = qobject_cast<QTcpSocket*>(obj);
        if(socket){
            errorMsg = socket->errorString();
        }
    }
    TasLogger::logger()->error("TasTcpServer::socketError " + errorMsg);
}

#if defined(TAS_USELOCALSOCKET)
TasLocalServer::TasLocalServer(const QString& name, TasServerServiceManager& serviceManager, QObject * parent)
    :QLocalServer(parent),mServiceManager(serviceManager)
{
    mName = name;
    mConnectionCount = 0;
}

TasLocalServer::~TasLocalServer()
{
}

bool TasLocalServer::start()
{
    //in unix envs in case of a crash the server will not start if the old is not removed.
    QLocalServer::removeServer(mName);

    bool started = listen(mName);
    if(!started){
        TasLogger::logger()->debug("Opening local server failed! Reason: " + errorString());
        started = false;
    }
    return started;
}

void TasLocalServer::incomingConnection ( quintptr socketDescriptor )
{
    mConnectionCount++;
    QLocalSocket* localSocket = new QLocalSocket(this);
    if (!localSocket->setSocketDescriptor(socketDescriptor)) {
        TasLogger::logger()->error("TasLocalServer::incomingConnection" + localSocket->errorString());
        delete localSocket;
    }
    else{
        TasLogger::logger()->debug("TasLocalServer::incomingConnection number " + QString::number(mConnectionCount));
        TasServerSocket *socket = new TasServerSocket(*localSocket, this);      
        connect(localSocket, SIGNAL(disconnected()), socket, SLOT(disconnected()));            
        connect(localSocket, SIGNAL(disconnected()), localSocket, SLOT(deleteLater()));            

        socket->setRequestHandler(&mServiceManager);
        socket->setResponseHandler(&mServiceManager);
    }
}
#endif