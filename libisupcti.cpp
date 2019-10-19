/**************************************************************************************************************
* Copyright (C) 2013 nihar.deb@gmail.com
* FileName : libisup.c
* Purpose: It is a shared library file contains all Juno CTI API. 
*
* History: 18/01/2013 Started by Nihar
*
*******************************************************************************************************************/

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

#include "libisup.h"
#include "error.h"
#include "signalingServer.h"
#include "mediaServer.h"
//#include "pic_struc.h"
#include "pic_decl.h"
#include "timer.h"
#include "common.h"


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


/*Defines*/
#define MAX_MSG_PER_SEC        400
#define MAX_RESERVED_MSG        50

/*End Defines*/

 /*Structures*/
 
typedef struct _callRec
{
    unsigned short portNo;
    int trunkNo;
    char DTMF_digit;
    int signalingState;
    int mediaSate;
    unsigned char portStatus;
    char callId[MAX_CALLID_LEN];
    char cgpn[MAX_NUMBER_LEN];
    char cdpn[MAX_NUMBER_LEN];
    unsigned char subField;
    unsigned short timerRetry;
}callRec;


typedef struct _cicInformation
{
    int channelNo;
    int channelStatus;
    unsigned short origcic;
    unsigned short linkset[MAX_SS7_LINK];
    unsigned short prevLink;
}cicInformation;

//std::list <cicInformation> cicInfo; 
 
 /*End Structures*/

/*Global variables*/
int gCountTraffic[MAX_SS7_LINK] ;
int cti_qid;
//int mediaslave_qid;

int udp_sock = 0;

int licensed = FALSE;
isupLog::Log msgLogger;
int g_maxUsedChan = 32;

pthread_t loggerThdHndl = 0;

std::hash_map <int,callRec> call;

//std::hash_map <int,callRec> :: iterator callItr ;
//std::hash_map <int,callRec> :: iterator callItrOut ;

typedef std::pair <int, callRec> Int_Pair;
std::hash_map <int,cicInformation> cicInfo;
typedef std::pair <int, cicInformation> cicPair;

std::list<stTimer> respTimerQue;
std::list<stTimer>::iterator respTimerQueItr;




int linkStatus[MAX_SS7_LINK];
pthread_mutex_t lockthread;
isupConfig conf;
unsigned short gcic;
unsigned short gLink;

stss7Timer timerSS7;
time_t timerTraffic;
time_t timerFreeCic;
criticalSection criticalSec;
criticalSection criticalHashICLock;
criticalSection criticalEventLock;
criticalSection criticalIAMLock;
criticalSection criticalProcessIAMLock;
criticalSection criticalACMLock;
criticalSection criticalANMLock;
criticalSection criticalRELLock;
criticalSection criticalRLCLock;
criticalSection criticalDTMFLock;
criticalSection criticalDTMFCollectLock;
criticalSection criticalProcessRLCLock;
criticalSection criticalFreeCICLock;
criticalSection ResetLock;
criticalSection criticalResetLock;
criticalSection criticalGrpResetLock;
criticalSection criticalGrpBlockLock;
criticalSection criticalGrpUnblockLock;
criticalSection criticalBlockLock;
criticalSection criticalUnblockLock;
criticalSection criticalAddTimerLock;
criticalSection criticalRemoveTimerLock;
criticalSection trunkStatusLock;
criticalSection cicStatusLock;
criticalSection initIAMLock;
criticalSection initACMLock;
criticalSection initANMLock;
criticalSection initRELLock;
criticalSection initRLCLock;
criticalSection parseIAMLock;
criticalSection parseACMLock;
criticalSection parseANMLock;
criticalSection parseRELLock;
criticalSection parseRLCLock;


criticalSection msgQueLock;
criticalSection insertLock;
criticalSection removeLock;
criticalSection mapLock;
criticalSection cicmapLock;
/*End Global variables*/




/*Extern variables*/

extern int logger_qid;
extern unsigned char g_dspStat;
extern unsigned char E1Status[MAX_E1+1];
extern unsigned char SS7LinkStatus[MAX_SS7_LINK];
/*End Extern variables*/
 
 
 
 
 /*Functions*/
 
 static int isup_readLicense();
 static int isup_startSignalingServer(jctiSignalConfig config) ;
 static int isup_startMediaServer(jctiMediaConfig config);
 static int isup_startLogger(jctiLog::jctiLogConfig config);
 static int createHashRecord(unsigned short portNo);
 static int createHashRecordOutgoing(unsigned short portNo);
 static void initialise(isupConfig config);
 int setIOTermination(unsigned short port,IOTERM term);
 int process_media_event(unsigned char rx_msg_size,unsigned char *msg);
 void process_play_complete(unsigned char rx_msg_size,unsigned char *msg);
 int isup_getIAMParam(unsigned char msg[1000],IAM *param);
 int startTimer(unsigned char timerName,unsigned short id);
 int stopTimer(unsigned char timerName,unsigned short id);
 int stopAllTimerOnCIC(unsigned short cic);
 static int resetCircuit(unsigned short cic);
 /*End of functions*/
 
 /*Extern functions*/
 
 extern unsigned short cicTotrunk(unsigned short cic);
 extern int starTimerProcess();
 //Media functions
 extern int loadFromIndex(int file_count,char * filePath);
 extern int announceOnly(unsigned short port,int announcements,unsigned short *announcement_id);
 extern int playFromFile(unsigned short port,char * filePath);
 extern int stopAnnouncement(unsigned short port);
 extern int stopRecord(unsigned short port);
 extern int startDTMFTermThread(unsigned int port,IOTERM *term);
 extern int reset_circuit(unsigned short port,unsigned char link);
 extern int saveToFile(unsigned short port,int duration,char * filePath);
 
 //Signaling Functions
 extern int parseIAM(unsigned char msg[1000],IAM  *iamParam);
 extern int parseACM(unsigned char msg[1000],ACM *param);
 extern int parseANM(unsigned char msg[1000],ANM *param);
 extern int parseREL(unsigned char msg[1000],REL *param);
 extern int parseRLC(unsigned char msg[1000],RLC *param);
 extern int initialiseIAM(IAM  *iamParam,BYTE switchType = ITUT);
 extern int initialiseACM(ACM  *param,BYTE switchType = ITUT);
 extern int initialiseANM(ANM  *param,BYTE switchType = ITUT);
 extern int initialiseREL(REL  *param,BYTE cause = CCCALLCLR,BYTE switchType = ITUT);
 extern int initialiseRLC(RLC  *param,BYTE switchType = ITUT);
 extern int send_iam(unsigned short port,IAM * param,char *source_number,char *dest_number,unsigned char link);
 extern int send_acm(unsigned short port, ACM *param,unsigned char link);
 extern int send_rel(unsigned short port, REL *param,unsigned char link);
 extern int send_rlc(unsigned short port,RLC * param,unsigned char link);
 extern int send_anm(unsigned short port, ANM * param,unsigned char link);
 extern int statusReq(unsigned short port,unsigned char link,BYTE msgType,stStatEvnt evt);
 
 extern void* reset_all_circuits(void *);
 extern int initialiseMsg(Msg_Packet *Ss7Msg);
 extern struct tm * getLocalTime();
 extern void setSs7ChanStatus(unsigned short port,unsigned short status);
 extern int sendGroupResetAck(int cic,int range);
 extern int  sendDTMF2Box(unsigned short port,unsigned short dtmf);
 /*End Extern functions*/
 
  static void insertCallRec(callRec rec)
 {
     mapLock.Lock();
     call.insert(Int_Pair(rec.portNo,rec));
     mapLock.Unlock();
 }
 
 static void removeCallRec(unsigned short cic )
 {
     std::hash_map <int,callRec> :: iterator callItr ;
     mapLock.Lock();
     callItr = call.find(cic);
     if(callItr != call.end())
        call.erase(callItr);
     usleep(2000);
     mapLock.Unlock();
 }
 
  std::hash_map <int,callRec> :: iterator  findCallRec(unsigned short cic )
 {
     mapLock.Lock();
     std::hash_map <int,callRec> :: iterator tempCallItr ;
     tempCallItr = call.find(cic);
     mapLock.Unlock();
     return tempCallItr;
 }


std::hash_map <int,callRec> :: iterator checkHashEnd()
{
        mapLock.Lock();
        std::hash_map <int,callRec> :: iterator tempCallItr ;
        tempCallItr = call.end();
        mapLock.Unlock();
        return tempCallItr;
}
 
 
  static void insertCICInfo(cicInformation info)
 {
     cicmapLock.Lock();
     cicInfo.insert(cicPair(info.channelNo,info));
     cicmapLock.Unlock();
 }
 
 std::hash_map <int,cicInformation> :: iterator  findCicInfo(unsigned short chanNo )
 {
     cicmapLock.Lock();
     std::hash_map <int,cicInformation> :: iterator tempCicInfoItr ;
     tempCicInfoItr = cicInfo.find(chanNo);
     cicmapLock.Unlock();
     return tempCicInfoItr;
 }
 
 std::hash_map <int,cicInformation> :: iterator  cicInfoBegin( )
 {
     cicmapLock.Lock();
     std::hash_map <int,cicInformation> :: iterator tempCicInfoItr ;
     tempCicInfoItr = cicInfo.begin();
     cicmapLock.Unlock();
     return tempCicInfoItr;
 }
 
 std::hash_map <int,cicInformation> :: iterator  cicInfoEnd()
 {
     cicmapLock.Lock();
     std::hash_map <int,cicInformation> :: iterator tempCicInfoItr ;
     tempCicInfoItr = cicInfo.end();
     cicmapLock.Unlock();
     return tempCicInfoItr;
 }
 
 
 
 static std::hash_map <int,cicInformation> :: iterator cicToChannel(int cic,int linkNo)
 {
     int signalLinkSetCount = 0,sigLink=0;
     std::hash_map <int,cicInformation> :: iterator tempCicInfoItr ;
     for(tempCicInfoItr = cicInfoBegin();tempCicInfoItr != cicInfoEnd();tempCicInfoItr++)
     {
         if(tempCicInfoItr->second.origcic == cic)
         {
             for(signalLinkSetCount = 0;signalLinkSetCount < MAX_SS7_LINK; signalLinkSetCount++)
             {
                 if(tempCicInfoItr->second.linkset[signalLinkSetCount] > 0 && tempCicInfoItr->second.linkset[signalLinkSetCount] <= MAX_SS7_LINK)
                 {
                     for(sigLink = 0;sigLink < MAX_SS7_LINK; sigLink++)
                     {
                        if(conf.linksetConfig[tempCicInfoItr->second.linkset[signalLinkSetCount] -1].ss7link[sigLink] == linkNo)
                        {
                            printf("Record found cic %d channel %d link %d\n",cic,tempCicInfoItr->second.channelNo,linkNo);
                            return(tempCicInfoItr);
                        }
                     }
                 }
                 
             }
         }
     }
     printf("Failed to get channel number  for corresponding CIC No %d link no %d\n",cic,linkNo);
     sprintf(msgLogger.applMsgTxt,"Failed to get channel number  for corresponding CIC No %d link no %d",cic,linkNo);
     logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
     return tempCicInfoItr;
 }
 
 
 static std::hash_map <int,cicInformation> :: iterator channelToCic(int chanNo)
 {
     std::hash_map <int,cicInformation> :: iterator tmpCicInfoItr;
     tmpCicInfoItr = findCicInfo(chanNo);
     if(tmpCicInfoItr == cicInfoEnd())
     {
        sprintf(msgLogger.applMsgTxt,"Failed to get CIC number for corresponding channel No %d ",chanNo);
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
     }
     return tmpCicInfoItr;
 }
 
 
int addMsgSlaveQue(Msg_Packet msg)
{
    struct msqid_ds msgbuf;
    msgQueLock.Lock();
    
    if(msgctl(cti_qid, IPC_STAT, &msgbuf) == -1)
    {
        sprintf(msgLogger.applMsgTxt,"Signaling slave QID does not exist. Error no : %d desc: %s" ,errno,getErrDesc(errno));
        logErr(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_0);
        //criticalSec.Unlock();
        msgQueLock.Unlock();
        return errno;
    }
    
    if( msgsnd(cti_qid,&msg,sizeof(Msg_Packet),0)==-1 )
    {
        sprintf(msgLogger.applMsgTxt,"Failed to add data in signaling Msg que. Error no : %d desc: %s" ,errno,getErrDesc(errno));
        logErr(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_0);
        //criticalSec.Unlock();
        msgQueLock.Unlock();
        return errno;
   }
    
   // criticalSec.Unlock();
    msgQueLock.Unlock();
    return SUCCESS;
     
}
 
int addTimerResp(stTimer t)
{
    printf("Timer Expiry added in Que\n");
    criticalAddTimerLock.Lock();
    respTimerQue.push_back(t);
    criticalAddTimerLock.Unlock();
    printf("Timer Expiry added in Que\n");
}

int processT1Resp()
{
    std::hash_map <int,callRec> :: iterator callItr ;
    Msg_Packet msg;
    int counter = 0;
    callItr = findCallRec(respTimerQueItr->id);
    if(callItr != checkHashEnd())
    {
       // printf("retry count %d t16 retry %d\n",callItr->second.timerRetry,timerSS7.t16.retry);
       if(callItr->second.signalingState == STATE_RELEASED_OG )//&&  callItr->second.timerRetry < timerSS7.t1.retry)
       {
             
            REL rel;// = (REL *) malloc(sizeof(REL));
            memset(&rel,0,sizeof(REL));
            isupInitialiseREL(&rel,CCCALLCLR);
            isup_Disconnect(respTimerQueItr->id,&rel);
            //free(rel);
           /* initialiseMsg(&msg);
            msg.type =respTimerQueItr->id;
            msg.id1=BOX_ID1;
            msg.id2=BOX_ID2;
            msg.debug=0;
            msg.opcode = PIC_SS7_MNTC;
            msg.subfield = SS7_TIMER_EXPIRY;
            counter=0;
            msg.msg[counter++]= (respTimerQueItr->id & 0xFF);
            msg.msg[counter++]= (respTimerQueItr->id >> 8) & 0x07;
            msg.msg[counter++]=SS7_T1_TIMER;
            msg.length=(FIXED_SIZE+counter) << 8;
            addMsgToSigSlaveQue(msg);*/
        }
      /*if(callItr->second.timerRetry > timerSS7.t1.retry)
       {
           callItr->second.timerRetry = 0; 
           sprintf(msgLogger.applMsgTxt,"REL not received RLC response on CIC %d",respTimerQueItr->id);
           logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
       }*/
       
    }
}


int processT5Resp()
{
    std::hash_map <int,callRec> :: iterator callItr ;
    Msg_Packet msg;
    int counter = 0;
    callItr = findCallRec(respTimerQueItr->id);
    if(callItr != checkHashEnd())
    {
      //  printf("retry count %d t16 retry %d\n",callItr->second.timerRetry,timerSS7.t16.retry);
       if(callItr->second.signalingState == STATE_RELEASED_OG)
       {
           stopTimer(SS7_T1_TIMER,respTimerQueItr->id);
           resetCircuit(respTimerQueItr->id);
           initialiseMsg(&msg);
            msg.type =respTimerQueItr->id;
            msg.id1=BOX_ID1;
            msg.id2=BOX_ID2;
            msg.debug=0;
            msg.opcode = PIC_SS7_MNTC;
            msg.subfield = SS7_T5_TIMER;
            counter=0;
            msg.msg[counter++]= (respTimerQueItr->id & 0xFF);
            msg.msg[counter++]= (respTimerQueItr->id >> 8) & 0x07;
            msg.msg[counter++]=SS7_TIMER_EXPIRY;
            msg.length=(FIXED_SIZE+counter) << 8;
            addMsgSlaveQue(msg);
       }
    }
}

int processT7Resp()
{
    std::hash_map <int,callRec> :: iterator callItr ;
    Msg_Packet msg;
    int counter = 0;
    
    callItr = findCallRec(respTimerQueItr->id);
    if(callItr != checkHashEnd())
    {
       if(callItr->second.signalingState == STATE_OFFERED_OG)
       {
            callItr->second.timerRetry = 0; 
            REL rel;// = (REL *) malloc(sizeof(REL));
            memset(&rel,0,sizeof(REL));
            isupInitialiseREL(&rel,CCCALLCLR);
            isup_Disconnect(respTimerQueItr->id,&rel);
            //free(rel);
            sprintf(msgLogger.applMsgTxt,"IAM did not receive ACM response on CIC %d callID %s",respTimerQueItr->id,callItr->second.callId);
            logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
            initialiseMsg(&msg);
            msg.type =respTimerQueItr->id;
            msg.id1=BOX_ID1;
            msg.id2=BOX_ID2;
            msg.debug=0;
            msg.opcode = PIC_SS7_MNTC;
            msg.subfield = SS7_T7_TIMER;
            counter=0;
            msg.msg[counter++]= (respTimerQueItr->id & 0xFF);
            msg.msg[counter++]= (respTimerQueItr->id >> 8) & 0x07;
            msg.msg[counter++]=SS7_TIMER_EXPIRY;
            msg.length=(FIXED_SIZE+counter) << 8;
            addMsgSlaveQue(msg);
       }
    }
}


int processT9Resp()
{
    std::hash_map <int,callRec> :: iterator callItr ;
    Msg_Packet msg;
    int counter = 0;
    callItr = findCallRec(respTimerQueItr->id);
    if(callItr != checkHashEnd())
    {
       
        if(callItr->second.signalingState == STATE_RINGING_IC
                || callItr->second.signalingState == STATE_RINGING_OG)
        {
            callItr->second.timerRetry = 0; 
            REL rel;// = (REL *) malloc(sizeof(REL));
            memset(&rel,0,sizeof(REL));
            isupInitialiseREL(&rel,CCCALLCLR);
            isup_Disconnect(respTimerQueItr->id,&rel);
            //free(rel);
            sprintf(msgLogger.applMsgTxt,"ACM did not receive ANM response on CIC %d callID %s",respTimerQueItr->id,callItr->second.callId);
            logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
            initialiseMsg(&msg);
            msg.type =respTimerQueItr->id;
            msg.id1=BOX_ID1;
            msg.id2=BOX_ID2;
            msg.debug=0;
            msg.opcode = PIC_SS7_MNTC;
            msg.subfield = SS7_T9_TIMER;
            counter=0;
            msg.msg[counter++]= (respTimerQueItr->id & 0xFF);
            msg.msg[counter++]= (respTimerQueItr->id >> 8) & 0x07;
            msg.msg[counter++]=SS7_TIMER_EXPIRY;
            msg.length=(FIXED_SIZE+counter) << 8;
            addMsgSlaveQue(msg);
        }
   
       
    }
}


int processT16Resp()
{
    std::hash_map <int,callRec> :: iterator callItr ;
    
    callItr = findCallRec(respTimerQueItr->id);
    if(callItr != checkHashEnd())
    {
       // printf("retry count %d t16 retry %d\n",callItr->second.timerRetry,timerSS7.t16.retry);
         if( callItr->second.signalingState == STATE_RESET_OG )//callItr->second.timerRetry < timerSS7.t16.retry && 
                
         {
              isup_resetCircuit(respTimerQueItr->id);
         }
        /*if(callItr->second.timerRetry > timerSS7.t16.retry)
        {
            sprintf(msgLogger.applMsgTxt,"Reset failed on CIC %d",respTimerQueItr->id);
           logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
           callItr->second.timerRetry = 0; 
        }*/
    }
}


