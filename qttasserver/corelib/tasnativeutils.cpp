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
 


#include "tasnativeutils.h"


/*!
    \class TasNativeUtils
    \brief Provides platform dependent operations.    
    
    TasNativeUtils contains generic operations for platform specific calls, such as
    retrieving the pid of the top most (or active window).
*/


/*!
  Returns the pid of the active (top most) window on the screen. returns -1 if the operation
  is not available, or the window can not be found.
*/

int TasNativeUtils::pidOfActiveWindow(const QList<QString>& pids)
{
    return -1;
}

int TasNativeUtils::bringAppToForeground(TasClient& app)
{
    return -1;
}