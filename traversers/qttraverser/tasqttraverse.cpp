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
 


#include <QtPlugin>
#include <QDebug>

#include <testabilityutils.h>
#include <taslogger.h>
#include "tasqttraverse.h"


Q_EXPORT_PLUGIN2(qttraverse, TasQtTraverse)

/*!
    \class TasQtTraverse
    \brief TasQtTraverse traverse QWidgets and QGraphicsItems
        
    Traverse the basic qt ui elements
*/

/*!
    Constructor
*/
TasQtTraverse::TasQtTraverse(QObject* parent)
    :QObject(parent)
{}

/*!
    Destructor
*/
TasQtTraverse::~TasQtTraverse()
{}

/*!
  Traverse QGraphicsItem based objects.
 */
void TasQtTraverse::traverseGraphicsItem(TasObject* objectInfo, QGraphicsItem* graphicsItem, TasCommand* command)
{
    bool embeddedApp = false;
    if (command && command->parameter("embedded") == "true") {
        embeddedApp = true;
    }

    objectInfo->addAttribute("objectType", embeddedApp? TYPE_WEB : TYPE_GRAPHICS_VIEW);                
    objectInfo->setId(TestabilityUtils::graphicsItemId(graphicsItem));
    addGraphicsItemCoordinates(objectInfo, graphicsItem, command);    
    printGraphicsItemProperties(objectInfo, graphicsItem);
}

/*!
  Traverse QObject based items. 
*/
void TasQtTraverse::traverseObject(TasObject* objectInfo, QObject* object, TasCommand* command)
{
    // Embedded apps must use coordinates for operations, as the parent has no knowledge of the 
    // Actual items
    bool embeddedApp = false;
    if (command && command->parameter("embedded") == "true") {
        embeddedApp = true;
    }



    //TasLogger::logger()->debug("TasQtTraverse::traverseObject in");
    addObjectDetails(objectInfo, object);
    QGraphicsWidget* graphicsWidget = qobject_cast<QGraphicsWidget*>(object);               
    if(graphicsWidget){
        objectInfo->addAttribute("objectType", embeddedApp? TYPE_WEB : TYPE_GRAPHICS_VIEW);
        printGraphicsWidgetAction(objectInfo, graphicsWidget);
        addGraphicsItemCoordinates(objectInfo, graphicsWidget, command);
        addFont(objectInfo, graphicsWidget->font());
        // Elided format "this is a text" -> "this is a..." text for
        // items that have the "text" property.
        QVariant text = graphicsWidget->property("text");
        if (text.isValid()) {
            addTextInfo(objectInfo, text.toString(), graphicsWidget->font(), graphicsWidget->size().width());
            
        }
        QVariant plainText = graphicsWidget->property("plainText");
        if (plainText.isValid()) {
            addTextInfo(objectInfo, plainText.toString(), graphicsWidget->font(), graphicsWidget->size().width());
            
        }
    }    
    else{
        //make sure that we are dealing with a widget
        if (object->isWidgetType()){      
            QWidget* widget = qobject_cast<QWidget*>(object);            

            // Widgets inside proxies are forced into Web type 
            // Operations are transformed into Web types
            QGraphicsProxyWidget* proxy = TestabilityUtils::parentProxy(widget);
            if (proxy) {
                objectInfo->addAttribute("objectType", TYPE_WEB);
            }
            else if(object != qApp){
                objectInfo->addAttribute("objectType", TYPE_STANDARD_VIEW );        
            }
            printWidgetAction(objectInfo, widget);
            addWidgetCoordinates(objectInfo, widget,command);
            addFont(objectInfo, widget->font());
            //check is the widget a viewport to graphicsscene
            QWidget* parentWidget = widget->parentWidget();
            bool isViewPort = false;
            if(parentWidget && parentWidget->inherits("QGraphicsView")){
                QGraphicsView* view = qobject_cast<QGraphicsView*>(parentWidget);
                if(view->viewport() == widget){
                    isViewPort = true;
                }
            }
            //add transformation details
            QGraphicsView* graphicsView = qobject_cast<QGraphicsView*>(object);
            if(graphicsView){
                objectInfo->addBooleanAttribute("isTransformed", graphicsView->isTransformed());
                if(graphicsView->isTransformed()){
                    QTransform tr = graphicsView->transform();
                    objectInfo->addAttribute("transformM11",tr.m11());
                    objectInfo->addAttribute("transformM12",tr.m12());
                    objectInfo->addAttribute("transformM13",tr.m13());
                    objectInfo->addAttribute("transformM21",tr.m21());
                    objectInfo->addAttribute("transformM22",tr.m22());
                    objectInfo->addAttribute("transformM23",tr.m23());
                    objectInfo->addAttribute("transformM31",tr.m31());
                    objectInfo->addAttribute("transformM32",tr.m32());
                    objectInfo->addAttribute("transformM33",tr.m33());
                }
            }
            objectInfo->addBooleanAttribute("isViewPort", isViewPort);                    
        } else {
            if(object != qApp){
                objectInfo->addAttribute("objectType", embeddedApp? TYPE_WEB : TYPE_STANDARD_VIEW );        
            }
        }
        
    }    
}