int processT17Resp()
{
    std::hash_map <int,callRec> :: iterator callItr ;
     Msg_Packet msg;
    int counter = 0;
    callItr = findCallRec(respTimerQueItr->id);
    if(callItr != checkHashEnd())
    {
        // printf("retry count %d t17 retry %d\n",callItr->second.timerRetry,timerSS7.t16.retry);
         if( callItr->second.signalingState == STATE_RESET_OG )//callItr->second.timerRetry < timerSS7.t16.retry && 
         {
             
            resetCircuit(respTimerQueItr->id);
            initialiseMsg(&msg);
            msg.type =respTimerQueItr->id;
            msg.id1=BOX_ID1;
            msg.id2=BOX_ID2;
            msg.debug=0;
            msg.opcode = PIC_SS7_MNTC;
            msg.subfield = SS7_T17_TIMER;
            counter=0;
            msg.msg[counter++]= (respTimerQueItr->id & 0xFF);
            msg.msg[counter++]= (respTimerQueItr->id >> 8) & 0x07;
            msg.msg[counter++]=SS7_TIMER_EXPIRY;
            msg.length=(FIXED_SIZE+counter) << 8;
            addMsgSlaveQue(msg);
         }
    }
}


int processSS7SigTimerResp()
{

      switch(respTimerQueItr->type)
      {
          case SS7_T1_TIMER:
              processT1Resp();
              break;
          case SS7_T5_TIMER:
              processT5Resp();
              break;
          case SS7_T7_TIMER:
              processT7Resp();
              break;
          case SS7_T9_TIMER:
              processT9Resp();
              break;
          case SS7_T16_TIMER:
              processT16Resp();
              break;
          case SS7_T17_TIMER:
              processT17Resp();
              break;    
          default:
            sprintf(msgLogger.applMsgTxt,"Unknown timer Type %d ID %d",respTimerQueItr->type,respTimerQueItr->id);
            logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
           // respTimerQue.pop_front();
            return INVALID_SOURCE;
                      
      }
  }
 
 int processTimerResp()
 {
    criticalRemoveTimerLock.Lock();
    respTimerQueItr = respTimerQue.begin();
    if(respTimerQueItr != respTimerQue.end())
    {
        //printf("Timer expired on ID %d source %d \n",respTimerQueItr->id,respTimerQueItr->source);
        switch(respTimerQueItr->source)
        {
            case SS7_SIG:
            {
                processSS7SigTimerResp();
            }
            break;
            case SS7_MED:
                break;
            default:
                sprintf(msgLogger.applMsgTxt,"Unknown timer source %d ID %d",respTimerQueItr->source,respTimerQueItr->id);
                logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
                respTimerQue.erase(respTimerQueItr);
                criticalRemoveTimerLock.Unlock();
                return INVALID_SOURCE;
        }
    }
    else
    {
       criticalRemoveTimerLock.Unlock();
       return EINVAL;
    }
    
    respTimerQue.erase(respTimerQueItr);
    criticalRemoveTimerLock.Unlock();
    return SUCCESS;
 }
 
class maintenanceAct
{
public:
	virtual void parseSanity(Msg_Packet * ss7SanityMsg) = 0;
	virtual void mntcAction(Msg_Packet * ss7SanityMsg) = 0;
};


class E1Statusmntc:public maintenanceAct
{
public:
	BYTE e1Status[MAX_E1];
	BYTE e1PrevStatus[MAX_E1];
public:
	E1Statusmntc()
	{
            int index = 0;
            for(index = E1_STATUS_START; index < E1_STATUS_START + MAX_E1; index++)
            {
                    e1Status[index] = 0x00;
                    e1PrevStatus[index] = 0x00;
            }
	}
	void parseSanity(Msg_Packet * ss7SanityMsg);
	void mntcAction(Msg_Packet * ss7SanityMsg);
};


class E1Configmntc:public maintenanceAct
{
public:
	BYTE e1Config[MAX_E1];
	BYTE e1PrevConfig[MAX_E1];
public:
	E1Configmntc()
	{
            int index = 0;
            for(index = E1_CONFIG_START; index < E1_CONFIG_START+ MAX_E1; index++)
            {
                e1Config[index - E1_CONFIG_START] = 0x00;
                e1PrevConfig[index - E1_CONFIG_START] = 0x00;
            }
	}
	void parseSanity(Msg_Packet * ss7SanityMsg);
	void mntcAction(Msg_Packet * ss7SanityMsg);
};


void E1Statusmntc::parseSanity(Msg_Packet * ss7SanityMsg)
{
int index = 0;
static int firstTime = 0;
	if(ss7SanityMsg->subfield ==  SANITY_FROM_PIC)
	{
        	//cout << "VMS Response Physical E1 status" << endl;
		for(index = E1_STATUS_START; index < E1_STATUS_START + MAX_E1; index++)
                {
                	if(ss7SanityMsg->msg[index] == ENABLED)
	                {
                           // cout << "E " << index +1 <<  "UP" ;
                        }
	                else if(ss7SanityMsg->msg[index] == DISABLED)
        	        {
                            //printf("E %d DWN ",index +1);
                        }
                	e1Status[index] = ss7SanityMsg->msg[index];
			if(firstTime == 0)
			{
                            e1PrevStatus[index] = ss7SanityMsg->msg[index];
			}
			
		}
		if(firstTime == 0)
                    firstTime++;
	}
	
} 

void E1Statusmntc::mntcAction(Msg_Packet * ss7SanityMsg)
{
int index = 0;
int chanNo = 0;
std::hash_map <int,callRec> :: iterator callItr;
std::hash_map <int,cicInformation> :: iterator cicInfoItr;

	parseSanity(ss7SanityMsg);
	for(index = E1_STATUS_START; index < E1_STATUS_START + MAX_E1; index++)
        {
		if(e1PrevStatus[index] != e1Status[index] && conf.E1Config[index].enable)
		{
			if(e1Status[index] == DISABLED)
			{
				//cout << "E " << index+1 << "Disabled" << endl;
				for(chanNo = (index *32) +1; chanNo < ((index * 32) +1) + 32;chanNo++)
				{
					//if(cic% 32 != 0 && strcmp(configXML.e1Lines[index + 1].e1Channel[cic].type,(char*)channelType[3]) != 0 )
                                        if(chanNo% 32 != 0)
					{
						createHashRecord(chanNo);					
        					callItr = findCallRec(chanNo);
					        if(callItr != checkHashEnd())
						{
							cicInfoItr = channelToCic(chanNo);
                                                        if(cicInfoItr != cicInfoEnd())
                                                        {
                                                            cicInfoItr->second.channelStatus = OOS;
                                                        }											
						}
					}
				}
			
			}
			else if(e1Status[index] == ENABLED)
			{
				chanNo = (index *32) + 1;
				isup_resetCircuitGrp(chanNo,0x1E);	
			}	
		}
		e1PrevStatus[index] = e1Status[index];
	}	

}


void E1Configmntc::parseSanity(Msg_Packet * ss7SanityMsg)
{
int index = 0;
static int firstTime = 0;

	if(ss7SanityMsg->subfield ==  SANITY_FROM_PIC)
	{
        	//cout << "VMS Response Physical E1 status" << endl;
		for(index = E1_CONFIG_START; index < E1_CONFIG_START + MAX_E1; index++)
                {
                	if(ss7SanityMsg->msg[index] == E1_CONFIG_NORMAL )
	                {
                	       // cout << "E " << index +1 <<  " NORMAL " ;
                        }
	                else if(ss7SanityMsg->msg[index] == E1_CONFIG_LOCAL_LOOPBACK)
        	        {
                               // cout << "E " << index +1 << " LOCAL LOOP";
                        }
                        else if(ss7SanityMsg->msg[index] == E1_CONFIG_REMOTE_LOOPBACK)
        	        {
                              //  cout << "E " << index +1 << " REMOTE LOOP";
                        }
                	e1Config[index- E1_CONFIG_START] = ss7SanityMsg->msg[index];
			if(firstTime == 0)
			{
				e1PrevConfig[index - E1_CONFIG_START] = ss7SanityMsg->msg[index];
			}
			
		}
		if(firstTime == 0)
                    firstTime++;
	}
} 

void E1Configmntc::mntcAction(Msg_Packet * ss7SanityMsg)
{
int index = 0;
int chanNo = 0;
static int firstTime = 0;
std::hash_map <int,callRec> :: iterator callItr;
std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
	parseSanity(ss7SanityMsg);
	for(index = E1_CONFIG_START; index < E1_CONFIG_START + MAX_E1; index++)
        {
                //printf(" %d E1conf %d E1prev %d \n",index,e1Config[index - E1_CONFIG_START], e1PrevConfig[index - E1_CONFIG_START]);
		if(firstTime == 0)
		{
			if(e1Config[index - E1_CONFIG_START] == E1_CONFIG_LOCAL_LOOPBACK || e1Config[index - E1_CONFIG_START] == E1_CONFIG_REMOTE_LOOPBACK )
                        {
                                //cout << "E " << index - E1_CONFIG_START +1 << "Loop backed" << endl;
                                for(chanNo = ((index - E1_CONFIG_START) *32) +1; chanNo < (((index- E1_CONFIG_START) * 32) +1) + 32;chanNo++)
                                {
					//printf("Channel type %s trunk %d cic %d\n",configXML.e1Lines[index - E1_CONFIG_START +1].e1Channel[cic].type,index - E1_CONFIG_START +1,cic);
					//if(cic% 32 != 0 && strcmp(configXML.e1Lines[index - E1_CONFIG_START +1].e1Channel[cic].type,(char*)channelType[3]) != 0 )
                                        if(chanNo% 32 != 0)
					{
                                        	createHashRecord(chanNo);
	                                        callItr = findCallRec(chanNo);
        	                                if(callItr != checkHashEnd())
                	                        {
                        	                        cicInfoItr = channelToCic(chanNo);
                                                        if(cicInfoItr != cicInfoEnd())
                                                        {
                                                            cicInfoItr->second.channelStatus = OOS;
                                                        }	                                                                          
                                	        }
					}
                                }

                        }

		}
		if(e1PrevConfig[index - E1_CONFIG_START  ] != e1Config[index - E1_CONFIG_START] && conf.E1Config[index - E1_CONFIG_START].enable
			&& firstTime != 0)
		{
			if(e1Config[index - E1_CONFIG_START] == E1_CONFIG_LOCAL_LOOPBACK || e1Config[index - E1_CONFIG_START] == E1_CONFIG_REMOTE_LOOPBACK )
			{
				//cout << "E " << index - E1_CONFIG_START +1 << "Loop backed" << endl;
				for(chanNo = ((index - E1_CONFIG_START) *32) +1; chanNo < (((index- E1_CONFIG_START) * 32) +1) + 32;chanNo++)
				{
					//if(cic% 32 != 0 && strcmp(configXML.e1Lines[index - E1_CONFIG_START +1].e1Channel[cic].type,channelType[3]) != 0 )
                                        if(chanNo% 32 != 0)
					{
						createHashRecord(chanNo);					
        					callItr = findCallRec(chanNo);
					        if(callItr != checkHashEnd())
						{
							cicInfoItr = channelToCic(chanNo);
                                                        if(cicInfoItr != cicInfoEnd())
                                                        {
                                                            cicInfoItr->second.channelStatus = OOS;
                                                        }											
						}
					}
				}
			
			}
			else if(e1Config[index - E1_CONFIG_START] == E1_CONFIG_NORMAL)
			{
                                //cout << "E " << index - E1_CONFIG_START +1 << "Normal" << endl;
				chanNo = ((index - E1_CONFIG_START) *32) + 1;
				isup_resetCircuitGrp(34,0x1E);	
			}	
		}
		e1PrevConfig[index - E1_CONFIG_START] = e1Config[index - E1_CONFIG_START];
	}	
	if(firstTime == 0)
            firstTime++;

}


class Maintenance
{
    private:
        E1Statusmntc * e1Statusmntc;
        E1Configmntc * e1Configmntc;
        Maintenance()
        {
             e1Statusmntc = new E1Statusmntc();
             e1Configmntc = new E1Configmntc();
        }
        Maintenance(const Maintenance &old); // disallow copy constructor
        const Maintenance &operator=(const Maintenance &old); 
        static BYTE instanceFlag;
        static Maintenance *mntc;
    
    public:
    ~Maintenance()
    {
        instanceFlag = FALSE;
        delete e1Statusmntc;
        delete e1Configmntc;
    }
    
    static Maintenance* getInstance();
    
    void doMaintenance(Msg_Packet * ss7SanityMsg)
    {
        e1Statusmntc->mntcAction(ss7SanityMsg);
        e1Configmntc->mntcAction(ss7SanityMsg);
    }
           
};


BYTE Maintenance::instanceFlag = FALSE;
Maintenance* Maintenance::mntc = NULL;

Maintenance * Maintenance::getInstance()
{
    if( ! instanceFlag)
    {
        mntc = new Maintenance();
        instanceFlag = TRUE;
        return mntc;
    }
    else
        return mntc;
}
 


void printConfig()
 {
    int i,j;
     sprintf(msgLogger.applMsgTxt,"Signaling server IP %s,Port %d,Default Action %d",
             conf.signalingServer[0].sigServerIP,conf.signalingServer[0].portNo,conf.signalingServer[0].defaultAction);
     logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
     
     sprintf(msgLogger.applMsgTxt,"Media server IP %s,Port %d,Default Action %d",
             conf.mediaServer[0].mediaServerIP,conf.mediaServer[0].startingPortNo,conf.mediaServer[0].defaultAction);
     logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
     
     sprintf(msgLogger.applMsgTxt,"Log debug %d err %d info %d debugLevel %d errLevel %d infoLevel %d",
             conf.logConfig.debug,conf.logConfig.err,conf.logConfig.info,conf.logConfig.logLeveldebug,conf.logConfig.logLevelerr,
             conf.logConfig.logLevelinfo);
     logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
     
     memset(&msgLogger,0,sizeof(msgLogger));
     for(i = 0; i < MAX_E1; i++)
     {
        sprintf(msgLogger.applMsgTxt,"E1 Trunk %d enable %d ",i,
                conf.E1Config[i].enable);
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
     }
     
     memset(msgLogger.applMsgTxt,0,sizeof(msgLogger.applMsgTxt));
     sprintf(msgLogger.applMsgTxt," Signaling CICs : ");
     for(i = 0; i < MAX_SS7_LINK; i++)
     {
         if(conf.signalingCIC[i] > 1 && conf.signalingCIC[i] < MAX_CHANNELS)
         {
                sprintf(&msgLogger.applMsgTxt[strlen(msgLogger.applMsgTxt)]," %d",conf.signalingCIC[i]);
         }
        
     }
     logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
 }

int setCICOOS(unsigned short cic)
{
    int i;
    if(cic < 1 || cic > MAX_CHANNELS)
        return OUT_OF_RANGE;

     for(i = 0; i < MAX_SS7_LINK; i++)
     {
         if(conf.signalingCIC[i] > 1 && conf.signalingCIC[i] < MAX_CHANNELS)
         {
             if(conf.signalingCIC[i] == cic)
                 return OUT_OF_SERVICE;
                
         }
     }
    
    cicInfo[cic].channelStatus = OOS;
    stopAllTimerOnCIC(cic);
    return SUCCESS;
}
 
int setCICINS(unsigned short cic)
{
    int i;
    if(cic < 1 || cic > MAX_CHANNELS)
        return OUT_OF_RANGE;

     for(i = 0; i < MAX_SS7_LINK; i++)
     {
         if(conf.signalingCIC[i] > 1 && conf.signalingCIC[i] < MAX_CHANNELS)
         {
             if(conf.signalingCIC[i] == cic)
                 return OUT_OF_SERVICE;
                
         }
    }
    cicInfo[cic].channelStatus = INS;
    return SUCCESS;
}


int isSignalingCIC(unsigned short cic)
{
    int i;
    if(cic < 1 || cic > MAX_CHANNELS)
        return OUT_OF_RANGE;

     for(i = 0; i < MAX_SS7_LINK; i++)
     {
         if(conf.signalingCIC[i] > 1 && conf.signalingCIC[i] < MAX_CHANNELS)
         {
             if(conf.signalingCIC[i] == cic)
                 return SUCCESS;
                
         }
    }
    return -1;
}

void * trafficController(void *)
{
    std::hash_map <int,callRec> :: iterator callItr ;
    time_t timerCur;
    time_t timerFreeCic;
    isupLog::Log tmpLogger;
    char msg[MAX_MSG_LEN];
    char msg1[MAX_MSG_LEN];
    int i =0;
    time(&timerFreeCic);
    while(TRUE)
    {
        
        time(&timerCur);
        if(difftime(timerCur,timerTraffic) > 1 )
        {
            for(i=0;i<MAX_SS7_LINK;i++)
                    gCountTraffic[i] = 0;

            time(&timerTraffic);
        }
        usleep(5000);
        if(difftime(timerCur,timerFreeCic) > 10)
        {
            memset(msgLogger.applMsgTxt,0,sizeof(msgLogger.applMsgTxt));
            memset(msg,0,sizeof(msg));
            memset(msg1,0,sizeof(msg1));
            //sprintf(msgLogger.applMsgTxt,"Free Cics: ");
            for(i=1;i<=MAX_CHANNELS;i++)
            {
                callItr = findCallRec(i);
                if(callItr == checkHashEnd())
                {
                    sprintf(&msg[strlen(msg)],"%d,",i);  
                    if(strlen(msg) > MAX_MSG_LEN - 100)
                    {
                        sprintf(msgLogger.applMsgTxt,"Free Cics: %s ",msg);
                        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_2);
                        memset(msg,0,sizeof(msg));
                        
                        
                    }
                }
                else
                {
                    sprintf(&msg1[strlen(msg1)],"%d,",i);  
                    if(strlen(msg1) > MAX_MSG_LEN - 100)
                    {
                        sprintf(msgLogger.applMsgTxt,"Busy Cics: %s ",msg1);
                        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_2);
                        memset(msg1,0,sizeof(msg1));
                        
                        
                    }
                }
            }
            sprintf(msgLogger.applMsgTxt,"Free Cics: %s ",msg);
            logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_2);
            sprintf(msgLogger.applMsgTxt,"Busy Cics: %s ",msg1);
            logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_2);
            time(&timerFreeCic);
        }
    }
}

static void setCICLinkset(int low, int high,int linkset)
{
int k =0,l=0,found = 0;    

    if(high != 0)
    {
        if(low <= 0)
            low =1;
        if(high > MAX_CHANNELS)
            high = MAX_CHANNELS;
        for( k = low; k <= high; k++)
        {
            found = 0;
            for(l=0; l < MAX_SS7_LINK; l++ )
            {
                if(cicInfo[k].linkset[l] > 0 && 
                        cicInfo[k].linkset[l] <= MAX_SS7_LINK) 
                {
                    if(cicInfo[k].linkset[l]  == linkset +1)
                    {
                        found = 1;
                        break;
                    }
                    else
                        continue;
                }
                else
                {
                    found =1;
                    cicInfo[k].linkset[l]  = linkset +1;
                    break;
                }
            
            }
            
            if(!found)
                cicInfo[k].linkset[l] = linkset + 1;   
        }
    }
    else
    {
        found = 0;
        for(l=0; l < MAX_SS7_LINK; l++ )
        {
            if(cicInfo[k].linkset[l] > 0 && 
                        cicInfo[k].linkset[l] <= MAX_SS7_LINK) 
            {
                if(cicInfo[k].linkset[l]  == linkset +1)
                {
                    found = 1;
                    break;
                }
                else
                    continue;
            }
            else
            {
                found =1;
                cicInfo[k].linkset[l]  = linkset +1;
                break;
            }

        }
            
        if(!found)
            cicInfo[k].linkset[l] = linkset + 1;  
    }
                            
}

