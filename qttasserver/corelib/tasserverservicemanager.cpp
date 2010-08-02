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
 

                      
#include <QMutableListIterator>

#include "tasserverservicemanager.h"

#include "tascommandparser.h"
#include "tasconstants.h"
#include "taslogger.h"

/*!
  \class TasServerServiceManager
  \brief TasServerServiceManager manages the service commands used by qttasserver.
    
  TasServerServiceManager is the manager for the commands in the service architecture
  used by qttasserver. The service requests are implemented using a relatively
  standard form of the chain of responsibility pattern. TasServerServiceManager class
  takes care of the command execution and management. The command implementations
  only need to concern with the actual command implementation. 
  
  The difference to TasServiceManager is that a waiter is started for the responses
  which originate from the connected plugins.

  Commands which are reqistered to the manager will be invoked on all 
  service requests untill on command consumed the request. This is done
  by returning "true" from the execute method.

*/

/*!
  Construct a new TasServerServiceManager
*/
TasServerServiceManager::TasServerServiceManager(QObject *parent)
    :QObject(parent)
{
    mClientManager = TasClientManager::instance();
}

/*!
  Destructor. Destroys all of the reqistered commands.
*/
TasServerServiceManager::~TasServerServiceManager()
{
    QMutableListIterator<TasServiceCommand*> i(mCommands);
    while (i.hasNext()){
        delete i.next();
    }
    mCommands.clear();
}

/*!
  Servers servicemanager first looks for a client to relay the message to. If none found then use the servers command chain
  to handle the service request.
*/
void TasServerServiceManager::handleServiceRequest(TasCommandModel& commandModel, TasSocket* requester, qint32 responseId)
{
    TasLogger::logger()->debug("TasServerServiceManager::handleServiceRequest: " + commandModel.service());
    TasClient* targetClient = 0;
    if (!commandModel.id().isEmpty() && commandModel.id() != "1"){
        TasClient* crashedApp = mClientManager->findCrashedApplicationById(commandModel.id());
        if (crashedApp){
            TasResponse response(responseId);
            response.setIsError(true);
            response.setErrorMessage("The application " + crashedApp->applicationName() + " with Id " + crashedApp->processId() + " has crashed.");
            requester->sendMessage(response);
            return;
        }
         
        targetClient = mClientManager->findByProcessId(commandModel.id());
        if(!targetClient){
            TasResponse response(responseId);
            response.setIsError(true);
            response.setErrorMessage("The application with Id " + commandModel.id() + " is no longer available.");
            requester->sendMessage(response);
            return;
        }
    }

    if(!targetClient && (commandModel.service() == APPLICATION_STATE || commandModel.service() == SCREEN_SHOT)){
        targetClient = mClientManager->findClient(commandModel);
    }
    else if (commandModel.service() == RESOURCE_LOGGING_SERVICE){
        targetClient = mClientManager->logMemClient();
    }
    else{
    }

    if(targetClient){
        TasLogger::logger()->debug("TasServerServiceManager::handleServiceRequest set waiter " + QString::number(responseId));
        ResponseWaiter* waiter = new ResponseWaiter(responseId, requester);
        if(commandModel.service() == CLOSE_APPLICATION){
            waiter->setResponseFilter(new ClientRemoveFilter(commandModel));
        }
        connect(waiter, SIGNAL(responded(qint32)), this, SLOT(removeWaiter(qint32)));
        reponseQueue.insert(responseId, waiter);
        targetClient->socket()->sendRequest(responseId, commandModel.sourceString());            
    }
    else{
        TasResponse response(responseId);
        response.setRequester(requester);
        performService(commandModel, response);
        //start app waits for register message and performs the response
        if( (commandModel.service() != START_APPLICATION && commandModel.service() != RESOURCE_LOGGING_SERVICE) || response.isError()){
            requester->sendMessage(response);
        }
    }
}

void TasServerServiceManager::serviceResponse(TasMessage& response)
{
    if(reponseQueue.contains(response.messageId())){
        reponseQueue.value(response.messageId())->sendResponse(response);
    }
    else{
        TasLogger::logger()->warning("TasServerServiceManager::serviceResponse unexpected response. Nobody interested!");
    }
}

void TasServerServiceManager::removeWaiter(qint32 responseId)
{
    TasLogger::logger()->debug("TasServerServiceManager::removeWaiter remove " + QString::number(responseId));
    reponseQueue.remove(responseId);
}

ResponseWaiter::ResponseWaiter(qint32 responseId, TasSocket* relayTarget, int timeout)
{
    mFilter = 0;
    mSocket = relayTarget;
    mResponseId = responseId;
    mWaiter.setSingleShot(true);    
    connect(&mWaiter, SIGNAL(timeout()), this, SLOT(timeout()));
    connect(mSocket, SIGNAL(socketClosed()), this, SLOT(socketClosed()));
    mWaiter.start(timeout);
}

ResponseWaiter::~ResponseWaiter()
{
    mWaiter.stop();
    if(mFilter){
        delete mFilter;
    }
}

/*!
  If set the response will be passed on to the filter before sent to the
  requesting socket. Ownership is taken.
*/
void ResponseWaiter::setResponseFilter(ResponseFilter* filter)
{
    mFilter = filter;
}

void ResponseWaiter::sendResponse(TasMessage& response)
{
    TasLogger::logger()->debug("ResponseWaiter::sendResponse");
    mWaiter.stop();
    if(mFilter){
        mFilter->filterResponse(response);
    }
    if(!mSocket->sendMessage(response)){
        TasLogger::logger()->error("ResponseWaiter::sendResponse socket not writable!");
    }
    emit responded(mResponseId);
    deleteLater();
}

void ResponseWaiter::timeout()
{
    TasLogger::logger()->error("ResponseWaiter::timeout for message: " + QString::number(mResponseId));
    TasResponse response(mResponseId);
    response.setErrorMessage("Connection to plugin timedout!");
    if(mFilter){
        mFilter->filterResponse(response);
    }
    if(!mSocket->sendMessage(response)){
        TasLogger::logger()->error("ResponseWaiter::timeout socket not writable!");
    }
    emit responded(mResponseId);
    deleteLater();
}

void ResponseWaiter::socketClosed()
{
    mWaiter.stop();
    TasLogger::logger()->error("ResponseWaiter::socketClosed. Connection to this waiter closed. Cannot respond. " );
    emit responded(mResponseId);
    deleteLater();
}








