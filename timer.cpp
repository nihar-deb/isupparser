#include <cstdlib>
#include <sys/ipc.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/msg.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <mysql/mysql.h>
#include <sys/time.h>
#include <iostream>
#include <string>
#include <errno.h>
#include <cstring>
#include <list>

#include "libjcti.h"
#include "error.h"
#include "signalingServer.h"
#include "mediaServer.h"
//#include "pic_struc.h"
#include "pic_decl.h"
#include "timer.h"


using namespace std;
#ifdef __GNUC__
#include <ext/hash_map>
#else
#include <hash_map>
#endif
namespace std
{
    using namespace __gnu_cxx;
}



std::hash_multimap <int,stTimer> mapTimer;

//std::hash_map <int,callRec> :: iterator callItr ;


typedef std::pair <int, stTimer> timer_Pair;

std::list<stTimer> timerQue;
std::list<stTimer>::iterator timerQueItr;


extern stss7Timer timerSS7;
extern int addTimerResp(stTimer t);
criticalSection criticalAddTimer;
criticalSection criticalTimer;
criticalSection criticalExpiryTimer;
criticalSection criticalRemoveLock;
criticalSection criticalInsertLock;
criticalSection mapTimerLock;


int processCancelTimer();
int processAddTimer();

int removeElement(std::hash_multimap <int,stTimer> :: iterator mapTimerItr)
{
std::hash_multimap <int,stTimer> :: iterator mapTimerItrTmp;
    mapTimerLock.Lock();
    for(mapTimerItrTmp = mapTimer.begin();mapTimerItrTmp != mapTimer.end(); mapTimerItrTmp++)
    {
        if(mapTimerItrTmp->second.source == mapTimerItr->second.source && mapTimerItrTmp->second.id == mapTimerItr->second.id 
                && mapTimerItrTmp->second.type == mapTimerItr->second.type)
        {
                 mapTimer.erase(mapTimerItr);
                 break;
        }
    }
    
    usleep(1000);
    mapTimerLock.Unlock();
}

int insertElement(stTimer t)
{
    mapTimerLock.Lock();
    mapTimer.insert(timer_Pair(t.id,t));
    mapTimerLock.Unlock();
}

int processTimerExpiry(std::hash_multimap <int,stTimer> :: iterator mapTimerItr )
{
jctiLog::Log timerLogger;
    criticalExpiryTimer.Lock();
    //mapTimerItr = mapTimer.find(id);
    if(mapTimerItr != mapTimer.end())
    {
        switch(mapTimerItr->second.source)
        {
            case SS7_SIG:
            case SS7_MED:
                sprintf(timerLogger.applMsgTxt,"Timer Expired.Source %d CIC %d Type %d",mapTimerItr->second.source,mapTimerItr->second.id,mapTimerItr->second.type);
                logDebug(_LFF,timerLogger,APPL_LOG,LOG_LEVEL_2);
                addTimerResp(mapTimerItr->second);
               break;
            default:
                sprintf(timerLogger.applMsgTxt,"Unknown timer source %d CIC %d",mapTimerItr->second.source,mapTimerItr->second.id);
                logWarning(_LFF,timerLogger,APPL_LOG,LOG_LEVEL_1);
                if(mapTimer.find(mapTimerItr->second.id) != mapTimer.end())
                        removeElement(mapTimerItr);
                //mapTimer.erase(mapTimerItr);
                criticalExpiryTimer.Unlock();
                return INVALID_SOURCE;
        }
             
        //mapTimer.erase(mapTimerItr);
        removeElement(mapTimerItr);
       
    }
    else
    {
        criticalExpiryTimer.Unlock();
        return EINVAL;
    }
    
    criticalExpiryTimer.Unlock();
    return SUCCESS;
}