static void setCICParam()
{
int i = 0,j =0, k =0,range = 0;
char E1[16];
char channelRange[MAX_IP_LEN];
char channelLow[10],channelHigh[10],temp[10];
int low,high;
isupLog::Log tmpLogger;

        memset(E1,0,sizeof(E1));
        memset(temp,0,sizeof(temp));
        memset(channelLow,0,sizeof(channelLow));
        memset(channelHigh,0,sizeof(channelHigh));
        memset(channelRange,0,sizeof(channelRange));
        for( i = 0; i< MAX_SS7_LINK; i++)
        {
            if(conf.linksetConfig[i].cicRange != NULL)
                strcpy(channelRange,conf.linksetConfig[i].cicRange);
            else
                continue;
            for(k = 0; k < strlen(channelRange); k++)
            {
                    memset(temp,0,sizeof(temp));
                    if(channelRange[k] == '-')
                    {
                        range = 1;
                        
                    }
                    else if(channelRange[k] == ',')
                    {
                        low = atoi(channelLow);
                        high = atoi(channelHigh);
                        setCICLinkset(low,high,i);
                        range =0;
                        memset(channelLow,0,sizeof(channelLow));
                        memset(channelHigh,0,sizeof(channelHigh));
                    }
                    else if(range == 0)
                    {
                            sprintf(temp,"%c",channelRange[k]);
                            strcat(channelLow,temp);

                    }
                    else if(range == 1)
                    {
                            sprintf(temp,"%c",channelRange[k]);
                            strcat(channelHigh,temp);
                    }
            }
            low = atoi(channelLow);
            high = atoi(channelHigh);
            setCICLinkset(low,high,i);
            range =0;
            memset(channelLow,0,sizeof(channelLow));
            memset(channelHigh,0,sizeof(channelHigh));

                                 
        }
        for( i = 1; i< MAX_CHANNELS; i++)
        {
            memset(tmpLogger.applMsgTxt,0,sizeof(tmpLogger.applMsgTxt));
            sprintf(tmpLogger.applMsgTxt,"CIC %d Linksets ",i);
            for( j = 0; j < MAX_SS7_LINK; j++)
            {
                if(cicInfo[i].linkset[j] > 0  && cicInfo[i].linkset[j] <= MAX_SS7_LINK)
                {
                    sprintf(&tmpLogger.applMsgTxt[strlen(tmpLogger.applMsgTxt)]," %d,",cicInfo[i].linkset[j]);
                }
            }
                
            logDebug(_LFF,tmpLogger,APPL_LOG,LOG_LEVEL_2);
        }
       
}



static int doCICMapping(isupConfig config)
{
int i = 0,j =0, k =0,range = 0;
char E1[MAX_E1];
char channelRange[MAX_IP_LEN];
char channelLow[10],channelHigh[10],temp[10];
int low,high,chanNo;
isupLog::Log tmpLogger;
cicInformation info;
    memset(&info,0,sizeof(cicInformation));
    memset(E1,0,sizeof(E1));
    memset(temp,0,sizeof(temp));
    memset(channelLow,0,sizeof(channelLow));
    memset(channelHigh,0,sizeof(channelHigh));
    memset(channelRange,0,sizeof(channelRange));
    for( i = 0; i < MAX_E1; i++)
    {
        chanNo =  (i*32) + 1;
        if(conf.E1Config[i].enable == TRUE)
        {

            if(conf.E1Config[i].origcicRange != NULL)
                strcpy(channelRange,conf.E1Config[i].origcicRange);
            else
                continue;
            
            for(k = 0; k < strlen(channelRange); k++)
            {
                memset(temp,0,sizeof(temp));
                if(channelRange[k] == '-')
                {
                    range = 1;

                }
                else if(channelRange[k] == ',')
                {
                    low = atoi(channelLow);
                    high = atoi(channelHigh);
                    
                    for(j = low; j <= high; j++ )
                    {
                        info.channelNo = chanNo;
                        info.channelStatus = INS;
                        info.origcic = j;
                        chanNo++;
                        insertCICInfo(info);
                    }
                    range =0;
                    memset(channelLow,0,sizeof(channelLow));
                    memset(channelHigh,0,sizeof(channelHigh));
                }
                else if(range == 0)
                {
                        sprintf(temp,"%c",channelRange[k]);
                        strcat(channelLow,temp);

                }
                else if(range == 1)
                {
                        sprintf(temp,"%c",channelRange[k]);
                        strcat(channelHigh,temp);
                }
            }
            low = atoi(channelLow);
            high = atoi(channelHigh);
            for(j = low; j <= high; j++ )
            {
                info.channelNo = chanNo;
                info.channelStatus = INS;
                info.origcic = j;
                chanNo++;
                insertCICInfo(info);
                
            }
            range =0;
            memset(channelLow,0,sizeof(channelLow));
            memset(channelHigh,0,sizeof(channelHigh));
        }
    }
}
   


 int isup_start(jctiConfig config)
 {
     int err,rc;
     pthread_t trafficThreadHndl = 0;
     memset(&conf,0,sizeof(isupConfig));
     memcpy(&conf,&config,sizeof(config));
     time(&timerTraffic);
     
     initialise(config);
     isup_startLogger(config.logConfig);
     sleep(2);
     setCICParam();
    rc = pthread_create(&trafficThreadHndl,NULL,&trafficController,NULL);
    if(rc == -1)
    {
        sprintf(msgLogger.applMsgTxt,"Failed to create trafficThread Thread. Error no : %d desc: %s" ,errno,getErrDesc(errno));
        logErr(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_0);
    }
     loadErrDescription();
     sprintf(msgLogger.applMsgTxt,"************ Starting jCTI API Version %f***********",VERSION);
     logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
     printConfig();
     err = isup_readLicense();
     
     if((err =starTimerProcess()) != SUCCESS)
         return err;
     
     if(licensed)
     {
         if((err = isup_startSignalingServer(config.signalingServer[0])) != SUCCESS)
             return err;
         if((cti_qid = msgget(SLAVE_KEY,0700 | IPC_CREAT)) == -1)
         {
            sprintf(msgLogger.applMsgTxt,"Failed to create slave message que. Error no : %d desc: %s" ,errno,getErrDesc(errno));
            logErr(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_0);
            return errno;
         }
       //  if((err = isup_startMediaServer(config.mediaServer)) != SUCCESS)
      //      return err;
         
         return SUCCESS;
            
         
         
     }
     else
         return err;
 }
 
 
 
 static void initialise(isupConfig config)
 {
     int i = 0,j =0;
     
     //std::strcpy(config.signalingServer.sigServerIP,"192.168.10.21");
     //config.signalingServer.portNo = 5000;
    memset(&cicInfo,0,sizeof(cicInfo));
  
    for(i = 0; i < MAX_CHANNELS; i++)
    {
        //channelStatus[i] = INS;
        createHashRecord(i+1);
        /*for(j = 0; j < MAX_SS7_LINK; j++)
                cicInfo[i+1].linkset[j] = 0;*/
    }
    for(i = 0; i < MAX_SS7_LINK; i++)
    {
        linkStatus[i] = INS;
        gCountTraffic[i] = 0;
    }
  
    doCICMapping(config);
     
    timerSS7.t1.timeout = T1_EXPIRY;
    timerSS7.t1.retry = DEFAULT_RETRY;

    timerSS7.t5.timeout = T5_EXPIRY;
    timerSS7.t5.retry = DEFAULT_RETRY;

    timerSS7.t7.timeout = T7_EXPIRY;
    timerSS7.t7.retry = DEFAULT_RETRY;

    timerSS7.t9.timeout = T9_EXPIRY;
    timerSS7.t9.retry = DEFAULT_RETRY;

    timerSS7.t16.timeout = T16_EXPIRY;
    timerSS7.t16.retry = DEFAULT_RETRY;

    timerSS7.t17.timeout = T17_EXPIRY;
    timerSS7.t17.retry = DEFAULT_RETRY;
     
         
 }
 

static int createHashRecord(unsigned short portNo)
{
 int n = rand() ;
 callRec rec;
 std::hash_map <int,callRec> :: iterator callItr ;
 criticalHashICLock.Lock();
 
    rec.portNo = portNo;
    rec.signalingState = IDLE;
    rec.mediaSate = IDLE;
    strcpy(rec.cdpn,"");
    strcpy(rec.cgpn,"");
    rec.timerRetry = 0;
    
   
    callItr = findCallRec(rec.portNo);
    if(callItr != checkHashEnd())
    {
        //printf("PortNo %d",callItr->second.portNo);
        criticalHashICLock.Unlock();
        return CIC_BUSY;
    }
    insertCallRec(rec);
    callItr = findCallRec(rec.portNo);
    //if( callItr != checkHashEnd())
     //   printf("PortNo %d",callItr->second.portNo);
    
    char dateTime[50];
    time_t now;
    time(&now);
    //struct tm *ltm = localtime(&now);
    struct tm *ltm = getLocalTime();
    struct timeval tim;
    gettimeofday(&tim, NULL);
    long int t1=(tim.tv_usec);

    sprintf(dateTime,"%6ld",t1);
    sprintf(callItr->second.callId,"%d%d%d%d%d%d%d%d%ld",n,portNo,ltm->tm_hour,ltm->tm_min,ltm->tm_sec,ltm->tm_mday,ltm->tm_mon,1900+ltm->tm_year,t1);
    criticalHashICLock.Unlock();
    return SUCCESS;
}

 
static int createHashRecordOutgoing(unsigned short portNo)
{
 int n = rand() ;
 callRec rec;
 std::hash_map <int,callRec> :: iterator callItr ;
    criticalSec.Lock();
    rec.portNo = portNo;
    rec.signalingState = IDLE;
    rec.mediaSate = IDLE;
    strcpy(rec.cdpn,"");
    strcpy(rec.cgpn,"");
    rec.timerRetry = 0;
   
    callItr = findCallRec(rec.portNo);
    if(callItr != checkHashEnd())
    {
       // printf("PortNo %d",callItr->second.portNo);
        criticalSec.Unlock();
        return CIC_BUSY;
    }
    insertCallRec(rec);
    callItr = findCallRec(rec.portNo);
    //if( callItr != checkHashEnd())
    //    printf("PortNo %d",callItr->second.portNo);
    
    char dateTime[50];
    time_t now;
    time(&now);
    //struct tm *ltm = localtime(&now);
    struct tm *ltm = getLocalTime();
    struct timeval tim;
    gettimeofday(&tim, NULL);
    long int t1=(tim.tv_usec);
    sprintf(dateTime,"%6ld",t1);
    sprintf(callItr->second.callId,"%d%d%d%d%d%d%d%d%ld",n,portNo,ltm->tm_hour,ltm->tm_min,ltm->tm_sec,ltm->tm_mday,ltm->tm_mon,1900+ltm->tm_year,t1);
    criticalSec.Unlock();
    return SUCCESS;
}


 static int isup_readLicense()
 {
     licensed = TRUE;
     return SUCCESS;
    // config.logConfig.loglevel = 0;
    // isup_startLogger(config);
 }
 
 
/***************************************************************************************************************
*FunName : isup_startSignalingServer()
*
*Arguments: Configuration structure. Which passes TCP server IP and port number.
*
*Purpose : It creates a TCP connection with Juno CTI Box. It creates a thread which receives all signaling packets
* 	   from the Box.Then pushes those messages in message que.
*
*Return:  On success return 0 else returns an error number defined in isuperror.h file.
*
*History: 18/01/2013 created by Nihar
*
*
*****************************************************************************************************************/

static int isup_startSignalingServer(jctiSignalConfig config) 
{
    int ret;
    ret = connectToSignalingServer(config) ;
    return ret;
        
}


static int isup_startMediaServer(jctiMediaConfig config) 
{
    int ret;
        //ret = connectToMediaServer(config) ;
        return ret;
        
}



/***************************************************************************************************************
*FunName : isup_startLogger()
*
*Arguments: Configuration structure. Which passes logging information.
*
*Purpose : It creates a logger thread which logs messages in a file.
*
*Return:  On success return 0 else returns an error number defined in isuperror.h file.
*
*History: 18/01/2013 created by Nihar
*
*
*****************************************************************************************************************/

static int isup_startLogger(jctiLog::jctiLogConfig config) 
{
   int rc; 
    setLoggingInfo(config);
    rc = pthread_create(&loggerThdHndl,NULL,&logger,(void *)&config);
    if(rc == -1)
    {
        perror("Failed to create Logger thread :");
        return errno;
        
    }
        
}


int  isup_setDebugInfo(jctiLog::jctiLogConfig debugInfo)
{
    setLoggingInfo(debugInfo);
}



/*char * getLastErrDesc(int errno)
{
    char errDesc[200];
    return errDesc;
}*/

int selectLink(unsigned short cic,unsigned char *link)
{
    int i =0,j =0,found =0;
    unsigned short sl = 0;
    isupLog::Log tmpLogger;
    memset(msgLogger.applMsgTxt,0,sizeof(msgLogger.applMsgTxt));
    sprintf(msgLogger.applMsgTxt,"Link ");
    for(i = 0; i < MAX_SS7_LINK; i++)
    {
        if(cicInfo[cic].linkset[i] > 0 && cicInfo[cic].linkset[i] <= MAX_SS7_LINK )
        {
            for(j =0; j < MAX_SS7_LINK; j++)
            {
                               
                if(conf.linksetConfig[cicInfo[cic].linkset[i] -1].ss7link[j] > 0 && 
                    conf.linksetConfig[cicInfo[cic].linkset[i] -1].ss7link[j] <= MAX_SS7_LINK)
                {
                    sprintf(tmpLogger.applMsgTxt,"CIC %d Algorithm %d Previous link %d current link %d ",cic,conf.linksetConfig[cicInfo[cic].linkset[i] -1].linkSelAlgo,
                       cicInfo[cic].prevLink ,conf.linksetConfig[cicInfo[cic].linkset[i] -1].ss7link[j] );
                    logDebug(_LFF,tmpLogger,APPL_LOG,LOG_LEVEL_4);
                    switch(conf.linksetConfig[cicInfo[cic].linkset[i] -1].linkSelAlgo)
                    {
                        case THREASHOLD:
                            break;
                        case LOADSHARING:
                        default:
                            if(cicInfo[cic].prevLink == conf.linksetConfig[cicInfo[cic].linkset[i] -1].ss7link[j])
                            {
                                continue;
                            }
                            break;
                                    
                    }
                    
                    
                    sl = conf.linksetConfig[cicInfo[cic].linkset[i] -1].ss7link[j];
                    
                    if(SS7LinkStatus[sl - 1] == 0x01) 
                    {
                      //  printf("gCountTraffic[sl - 1] %d \n",gCountTraffic[sl - 1] );
                        if(gCountTraffic[sl - 1] < (MAX_MSG_PER_SEC - MAX_RESERVED_MSG))
                        {
                            found = 1;
                            *link = sl -1;
                            cicInfo[cic].prevLink = sl;
                            sprintf(&msgLogger.applMsgTxt[strlen(msgLogger.applMsgTxt)]," %d SELECTED ",sl - 1); 
                            break;
                        }
                        else
                          sprintf(&msgLogger.applMsgTxt[strlen(msgLogger.applMsgTxt)]," %d CONGESTED ",sl - 1);  

                    }
                    else
                    {
                        sprintf(&msgLogger.applMsgTxt[strlen(msgLogger.applMsgTxt)]," %d DOWN ",sl -1);
                    }
                }
                else
                    break;
            }
            if(found)
                break;
            
        }
    }
    
    if(found)
    {
        sprintf(&msgLogger.applMsgTxt[strlen(msgLogger.applMsgTxt)]," on CIC %d",cic); 
        logDebug(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_2);
        return SUCCESS;
    }
    else
    {
        for(i = 0; i < MAX_SS7_LINK; i++)
        {
            if(cicInfo[cic].linkset[i] > 0 && cicInfo[cic].linkset[i] <= MAX_SS7_LINK )
            {
                for(j =0; j < MAX_SS7_LINK; j++)
                {
                    if(conf.linksetConfig[cicInfo[cic].linkset[i] -1].ss7link[j] > 0 && 
                        conf.linksetConfig[cicInfo[cic].linkset[i] -1].ss7link[j] <= MAX_SS7_LINK)
                    {
                        if(SS7LinkStatus[conf.linksetConfig[cicInfo[cic].linkset[i] -1].ss7link[j] - 1] == 0x01 )
                        {
                            found = 1;
                            *link = conf.linksetConfig[cicInfo[cic].linkset[i] -1].ss7link[j] - 1 ;
                            cicInfo[cic].prevLink = conf.linksetConfig[cicInfo[cic].linkset[i] -1].ss7link[j] ;
                            sprintf(&msgLogger.applMsgTxt[strlen(msgLogger.applMsgTxt)]," %d SELECTED ",
                                    conf.linksetConfig[cicInfo[cic].linkset[i] -1].ss7link[j] - 1); 
                            break;
                        }
                        
                    }
                    else
                        break;
                }
                
            }
            if(found)
                break;
        }
    }
    
    if(found)
    {
        sprintf(&msgLogger.applMsgTxt[strlen(msgLogger.applMsgTxt)]," on CIC %d",cic); 
        logDebug(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_4);
        if(gCountTraffic[*link + 1] < (MAX_MSG_PER_SEC - MAX_RESERVED_MSG))
                return SUCCESS;
        else            
                return SWITCH_CONGESTION;
    }
    else
    {
        sprintf(&msgLogger.applMsgTxt[strlen(msgLogger.applMsgTxt)]," on CIC %d",cic); 
        logDebug(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_2);
        return SIG_LINK_DOWN;
    }
}


int checkCICFree(unsigned short cic)
{
    unsigned short trunk;
    unsigned char link;
    int ret,i=0,j = 0,found=0;
    std::hash_map <int,callRec> :: iterator callItr ;
    
    //criticalSec.Lock();
    criticalFreeCICLock.Lock();
    trunk = cicTotrunk(cic);
    if(conf.E1Config[trunk].enable == FALSE)
    {
        //criticalSec.Unlock();
        criticalFreeCICLock.Unlock();
        return E1_DISABLED;
    }
        
    
    if(E1Status[trunk] == 0)
    {
        //criticalSec.Unlock();
        criticalFreeCICLock.Unlock();
        return E1_DOWN;
    }
    found = 0;
    for(i = 0; i < MAX_SS7_LINK; i++)
    {
        if(cicInfo[cic].linkset[i] > 0 && cicInfo[cic].linkset[i] <= MAX_SS7_LINK )
        {
            for(j =0; j < MAX_SS7_LINK; j++)
            {
                if(conf.linksetConfig[cicInfo[cic].linkset[i] -1].ss7link[j] > 0 && 
                    conf.linksetConfig[cicInfo[cic].linkset[i] -1].ss7link[j] <= MAX_SS7_LINK)
                {
                    if(SS7LinkStatus[conf.linksetConfig[cicInfo[cic].linkset[i] -1].ss7link[j] - 1] == 0x01
                            && gCountTraffic[conf.linksetConfig[cicInfo[cic].linkset[i] -1].ss7link[j] - 1] < (MAX_MSG_PER_SEC - MAX_RESERVED_MSG))
                    {
                        found = 1;
                        break;
                    }
                }
            }
            if(found)
                break;
        }
    }
    
    if(!found)
    {
      //  if(SS7LinkStatus[conf.E1Config[trunk].failoverLink] == 0x00)
     //   {
            criticalFreeCICLock.Unlock();
            return SIG_LINK_DOWN;
     //   }
        
   }
    
  
    
    /*if(SS7LinkStatus[conf.E1Config[cicTotrunk(cic)-1].ss7link[]] == 0 &&
          SS7LinkStatus[conf.E1Config[cicTotrunk(cic)-1].failoverLink] == 0  )
    {
       //criticalSec.Unlock();
        criticalFreeCICLock.Unlock();
        return SS7_LINK_DOWN;
    }*/
    
    
    if(g_dspStat == 0x00 )
    {
        //criticalSec.Unlock();
        criticalFreeCICLock.Unlock();
                
        return MEDIA_DOWN;
    }
    
    if(cicInfo[cic].channelStatus == OOS) 
    {
        //criticalSec.Unlock();
        criticalFreeCICLock.Unlock();
        return OUT_OF_SERVICE;
    }
    
    callItr = findCallRec(cic);
    if( callItr != checkHashEnd())
    {
        //criticalSec.Unlock();
        criticalFreeCICLock.Unlock();
        return CIC_BUSY;
    }
    //criticalSec.Unlock();
    criticalFreeCICLock.Unlock();
    return SUCCESS;
    
}


