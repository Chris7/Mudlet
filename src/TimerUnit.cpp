/***************************************************************************
 *   Copyright (C) 2008-2013 by Heiko Koehn - KoehnHeiko@googlemail.com    *
 *   Copyright (C) 2014 by Ahmed Charles - acharles@outlook.com            *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/


#include "TimerUnit.h"


#include "Host.h"
#include "TLuaInterpreter.h"
#include "TTimer.h"


using namespace std;

void TimerUnit::_uninstall( TTimer * pChild, QString packageName )
{
    list<TTimer*> * childrenList = pChild->mpMyChildrenList;
    for(auto it2 = childrenList->begin(); it2 != childrenList->end(); it2++)
    {
        TTimer * pT = *it2;
        _uninstall( pT, packageName );
        uninstallList.append( pT );
    }
}


void TimerUnit::uninstall( QString packageName )
{
    for(auto it = mTimerRootNodeList.begin(); it != mTimerRootNodeList.end(); it ++ )
    {
        TTimer * pT = *it;

        if( pT->mPackageName == packageName )
        {
            _uninstall( pT, packageName );
            uninstallList.append( pT );
        }
    }
    for( int i=0; i<uninstallList.size(); i++ )
    {
        unregisterTimer(uninstallList[i]);
    }
     uninstallList.clear();
}

void TimerUnit::stopAllTriggers()
{
    for(auto it = mTimerRootNodeList.begin(); it != mTimerRootNodeList.end(); it++)
    {
        TTimer * pChild = *it;
        pChild->disableTimer( pChild->getID() );
    }
}

void TimerUnit::compileAll()
{
    for(auto it = mTimerRootNodeList.begin(); it != mTimerRootNodeList.end(); it++)
    {
        TTimer * pChild = *it;
        if( pChild->isActive() )
        {
            pChild->mNeedsToBeCompiled = true;
        }
    }
}

void TimerUnit::reenableAllTriggers()
{
    for(auto it = mTimerRootNodeList.begin(); it != mTimerRootNodeList.end(); it++)
    {
        TTimer * pChild = *it;
        pChild->enableTimer( pChild->getID() );
    }
}


void TimerUnit::addTimerRootNode( TTimer * pT, int parentPosition, int childPosition )
{
    if( ! pT ) return;
    if( ! pT->getID() )
    {
        pT->setID( getNewID() );
    }

    if( ( parentPosition == -1 ) || ( childPosition >= static_cast<int>(mTimerRootNodeList.size()) ) )
    {
        mTimerRootNodeList.push_back( pT );
    }
    else
    {
        // insert item at proper position
        int cnt = 0;
        for(auto it = mTimerRootNodeList.begin(); it != mTimerRootNodeList.end(); it ++ )
        {
            if( cnt >= childPosition )
            {
                mTimerRootNodeList.insert( it, pT );
                break;
            }
            cnt++;
        }
    }

    mTimerMap.insert( pT->getID(), pT );
    // kein lookup table eintrag siehe addTimer()
}

void TimerUnit::reParentTimer( int childID, int oldParentID, int newParentID, int parentPosition, int childPosition )
{
    TTimer * pOldParent = getTimerPrivate( oldParentID );
    TTimer * pNewParent = getTimerPrivate( newParentID );
    TTimer * pChild = getTimerPrivate( childID );
    if( ! pChild )
    {
        return;
    }

    pChild->disableTimer( childID );

    if( pOldParent )
    {
        pOldParent->popChild( pChild );
    }
    if( ! pOldParent )
    {
        mTimerRootNodeList.remove( pChild );
    }
    if( pNewParent )
    {
        pNewParent->addChild( pChild, parentPosition, childPosition );
        if( pChild ) pChild->setParent( pNewParent );
    }
    else
    {
        pChild->Tree<TTimer>::setParent( 0 );
        addTimerRootNode( pChild, parentPosition, childPosition );
    }

    pChild->enableTimer( childID );
}

void TimerUnit::removeAllTempTimers()
{
    mCleanupList.clear();
    for(auto it = mTimerRootNodeList.begin(); it != mTimerRootNodeList.end(); it++)
    {
        TTimer * pChild = *it;
        if( pChild->isTempTimer() )
        {
            pChild->killTimer();
            pChild->mOK_code = false; //important to not crash on stale Lua function args
            markCleanup( pChild );
        }
    }
}

void TimerUnit::_removeTimerRootNode( TTimer * pT )
{
    if( ! pT ) return;
    // temp timers do not need to check for names referring to multiple different
    // objects as names=ID -> much faster tempTimer creation
    if( ! pT->mIsTempTimer )
    {
        mLookupTable.remove( pT->mName, pT );
    }
    else
        mLookupTable.remove( pT->getName() );

    mTimerMap.remove( pT->getID() );
    mTimerRootNodeList.remove( pT );
}

TTimer * TimerUnit::getTimer( int id )
{
    if( mTimerMap.find( id ) != mTimerMap.end() )
    {
        return mTimerMap.value( id );
    }
    else
    {
        return 0;
    }
}

TTimer * TimerUnit::getTimerPrivate( int id )
{
    if( mTimerMap.find( id ) != mTimerMap.end() )
    {
        return mTimerMap.value( id );
    }
    else
    {
        return 0;
    }
}

bool TimerUnit::registerTimer( TTimer * pT )
{
    if( ! pT ) return false;

    if( pT->getParent() )
    {
        addTimer( pT );
        pT->setIsActive( false );
        return true;
    }
    else
    {
        addTimerRootNode( pT );
        pT->setIsActive( false );
        return true;
    }
}

void TimerUnit::unregisterTimer( TTimer * pT )
{
    if( ! pT ) return;
    pT->deactivate();
    pT->stop();
    if( pT->getParent() )
    {
        _removeTimer( pT );
        return;
    }
    else
    {
        _removeTimerRootNode( pT );
        return;
    }
}



void TimerUnit::addTimer( TTimer * pT )
{
    if( ! pT ) return;

    if( ! pT->getID() )
    {
        pT->setID( getNewID() );
    }

    mTimerMap.insert( pT->getID(), pT );
    // in den lookup table wird der timer erst dann eingetragen, wenn er auch einen namen hat -> setName()
}

void TimerUnit::_removeTimer( TTimer * pT )
{
    if( ! pT ) return;

    // temp timers do not need to check for names referring to multiple different
    // objects as names=ID -> much faster tempTimer creation
    if( ! pT->mIsTempTimer )
    {
        mLookupTable.remove( pT->mName, pT );
    }
    else
        mLookupTable.remove( pT->getName() );
    mTimerMap.remove( pT->getID() );
}


bool TimerUnit::enableTimer(const QString & name )
{
    bool found = false;
    QMap<QString, TTimer *>::const_iterator it = mLookupTable.find( name );
    while( it != mLookupTable.end() && it.key() == name )
    {
        TTimer * pT = it.value();

        if( ! pT->isOffsetTimer() )
           /* ret = */ pT->setIsActive( true );
        else
            pT->setShouldBeActive( true );


        if( pT->isFolder() )
        {
            // disable or enable all timers in the respective branch
            // irrespective of the user defined state.
            if( pT->shouldBeActive() )
            {
                pT->enableTimer();
            }
            else
            {
                pT->disableTimer();
            }
        }
        else
        {
            if( pT->isOffsetTimer() )
            {
                // state of offset timers is managed by the trigger engine
                if( pT->shouldBeActive() )
                {
                    pT->enableTimer();
                }
                else
                {
                    pT->disableTimer();
                }
            }
        }

        ++it;
        found = true;
    }
    return found;
}