void TasQtTraverse::addWidgetCoordinates(TasObject* objectInfo, QWidget* widget, TasCommand* command)
{
    objectInfo->addBooleanAttribute("isWindow", widget->isWindow());
    
    QGraphicsProxyWidget* proxy = TestabilityUtils::parentProxy(widget);
    if (proxy) {
        //TasLogger::logger()->debug("TasQtTraverse::addWidgetCoordinates got proxy");       
        //print window coordinates

        
        QRectF sceneRect = proxy->sceneBoundingRect();
        QGraphicsView* view = TestabilityUtils::getViewForItem(proxy);
        if(!view->viewportTransform().isIdentity()){
            QTransform transform = view->viewportTransform();
            sceneRect = transform.mapRect(sceneRect);
        }
        QPoint point = sceneRect.topLeft().toPoint();
        

        
        //Print screen coordinates
        QPoint windowPoint = widget->mapTo(widget->window(),QPoint(0, 0));
        windowPoint += point; 
        objectInfo->addAttribute("x", windowPoint.x());
        objectInfo->addAttribute("y", windowPoint.y());

        objectInfo->addAttribute("x_absolute", windowPoint.x());
        objectInfo->addAttribute("y_absolute", windowPoint.y());           
    } 
    else {
         //print window coordinates
        QPoint windowPoint = widget->mapTo(widget->window(),QPoint(0, 0));
        objectInfo->addAttribute("x", windowPoint.x());
        objectInfo->addAttribute("y", windowPoint.y());
            
        
        if (command && command->parameter("x_parent_absolute") != "" &&
            command->parameter("y_parent_absolute") != "") {
            //TasLogger::logger()->debug("TasQtTraverse::addWidgetCoordinates moving point");
            QPoint p(command->parameter("x_parent_absolute").toInt(), 
                     command->parameter("y_parent_absolute").toInt());
            QPoint transP = widget->mapToGlobal(p);
            
            objectInfo->addAttribute("x_absolute", transP.x());
            objectInfo->addAttribute("y_absolute", transP.y());
            
        } else {
            //TasLogger::logger()->debug("TasQtTraverse::addWidgetCoordinates using regular coords");
            QPoint screenPoint = widget->mapToGlobal(QPoint(0, 0));
            objectInfo->addAttribute("x_absolute", screenPoint.x());
            objectInfo->addAttribute("y_absolute", screenPoint.y());
        }
        
    }
    // Explicitly add width and height (property added changes name)
    objectInfo->addAttribute("width", widget->width());
    objectInfo->addAttribute("height", widget->height());
}
    

/*!
  
  Prints all of the actions that a widget has under the widget. 
  Makes it possible to easily map the correct action to the 
  correct widget and also command the correct widget.
  Returns true if an action was added.  
  
 */
void TasQtTraverse::printWidgetAction(TasObject* parentObject, QWidget* widget)
{ 
    QList<QAction*> actions = widget->actions();
    if(actions.size() > 0){                   
        for(int i = 0 ; i < actions.size(); i++){
            QObject* action = actions.at(i);              
            traverseObject(&parentObject->addObject(), action);
         }
    }     
}

void TasQtTraverse::printGraphicsWidgetAction(TasObject* parentObject, QGraphicsWidget* widget)
{
    QList<QAction*> actions = widget->actions();
    if(actions.size() > 0){                   
        for(int i = 0 ; i < actions.size(); i++){
            QObject* action = actions.at(i);              
            traverseObject(&parentObject->addObject(), action);
         }
    }     
}

/*! 
    Print graphicsitem details that could be usable to the model.
*/
void TasQtTraverse::printGraphicsItemProperties(TasObject* objectInfo, QGraphicsItem* graphicsItem)
{       
    if(includeAttribute("visible")){
        objectInfo->addBooleanAttribute("visible", graphicsItem->isVisible());
    }
    if(includeAttribute("enabled")){
        objectInfo->addBooleanAttribute("enabled", graphicsItem->isEnabled());
    }
    if(includeAttribute("selected")){
        objectInfo->addBooleanAttribute("selected", graphicsItem->isSelected());
    }
    if(includeAttribute("obscured")){
        objectInfo->addBooleanAttribute("obscured", graphicsItem->isObscured());
    }
    if(includeAttribute("focus")){
        objectInfo->addBooleanAttribute("focus", graphicsItem->hasFocus());
    }
    if(includeAttribute("under-mouse")){
        objectInfo->addBooleanAttribute("under-mouse", graphicsItem->isUnderMouse());
    }
    if(includeAttribute("droppable")){
        objectInfo->addBooleanAttribute("droppable", graphicsItem->acceptDrops());
    }
    if(includeAttribute("hoverable")){
        objectInfo->addBooleanAttribute("hoverable", graphicsItem->acceptHoverEvents());    
    }
    if(includeAttribute("tooltip")){
        objectInfo->addAttribute("tooltip", graphicsItem->toolTip());   
    }
    if(includeAttribute("z-value")){
        objectInfo->addAttribute("z-value", QString::number(graphicsItem->zValue()));   
    }
}