/*int getFreeCIC(unsigned short cic)
{
    int count = 0;
    for(count = 1 count <= MAX_CHANNELS; count++ )
    {
         if(checkCICFree(count);
    
}*/



int checkCICStatus(unsigned short cic,unsigned char *link)
{
    unsigned short trunk;
    int ret;
    cicStatusLock.Lock();
    trunk = cicTotrunk(cic);
    
    if(cic < 0 || cic > MAX_CHANNELS)
    {
        cicStatusLock.Unlock();
        return EINVAL;
    }
    
    if(conf.E1Config[trunk].enable == FALSE)
    {
        cicStatusLock.Unlock();
        return E1_DISABLED;
    }
    
    if(E1Status[trunk] == 0)
    {
        cicStatusLock.Unlock();
        return E1_DOWN;
    }
    
    if(g_dspStat == 0x00 )
    {
        cicStatusLock.Unlock();
        return MEDIA_DOWN;
    }
    
    if((ret = selectLink(cic,link)) == SIG_LINK_DOWN)
    {
       // if(SS7LinkStatus[conf.E1Config[trunk].failoverLink] == 0x00)
       // {
            cicStatusLock.Unlock();
            return SIG_LINK_DOWN;
       // }
       // *link = conf.E1Config[trunk].failoverLink;
    }
    
    if(cicInfo[cic].channelStatus == OOS)
    {
        cicStatusLock.Unlock();
        return OUT_OF_SERVICE;
    }
    
    if(ret != SUCCESS)
    {
        cicStatusLock.Unlock();
        return ret;
    }
    
    cicStatusLock.Unlock();
    return SUCCESS;
    
}


int checkTrunkStatus(unsigned short cic,unsigned char *link)
{
   // printConfig();
    int ret;
    unsigned short trunk;
    trunkStatusLock.Lock();
    trunk = cicTotrunk(cic);
    if(conf.E1Config[trunk].enable == 0x00)
    {
        trunkStatusLock.Unlock();
        return E1_DISABLED;
    }
    if(E1Status[trunk] == 0x00)
    {
        trunkStatusLock.Unlock();
        return E1_DOWN;
    }
    
    if((ret = selectLink(cic,link)) == SIG_LINK_DOWN)
    {
       // if(SS7LinkStatus[conf.E1Config[trunk].failoverLink] == 0x00)
      //  {
            trunkStatusLock.Unlock();
            return SIG_LINK_DOWN;
      //  }
        
       // *link = conf.E1Config[trunk].failoverLink;
    }
   
    if(ret != SUCCESS)
    {
        trunkStatusLock.Unlock();
        return ret;
    }
    
    trunkStatusLock.Unlock();
    return SUCCESS;
}

Maintenance * mntc = Maintenance::getInstance();

int isup_getEvt(EVENT *evt )
{
    int size, found = 0,i = 0,E1count,ret;
    unsigned short portNo;
    Msg_Packet ss7_rx_msg;
    time_t timerCur;
    isupLog::Log tmpLogger;
    std::hash_map <int,callRec> :: iterator callItr ;
    static std::hash_map <int,cicInformation> :: iterator cicInfoItr;
    
   // criticalSec.Lock();
    criticalEventLock.Lock();
    memset(&ss7_rx_msg,0,sizeof(ss7_rx_msg));
    memset(evt,0,sizeof(EVENT));
    // sprintf(msgLogger.applMsgTxt,"Called getEvent()");
     // logDebug(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_0);
    time(&timerCur);
    if(difftime(timerCur,timerTraffic) > 1 )
    {
        for(i=0;i<MAX_SS7_LINK;i++)
                gCountTraffic[i] = 0;
        time(&timerTraffic);
    }
    
      processTimerResp();
      if((size=msgrcv(cti_qid, &ss7_rx_msg, sizeof(Msg_Packet),0, IPC_NOWAIT)) == -1)
      {
                //sprintf(msgLogger.applMsgTxt,"Failed to receive Packet. Error no : %d desc: %s" ,errno,getErrDesc(errno));
                //logDebug(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_0);
               // criticalSec.Unlock();
                //free(rel);
                criticalEventLock.Unlock();
                return errno;
      }
      else
      {
        if(size == 0xFF)
        {
            sprintf(msgLogger.applMsgTxt,"Received alarm");
            logDebug(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
           // criticalSec.Unlock();
            criticalEventLock.Unlock();
            return PORT_ALARM;
        }

       // sprintf(msgLogger.applMsgTxt,"Received successful message. Error no : %d desc: %s" ,errno,getErrDesc(errno));
       // logDebug(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_0);
        if( ss7_rx_msg.opcode != 0x40 && ss7_rx_msg.opcode != 0x41 &&  ss7_rx_msg.opcode != PIC_SS7_MNTC 
                && ss7_rx_msg.opcode != DTMF_TIMEOUT && ss7_rx_msg.opcode != PIC_DTMF_CALLPROC)
        {
            cicInfoItr = cicToChannel(ss7_rx_msg.type,ss7_rx_msg.subfield+1);
            if(cicInfoItr == cicInfoEnd())
            {
                sprintf(msgLogger.applMsgTxt,"Failed to get channel number  for corresponding CIC No %d link no %d opcode %x",ss7_rx_msg.type,ss7_rx_msg.subfield+1,ss7_rx_msg.msg[2]);
                logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
                criticalEventLock.Unlock();
                return OUT_OF_RANGE;
            }

            portNo = cicInfoItr->second.channelNo;
        }
        else
            portNo = ss7_rx_msg.type;
        
        //portNo = ss7_rx_msg.type;
        if(portNo <= 0 && (ss7_rx_msg.opcode != 0x40 || ss7_rx_msg.opcode != 0x41 ||  ss7_rx_msg.opcode != PIC_SS7_MNTC ))
        {
            //criticalSec.Unlock();
            criticalEventLock.Unlock();
            return OUT_OF_RANGE;
        }
    
        evt->chanNo = portNo;
        evt->cic = ss7_rx_msg.type;
        gcic = ss7_rx_msg.type;
        evt->opcode = ss7_rx_msg.opcode;
        evt->subField = ss7_rx_msg.subfield;
        gLink = ss7_rx_msg.subfield + 1;
        evt->trunk = (ss7_rx_msg.msg[0] >> 5) | (ss7_rx_msg.msg[1] << 3);
        evt->msgType = ss7_rx_msg.msg[2];
        i = 0;
        printf("Length %d \n",ss7_rx_msg.length);
        while( i <  ss7_rx_msg.length - 8 && i < MAX_ISUP_MSU_LEN)
        {
            evt->msg[i] = ss7_rx_msg.msg[i];
            i++;
        }
        
        switch(ss7_rx_msg.opcode)
        {
            case PIC_SS7_CALLPROC:
                  if(portNo!=0)
                  {
                        callItr = findCallRec(portNo);
                        if(callItr == checkHashEnd())
                        {
                              if(ss7_rx_msg.msg[2]  == MSG_TYPE_ADDRESS_COMPLETE || 
                                       ss7_rx_msg.msg[2]  == MSG_TYPE_ANSWER )
                              {
                                  sprintf(msgLogger.applMsgTxt,"Received msg %d on FREE CIC %d. Dropping packet.",ss7_rx_msg.msg[2],portNo);
                                  logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_0);
                                  criticalEventLock.Unlock();
                                  return INVALID_STATE;
                              }
                        }
                        if((ret = createHashRecord(portNo)) != SUCCESS)
                        {
                            if(ret == CIC_BUSY)
                            {
                                if(ss7_rx_msg.msg[2]  == MSG_TYPE_INITIAL_ADDRESS)
                                {
                                   /* REL *rel = (REL *) malloc(sizeof(REL));
                                    isupInitialiseREL(rel,CCREQUNAVAIL);
                                    isup_Disconnect(portNo,rel);
                                    free(rel);*/
                                    sprintf(msgLogger.applMsgTxt,"Received IAM on BUSY CIC %d. Dropping packet.",portNo);
                                    logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_0);
                                    criticalEventLock.Unlock();
                                    return OUT_OF_RANGE;
                                }
                            }
                        }

                        callItr = findCallRec(portNo);
                        callItr->second.subField = ss7_rx_msg.subfield;
                        
                        process_isup_message(ss7_rx_msg.length,ss7_rx_msg.msg);
                        evt->signalingState = callItr->second.signalingState;
                        evt->mediaState = callItr->second.mediaSate;
                  }

                  break;
            case PIC_MEDIA_CALLPROC:
                 if(portNo!=0)
                 {

                      process_media_event(ss7_rx_msg.length,ss7_rx_msg.msg);
                 }
               break;
            case PIC_SS7_MNTC:
                //    printf("SS7 mntc message received for port %d\n",PORTNO);
                      if(ss7_rx_msg.subfield ==  0x02)
                      {
                           mntc->doMaintenance(&ss7_rx_msg);
                          /*printf("VMS Response Physical E1 status\n");
                          for(E1count = 0; E1count < MAX_E1; E1count++)
                          {
                              E1Status[E1count]  = ss7_rx_msg.msg[E1count];
                              SS7LinkStatus[E1count] = ss7_rx_msg.msg[E1count+16];
                          }
                          g_dspStat = ss7_rx_msg.msg[64];
                          memset(msgLogger.applMsgTxt,0,sizeof(msgLogger.applMsgTxt));
                          sprintf(msgLogger.applMsgTxt,"SANITY : ");
                          for(i = 0; i <= 64;i++)
                          {
                              sprintf(&msgLogger.applMsgTxt[strlen(msgLogger.applMsgTxt)],"%02x ",ss7_rx_msg.msg[i]);
                              //sendMsgToLogger(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_2);
                          }
                          sprintf(&msgLogger.applMsgTxt[strlen(msgLogger.applMsgTxt)],"\n");
                          logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_2);*/

                     }


                     break;
            case DTMF_TIMEOUT:
                break;
            case PIC_DTMF_CALLPROC:
                sprintf(msgLogger.applMsgTxt,"Received DTMF  %d channo %d  cic %d",evt->msg[14],evt->chanNo,evt->cic);
                logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
                break;
                         

            default:
               sprintf(msgLogger.applMsgTxt,"Received unknown opcode %x",ss7_rx_msg.opcode);
               logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
                break;
        }
        
        //criticalSec.Unlock();
        criticalEventLock.Unlock();
        return SUCCESS;
      }
    
}

int process_media_event(unsigned char rx_msg_size,unsigned char *msg)
{
      switch(msg[2])
      {
        case  RTP_PLAY_COMPLETE:
              // process_play_complete(rx_msg_size,msg);
               break;
        default:
               sprintf(msgLogger.applMsgTxt,"Received unknown message type %d",msg[2]);
               logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_2);
               break; 
      }
}


int process_isup_message(unsigned char rx_msg_size,unsigned char *msg)
{
        
        int i = 0;
        
        switch(msg[2])
        {
                case  MSG_TYPE_INITIAL_ADDRESS:
                        process_msg_type_initial_message(rx_msg_size,msg);
                        break;
                case  MSG_TYPE_SUBSEQUENT_ADDRESS:
                        process_msg_type_subsequent_address(msg);
                        break;
                case  MSG_TYPE_INFORMATION_REQ:
                        process_msg_type_information_req(msg);
                        break;
                case  MSG_TYPE_INFORMATION:
                        process_msg_type_information(msg);
                        break;
                case  MSG_TYPE_CONTINUITY:
                        process_msg_type_continuity(msg);
                        break;
                case  MSG_TYPE_ADDRESS_COMPLETE:
                        process_msg_type_address_complete(msg);
                        break;
                case  MSG_TYPE_CONNECT:
                        process_msg_type_connect(msg);
                        break;
                case  MSG_TYPE_FORWARD_TRANSFER:
                        process_msg_type_forward_transfer(msg);
                        break;
                case  MSG_TYPE_ANSWER:
                        process_msg_type_answer(msg);
                        break;
                case  MSG_TYPE_RELEASE:
                        process_msg_type_release(msg);
                        break;
                case  MSG_TYPE_SUSPEND:
                        process_msg_type_suspend(msg);
                        break;
                case  MSG_TYPE_RESUME:
                        process_msg_type_resume(msg);
                        break;
                case  MSG_TYPE_RELEASE_COMPLETE:
                        process_msg_type_release_complete(msg);
                     //   Ss7_ch_status(PORTNO,PIC_DSP_TXEN_RXDIS);
                        break;
                case  MSG_TYPE_CONTINUITY_CHECK_REQ:
                        process_msg_type_continuity_check_req(msg);
                        break;
                case  MSG_TYPE_RESET_CIRCUIT:
                       // Ss7_ch_status(PORTNO,PIC_DSP_TXEN_RXDIS);
                        process_msg_type_reset_circuit(msg);

                        break;
                case  MSG_TYPE_BLOCKING:
                        //Ss7_ch_status(PORTNO,PIC_DSP_TXEN_RXDIS);
                        process_msg_type_blocking(msg);
                        break;
                case  MSG_TYPE_UNBLOCKING:
                        process_msg_type_unblocking(msg);
                        break;
                case  MSG_TYPE_BLOCKING_ACK:
                        process_msg_type_blocking_ack(msg);
                        break;
                case  MSG_TYPE_UNBLOCKING_ACK:
                        process_msg_type_unblocking_ack(msg);
                        break;
                case  MSG_TYPE_CIRCUIT_GROUP_RESET:
                        process_msg_type_circuit_group_reset(msg);
                        break;
                case  MSG_TYPE_CIRCUIT_GROUP_BLOCK:
                        process_msg_type_group_block(msg);
                        break;
                case  MSG_TYPE_CIRCUIT_GROUP_UNBLOCK:
                        process_msg_type_group_unblock(msg);
                        break;
                case  MSG_TYPE_CIRCUIT_GROUP_BLOCK_ACK:
                        process_msg_type_group_block_ack(msg);
                        break;
                case  MSG_TYPE_CIRCUIT_GROUP_UNBLOCK_ACK:
                        process_msg_type_group_unblock_ack(msg);
                        break;
                case  MSG_TYPE_FACILITY_REQUEST:
                        process_msg_type_facility_request(msg);
                        break;
                case  MSG_TYPE_FACILITY_ACCEPTED:
                        process_msg_type_facility_accepted(msg);
                        break;
                case  MSG_TYPE_FACILITY_REJECT:
                        process_msg_type_facility_reject(msg);
                        break;
                case  MSG_TYPE_LOOPBACK_ACK:
                        process_msg_type_loopback_ack(msg);
                        break;
                case  MSG_TYPE_PASSALONG:
                        process_msg_type_passalong(msg);
                        break;
                case  MSG_TYPE_CIRCUIT_GROUP_RESET_ACK:
                        process_msg_type_circuit_group_reset_ack(msg);
                        break;
                case  MSG_TYPE_CIRCUIT_GROUP_QUERY:
                        process_msg_type_circuit_group_query(msg);
                        break;
                case  MSG_TYPE_CIRCUIT_GROUP_QUERY_RESP:
                        process_msg_type_circuit_group_query_resp(msg);
                        break;
                case  MSG_TYPE_CALL_PROGRESS:
                        process_msg_type_call_progress(msg);
                case  MSG_TYPE_USER_TO_USER_INFORMATION:
                        process_msg_type_user_to_user_information(msg);
                        break;
                case  MSG_TYPE_UNEQUIPPED_CIC:
                        process_msg_type_unquipped_cic(msg);
                        break;
                case  MSG_TYPE_CONFUSION:
                        process_msg_type_confusion(msg);
                        break;
                case  MSG_TYPE_OVERLOAD:
                        process_msg_type_overload(msg);
                        break;
                case  MSG_TYPE_CHARGE_INFORMATION:
                        process_msg_type_charge_information(msg);
                        break;
                case  MSG_TYPE_NETWORK_RESOURCE_MANAGEMENT:
                        process_msg_type_network_resource_management(msg);
                        break;
                case  MSG_TYPE_FACILITY:
                        process_msg_type_type_facility(msg);
                        break;
                case  MSG_TYPE_USER_PART_TEST:
                        process_msg_type_user_part_test(msg);
                        break;
                case  MSG_TYPE_USER_PART_AVAILBLE:
                        process_msg_type_user_part_available(msg);
                        break;
                case  MSG_TYPE_IDENTIFICATION_REQ:
                        process_msg_type_identification_req(msg);
                        break;
                case  MSG_TYPE_IDENTIFICATION_RESP:
                        process_msg_type_identification_rest(msg);
                        break;
                case  MSG_TYPE_SEGMENTATION:
                        process_msg_type_type_segmentation(msg);
                        break;
                default:
                        sprintf(msgLogger.applMsgTxt,"Received unknown message type %d",msg[2]);
                        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
                        break;               
        }
}



void process_msg_type_initial_message(unsigned char rx_msg_size,unsigned char *msg)
{
unsigned short port,tmp;
unsigned char dtk;
IAM param;// =(IAM *) malloc(sizeof(IAM));
ACM acmParam;// = (ACM *) malloc(sizeof(ACM));
std::hash_map <int,callRec> :: iterator callItr ;
std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
    criticalProcessIAMLock.Lock();
    tmp = msg[1] & 0x07;
    port=msg[0] | (tmp << 8);
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    cicInfoItr = cicToChannel(port,gLink);
    if(cicInfoItr == cicInfoEnd())
    {
        sprintf(msgLogger.applMsgTxt,"Failed to get channel number  for corresponding CIC No %d link no %d",port,gLink);
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        criticalProcessIAMLock.Unlock();
        return ;
    }
    callItr = findCallRec(cicInfoItr->second.channelNo);
    isup_getIAMParam(msg,&param);
    if(param.cdpn.addrSig != NULL)
    {
       strcpy(callItr->second.cdpn,param.cdpn.addrSig);
    }
    
    if(param.cgpn.stat.pres == TRUE && param.cgpn.addrSig != NULL)
        strcpy(callItr->second.cgpn,param.cgpn.addrSig);
    
    if(conf.signalingServer[0].defaultAction == TRUE)
    {
        isupInitialiseACM(&acmParam);
        isup_sendACM(gcic,&acmParam);
    }
        
    if(callItr != checkHashEnd())
    {
        sprintf(msgLogger.applMsgTxt,"Received IAM on CIC %d channel %d trunk %d CDPN %s CGPN %s callID %s link %d",port,cicInfoItr->second.channelNo,dtk,
                    callItr->second.cdpn,callItr->second.cgpn,callItr->second.callId,callItr->second.subField);
        
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_0);
        callItr->second.signalingState = STATE_OFFERED_IC;
        if(callItr->second.subField >= 0 && callItr->second.subField < MAX_SS7_LINK)
            gCountTraffic[callItr->second.subField]++;
    }
    
    //free(param);
    //free(acmParam);
    criticalProcessIAMLock.Unlock();
}