bool TimerUnit::disableTimer(const QString & name )
{
    bool found = false;
    QMap<QString, TTimer *>::const_iterator it = mLookupTable.find( name );
    while( it != mLookupTable.end() && it.key() == name )
    {
        TTimer * pT = it.value();
        if( pT->isOffsetTimer() )
            pT->setShouldBeActive( false );
        else
            pT->setIsActive( false );

        pT->disableTimer();
        ++it;
        found = true;
    }
    return found;
}

TTimer * TimerUnit::findTimer(const QString & name )
{
    QMap<QString, TTimer *>::const_iterator it = mLookupTable.find( name );
    while( it != mLookupTable.end() && it.key() == name )
    {
        TTimer * pT = it.value();
        return pT;
    }
    return 0;
}


bool TimerUnit::killTimer(const QString & name )
{
    for(auto it = mTimerRootNodeList.begin(); it != mTimerRootNodeList.end(); it++)
    {
        TTimer * pChild = *it;
        if( pChild->getName() == name )
        {
            // only temporary timers can be killed
            if( ! pChild->isTempTimer() ) return false;
            pChild->killTimer();
            markCleanup( pChild );
            return true;
        }
    }
    return false;
}

int TimerUnit::getNewID()
{
    return ++mMaxID;
}

void TimerUnit::doCleanup()
{
    for(auto it = mCleanupList.begin(); it != mCleanupList.end(); it++)
    {
        delete (*it);
    }
    mCleanupList.clear();
}

void TimerUnit::markCleanup( TTimer * pT )
{
    for(auto it = mCleanupList.begin(); it != mCleanupList.end(); it++)
    {
        if( *it == pT )
        {
            return;
        }
    }

    mCleanupList.push_back( pT );
}

void TimerUnit::_assembleReport( TTimer * pChild )
{
    list<TTimer*> * childrenList = pChild->mpMyChildrenList;
    for(auto it2 = childrenList->begin(); it2 != childrenList->end(); it2++)
    {
        TTimer * pT = *it2;
        _assembleReport( pT );
        if( pT->isActive() ) statsActiveTriggers++;
        if( pT->isTempTimer() ) statsTempTriggers++;
        statsTriggerTotal++;
    }
}

QString TimerUnit::assembleReport()
{
    statsActiveTriggers = 0;
    statsTriggerTotal = 0;
    statsTempTriggers = 0;
    for(auto it = mTimerRootNodeList.begin(); it != mTimerRootNodeList.end(); it++)
    {
        TTimer * pChild = *it;
        if( pChild->isActive() ) statsActiveTriggers++;
        if( pChild->isTempTimer() ) statsTempTriggers++;
        statsTriggerTotal++;
        list<TTimer*> * childrenList = pChild->mpMyChildrenList;
        for(auto it2 = childrenList->begin(); it2 != childrenList->end(); it2++)
        {
            TTimer * pT = *it2;
            _assembleReport( pT );
            if( pT->isActive() ) statsActiveTriggers++;
            if( pT->isTempTimer() ) statsTempTriggers++;
            statsTriggerTotal++;
        }
    }
    QStringList msg;
    msg << "timers current total: " << QString::number(statsTriggerTotal) << "\n"
        << "tempTimers current total: " << QString::number(statsTempTriggers) << "\n"
        << "active timers: " << QString::number(statsActiveTriggers) << "\n";

    return msg.join("");
}