void * timer(void *)
{
  jctiLog::Log timerLogger;  
    time_t timerCur;
    int i = 1;
    std::hash_multimap <int,stTimer> :: iterator mapTimerItr ;
   // time(&timerQue);
    while(TRUE)
    {
        /*time(&timerCur);
        if(difftime(timerCur,timerQue) > 10)
        {
            memset(timerLogger.applMsgTxt,0,sizeof(timerLogger.applMsgTxt));
            sprintf(timerLogger.applMsgTxt,"Timers: ");
            for(mapTimerItr = mapTimer.begin();mapTimerItr != mapTimer.end(); mapTimerItr++)
            {
                i++;
               sprintf(&timerLogger.applMsgTxt[strlen(timerLogger.applMsgTxt)],"%d,%d;",mapTimerItr->second.id,mapTimerItr->second.type); 
               if(i%200 == 0)
               {
                   i = 1;
                   logDebug(_LFF,timerLogger,APPL_LOG,LOG_LEVEL_2);
                   memset(timerLogger.applMsgTxt,0,sizeof(timerLogger.applMsgTxt));
                   sprintf(timerLogger.applMsgTxt,"Timers: ");
               }
            }

            
            time(&timerQue);
        }*/
        timerQueItr = timerQue.begin();
        for(; timerQueItr != timerQue.end();)
        {
            if(timerQueItr->addRemove == TIMER_REMOVE)
                processCancelTimer();
            else if(timerQueItr->addRemove == TIMER_ADD)
                processAddTimer();
            timerQue.pop_front();
            timerQueItr = timerQue.begin();
            i++;
            if(i == 500)
                break;
        }
        i =1;

       for(mapTimerItr = mapTimer.begin();mapTimerItr != mapTimer.end(); ++mapTimerItr)
       {
           time(&timerCur);
           
                     
           if(mapTimerItr == mapTimer.end())
               break;
           switch(mapTimerItr->second.type)
           {
               case SS7_T1_TIMER:
                    if(difftime(timerCur,mapTimerItr->second.callstartTime) > timerSS7.t1.timeout )
                    {
                        processTimerExpiry(mapTimerItr);
                        mapTimerItr = mapTimer.begin();
                        
                    }
                    break;
                case SS7_T5_TIMER:
                    if(difftime(timerCur,mapTimerItr->second.callstartTime) > timerSS7.t5.timeout )
                    {
                        processTimerExpiry(mapTimerItr);
                        mapTimerItr = mapTimer.begin();
                        
                    }
                    break;
               case SS7_T7_TIMER:
                    if(difftime(timerCur,mapTimerItr->second.callstartTime) > timerSS7.t7.timeout )
                    {
                        processTimerExpiry(mapTimerItr);
                        mapTimerItr = mapTimer.begin();
                    }
                    break;
               case SS7_T9_TIMER:
                    if(difftime(timerCur,mapTimerItr->second.callstartTime) > timerSS7.t9.timeout )
                    {
                        processTimerExpiry(mapTimerItr);
                        mapTimerItr = mapTimer.begin();
                    }
                    break;
               case SS7_T16_TIMER:
                   //printf("Found T16 timer\n");
                    if(difftime(timerCur,mapTimerItr->second.callstartTime) > timerSS7.t16.timeout )
                    {
                        printf("Timer Expired\n");
                        processTimerExpiry(mapTimerItr);
                        mapTimerItr = mapTimer.begin();
                    }
                    break;
               case SS7_T17_TIMER:
                    if(difftime(timerCur,mapTimerItr->second.callstartTime) > timerSS7.t17.timeout )
                    {
                        processTimerExpiry(mapTimerItr);
                        mapTimerItr = mapTimer.begin();
                        
                    }
                    break;
               default:
                    if(difftime(timerCur,mapTimerItr->second.callstartTime) > DEFAULT_TIMEOUT )
                    {
                        processTimerExpiry(mapTimerItr);
                        mapTimerItr = mapTimer.begin();
                    }
                    break;

           }
           if(mapTimerItr == mapTimer.end())
               break;
       }
     
       
    }
}