void process_msg_type_subsequent_address(unsigned char *msg)
{
    unsigned short port,tmp;
    unsigned char dtk;
    tmp = msg[1] & 0x07;
    port=msg[0] | (tmp << 8);
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    sprintf(msgLogger.applMsgTxt,"Received SAM on CIC %d trunk %d",port,dtk);
    logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
}

void process_msg_type_information_req(unsigned char *msg)
{
unsigned short port,tmp;
unsigned char dtk;
    tmp = msg[1] & 0x07;
    port=msg[0] | (tmp << 8);
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    sprintf(msgLogger.applMsgTxt,"Received Information request message on CIC %d trunk %d",port,dtk);
    logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
}

void process_msg_type_information(unsigned char *msg)
{
unsigned short port,tmp;
unsigned char dtk;
    tmp = msg[1] & 0x07;
    port=msg[0] | (tmp << 8);
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    sprintf(msgLogger.applMsgTxt,"Received Information message on CIC %d trunk %d",port,dtk);
    logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
}

void process_msg_type_continuity(unsigned char *msg)
{
unsigned short port,tmp;
unsigned char dtk;
    tmp = msg[1] & 0x07;
    port=msg[0] | (tmp << 8);
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    sprintf(msgLogger.applMsgTxt,"Received Continuity request message on CIC %d trunk %d",port,dtk);
    logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_2);
}

void process_msg_type_address_complete(unsigned char *msg)
{
    unsigned short port,tmp;
    unsigned char dtk;
    std::hash_map <int,callRec> :: iterator callItr ;
    std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
    tmp = msg[1] & 0x07;
    port=msg[0] | (tmp << 8);
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    cicInfoItr = cicToChannel(port,gLink);
    if(cicInfoItr == cicInfoEnd())
    {
        sprintf(msgLogger.applMsgTxt,"Failed to get channel number  for corresponding CIC No %d link no %d",port,gLink);
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        return ;
    }
    stopTimer(SS7_T7_TIMER,cicInfoItr->second.channelNo);
    startTimer(SS7_T9_TIMER,cicInfoItr->second.channelNo);
    callItr = findCallRec(cicInfoItr->second.channelNo);
    if(callItr != checkHashEnd())
    {
        sprintf(msgLogger.applMsgTxt,"Received ACM message on CIC %d channel %d trunk %d callID %s link %d",port,cicInfoItr->second.channelNo,dtk,callItr->second.callId,callItr->second.subField);
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        callItr->second.signalingState = STATE_RINGING_IC;
        if(callItr->second.subField >= 0 && callItr->second.subField < MAX_SS7_LINK)
                gCountTraffic[callItr->second.subField]++;
    }

}

void process_msg_type_connect(unsigned char *msg)
{
     unsigned short port,tmp;
    unsigned char dtk;
    std::hash_map <int,callRec> :: iterator callItr ;
    tmp = msg[1] & 0x07;
    port=msg[0] | (tmp << 8);
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    callItr = findCallRec(port);
    sprintf(msgLogger.applMsgTxt,"Received Connect message on CIC %d trunk %d",port,dtk);
    logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_2);
}

void process_msg_type_answer(unsigned char *msg)
{
     unsigned short port,tmp;
    unsigned char dtk;
    std::hash_map <int,callRec> :: iterator callItr ;
    std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
     tmp = msg[1] & 0x07;
    port=msg[0] | (tmp << 8);
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    //stopTimer(SS7_T9_TIMER,port);
    cicInfoItr = cicToChannel(port,gLink);
    if(cicInfoItr == cicInfoEnd())
    {
        sprintf(msgLogger.applMsgTxt,"Failed to get channel number  for corresponding CIC No %d link no %d",port,gLink);
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        return ;
    }
    stopAllTimerOnCIC(cicInfoItr->second.channelNo);
    callItr = findCallRec(cicInfoItr->second.channelNo);
    if(callItr != checkHashEnd())
    {
        callItr->second.signalingState = STATE_ANSWERED_IC;
        sprintf(msgLogger.applMsgTxt,"Received ANM message on CIC %d channel %d trunk %d callID %s link %d",port,cicInfoItr->second.channelNo,dtk,callItr->second.callId,callItr->second.subField);
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_0);
        if(callItr->second.subField >= 0 && callItr->second.subField < MAX_SS7_LINK)
            gCountTraffic[callItr->second.subField]++;
    }
     
}


void process_msg_type_forward_transfer(unsigned char *msg)
{
    unsigned short port,tmp;
    unsigned char dtk;
     tmp = msg[1] & 0x07;
    port=msg[0] | (tmp << 8);
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    sprintf(msgLogger.applMsgTxt,"Received Forward Transfer message on CIC %d trunk %d",port,dtk);
    logDebug(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_2);
}

void process_msg_type_release(unsigned char *msg)
{
    unsigned short port,tmp;
    unsigned char dtk;
    REL param;
    std::hash_map <int,callRec> :: iterator callItr ;
    std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
     tmp = msg[1] & 0x07;
    port=msg[0] | (tmp << 8);
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    isup_getRELParam(msg,&param);
    cicInfoItr = cicToChannel(port,gLink);
    if(cicInfoItr == cicInfoEnd())
    {
        sprintf(msgLogger.applMsgTxt,"Failed to get channel number  for corresponding CIC No %d link no %d",port,gLink);
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        return ;
    }
    callItr = findCallRec(cicInfoItr->second.channelNo);
    if(callItr != checkHashEnd())
    {
        callItr->second.signalingState = STATE_RELEASED_IC;
        sprintf(msgLogger.applMsgTxt,"Received REL message on CIC %d channel %d trunk %d callID %s link %d release cause %d",port,cicInfoItr->second.channelNo,dtk,callItr->second.callId,
                callItr->second.subField,param.causeInd.causeVal);
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_0);
        if(callItr->second.subField >= 0 && callItr->second.subField < MAX_SS7_LINK)
                gCountTraffic[callItr->second.subField]++;
    }
    
}

void process_msg_type_suspend(unsigned char *msg)
{
 unsigned short port,tmp;
    unsigned char dtk;
     tmp = msg[1] & 0x07;
    port=msg[0] | (tmp << 8);
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    sprintf(msgLogger.applMsgTxt,"Received Suspend message on CIC %d trunk %d",port,dtk);
    logDebug(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    //    disconnect_circuit(port,dtk);
}

void process_msg_type_resume(unsigned char *msg)
{
     unsigned short port,tmp;
    unsigned char dtk;
    tmp = msg[1] & 0x07;
    port=msg[0] | (tmp << 8);
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    sprintf(msgLogger.applMsgTxt,"Received Resume message on CIC %d trunk %d",port,dtk);
    logDebug(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_2);
}

void process_msg_type_release_complete(unsigned char *msg)
{
    unsigned short port,tmp;
    unsigned char dtk;
    std::hash_map <int,callRec> :: iterator callItr ;
    std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
    criticalProcessRLCLock.Lock();
    
    tmp = msg[1] & 0x07;
    port=msg[0] | (tmp << 8);
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    /*stopTimer(SS7_T16_TIMER,port);
    stopTimer(SS7_T1_TIMER,port);*/
    cicInfoItr = cicToChannel(port,gLink);
    if(cicInfoItr == cicInfoEnd())
    {
        sprintf(msgLogger.applMsgTxt,"Failed to get channel number  for corresponding CIC No %d link no %d",port,gLink);
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        criticalProcessRLCLock.Unlock();
        return ;
    }
    stopAllTimerOnCIC(cicInfoItr->second.channelNo);
    callItr = findCallRec(cicInfoItr->second.channelNo);
    if(callItr != checkHashEnd())
    {
        sprintf(msgLogger.applMsgTxt,"Received RLC message on CIC %d channel %d trunk %d callID %s link %d",port,cicInfoItr->second.channelNo,dtk,
                callItr->second.callId,callItr->second.subField);
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        callItr->second.signalingState = STATE_RELEASE_COMPLETE_IC;
        if(callItr->second.subField >= 0 && callItr->second.subField < MAX_SS7_LINK)
                gCountTraffic[callItr->second.subField]++;
        //call.erase(callItr);
        removeCallRec(cicInfoItr->second.channelNo);
    }
    criticalProcessRLCLock.Unlock();
}


void process_msg_type_continuity_check_req(unsigned char *msg)
{
}

void process_msg_type_reset_circuit(unsigned char *msg)
{
unsigned short port,tmp;
unsigned short dtk;
Msg_Packet   Ss7TxMsg;
RLC param; //= (RLC *) malloc(sizeof(RLC));
std::hash_map <int,callRec> :: iterator callItr ;
std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
    memset(&param,0,sizeof(RLC));
    tmp = msg[1] & 0x07;
    port=msg[0] | (tmp << 8);
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    cicInfoItr = cicToChannel(port,gLink);
    if(cicInfoItr == cicInfoEnd())
    {
        sprintf(msgLogger.applMsgTxt,"Failed to get channel number  for corresponding CIC No %d link no %d",port,gLink);
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        return ;
    }
    callItr = findCallRec(cicInfoItr->second.channelNo);
    if(callItr != checkHashEnd())
    {
        sprintf(msgLogger.applMsgTxt,"RESET CIRCUIT RECEIVED ON TRUNK %d, CIC %d channel %d callId %s link %d",dtk,port,cicInfoItr->second.channelNo,
                callItr->second.callId,callItr->second.subField);
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        callItr->second.signalingState = STATE_RESET_IC;
        if(callItr->second.subField >= 0 && callItr->second.subField < MAX_SS7_LINK)
                gCountTraffic[callItr->second.subField]++;
    }
    if(conf.signalingServer[0].defaultAction == TRUE)
    {
       isupInitialiseRLC(&param);
       isup_sendRLC(gcic,&param);

    }
    //free(param);
    
}

void process_msg_type_blocking(unsigned char *msg)
{

unsigned short port,tmp,ret=0;
unsigned short dtk;
Msg_Packet   Ss7TxMsg;
RLC param; //= (RLC *) malloc(sizeof(RLC));
std::hash_map <int,callRec> :: iterator callItr ;
std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
stStatEvnt evt;

    memset(&param,0,sizeof(RLC));
    tmp = msg[1] & 0x07;
    port=msg[0] | (tmp << 8);
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    cicInfoItr = cicToChannel(port,gLink);
    if(cicInfoItr == cicInfoEnd())
    {
        
        return ;
    }
    callItr = findCallRec(cicInfoItr->second.channelNo);
    if(callItr != checkHashEnd())
    {
        sprintf(msgLogger.applMsgTxt,"BLOCK CIRCUIT RECEIVED ON TRUNK %d, CIC %d channel %d callId %s link %d",dtk,port,cicInfoItr->second.channelNo,
                callItr->second.callId,callItr->second.subField);
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        callItr->second.signalingState = STATE_BLOCKED_IC;
        
    }
    cicInfoItr->second.channelStatus = OOS;
    memset(&evt,0,sizeof(evt));
    evt.rangStat.stat.pres = FALSE;
    
    ret = statusReq(port,gLink,MSG_TYPE_BLOCKING_ACK,evt);
    if(ret == SUCCESS)
    {
        sprintf(msgLogger.applMsgTxt,"Block ACK send on CIC %d link %d ",port,link);
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    }
    else
    {
        sprintf(msgLogger.applMsgTxt,"Failed to send Block ACK on CIC %d trunk %d Erro no %d Desc %s ",port,dtk,ret,getErrDesc(ret));
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    }
}

void process_msg_type_unblocking(unsigned char *msg)
{
unsigned short port,tmp,ret=0;
unsigned short dtk;
Msg_Packet   Ss7TxMsg;
RLC param; //= (RLC *) malloc(sizeof(RLC));
std::hash_map <int,callRec> :: iterator callItr ;
std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
stStatEvnt evt;

    memset(&param,0,sizeof(RLC));
    tmp = msg[1] & 0x07;
    port=msg[0] | (tmp << 8);
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    cicInfoItr = cicToChannel(port,gLink);
    if(cicInfoItr == cicInfoEnd())
    {
        return ;
    }
    callItr = findCallRec(cicInfoItr->second.channelNo);
    if(callItr != checkHashEnd())
    {
        sprintf(msgLogger.applMsgTxt,"UNBLOCK CIRCUIT RECEIVED ON TRUNK %d, CIC %d channel %d callId %s link %d",dtk,port,cicInfoItr->second.channelNo,
                callItr->second.callId,callItr->second.subField);
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        removeCallRec(port);
    }
    cicInfoItr->second.channelStatus = INS;
    memset(&evt,0,sizeof(evt));
    evt.rangStat.stat.pres = FALSE;
    
    ret = statusReq(port,gLink,MSG_TYPE_UNBLOCKING_ACK,evt);
    if(ret == SUCCESS)
    {
        sprintf(msgLogger.applMsgTxt,"UnBlock ACK send on CIC %d link %d ",port,link);
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    }
    else
    {
        sprintf(msgLogger.applMsgTxt,"Failed to send UnBlock ACK on CIC %d trunk %d Erro no %d Desc %s ",port,dtk,ret,getErrDesc(ret));
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    }

}

void process_msg_type_blocking_ack(unsigned char *msg)
{
unsigned short port,tmp,ret=0;
unsigned short dtk;
Msg_Packet   Ss7TxMsg;
RLC param; //= (RLC *) malloc(sizeof(RLC));
std::hash_map <int,callRec> :: iterator callItr ;
std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
stStatEvnt evt;

    memset(&param,0,sizeof(RLC));
    tmp = msg[1] & 0x07;
    port=msg[0] | (tmp << 8);
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    cicInfoItr = cicToChannel(port,gLink);
    if(cicInfoItr == cicInfoEnd())
    {
        
        return ;
    }
    callItr = findCallRec(cicInfoItr->second.channelNo);
    if(callItr != checkHashEnd())
    {
        sprintf(msgLogger.applMsgTxt,"BLOCK CIRCUIT ACK RECEIVED ON TRUNK %d, CIC %d channel %d callId %s link %d",dtk,port,cicInfoItr->second.channelNo,
                callItr->second.callId,callItr->second.subField);
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        callItr->second.signalingState = STATE_BLOCKED_OG;
        
    }
    else
    {
        createHashRecord(cicInfoItr->second.channelNo);
        callItr = findCallRec(cicInfoItr->second.channelNo);
        if(callItr != checkHashEnd())
        {
            sprintf(msgLogger.applMsgTxt,"BLOCK CIRCUIT ACK RECEIVED ON TRUNK %d, CIC %d channel %d callId %s link %d",dtk,port,cicInfoItr->second.channelNo,
                callItr->second.callId,callItr->second.subField);
            logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
            callItr->second.signalingState = STATE_BLOCKED_OG;
        }
    }
    cicInfoItr->second.channelStatus = OOS;

}

void process_msg_type_unblocking_ack(unsigned char *msg)
{
unsigned short port,tmp,ret=0;
unsigned short dtk;
Msg_Packet   Ss7TxMsg;
RLC param; //= (RLC *) malloc(sizeof(RLC));
std::hash_map <int,callRec> :: iterator callItr ;
std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
stStatEvnt evt;

    memset(&param,0,sizeof(RLC));
    tmp = msg[1] & 0x07;
    port=msg[0] | (tmp << 8);
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    cicInfoItr = cicToChannel(port,gLink);
    if(cicInfoItr == cicInfoEnd())
    {
        return ;
    }
    callItr = findCallRec(cicInfoItr->second.channelNo);
    if(callItr != checkHashEnd())
    {
        sprintf(msgLogger.applMsgTxt,"UNBLOCK CIRCUIT RECEIVED ON TRUNK %d, CIC %d channel %d callId %s link %d",dtk,port,cicInfoItr->second.channelNo,
                callItr->second.callId,callItr->second.subField);
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        removeCallRec(port);
    }
    cicInfoItr->second.channelStatus = INS;
}

void process_msg_type_circuit_group_reset(unsigned char *msg)
{
unsigned short port,tmp,counter = 0,i = 0,ret =0,len = 0;
unsigned short dtk;
std::hash_map <int,callRec> :: iterator callItr;
std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
stStatEvnt evt;
Msg_Packet   Ss7TxMsg;
unsigned char range = 0x00;

        tmp = msg[1]&0x1F;
        port= msg[0] | (tmp << 8) ;
        dtk=(msg[0] >> 5) | (msg[1] << 3);
        cicInfoItr = cicToChannel(port,gLink);
        if(cicInfoItr == cicInfoEnd())
        {
            return;
        }
        range = msg[5];
        sprintf(msgLogger.applMsgTxt,"GROUP RESET RECEIVED ON TRUNK = %d, PORT = %d range = %d",dtk,port,range);
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        memset(&tmp,0,sizeof(tmp));
        tmp = cicInfoItr->second.channelNo;
        sprintf(msgLogger.applMsgTxt,"Group reset successful on CICs : ");
        for(i =0; i <= range;i++)
        {
            cicInfoItr = cicToChannel(port+i,gLink);
            if(tmp% 32 != 0 && isSignalingCIC(port+i) != 0 )
            {
                callItr = findCallRec(tmp);
                if(callItr != checkHashEnd())
                    removeCallRec(tmp);
                sprintf(&msgLogger.applMsgTxt[strlen(msgLogger.applMsgTxt)]," %d",port+i);
                printf("Group reset cic %d \n",tmp);
            }
            if(cicInfoItr == cicInfoEnd())
                cicInfoItr->second.channelStatus = INS;
            
            tmp++;
        }
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_2);
        
        /*if((range % 8) != 0)
        {
                len = (range/8)+1;
        }
        else
                len = range/8;
       // Ss7TxMsg.length=(FIXED_SIZE+8) << 8;
        Ss7TxMsg.opcode=0;
        Ss7TxMsg.subfield=0;
        Ss7TxMsg.debug=0;
        counter = 0;
        Ss7TxMsg.msg[counter++]=port & 0xFF;
        Ss7TxMsg.msg[counter++]=(port >> 8) & 0x07;
        Ss7TxMsg.msg[counter++]=MSG_TYPE_CIRCUIT_GROUP_RESET_ACK;
        Ss7TxMsg.msg[counter++]=1;
        Ss7TxMsg.msg[counter++]=len +1;
        Ss7TxMsg.msg[counter++]=range;
        for( i = 0; i < len ; i++)
                Ss7TxMsg.msg[counter++]=0x00;
        Ss7TxMsg.length=(FIXED_SIZE+counter) << 8;

        send_msg(&Ss7TxMsg);
        sprintf(msgLogger.applMsgTxt,"SENT GROUP RESET ACK ON TRUNK %d, PORT %d range %d\n",dtk,port,range);
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);*/
        memset(&evt,0,sizeof(evt));
        evt.rangStat.stat.pres = TRUE;
        evt.rangStat.range = range;
        ret = statusReq(port,gLink,MSG_TYPE_CIRCUIT_GROUP_RESET_ACK,evt);
        if(ret == SUCCESS)
        {
            sprintf(msgLogger.applMsgTxt,"Group reset ACK send on CIC %d link %d range %d",port,link,range);
            logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        }
        else
        {
            sprintf(msgLogger.applMsgTxt,"Failed to send group reset ACK on CIC %d trunk %d Erro no %d Desc %s ",port,dtk,ret,getErrDesc(ret));
            logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        }
}

void process_msg_type_group_block(unsigned char *msg)
{
printf("Group block received on trunk = %d, port = %d\n",msg[1],msg[0]);
unsigned short port,tmp,counter = 0,i = 0,len = 0,ret =0;
unsigned short dtk;
std::hash_map <int,callRec> :: iterator callItr;
std::hash_map <int,callRec> :: iterator callItr_temp;
std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
Msg_Packet   Ss7TxMsg;
unsigned char range = 0x00;
stStatEvnt evt;

    tmp = msg[1]&0x1F;
    port= msg[0] | (tmp << 8) ;
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    range = msg[6];
    
    sprintf(msgLogger.applMsgTxt,"GROUP BLOCK  RECEIVED ON TRUNK = %d, PORT = %d range = %d",dtk,port,range);
    logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    cicInfoItr = cicToChannel(port,gLink);
    if(cicInfoItr == cicInfoEnd())
    {
        return;
    }
    memset(&tmp,0,sizeof(tmp));
    tmp = cicInfoItr->second.channelNo;
    sprintf(msgLogger.applMsgTxt,"Group BLOCK  received  on CICs : ");
    for(i =0; i <= range;i++)
    {
        cicInfoItr = cicToChannel(port+i,gLink);
        if(tmp% 32 != 0 && isSignalingCIC(port+i) != 0 )
        {
            callItr = findCallRec(tmp);
            if(callItr == checkHashEnd())
            {
                //removeCallRec(tmp);
                sprintf(&msgLogger.applMsgTxt[strlen(msgLogger.applMsgTxt)]," %d",port+i);
                printf("Group BLOCK cic %d \n",tmp);
                createHashRecord(tmp);
                callItr = findCallRec(tmp);
            }
            if(callItr != checkHashEnd())
                callItr->second.signalingState = STATE_BLOCKED_IC;
            if(cicInfoItr == cicInfoEnd())
                cicInfoItr->second.channelStatus = OOS;

        }
        tmp++;
    }
    
    logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_2);
    memset(&evt,0,sizeof(evt));
    evt.rangStat.stat.pres = TRUE;
    evt.rangStat.range = range;
    ret = statusReq(port,gLink,MSG_TYPE_CIRCUIT_GROUP_BLOCK_ACK,evt);
    if(ret == SUCCESS)
    {
        sprintf(msgLogger.applMsgTxt,"Group Block ACK send on CIC %d link %d range %d",port,link,range);
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    }
    else
    {
        sprintf(msgLogger.applMsgTxt,"Failed to send group Block ACK on CIC %d trunk %d Erro no %d Desc %s ",port,dtk,ret,getErrDesc(ret));
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    }
}


void process_msg_type_group_unblock(unsigned char *msg)
{
printf("Group Unblock received on trunk = %d, port = %d\n",msg[1],msg[0]);
unsigned short port,tmp,counter = 0,ret =0,i = 0,len = 0;
unsigned short dtk;
std::hash_map <int,callRec> :: iterator callItr;
std::hash_map <int,callRec> :: iterator callItr_temp;
std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
Msg_Packet   Ss7TxMsg;
unsigned char range = 0x00;
stStatEvnt evt;

    tmp = msg[1]&0x1F;
    port= msg[0] | (tmp << 8) ;
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    range = msg[6];
    sprintf(msgLogger.applMsgTxt,"GROUP UnBLOCK  RECEIVED ON TRUNK = %d, PORT = %d range = %d",dtk,port,range);
    logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    memset(&tmp,0,sizeof(tmp));
    cicInfoItr = cicToChannel(port,gLink);
    if(cicInfoItr == cicInfoEnd())
    {
        return;
    }
    tmp = cicInfoItr->second.channelNo;
    sprintf(msgLogger.applMsgTxt,"Group UnBLOCK  received  on CICs : ");
    for(i =0; i <= range;i++)
    {
        cicInfoItr = cicToChannel(port+i,gLink);
        if(tmp% 32 != 0 &&  isSignalingCIC(port+i) != 0  )
        {
            callItr = findCallRec(tmp);
            if(callItr != checkHashEnd())
            {
                removeCallRec(tmp);
                sprintf(&msgLogger.applMsgTxt[strlen(msgLogger.applMsgTxt)]," %d",tmp);
                printf("Group UnBLOCK cic %d \n",tmp);

            }
            if(cicInfoItr == cicInfoEnd())
                cicInfoItr->second.channelStatus = INS;
        }
        tmp++;
    }
    logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_2);
    memset(&evt,0,sizeof(evt));
    evt.rangStat.stat.pres = TRUE;
    evt.rangStat.range = range;
    ret = statusReq(port,gLink,MSG_TYPE_CIRCUIT_GROUP_UNBLOCK_ACK,evt);
    if(ret == SUCCESS)
    {
        sprintf(msgLogger.applMsgTxt,"Group Unblock ACK send on CIC %d link %d range %d",port,link,range);
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    }
    else
    {
        sprintf(msgLogger.applMsgTxt,"Failed to send group Unblock ACK on CIC %d trunk %d Erro no %d Desc %s ",port,dtk,ret,getErrDesc(ret));
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    }
}

void process_msg_type_group_block_ack(unsigned char *msg)
{
unsigned short port,tmp,counter = 0,i = 0,len = 0;
unsigned short dtk;
std::hash_map <int,callRec> :: iterator callItr;
std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
Msg_Packet   Ss7TxMsg;
unsigned char range = 0x00;

    tmp = msg[1]&0x1F;
    port= msg[0] | (tmp << 8) ;
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    range = msg[5];
    sprintf(msgLogger.applMsgTxt,"GROUP BLOCK ACK RECEIVED ON TRUNK = %d, PORT = %d range = %d",dtk,port,range);
    logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    memset(&tmp,0,sizeof(tmp));
    cicInfoItr = cicToChannel(port,gLink);
    if(cicInfoItr == cicInfoEnd())
    {
        return;
    }
    tmp = cicInfoItr->second.channelNo;
    sprintf(msgLogger.applMsgTxt,"Group BLOCK ACK successful on CICs : ");
    for(i =0; i <= range;i++)
    {
        cicInfoItr = cicToChannel(port+i,gLink);
        if(tmp% 32 != 0 && isSignalingCIC(port+i) != 0 )
        {
            callItr = findCallRec(tmp);
            if(callItr == checkHashEnd())
            {
                //removeCallRec(tmp);
                sprintf(&msgLogger.applMsgTxt[strlen(msgLogger.applMsgTxt)]," %d",port+i);
                printf("Group BLOCK cic %d \n",tmp);
                createHashRecord(tmp);
                callItr = findCallRec(tmp);
            }
            if(callItr != checkHashEnd())
                callItr->second.signalingState = STATE_BLOCKED_OG;
            if(cicInfoItr == cicInfoEnd())
                cicInfoItr->second.channelStatus = OOS;

        }
        tmp++;
    }
    logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_2);
}



void process_msg_type_group_unblock_ack(unsigned char *msg)
{
unsigned short port,tmp,counter = 0,i = 0,len = 0;
unsigned short dtk;
std::hash_map <int,callRec> :: iterator callItr;
std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
Msg_Packet   Ss7TxMsg;
unsigned char range = 0x00;

    tmp = msg[1]&0x1F;
    port= msg[0] | (tmp << 8) ;
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    range = msg[5];
    sprintf(msgLogger.applMsgTxt,"GROUP UNBLOCK ACK RECEIVED ON TRUNK = %d, PORT = %d range = %d",dtk,port,range);
    logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    memset(&tmp,0,sizeof(tmp));
    cicInfoItr = cicToChannel(port,gLink);
    if(cicInfoItr == cicInfoEnd())
    {
        return;
    }
    tmp = cicInfoItr->second.channelNo;
    sprintf(msgLogger.applMsgTxt,"Group UNBLOCK ACK successful on CICs : ");
    for(i =0; i <= range;i++)
    {
         cicInfoItr = cicToChannel(port+i,gLink);
        if(tmp% 32 != 0 && isSignalingCIC(port+i) != 0 )
        {
            callItr = findCallRec(tmp);
            if(callItr != checkHashEnd())
                removeCallRec(tmp);
            sprintf(&msgLogger.applMsgTxt[strlen(msgLogger.applMsgTxt)]," %d",tmp);
            printf("Group unblock cic %d \n",tmp);
        }
        if(cicInfoItr == cicInfoEnd())
            cicInfoItr->second.channelStatus = INS;
        tmp++;
    }
    logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_2);
}


void process_msg_type_facility_request(unsigned char *msg)
{
}

void process_msg_type_facility_accepted(unsigned char *msg)
{
}

void process_msg_type_facility_reject(unsigned char *msg)
{
}

void process_msg_type_loopback_ack(unsigned char *msg)
{
}

void process_msg_type_passalong(unsigned char *msg)
{
}

void process_msg_type_circuit_group_reset_ack(unsigned char *msg)
{
unsigned short port,tmp,counter = 0,i = 0,len = 0;
unsigned short dtk;
std::hash_map <int,callRec> :: iterator callItr;
std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
Msg_Packet   Ss7TxMsg;
unsigned char range = 0x00;

    tmp = msg[1]&0x1F;
    port= msg[0] | (tmp << 8) ;
    dtk=(msg[0] >> 5) | (msg[1] << 3);
    range = msg[5];
    sprintf(msgLogger.applMsgTxt,"GROUP RESET ACK RECEIVED ON TRUNK = %d, PORT = %d range = %d",dtk,port,range);
    logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    memset(&tmp,0,sizeof(tmp));
    cicInfoItr = cicToChannel(port,gLink);
    if(cicInfoItr == cicInfoEnd())
    {
        return;
    }
    tmp = cicInfoItr->second.channelNo;
    sprintf(msgLogger.applMsgTxt,"Group reset ACK successful on CICs : ");
    for(i =0; i <= range;i++)
    {
        cicInfoItr = cicToChannel(port+i,gLink);
        if(tmp% 32 != 0 && isSignalingCIC(port+i) != 0 )
        {
            callItr = findCallRec(tmp);
            if(callItr != checkHashEnd())
                removeCallRec(tmp);
            sprintf(&msgLogger.applMsgTxt[strlen(msgLogger.applMsgTxt)]," %d",tmp);
            printf("Group reset cic %d \n",tmp);
        }
        if(cicInfoItr != cicInfoEnd())
            cicInfoItr->second.channelStatus = INS;
        tmp++;
    }
    logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_2);
}

void process_msg_type_circuit_group_query(unsigned char *msg)
{
}

void process_msg_type_circuit_group_query_resp(unsigned char *msg)
{
}

void process_msg_type_call_progress(unsigned char *msg)
{
        unsigned short port,tmp;
        unsigned short dtk;
        tmp = msg[1] & 0x07;
        port=msg[0] | (tmp << 8);
        dtk=(msg[0] >> 5) | (msg[1] << 3);
        //DTK = dtk;
}

void process_msg_type_user_to_user_information(unsigned char *msg)
{
}

void process_msg_type_unquipped_cic(unsigned char *msg)
{
}

void process_msg_type_confusion(unsigned char *msg)
{
}

void process_msg_type_overload(unsigned char *msg)
{
}

void process_msg_type_charge_information(unsigned char *msg)
{
}

void process_msg_type_network_resource_management(unsigned char *msg)
{
}

void process_msg_type_type_facility(unsigned char *msg)
{
}

void process_msg_type_user_part_test(unsigned char *msg)
{
}

void process_msg_type_user_part_available(unsigned char *msg)
{
}

void process_msg_type_identification_req(unsigned char *msg)
{
}

void process_msg_type_identification_rest(unsigned char *msg)
{
}

void process_msg_type_type_segmentation(unsigned char *msg)
{
}


int isup_getSignalingState(int cic)
{
    std::hash_map <int,callRec> :: iterator callItr ;
    if(cicInfo[cic].channelStatus == OOS) //out of service
        return UNKNOWN;
    callItr = findCallRec(cic);
    if( callItr != checkHashEnd())
        return callItr->second.signalingState;
   return IDLE;
    
}

int isup_getMediaState(int cic)
{
    std::hash_map <int,callRec> :: iterator callItr ;
    if(cicInfo[cic].channelStatus == OOS)
        return UNKNOWN;
    callItr = findCallRec(cic);
    if( callItr != checkHashEnd())
        return callItr->second.mediaSate;
   
    return IDLE;
    
}

int isup_getchannelStatus(int cic)
{
    return cicInfo[cic].channelStatus;
}

int isup_getlinkStatus(int link)
{
    if(link > 0 && link <= MAX_SS7_LINK)
    {
        if(SS7LinkStatus[link - 1] == 0x01)
            return INS;
        else
            return OOS;
    }
    else
        return OUT_OF_RANGE;
    
}

isupSignalConfig jcti_getSignalingServerSettings()
{
    return conf.signalingServer[0];
}

int isup_changeSignalingServerSettings(jctiSignalConfig config)
{
    conf.signalingServer[0].defaultAction = config.defaultAction;
    return SUCCESS;
}

char * getCRN(unsigned short cic)
{
    std::hash_map <int,callRec> :: iterator callItr ;
    callItr = findCallRec(cic);
    if( callItr == checkHashEnd())
    {
        return NULL;
    }
    else
        return (callItr->second.callId);
}

int isup_getIAMParam(unsigned char msg[1000],IAM *param)
{
    std::hash_map <int,callRec> :: iterator callItr ;
    //pthread_mutex_lock(&lockthread);
    unsigned short cic,ret,tmp;
    parseIAMLock.Lock();
    tmp = msg[1] & 0x07;
    cic = msg[0] | (tmp << 8);
    //cic=msg[0]&0x1F;
    callItr = findCallRec(cic);
    if( callItr == checkHashEnd())
    {
        parseIAMLock.Unlock();
        return INVALID_STATE;
    }
    if(msg[2] != MSG_TYPE_INITIAL_ADDRESS )
    {
        parseIAMLock.Unlock();
        return INCORRECT_MSG_TYPE;
    }
    
    if(param == NULL)
    {
        parseIAMLock.Unlock();
        return EINVAL;
    }
    
    ret = parseIAM(&msg[2],param) ;
    if(ret != SUCCESS)
    {
        //to do release call with proper cause codes
        if(conf.signalingServer[0].defaultAction == TRUE)
        {
            if(param->cgpn.addrSig != NULL)
            {
                //todo release call
            }
        }
    }
    //    printf("Failed parseIAM %d %s",ret,getErrDesc(ret));
    if(param->cgpn.addrSig != NULL)
        strcpy(callItr->second.cgpn,param->cgpn.addrSig);
    if(param->cdpn.addrSig != NULL)
        strcpy(callItr->second.cdpn,param->cdpn.addrSig);
    //pthread_mutex_unlock(&lockthread);
    parseIAMLock.Unlock();
    return SUCCESS;
    
}

char * isup_getCallingPartyNumber(unsigned short cic)
{
    std::hash_map <int,callRec> :: iterator callItr ;
    callItr = findCallRec(cic);
    if( callItr == checkHashEnd())
    {
        return NULL;
    }
    else
        return (callItr->second.cgpn);
}

char * isup_getCalledPartyNumber(unsigned short cic)
{
    std::hash_map <int,callRec> :: iterator callItr ;
    callItr = findCallRec(cic);
    if( callItr == checkHashEnd())
    {
        return NULL;
    }
    else
        return (callItr->second.cdpn);
}

int isup_getACMParam(unsigned char msg[1000],ACM *param)
{
    std::hash_map <int,callRec> :: iterator callItr ;
    unsigned short cic,tmp;
    parseACMLock.Lock();
    tmp = msg[1] & 0x07;
    cic = msg[0] | (tmp << 8);
    callItr = findCallRec(cic);
    if( callItr == checkHashEnd())
    {
        parseACMLock.Unlock();
        return INVALID_STATE;
    }
    if(msg[2] != MSG_TYPE_ANSWER )
    {
        parseACMLock.Unlock();
        return INCORRECT_MSG_TYPE;
    }
    
    parseACM(&msg[2],param);
    parseACMLock.Unlock();
    return SUCCESS;
}

int isup_getANMParam(unsigned char msg[1000],ANM *param)
{
    std::hash_map <int,callRec> :: iterator callItr ;
    unsigned short cic,tmp;
    parseANMLock.Lock();
   tmp = msg[1] & 0x07;
    cic = msg[0] | (tmp << 8);
    callItr = findCallRec(cic);
    if( callItr == checkHashEnd())
    {
        parseANMLock.Unlock();
        return INVALID_STATE;
    }
    if(msg[2] != MSG_TYPE_ANSWER )
    {
        parseANMLock.Unlock();
        return INCORRECT_MSG_TYPE;
    }
    
    parseANM(&msg[2],param);
    
    parseANMLock.Unlock();
    return SUCCESS;
}

int isup_getRELParam(unsigned char msg[1000],REL *param)
{
    std::hash_map <int,callRec> :: iterator callItr ;
    unsigned short cic,tmp;
    parseRELLock.Lock();
    tmp = msg[1] & 0x07;
    cic = msg[0] | (tmp << 8);
    callItr = findCallRec(cic);
    if( callItr == checkHashEnd())
    {
        parseRELLock.Unlock();
        return INVALID_STATE;
    }
    if(msg[2] != MSG_TYPE_RELEASE )
    {
        parseRELLock.Unlock();
        return INCORRECT_MSG_TYPE;
    }
    
    parseREL(&msg[2],param);
    parseRELLock.Unlock();
    return SUCCESS;
}

int isup_getRLCParam(unsigned char msg[1000],RLC *param)
{
    unsigned short cic,tmp;
    std::hash_map <int,callRec> :: iterator callItr ;
    parseRLCLock.Lock();
    tmp = msg[1] & 0x07;
    cic = msg[0] | (tmp << 8);
    callItr = findCallRec(cic);
    if( callItr == checkHashEnd())
    {
        parseRLCLock.Unlock();
        return INVALID_STATE;
    }
    if(msg[2] != MSG_TYPE_RELEASE )
    {
        parseRLCLock.Unlock();
        return INCORRECT_MSG_TYPE;
    }
    
    parseRLC(&msg[2],param);
    parseRLCLock.Unlock();
    return SUCCESS;
}

int isupInitialiseIAM(IAM  *param,BYTE switchType)
{
    int ret;
    initIAMLock.Lock();
    ret = initialiseIAM(param,switchType);
    initIAMLock.Unlock();
    return ret;
}

int isupInitialiseRLC(RLC  *param,BYTE switchType)
{
    int ret;
    initRLCLock.Lock();
    ret = initialiseRLC(param,switchType);
    initRLCLock.Unlock();
    return ret;
}

int isupInitialiseACM(ACM  *param,BYTE switchType)
{
    int ret;
    initACMLock.Lock();
    ret = initialiseACM(param,switchType);
    initACMLock.Unlock();
    return ret;
}

int isupInitialiseANM(ANM  *param,BYTE switchType)
{
    int ret;
    initANMLock.Lock();
    ret = initialiseANM(param,switchType);
    initANMLock.Unlock();
    return ret;
}

int isupInitialiseREL(REL  *param,BYTE cause,BYTE switchType)
{
    int ret;
    initRELLock.Lock();
    ret = initialiseREL(param,cause,switchType);
    initRELLock.Unlock();
    return ret;
}