int processAddTimer()
{
    std::hash_multimap <int,stTimer> :: iterator mapTimerItr ;
    jctiLog::Log timerLogger;
    criticalTimer.Lock();
    switch(timerQueItr->source)
    {
        case SS7_SIG:
            if(timerQueItr->id < 1 && timerQueItr->id > MAX_CHANNELS )
            {
                criticalTimer.Unlock();
                return INVALID_ID;
            }
            /*for(mapTimerItr = mapTimer.begin();mapTimerItr != mapTimer.end(); mapTimerItr++)
            {
                if(mapTimerItr->second.source == SS7_SIG && mapTimerItr->second.id == t.id)
                {
                    sprintf(timerLogger.applMsgTxt,"SS7 timer id %d already exists",mapTimerItr->second.id);
                    logWarning(_LFF,timerLogger,APPL_LOG,LOG_LEVEL_1);
                    criticalAddTimer.Unlock();
                    return INVALID_ID;
                }
            }*/
           
            break;
        case SS7_MED:
            break;
        default:
            sprintf(timerLogger.applMsgTxt,"Unknown timer source %d ID %d",timerQueItr->source,timerQueItr->id);
            logWarning(_LFF,timerLogger,APPL_LOG,LOG_LEVEL_1);
            criticalTimer.Unlock();
            return INVALID_SOURCE;
    }
   /* mapTimerItr = mapTimer.find(t.id);
    if(mapTimerItr == mapTimer.end())
    {*/
        time(&timerQueItr->callstartTime);
        //mapTimer.insert(timer_Pair(t.id,t));
        insertElement(*timerQueItr);
       // printf("Timer Added\n");
        sprintf(timerLogger.applMsgTxt,"Timer Added.Source %d CIC %d Type %d",timerQueItr->source,timerQueItr->id,timerQueItr->type);
        logDebug(_LFF,timerLogger,APPL_LOG,LOG_LEVEL_2);
   // }
    criticalTimer.Unlock();
    return SUCCESS;
}
int cancelTimer(stTimer t)
{
    criticalTimer.Lock();
    t.addRemove = TIMER_REMOVE;
    timerQue.push_back(t);
    criticalTimer.Unlock();
}

int addTimer(stTimer t)
{
    criticalTimer.Lock();
    t.addRemove = TIMER_ADD;
    timerQue.push_back(t);
    criticalTimer.Unlock();
}

int processCancelTimer()
{
    std::hash_multimap <int,stTimer> :: iterator mapTimerItr ;
    jctiLog::Log timerLogger;
    unsigned char found = 0;
    criticalTimer.Lock();
   
    switch(timerQueItr->source)
    {
        case SS7_SIG:
        case SS7_MED:
            break;
        default:
            sprintf(timerLogger.applMsgTxt,"Unknown timer source %d ID %d",timerQueItr->source,timerQueItr->id);
            logWarning(_LFF,timerLogger,APPL_LOG,LOG_LEVEL_1);
            criticalTimer.Unlock();
            return INVALID_SOURCE;
    }
    
    for(mapTimerItr = mapTimer.begin();mapTimerItr != mapTimer.end(); ++mapTimerItr)
    {
        if(timerQueItr->type == TIMER_NONE)
        {
            if(mapTimerItr->second.source == timerQueItr->source && mapTimerItr->second.id == timerQueItr->id)
            {
                //mapTimer.erase(mapTimerItr);
                removeElement(mapTimerItr);
                sprintf(timerLogger.applMsgTxt,"Timer Removed.Source %d CIC %d Type %d",timerQueItr->source,timerQueItr->id,mapTimerItr->second.type);
                logDebug(_LFF,timerLogger,APPL_LOG,LOG_LEVEL_2);
                mapTimerItr = mapTimer.begin();
                found = 1;
             //   break;

            }
        }
        else if(mapTimerItr->second.source == timerQueItr->source && mapTimerItr->second.id == timerQueItr->id 
                && mapTimerItr->second.type == timerQueItr->type)
        {
           // mapTimer.erase(mapTimerItr);
            removeElement(mapTimerItr);
            sprintf(timerLogger.applMsgTxt,"Timer Removed.Source %d CIC %d Type %d",timerQueItr->source,timerQueItr->id,timerQueItr->type);
            logDebug(_LFF,timerLogger,APPL_LOG,LOG_LEVEL_2);
            mapTimerItr = mapTimer.begin();
            found = 1;
           // break;
            
        }
        if(mapTimerItr == mapTimer.end())
               break;
    }
    if(found)
    {
        criticalTimer.Unlock();
        return SUCCESS;
    }
    else
    {
        criticalTimer.Unlock();
        return INVALID_ID;
    }
    
}



int starTimerProcess()
{
    int ret;
    pthread_t hndlTimer;
    jctiLog::Log timerLogger;
    ret = pthread_create(&hndlTimer,NULL,&timer,NULL);
    if(ret == -1)
    {
        sprintf(timerLogger.applMsgTxt,"Failed to create Sanity Thread. Error no : %d desc: %s" ,errno,getErrDesc(errno));
        logInfo(_LFF,timerLogger,APPL_LOG,LOG_LEVEL_0);
        return errno;
    }
    else
        return SUCCESS;
}