int isup_Dial(unsigned short chanNo, IAM * param,char *source_number, char *dest_number)
{
    int ret,i=0 ;
    time_t timerCur;
    unsigned char link;
    std::hash_map <int,callRec> :: iterator callItr ;
    std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
    criticalIAMLock.Lock();
    time(&timerCur);
    
    if(source_number == NULL || dest_number == NULL || param == NULL)
    {
        criticalIAMLock.Unlock();
        return EINVAL;
    }
    
    if(strlen(source_number) == 0 || 
            strlen(dest_number) == 0)
    {
        criticalIAMLock.Unlock();
        return EINVAL;
    }
        
    if(difftime(timerCur,timerTraffic) > 1 )
    {
        for(i=0; i < MAX_SS7_LINK; i++)
                gCountTraffic[i] = 0;
        time(&timerTraffic);
    }
    
    cicInfoItr = channelToCic(chanNo);
    if(cicInfoItr == cicInfoEnd())
    {
        criticalIAMLock.Unlock();
        return OUT_OF_RANGE;
    }
        
    if((ret = checkCICStatus(chanNo,&link)) != SUCCESS)
    {
        if(ret != SWITCH_CONGESTION)
        {
            sprintf(msgLogger.applMsgTxt,"Invalid CIC status CIC %d channel %d Erro no %d Desc %s ",cicInfoItr->second.origcic,chanNo,ret,getErrDesc(ret));
            logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        }
        criticalIAMLock.Unlock();
        return ret;
    }
       
    if( (ret = createHashRecord(chanNo)) != SUCCESS)
    {
        criticalIAMLock.Unlock();
        return ret;
        
    }
       
    callItr = findCallRec(chanNo);
    if( callItr == checkHashEnd())
    {
        criticalIAMLock.Unlock();
        return INVALID_STATE;
    }
    
    
    
    ret = send_iam(cicInfoItr->second.origcic,param,source_number,dest_number,link);
    if(ret == SUCCESS)
    {
       stopAllTimerOnCIC(chanNo);
       startTimer(SS7_T7_TIMER,chanNo);
       callItr->second.signalingState = STATE_OFFERED_OG;      
       sprintf(msgLogger.applMsgTxt,"Send IAM on CIC %d channel %d callID %s link %d cgpn %s cdpn %s",cicInfoItr->second.origcic,chanNo,callItr->second.callId,link,source_number,dest_number);
       logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
       gCountTraffic[link]++;
         
    }
    else
    {
        sprintf(msgLogger.applMsgTxt,"Failed to send IAM on CIC %d channel %d  Erro no %d Desc %s ",cicInfoItr->second.origcic,chanNo, ret,getErrDesc(ret));
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    }
    criticalIAMLock.Unlock();
  
    return ret;
}




int isup_Answer(unsigned short chanNo,ANM *param)
{
    int ret;
    unsigned char link;
    std::hash_map <int,callRec> :: iterator callItr ;
    std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
    criticalANMLock.Lock();
    cicInfoItr = channelToCic(chanNo);
    if(cicInfoItr == cicInfoEnd())
    {
        criticalANMLock.Unlock();
        return OUT_OF_RANGE;
    }
    
    if((ret = checkCICStatus(chanNo,&link)) != SUCCESS)
    {
       sprintf(msgLogger.applMsgTxt,"Invalid CIC status CIC %d channel %d Erro no %d Desc %s ",cicInfoItr->second.origcic,chanNo,ret,getErrDesc(ret));
       logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
       if(ret != SWITCH_CONGESTION)
       {
            criticalANMLock.Unlock();
            return ret;     
       }
      
    }
    callItr = findCallRec(chanNo);
    if( callItr == checkHashEnd())
    {
        criticalANMLock.Unlock();
        return INVALID_STATE;
    }
    
  
    ret = send_anm(cicInfoItr->second.origcic,param,link);
    if(ret == SUCCESS)
    {
       callItr->second.signalingState = STATE_ANSWERED_OG;
       sprintf(msgLogger.applMsgTxt,"Send ANM on CIC %d  channel %d callID %s link %d",cicInfoItr->second.origcic,chanNo,callItr->second.callId,link );
       logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
       gCountTraffic[link]++;
    }
    else
    {
        sprintf(msgLogger.applMsgTxt,"Failed to send ANM on CIC %d channel %d Erro no %d Desc %s ",cicInfoItr->second.origcic,chanNo, ret,getErrDesc(ret));
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    }
    criticalANMLock.Unlock();
    return ret;
}

int isup_Disconnect(unsigned short chanNo,REL *param)
{
    int ret;
    unsigned char link;
    std::hash_map <int,callRec> :: iterator callItr ;
    std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
 
    
    criticalRELLock.Lock();
    callItr = findCallRec(chanNo);
    if( callItr == checkHashEnd())
    {
        criticalRELLock.Unlock();
        return INVALID_STATE;
    }
    
    cicInfoItr = channelToCic(chanNo);
    if(cicInfoItr == cicInfoEnd())
    {
        criticalRELLock.Unlock();
        return OUT_OF_RANGE;
    }
   if((ret = checkCICStatus(chanNo,&link)) != SUCCESS)
    {
       sprintf(msgLogger.applMsgTxt,"Invalid CIC status CIC %d channel %d Erro no %d Desc %s ",cicInfoItr->second.origcic,chanNo,ret,getErrDesc(ret));
       logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
       if(ret == E1_DOWN || ret == MEDIA_DOWN || ret == SIG_LINK_DOWN)
       {
            if(callItr->second.timerRetry == 0)
                startTimer(SS7_T5_TIMER,chanNo);
             startTimer(SS7_T1_TIMER,chanNo);
       }
       if(ret != SWITCH_CONGESTION)
       {
            criticalRELLock.Unlock();
            return ret;
       }
    }
    
    ret = send_rel(chanNo,param,link);
    if(ret == SUCCESS)
    {
        
        if(callItr->second.timerRetry == 0)
            startTimer(SS7_T5_TIMER,chanNo);
        startTimer(SS7_T1_TIMER,chanNo);
        callItr->second.signalingState = STATE_RELEASED_OG;
        sprintf(msgLogger.applMsgTxt,"Send REL on CIC %d channel %d callId %s link %d",cicInfoItr->second.origcic,chanNo,callItr->second.callId,link );
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        gCountTraffic[link]++;
    }
    else
    {
        sprintf(msgLogger.applMsgTxt,"Failed to send REL on CIC %d channel %d Erro no %d Desc %s ",cicInfoItr->second.origcic,chanNo,ret,getErrDesc(ret));
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    }
    criticalRELLock.Unlock();
    return ret;
}

int isup_sendACM(unsigned short chanNo, ACM *param)
{
    int ret;
    unsigned char link;
    std::hash_map <int,callRec> :: iterator callItr ;
    std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
    criticalACMLock.Lock();
    cicInfoItr = channelToCic(chanNo);
    if(cicInfoItr == cicInfoEnd())
    {
        criticalACMLock.Unlock();
        return OUT_OF_RANGE; 
    }
    
    if((ret = checkCICStatus(chanNo,&link)) != SUCCESS)
    {
       sprintf(msgLogger.applMsgTxt,"Invalid CIC status CIC %d channel %d Erro no %d Desc %s ",cicInfoItr->second.origcic,chanNo,ret,getErrDesc(ret));
       logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
       if(ret != SWITCH_CONGESTION)
       {
            criticalACMLock.Unlock();
            return ret;
       }
    }
    callItr = findCallRec(chanNo);
    if( callItr == checkHashEnd())
    {
        criticalACMLock.Unlock();
        return INVALID_STATE;
    }
    if(callItr->second.signalingState != STATE_OFFERED_IC)
    {
        criticalACMLock.Unlock();
        return INVALID_STATE;
    }
    
    
    
    ret = send_acm(cicInfoItr->second.origcic,param,link);
    if(ret == SUCCESS)
    {
       callItr->second.signalingState = STATE_RINGING_OG;
       sprintf(msgLogger.applMsgTxt,"Send ACM for callId %s CIC %d channel %d link %d",callItr->second.callId,cicInfoItr->second.origcic,chanNo,link );
       logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
       gCountTraffic[link]++;
    }
    else
    {
        sprintf(msgLogger.applMsgTxt,"Failed to send ACM on CIC %d channel %d Erro no %d Desc %s ",cicInfoItr->second.origcic,chanNo,ret,getErrDesc(ret));
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    }
    criticalACMLock.Unlock();
    return ret;
}


int isup_sendRLC(unsigned short chanNo,RLC *param)
{
    int ret;
    unsigned char link;
    std::hash_map <int,callRec> :: iterator callItr ;
    std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
    criticalRLCLock.Lock();
    callItr = findCallRec(chanNo);
    if( callItr == checkHashEnd())
    {
        criticalRLCLock.Unlock();
        return INVALID_STATE;
    }
    cicInfoItr = channelToCic(chanNo);
    if(cicInfoItr == cicInfoEnd())
    {
        criticalRLCLock.Unlock();
        return OUT_OF_RANGE; 
    }
    if((ret = checkCICStatus(chanNo,&link)) != SUCCESS)
    {
        sprintf(msgLogger.applMsgTxt,"Invalid CIC status CIC %d channel %d Erro no %d Desc %s  ",cicInfoItr->second.origcic,chanNo,ret,getErrDesc(ret));
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        //stopAllTimerOnCIC(cic);
        // callItr->second.signalingState = STATE_RELEASE_COMPLETE_OG;
        //call.erase(callItr);
        // removeCallRec(cic);
       if(ret != SWITCH_CONGESTION)
       {
            criticalRLCLock.Unlock();
            return ret;
       }
    }
  
    switch(callItr->second.signalingState)
    {
        case STATE_RELEASED_IC:
        case STATE_RESET_IC:
            break;
        default:
            criticalRLCLock.Unlock();
            return INVALID_STATE;
    }
    
    ret = send_rlc(cicInfoItr->second.origcic,param,link);
    if(ret == SUCCESS)
    {
       sprintf(msgLogger.applMsgTxt,"Send RLC on CIC %d channel No %d callID %s link %d",cicInfoItr->second.origcic,chanNo,callItr->second.callId,link );
       logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
       stopAllTimerOnCIC(chanNo);
       callItr->second.signalingState = STATE_RELEASE_COMPLETE_OG;
       //call.erase(callItr);
       removeCallRec(chanNo);
       gCountTraffic[link]++;
    }
    else
    {
        sprintf(msgLogger.applMsgTxt,"Failed to send RLC on CIC %d channel no %d Erro no %d Desc %s ",cicInfoItr->second.origcic,chanNo,ret,getErrDesc(ret));
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    }
  
    criticalRLCLock.Unlock();
    return ret;
}



int isup_resetCircuit(unsigned short chanNo)
{
    int ret;
    unsigned char link;
    stTimer t;
    std::hash_map <int,callRec> :: iterator callItr ;
    std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
    unsigned short trunk;
    criticalResetLock.Lock();
    trunk = cicTotrunk(chanNo);
    
    /*if((ret = checkTrunkStatus(trunk,&link)) != SUCCESS)
    {
        sprintf(msgLogger.applMsgTxt,"Reset Failed on CIC = %d trunk %d Erro no %d Desc %s ",cic, trunk,ret,getErrDesc(ret));
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        criticalResetLock.Unlock();
        return ret;
    }*/
    cicInfoItr = channelToCic(chanNo);
    if(cicInfoItr == cicInfoEnd())
    {
        criticalResetLock.Unlock();
        return OUT_OF_RANGE; 
    }
    
    if((ret = checkCICStatus(chanNo,&link)) != SUCCESS)
    {
       sprintf(msgLogger.applMsgTxt,"Invalid CIC status CIC %d channel %d Erro no %d Desc %s ",cicInfoItr->second.origcic,chanNo,ret,getErrDesc(ret));
       logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
       if(ret == E1_DOWN || ret == MEDIA_DOWN || ret == SIG_LINK_DOWN)
       {
            callItr = findCallRec(chanNo);
            if( callItr != checkHashEnd())
            {
                callItr->second.signalingState = STATE_RESET_OG;
                if(callItr->second.timerRetry == 0)
                        startTimer(SS7_T17_TIMER,chanNo);
                //call.erase(callItr);
            }
            startTimer(SS7_T16_TIMER,chanNo);
       }
       
       if(ret != SWITCH_CONGESTION )
       {
            if(ret == OUT_OF_SERVICE)
            {
                callItr = findCallRec(chanNo);
                if( callItr != checkHashEnd())
                {
                    if(callItr->second.signalingState == STATE_BLOCKED_IC || callItr->second.signalingState == STATE_BLOCKED_OG)
                    {
                         criticalResetLock.Unlock();
                         return ret;
                    }
                    
                }
            }
            else
            {
                criticalResetLock.Unlock();
                return ret;
            }
       }
    }
  
    callItr = findCallRec(chanNo);
    if( callItr == checkHashEnd())
    {
        if(createHashRecord(chanNo) != SUCCESS)
        {
             criticalResetLock.Unlock();
              return INVALID_STATE;
        }
    }
    ret = reset_circuit(cicInfoItr->second.origcic,link);
    if(ret == SUCCESS)
    {
        /*t.id = cic;
        t.source = SS7_SIG;
        t.type = SS7_T16_TIMER;
        addTimer(t);*/
        //stopTimer(SS7_T16_TIMER,cic);
        sprintf(msgLogger.applMsgTxt,"Reset send on channel %d cic %d link %d",chanNo,cicInfoItr->second.origcic,link);
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        //callItr = findCallRec(cic);
        gCountTraffic[link]++;
      
    }
    else
    {
        sprintf(msgLogger.applMsgTxt,"Failed to send reset on CIC %d trunk %d Erro no %d Desc %s ",chanNo, trunk,ret,getErrDesc(ret));
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    }
    callItr = findCallRec(chanNo);
    if( callItr != checkHashEnd())
    {
        callItr->second.signalingState = STATE_RESET_OG;
        if(callItr->second.timerRetry == 0)
                startTimer(SS7_T17_TIMER,chanNo);
        //call.erase(callItr);
    }
    startTimer(SS7_T16_TIMER,chanNo);
    criticalResetLock.Unlock();
    return ret;
}


int isup_blockCircuit(unsigned short chanNo)
{
    int ret;
    unsigned char link;
    stTimer t;
    std::hash_map <int,callRec> :: iterator callItr ;
    std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
    unsigned short trunk;
    stStatEvnt evt;
 
    criticalBlockLock.Lock();
    memset(&evt,0,sizeof(stStatEvnt));
    trunk = cicTotrunk(chanNo);
    
    /*if((ret = checkTrunkStatus(trunk,&link)) != SUCCESS)
    {
        sprintf(msgLogger.applMsgTxt,"Reset Failed on CIC = %d trunk %d Erro no %d Desc %s ",cic, trunk,ret,getErrDesc(ret));
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        criticalBlockLock.Unlock();
        return ret;
    }*/
    cicInfoItr = channelToCic(chanNo);
    if(cicInfoItr == cicInfoEnd())
    {
        criticalBlockLock.Unlock();
        return OUT_OF_RANGE; 
    }
    
    if((ret = checkCICStatus(chanNo,&link)) != SUCCESS)
    {
       sprintf(msgLogger.applMsgTxt,"Invalid CIC status CIC %d channel %d Erro no %d Desc %s ",cicInfoItr->second.origcic,chanNo,ret,getErrDesc(ret));
       logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
       
       
       if(ret != SWITCH_CONGESTION || ret != MEDIA_DOWN )
       {
            if(ret == OUT_OF_SERVICE)
            {
                callItr = findCallRec(chanNo);
                if( callItr != checkHashEnd())
                {
                    if(callItr->second.signalingState == STATE_BLOCKED_IC || callItr->second.signalingState == STATE_BLOCKED_OG)
                    {
                         criticalBlockLock.Unlock();
                         return ret;
                    }
                    
                }
            }
            else
            {
                criticalBlockLock.Unlock();
                return ret;
            }
       }
    }
  
    callItr = findCallRec(chanNo);
    if( callItr == checkHashEnd())
    {
        if(createHashRecord(chanNo) != SUCCESS)
        {
             criticalBlockLock.Unlock();
              return INVALID_STATE;
        }
    }
    ret = statusReq(cicInfoItr->second.origcic,link,MSG_TYPE_BLOCKING,evt);
    if(ret == SUCCESS)
    {
        /*t.id = cic;
        t.source = SS7_SIG;
        t.type = SS7_T16_TIMER;
        addTimer(t);*/
        //stopTimer(SS7_T16_TIMER,cic);
        sprintf(msgLogger.applMsgTxt,"Block send on channel %d cic %d link %d",chanNo,cicInfoItr->second.origcic,link);
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        //callItr = findCallRec(cic);
        gCountTraffic[link]++;
        callItr->second.signalingState = STATE_BLOCKED_OG;
      
    }
    else
    {
        sprintf(msgLogger.applMsgTxt,"Failed to send Block on CIC %d trunk %d Erro no %d Desc %s ",chanNo, trunk,ret,getErrDesc(ret));
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    }
    criticalBlockLock.Unlock();
    return ret;
}


int isup_unblockCircuit(unsigned short chanNo)
{
    int ret;
    unsigned char link;
    stTimer t;
    std::hash_map <int,callRec> :: iterator callItr ;
    std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
    unsigned short trunk;
    stStatEvnt evt;
 
    criticalUnblockLock.Lock();
    memset(&evt,0,sizeof(stStatEvnt));
    trunk = cicTotrunk(chanNo);
    
    /*if((ret = checkTrunkStatus(trunk,&link)) != SUCCESS)
    {
        sprintf(msgLogger.applMsgTxt,"Reset Failed on CIC = %d trunk %d Erro no %d Desc %s ",cic, trunk,ret,getErrDesc(ret));
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        criticalUnblockLock.Unlock();
        return ret;
    }*/
    cicInfoItr = channelToCic(chanNo);
    if(cicInfoItr == cicInfoEnd())
    {
        criticalUnblockLock.Unlock();
        return OUT_OF_RANGE; 
    }
    
    callItr = findCallRec(chanNo);
    if( callItr != checkHashEnd())
    {
        if(callItr->second.signalingState != STATE_BLOCKED_OG)
        {
             criticalUnblockLock.Unlock();
             return INVALID_STATE;
        }

    }
    if((ret = checkCICStatus(chanNo,&link)) != SUCCESS)
    {
       sprintf(msgLogger.applMsgTxt,"Invalid CIC status CIC %d channel %d Erro no %d Desc %s ",cicInfoItr->second.origcic,chanNo,ret,getErrDesc(ret));
       logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
       
       
       if(ret != SWITCH_CONGESTION || ret != MEDIA_DOWN || ret != OUT_OF_SERVICE)
       {
            
                criticalUnblockLock.Unlock();
                return ret;
       }
    }
  
    callItr = findCallRec(chanNo);
    if( callItr == checkHashEnd())
    {
        if(createHashRecord(chanNo) != SUCCESS)
        {
             criticalUnblockLock.Unlock();
              return INVALID_STATE;
        }
    }
    ret = statusReq(cicInfoItr->second.origcic,link,MSG_TYPE_UNBLOCKING,evt);
    if(ret == SUCCESS)
    {
        /*t.id = cic;
        t.source = SS7_SIG;
        t.type = SS7_T16_TIMER;
        addTimer(t);*/
        //stopTimer(SS7_T16_TIMER,cic);
        sprintf(msgLogger.applMsgTxt,"UnBlock send on channel %d cic %d link %d",chanNo,cicInfoItr->second.origcic,link);
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        //callItr = findCallRec(cic);
        gCountTraffic[link]++;
        callItr->second.signalingState = STATE_UNBLOCKED_OG;
      
    }
    else
    {
        sprintf(msgLogger.applMsgTxt,"Failed to send UnBlock on CIC %d trunk %d Erro no %d Desc %s ",chanNo, trunk,ret,getErrDesc(ret));
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    }
    criticalUnblockLock.Unlock();
    return ret;
}


static int resetCircuit(unsigned short chanNo)
{
    int ret;
    unsigned char link;
    stTimer t;
    std::hash_map <int,callRec> :: iterator callItr ;
    std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
    unsigned short trunk;
    ResetLock.Lock();
    trunk = cicTotrunk(chanNo);
    cicInfoItr = channelToCic(chanNo);
    if(cicInfoItr == cicInfoEnd())
    {
        ResetLock.Unlock();
        return OUT_OF_RANGE; 
    }
    
    if((ret = checkCICStatus(chanNo,&link)) != SUCCESS)
    {
        
        callItr = findCallRec(chanNo);
        if( callItr != checkHashEnd())
        {
            callItr->second.signalingState = STATE_RESET_OG;
            //call.erase(callItr);
        }
        if(ret == E1_DOWN || ret == MEDIA_DOWN || ret == SIG_LINK_DOWN)
        {
            stopTimer(SS7_T16_TIMER,chanNo);
            startTimer(SS7_T17_TIMER,chanNo);
        }
        if(ret != SWITCH_CONGESTION)
        {
             if(ret == OUT_OF_SERVICE)
            {
                callItr = findCallRec(chanNo);
                if( callItr != checkHashEnd())
                {
                    if(callItr->second.signalingState == STATE_BLOCKED_IC || callItr->second.signalingState == STATE_BLOCKED_OG)
                    {
                        sprintf(msgLogger.applMsgTxt,"Reset Failed on CIC %d channel %d trunk %d Erro no %d Desc %s ",cicInfoItr->second.origcic,chanNo, trunk,ret,getErrDesc(ret));
                        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
                         ResetLock.Unlock();
                         return ret;
                    }
                    
                }
            }
            else
            {
                sprintf(msgLogger.applMsgTxt,"Reset Failed on CIC %d channel %d trunk %d Erro no %d Desc %s ",cicInfoItr->second.origcic,chanNo, trunk,ret,getErrDesc(ret));
                logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
                ResetLock.Unlock();
                return ret;
            }
        }
    }
    
    callItr = findCallRec(chanNo);
    if( callItr == checkHashEnd())
    {
        if(createHashRecord(chanNo) != SUCCESS)
        {
             ResetLock.Unlock();
             return INVALID_STATE;
        }
        
    }
    
    
    ret = reset_circuit(cicInfoItr->second.origcic,link);
    if(ret == SUCCESS)
    {
        sprintf(msgLogger.applMsgTxt,"Reset send on CIC %d channel %d link %d",cicInfoItr->second.origcic,chanNo,link);
        logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        //callItr = findCallRec(cic);
       gCountTraffic[link]++;
    }
    else
    {
        sprintf(msgLogger.applMsgTxt,"Failed to send reset on CIC %d channel %d trunk %d Erro no %d Desc %s ",cicInfoItr->second.origcic,chanNo, trunk,ret,getErrDesc(ret));
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    }
    callItr = findCallRec(chanNo);
    if( callItr != checkHashEnd())
    {
            callItr->second.signalingState = STATE_RESET_OG;
            //call.erase(callItr);
    }
    stopTimer(SS7_T16_TIMER,chanNo);
    startTimer(SS7_T17_TIMER,chanNo);
    ResetLock.Unlock();
    return ret;
}



int isup_resetCircuitGrp(unsigned short chanNo,BYTE range)
{
    int ret = 0,count =0,index =0,tmpchan =0;
    stStatEvnt evt;
    unsigned short trunk;
    unsigned char link;
    std::hash_map <int,callRec> :: iterator callItr ;
    std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
    
        criticalGrpResetLock.Lock();
        memset(&evt,0,sizeof(stStatEvnt));
        trunk = cicTotrunk(chanNo);
        tmpchan = chanNo;
        
        while(count < range)
        {
            
            cicInfoItr = channelToCic(chanNo);
            if(cicInfoItr == cicInfoEnd())
            {
               // criticalGrpResetLock.Unlock();
               // return OUT_OF_RANGE; 
                count++;
                if(tmpchan == chanNo)
                {
                    tmpchan = ++chanNo;
                    continue;
                }
                
                    
            }
            else
            {

                if(isSignalingCIC(chanNo) != SUCCESS)
                {
                    if((ret = checkCICStatus(chanNo,&link)) != SUCCESS)
                    {

                        callItr = findCallRec(chanNo);
                        if( callItr != checkHashEnd())
                        {
                            callItr->second.signalingState = STATE_RESET_OG;
                            //call.erase(callItr);
                        }
                        /*if(ret == E1_DOWN || ret == MEDIA_DOWN || ret == SIG_LINK_DOWN)
                        {
                            stopTimer(SS7_T16_TIMER,cic);
                            startTimer(SS7_T17_TIMER,cic);
                        }*/
                        if(ret != SWITCH_CONGESTION)
                        {
                            if(ret == OUT_OF_SERVICE)
                            {
                                callItr = findCallRec(chanNo);
                                if( callItr != checkHashEnd())
                                {
                                    if(callItr->second.signalingState == STATE_BLOCKED_IC || callItr->second.signalingState == STATE_BLOCKED_OG)
                                    {
                                        sprintf(msgLogger.applMsgTxt,"Reset Failed on CIC %d trunk %d Erro no %d Desc %s ",chanNo, trunk,ret,getErrDesc(ret));
                                        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
                                        //criticalGrpResetLock.Unlock();
                                       // return ret;
                                    }
                                    else
                                    {
                                        count++;
                                        chanNo++;
                                        continue;
                                    }

                                }
                            }
                            else
                            {
                                sprintf(msgLogger.applMsgTxt,"Reset Failed on CIC %d trunk %d Erro no %d Desc %s ",chanNo, trunk,ret,getErrDesc(ret));
                                logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
                                //criticalGrpResetLock.Unlock();
                               // return ret;
                            }


                        }
                        else
                        {
                            count++;
                            chanNo++;
                            continue;
                        }

                    }
                }
                
            }
            callItr = findCallRec(tmpchan);
            if( callItr == checkHashEnd())
            {
                if(createHashRecord(tmpchan) != SUCCESS)
                {
                    //criticalGrpResetLock.Unlock();
                   // return INVALID_STATE;
                }
            }
            cicInfoItr = channelToCic(tmpchan);
            if(cicInfoItr == cicInfoEnd())
            {
                
            }
            evt.rangStat.stat.pres = TRUE;
            evt.rangStat.range = count + 1;
            ret = statusReq(cicInfoItr->second.origcic,link,MSG_TYPE_CIRCUIT_GROUP_RESET,evt);
            if(ret == SUCCESS)
            {
                sprintf(msgLogger.applMsgTxt,"Group reset send on CIC %d link %d range %d",tmpchan,link,range);
                logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);

            }
            else
            {
                sprintf(msgLogger.applMsgTxt,"Failed to send group reset on CIC %d trunk %d Erro no %d Desc %s ",tmpchan, trunk,ret,getErrDesc(ret));
                logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
            }
            count++;
            tmpchan = ++chanNo;
        }
        criticalGrpResetLock.Unlock();
        return SUCCESS;
}


int isup_blockCircuitGrp(unsigned short chanNo,BYTE range)
{
int ret = 0;
stStatEvnt evt;
unsigned short trunk;
unsigned char link;
std::hash_map <int,callRec> :: iterator callItr ;
std::hash_map <int,cicInformation> :: iterator cicInfoItr ;

        criticalGrpBlockLock.Lock();
        memset(&evt,0,sizeof(stStatEvnt));
        trunk = cicTotrunk(chanNo);
        evt.rangStat.stat.pres = TRUE;
        evt.rangStat.range = range;
        cicInfoItr = channelToCic(chanNo);
        if(cicInfoItr == cicInfoEnd())
        {
            criticalGrpBlockLock.Unlock();
            return OUT_OF_RANGE; 
        }
        if((ret = checkCICStatus(chanNo,&link)) != SUCCESS)
        {
            sprintf(msgLogger.applMsgTxt,"Group Block Failed on CIC %d trunk %d Erro no %d Desc %s ",chanNo, trunk,ret,getErrDesc(ret));
            logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
            callItr = findCallRec(chanNo);
            if( callItr != checkHashEnd())
            {
                callItr->second.signalingState = STATE_RESET_OG;
                //call.erase(callItr);
            }
            /*if(ret == E1_DOWN || ret == MEDIA_DOWN || ret == SIG_LINK_DOWN)
            {
                stopTimer(SS7_T16_TIMER,cic);
                startTimer(SS7_T17_TIMER,cic);
            }*/
            if(ret != SWITCH_CONGESTION)
            {
                 criticalGrpBlockLock.Unlock();
                 return ret;
            }
        }

        callItr = findCallRec(chanNo);
        if( callItr == checkHashEnd())
        {
            if(createHashRecord(chanNo) != SUCCESS)
            {
                 criticalGrpBlockLock.Unlock();
                 return INVALID_STATE;
            }

       }
        ret = statusReq(cicInfoItr->second.origcic,link,MSG_TYPE_CIRCUIT_GROUP_BLOCK,evt);
        if(ret == SUCCESS)
        {
            sprintf(msgLogger.applMsgTxt,"Group Block send on CIC %d link %d range %d",chanNo,link,range);
            logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);

        }
        else
        {
            sprintf(msgLogger.applMsgTxt,"Failed to send group block on CIC %d trunk %d Erro no %d Desc %s ",chanNo, trunk,ret,getErrDesc(ret));
            logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        }
        criticalGrpBlockLock.Unlock();
        return SUCCESS;
}

int isup_unblockCircuitGrp(unsigned short chanNo,BYTE range)
{
int ret = 0;
stStatEvnt evt;
unsigned short trunk;
unsigned char link;
std::hash_map <int,callRec> :: iterator callItr ;
std::hash_map <int,cicInformation> :: iterator cicInfoItr ;

        criticalGrpUnblockLock.Lock();
        memset(&evt,0,sizeof(stStatEvnt));
        trunk = cicTotrunk(chanNo);
        evt.rangStat.stat.pres = TRUE;
        evt.rangStat.range = range;
        cicInfoItr = channelToCic(chanNo);
        if(cicInfoItr == cicInfoEnd())
        {
            criticalGrpUnblockLock.Unlock();
            return OUT_OF_RANGE; 
        }
        if((ret = checkCICStatus(chanNo,&link)) != SUCCESS)
        {
            sprintf(msgLogger.applMsgTxt,"Group UnBlock Failed on CIC %d trunk %d Erro no %d Desc %s ",chanNo, trunk,ret,getErrDesc(ret));
            logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
            callItr = findCallRec(chanNo);
            if( callItr != checkHashEnd())
            {
                callItr->second.signalingState = STATE_RESET_OG;
                //call.erase(callItr);
            }
            /*if(ret == E1_DOWN || ret == MEDIA_DOWN || ret == SIG_LINK_DOWN)
            {
                stopTimer(SS7_T16_TIMER,cic);
                startTimer(SS7_T17_TIMER,cic);
            }*/
            if(ret != SWITCH_CONGESTION)
            {
                 criticalGrpUnblockLock.Unlock();
                 return ret;
            }
        }

        callItr = findCallRec(chanNo);
        if( callItr == checkHashEnd())
        {
            if(createHashRecord(chanNo) != SUCCESS)
            {
                 criticalGrpUnblockLock.Unlock();
                 return INVALID_STATE;
            }

       }
        ret = statusReq(cicInfoItr->second.origcic,link,MSG_TYPE_CIRCUIT_GROUP_UNBLOCK,evt);
        if(ret == SUCCESS)
        {
            sprintf(msgLogger.applMsgTxt,"Group UnBlock send on CIC %d link %d range %d",chanNo,link,range);
            logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);

        }
        else
        {
            sprintf(msgLogger.applMsgTxt,"Failed to send group unblock on CIC %d trunk %d Erro no %d Desc %s ",chanNo, trunk,ret,getErrDesc(ret));
            logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
        }
        criticalGrpUnblockLock.Unlock();
        return SUCCESS;
}


int isup_resetAllCircuits()
{
    pthread_t temp_thread_handle;
    int rc =0;
    rc =  pthread_create(&temp_thread_handle,NULL,&reset_all_circuits,NULL);
    if(rc == -1)
    {
        sprintf(msgLogger.applMsgTxt,"Failed to create Reset circuit Thread.Error no : %d desc: %s" ,errno,getErrDesc(errno));
        logErr(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_0);
        return errno;
    }
    else
        return SUCCESS;
}


int isup_setChanStatus(unsigned short chanNo,unsigned short status)
{
    int ret = 0;
    unsigned char link;
    if((ret = checkCICStatus(chanNo,&link)) != SUCCESS)
    {
        if(ret == EINVAL)
        {
            sprintf(msgLogger.applMsgTxt,"Invalid CIC status CIC %d Errno no %d Desc %s ",chanNo,ret,getErrDesc(ret));
            logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
            return ret;
        }
    }
    switch(status)
    {
        case PIC_DSP_TXDIS_RXDIS:
        case PIC_DSP_TXDIS_RXEN:
        case PIC_DSP_TXEN_RXDIS:
        case PIC_DSP_TXEN_RXEN:
        case PIC_DSP_QUERY_CHNL_STATUS:
            break;
        default:
            sprintf(msgLogger.applMsgTxt,"Invalid status value %d channo %d  ",status,chanNo);
            logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
            return EINVAL;
    }
    
    setSs7ChanStatus(chanNo,status);
    sprintf(msgLogger.applMsgTxt,"Send channel status value %d channo %d  cic %d",status,chanNo);
    logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    return SUCCESS;
}


int isup_startDTMFcollect(unsigned short chanNo,IOTERM *term)
{
    int ret;
    unsigned char link;
    std::hash_map <int,callRec> :: iterator callItr ;
    std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
    criticalDTMFCollectLock.Lock();
    callItr = findCallRec(chanNo);
    if( callItr == checkHashEnd())
    {
        criticalDTMFCollectLock.Unlock();
        return INVALID_STATE;
    }
    cicInfoItr = channelToCic(chanNo);
    if(cicInfoItr == cicInfoEnd())
    {
        criticalDTMFCollectLock.Unlock();
        return OUT_OF_RANGE; 
    }
    
    switch(callItr->second.signalingState)
    {
        case STATE_RINGING_IC:
        case STATE_RINGING_OG:
        case STATE_ANSWERED_IC:
        case STATE_ANSWERED_OG:
            break;
        default:
            criticalDTMFCollectLock.Unlock();
            return INVALID_STATE;
    }
    
    ret = startDTMFTermThread(chanNo,term);
    if(ret == SUCCESS)
    {
       sprintf(msgLogger.applMsgTxt,"DTMF thread started on CIC %d channel No %d callID %s link %d",cicInfoItr->second.origcic,chanNo,callItr->second.callId,link );
       logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_3);
    }
    else
    {
        sprintf(msgLogger.applMsgTxt,"Failed to to start DTMF thread on CIC %d channel no %d Erro no %d Desc %s ",cicInfoItr->second.origcic,chanNo,ret,getErrDesc(ret));
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    }
    criticalDTMFCollectLock.Unlock();
    return ret;
}


int isup_getDigit(unsigned char msg[0],char * digitBuf )
{
    char tempDtmf[MAX_NUMBER_LEN];
    int count = 0;
    memset(tempDtmf,0,MAX_NUMBER_LEN);
    for(count = 0; count < msg[13] ; count++)
    {
        tempDtmf[count] = msg[14+count];
    }
    
    if(count > 0)
    {
        strcpy(digitBuf,tempDtmf);
        return SUCCESS;
    }
    return DTMF_BUF_EMPTY;
}


int isup_sendDTMF(unsigned short chanNo,unsigned short dtmf)
{
    int ret;
    unsigned char link;
    std::hash_map <int,callRec> :: iterator callItr ;
    std::hash_map <int,cicInformation> :: iterator cicInfoItr ;
    criticalDTMFLock.Lock();
    callItr = findCallRec(chanNo);
    if( callItr == checkHashEnd())
    {
        criticalDTMFLock.Unlock();
        return INVALID_STATE;
    }
    cicInfoItr = channelToCic(chanNo);
    if(cicInfoItr == cicInfoEnd())
    {
        criticalDTMFLock.Unlock();
        return OUT_OF_RANGE; 
    }
    
    switch(callItr->second.signalingState)
    {
        case STATE_RINGING_IC:
        case STATE_RINGING_OG:
        case STATE_ANSWERED_IC:
        case STATE_ANSWERED_OG:
            break;
        default:
            criticalDTMFLock.Unlock();
            return INVALID_STATE;
    }
    
    ret = sendDTMF2Box(chanNo,dtmf);
    if(ret == SUCCESS)
    {
       sprintf(msgLogger.applMsgTxt,"DTMF send on CIC %d channel No %d callID %s link %d",cicInfoItr->second.origcic,chanNo,callItr->second.callId,link );
       logInfo(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_3);
    }
    else
    {
        sprintf(msgLogger.applMsgTxt,"Failed to send DTMF thread on CIC %d channel no %d Erro no %d Desc %s ",cicInfoItr->second.origcic,chanNo,ret,getErrDesc(ret));
        logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
    }
    criticalDTMFLock.Unlock();
    return ret;
}

int setSS7TimerParam(unsigned char timerName,unsigned short timeout,unsigned short retry)
{
    switch(timerName)
    {
        case SS7_T1_TIMER:
           timerSS7.t1.timeout = timeout;
           timerSS7.t1.retry = retry; 
           break;
        case SS7_T5_TIMER:
           timerSS7.t5.timeout = timeout;
           timerSS7.t5.retry = retry; 
           break;
        case SS7_T7_TIMER:
           timerSS7.t7.timeout = timeout;
           timerSS7.t7.retry = retry; 
           break;
        case SS7_T9_TIMER:
           timerSS7.t9.timeout = timeout;
           timerSS7.t9.retry = retry; 
           break;
        case SS7_T16_TIMER:
           timerSS7.t16.timeout = timeout;
           timerSS7.t16.retry = retry; 
           break;
       case SS7_T17_TIMER:
           timerSS7.t17.timeout = timeout;
           timerSS7.t17.retry = retry; 
           break;
        default:
            sprintf(msgLogger.applMsgTxt,"Unknown/Unsupported timer type %d",timerName);
            logWarning(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_1);
            return INVALID_TIMER;
    }
    
    return SUCCESS;
    
}



int startTimer(unsigned char timerName,unsigned short id)
{
    stTimer t;
    std::hash_map <int,callRec> :: iterator callItr ;
    t.id = id;
    t.type = timerName;
    t.source = SS7_SIG;
    if(t.source == SS7_SIG)
    {
        callItr = findCallRec(id);
        if(callItr != checkHashEnd())
        {
            //t.retry =0;//callItr->second.timerRetry ;
            callItr->second.timerRetry++;
       }
    }
    return(addTimer(t) );
}


int stopTimer(unsigned char timerName,unsigned short id)
{
  stTimer t;
  std::hash_map <int,callRec> :: iterator callItr ;
  memset(&t,0,sizeof(stTimer));
    t.id = id;
    t.type = timerName;
    t.source = SS7_SIG;
    callItr = findCallRec(id);
    if(callItr != checkHashEnd())
    {
       // t.retry =callItr->second.timerRetry ;
        callItr->second.timerRetry = 0;
    }
    return(cancelTimer(t));
}


int stopAllTimerOnCIC(unsigned short cic)
{
  stTimer t;
  std::hash_map <int,callRec> :: iterator callItr ;
    memset(&t,0,sizeof(stTimer));
    t.id = cic;
    t.type = TIMER_NONE;
    t.source = SS7_SIG;
    callItr = findCallRec(cic);
    if(callItr != checkHashEnd())
    {
       // t.retry =callItr->second.timerRetry ;
        callItr->second.timerRetry = 0;
    }
   
    return(cancelTimer(t));
}
           
    


           
