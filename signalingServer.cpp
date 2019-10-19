#include<stdio.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>
#include <cstring>
#include <stdlib.h>
#include <string>
#include <algorithm>
#include "libisup.h"
#include "pic_struc.h"
#include "signalingServer.h"
#include "error.h"
#include "common.h"
#include "HashMap.h"



using namespace std;
        
        
/*defines*/

/*End defines*/


/*Global variables*/

HashMap<int,Digit_Data> dtmfRec;
int tcp_sock = 0;
int signaling_qid = 0;

pthread_t tcpThdHndl = 0;
pthread_t temp_thread_handle;
pthread_t keep_alive_message_handle;
pthread_t dtmfHandle;

Ss7msg Ss7txbuf1,Ss7txbuf2;
Ss7msg *Ss7txMsg;

isupLog::Log sigmsgLogger;
int keep_alive_count=0;
unsigned char route_code_length, source_number_length, destination_number_length;
char Source_Number[20];
char Forwarded_Number[20];
char Destination_Number[20];
char Redirected_Number[20];
int dtmfThread[MAX_CHANNELS];
int dtmfFlag[MAX_CHANNELS];
int releaseFlag[MAX_CHANNELS];
unsigned char dtmfBuf[MAX_CHANNELS];
unsigned char called_number[30];

unsigned char g_dspStat;
unsigned char E1Status[MAX_E1+1];
unsigned char SS7LinkStatus[MAX_SS7_LINK];
unsigned char sanityStatus = 0;

criticalSection criticalSockLock;
criticalSection criticalSockLock1;
criticalSection criticalMsgQueLock;
criticalSection criticalStatReqRespLock;
criticalSection initialiseLock1;
criticalSection initialiseLock2;
/*End Global variables*/


/*Extern variables*/

extern int cti_qid ; 
extern int g_maxUsedChan ;
extern isupConfig conf;

/*End Extern variables*/


/*Function names*/

void * startSignalingServer(void *);
int addMsgToSigSlaveQue( Msg_Packet msg);
static int send_message_to_slave(Ss7msg *Ss7RxMsg);
static int send_spcl_message_to_slave(Ss7msg *Ss7RxMsg);
void* sanity_to_pic(void *);
static void enable_ss7();
void* reset_all_circuits(void *);
int send_msg(Ss7msg *buf);
int startDTMFTermThread(unsigned int port,short int timeout,IOTERM term);
int reset_circuit(unsigned short port,unsigned char link);
int initialiseMsg(Msg_Packet *Ss7Msg);
int initialiseMsg(Ss7msg *Ss7Msg);
int initialiseIAM(IAM  *iamParam,BYTE switchType = ITUT);
int initialiseACM(ACM  *param,BYTE switchType = ITUT);
int initialiseANM(ANM  *param,BYTE switchType = ITUT);
int initialiseREL(REL  *param,BYTE cause = CCCALLCLR,BYTE switchType = ITUT);
int initialiseRLC(RLC  *param,BYTE switchType = ITUT);
int send_iam(unsigned short port,IAM * param,char *source_number,char *dest_number,unsigned char link);
int send_acm(unsigned short port, ACM *param,unsigned char link);
int send_rel(unsigned short port, REL *param,unsigned char link);
int send_rlc(unsigned short port,RLC * param,unsigned char link);
int send_anm(unsigned short port, ANM * param,unsigned char link);
int statusReq(unsigned short port,unsigned char link,BYTE msgType,stStatEvnt evt);
/*End Function names*/

/*Extern Functions*/
extern unsigned short cicTotrunk(unsigned short cic);
extern int convertToHex(char * val,BYTE * result);
extern int isup_resetCircuit(unsigned short cic);
void printConfig();
/*End Extern Functions*/






int connectToSignalingServer(isupSignalConfig config)
{
struct sockaddr_in server;      /* server address */
int RetVal = 0,rc = -1,sendbuff;

        tcp_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
        if( tcp_sock == -1 )
        {
                //fprintf(stderr,"Socket was not created.\n");
                sprintf(sigmsgLogger.applMsgTxt,"Failed to create Signaling socket. Error no : %d desc: %s" ,errno,getErrDesc(errno));
                logErr(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_0);
                close(tcp_sock);
                return errno;
        }
        if(config.sigServerIP == NULL)
        {
           sprintf(sigmsgLogger.applMsgTxt,"Invalid Signaling ServerIP" );
           logWarning(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_0);
           return EINVAL;
        }
        sendbuff = 250000;

        printf("sets the send buffer to %d\n", sendbuff);
        RetVal = setsockopt(tcp_sock, SOL_SOCKET, SO_SNDBUF, &sendbuff, sizeof(sendbuff));

        if(RetVal == -1)
            printf("Error setsockopt");  
        RetVal = setsockopt(tcp_sock, SOL_SOCKET, SO_RCVBUF, &sendbuff, sizeof(sendbuff));

        if(RetVal == -1)
            printf("Error setsockopt");
        
        server.sin_family = AF_INET;                    /* set up the server name */
        server.sin_port = htons(config.portNo);                  /* Port on which it listens */
        server.sin_addr.s_addr = inet_addr(config.sigServerIP );
        RetVal =  connect(tcp_sock,(struct sockaddr *) &server, sizeof(server));
        if(RetVal == -1)
        {
           sprintf(sigmsgLogger.applMsgTxt,"Failed to connect to Signaling Server %s port %d, Error no : %d desc: %s" ,config.sigServerIP,config.portNo,errno,getErrDesc(errno));
           logErr(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_0);
           return errno;
        }
        else
        {
            if((signaling_qid = msgget(TCPWRITE_KEY,0700 | IPC_CREAT)) == -1)
            {
                sprintf(sigmsgLogger.applMsgTxt,"Failed to create Signaling message que. Error no : %d desc: %s" ,errno,getErrDesc(errno));
                logErr(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_0);
                return errno;
            }
            else
            {
               sprintf(sigmsgLogger.applMsgTxt,"Signaling Que ID : %d" ,signaling_qid);
               logDebug(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_2);
            }

            sprintf(sigmsgLogger.applMsgTxt,"Successfully connected to Signaling Server %s port %d" ,config.sigServerIP,config.portNo);
            logInfo(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_1);
            //pthread_create(&tcp_message_handle,NULL,&relay_to_pic,NULL);
            rc = pthread_create(&keep_alive_message_handle,NULL,&sanity_to_pic,NULL);
            if(rc == -1)
            {
                sprintf(sigmsgLogger.applMsgTxt,"Failed to create Sanity Thread. Error no : %d desc: %s" ,errno,getErrDesc(errno));
                logErr(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_0);
             }

            sleep(5);
            enable_ss7();

            keep_alive_count=0;
            rc = pthread_create(&tcpThdHndl,NULL,&startSignalingServer,NULL);
            if(rc == -1)
            {
                sprintf(sigmsgLogger.applMsgTxt,"Failed to create Signaling Thread. Error no : %d desc: %s" ,errno,getErrDesc(errno));
                logErr(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_0);
                return errno;
            }

            return SUCCESS;
            
        }
    
}


void * startSignalingServer(void *)
{ 
    int nBytes = 0;
    int index=0,e1EnableCount = 0;
    int ping=1;
    unsigned char bool_flag =0 ;
    int first_flag=0,rc = 0;
    int E1count = 0;
    
        while(TRUE)
        {
                Ss7txMsg = &Ss7txbuf2;
                
                if( (nBytes = recv(tcp_sock,(unsigned char *)(Ss7txMsg),sizeof(Ss7msg), 0)) == -1 )
                {
                    sprintf(sigmsgLogger.applMsgTxt,"Failed to receive data from Signaling server. Error no : %d desc: %s" ,errno,getErrDesc(errno));
                    logDebug(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_0);
                    continue;
                }
               
                if(nBytes == 0)
                {
                    sprintf(sigmsgLogger.applMsgTxt,"Connection closed from remote server");
                    logDebug(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_0);
                    sleep(1);
                    continue;
                }
                
                if(nBytes < 16)
                {
                    sprintf(sigmsgLogger.applMsgTxt,"Packet received from Signaling server is %d bytes",nBytes);
                    logDebug(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_2);
                    continue;
                }
                
                while( (nBytes >= 16) && (nBytes >= ((Ss7txMsg->length >> 8) +8)))
                {

                        /*if( (Ss7txMsg->id1 != BOX_ID1) && (Ss7txMsg->id2 != BOX_ID2) )
                        {
                                sprintf(sigmsgLogger.applMsgTxt,"Ids are not matching. Dropping the rest of the packet\n");
                                logDebug(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_2);
                                break;
                        }
                        */
                        /*printf("length = %x ",Ss7txMsg->length);
                        printf("opcode = %x ",Ss7txMsg->opcode);
                        printf("subfield = %x ",Ss7txMsg->subfield);
                        printf("debug = %x Data ",Ss7txMsg->debug);*/
                        memset(sigmsgLogger.applMsgTxt,0,sizeof(sigmsgLogger.applMsgTxt));
                        sprintf(sigmsgLogger.applMsgTxt,"length %x opcode %x subfield %x debug %x ISUP HEX : ",
                                htons(Ss7txMsg->length),htons(Ss7txMsg->opcode),htons(Ss7txMsg->subfield),htons(Ss7txMsg->debug));
                        for(index = 0; index < ((Ss7txMsg->length >> 8) - 8); index++)
                        {
                                //printf("%x ",Ss7txMsg->msg[index]);
                                sprintf(&sigmsgLogger.applMsgTxt[strlen(sigmsgLogger.applMsgTxt)],"%x ",Ss7txMsg->msg[index]);
                        }
                         logDebug(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_4);
                        //printf("\n");

                        switch(htons(Ss7txMsg->opcode))
                        {
                            case PIC_SS7_MNTC:
                                switch(htons(Ss7txMsg->subfield))
                                {
                                        case 0x00:
                                                sprintf(sigmsgLogger.applMsgTxt,"\033[7m \033[32m SS7 LINK UP \033[37m \033[0m\n ");
                                                logDebug(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_2);
                                                //prev_ss7_link_status=1;
                                              //  ss7_link_status=1;
                                                //pthread_create(&temp_thread_handle,NULL,&reset_all_circuits,NULL);
                                                break;
                                        case 0x01:
                                                sprintf(sigmsgLogger.applMsgTxt,"\033[7m \033[35m SS7 LINK DOWN \033[37m \033[0m\n ");
                                                logDebug(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_2);

                                              /*  send_link_down();
                                                if(ss7_link_status) // if it was up before
                                                {
                                                        exit(0);
                                                }*/
                                               break;
                                        case 0x02:
                                               // sprintf(sigmsgLogger.applMsgTxt,"\033[33mSANITY FROM PIC \033[0m\n ");
                                               // logDebug(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_2);
                                                sanityStatus = 1;
                                                printf("VMS Response Physical E1 status\n");
                                                for(E1count = 0; E1count < MAX_E1; E1count++)
                                                {
                                                    E1Status[E1count]  = Ss7txMsg->msg[E1count];
                                                    SS7LinkStatus[E1count] = Ss7txMsg->msg[E1count+16];
                                                }
                                                g_dspStat = Ss7txMsg->msg[64];
                                                memset(sigmsgLogger.applMsgTxt,0,sizeof(sigmsgLogger.applMsgTxt));
                                                sprintf(sigmsgLogger.applMsgTxt,"SANITY : ");
                                                for(E1count = 0; E1count <= 64;E1count++)
                                                {
                                                    sprintf(&sigmsgLogger.applMsgTxt[strlen(sigmsgLogger.applMsgTxt)],"%02x ",Ss7txMsg->msg[E1count]);
                                                    //sendMsgToLogger(_LFF,msgLogger,APPL_LOG,LOG_LEVEL_2);
                                                }
                                                sprintf(&sigmsgLogger.applMsgTxt[strlen(sigmsgLogger.applMsgTxt)],"\n");
                                                logInfo(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_2);

                                                
                                                if(first_flag==0)
                                                {
                                                    rc =  pthread_create(&temp_thread_handle,NULL,&reset_all_circuits,NULL);
                                                    if(rc == -1)
                                                    {
                                                        sprintf(sigmsgLogger.applMsgTxt,"Failed to create Reset circuit Thread.Error no : %d desc: %s" ,errno,getErrDesc(errno));
                                                        logErr(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_0);
                                                    }

                                                }
                                                first_flag=1;
                                                keep_alive_count=0;
                                                send_message_to_slave(Ss7txMsg);
                                                break;
                                        default:
                                                break;
                                }
                                break;

                                
                            case PIC_CHNL_STATUS_CALLPROC:
                            case PIC_TIMESWITCH_CALLPROC:
                            case PIC_TIMESLOT_CALLPROC:
                            case PIC_MEMORY_ACCESS_CALLPROC:
                                send_spcl_message_to_slave(Ss7txMsg);
                                break;
                                
                            case PIC_DTMF_CALLPROC:
                                if(dtmfThread[Ss7txMsg->msg[0]])
                                {
//                                        printf("####DTMF received thread alive %d \n",dtmfThread[Ss7txMsg->msg[0]]);
                                        dtmfFlag[Ss7txMsg->msg[0]] = TRUE;
                                        dtmfBuf[Ss7txMsg->msg[0]]  = Ss7txMsg->msg[14];
                                }
                                else
                                {
                                        Ss7txMsg->msg[13] = 0x01;//DTMF length
                                        send_message_to_slave(Ss7txMsg);
                                       // printf("####DTMF received thread dead \n");
                                }
                                break;

                            case PIC_SS7_CALLPROC:
                                if(Ss7txMsg->msg[2] ==  MSG_TYPE_RELEASE)
                                      releaseFlag[Ss7txMsg->msg[0]] = TRUE;
                                send_message_to_slave(Ss7txMsg);
                                break;
                            default:
                                printf("Invalid OPCODE received %d\n",htons(Ss7txMsg->opcode));
                                break;
                        }
                        nBytes = nBytes - (((Ss7txMsg->length) >> 8) + 8);//8 added for id1 and id2

                        if(nBytes > 0)
                        {
                                sprintf(sigmsgLogger.applMsgTxt,"\033[7m \033[35m Concatenated packets \033[37m \033[27m\n");
                                logDebug(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_2);
                                //Ss7txMsg = (char *) Ss7txMsg + (((Ss7txMsg->length) >> 8) + 8);//8 added for id1 and id2
                                Ss7txMsg =  Ss7txMsg + (((Ss7txMsg->length) >> 8) + 8);//8 added for id1 and id2
                        }
                        else if(nBytes <0)
                        {
                                printf("nBytes < 0\n");
                        }
                 }
        }
}

static int send_spcl_message_to_slave(Ss7msg *Ss7RxMsg)
{
        Msg_Packet msg,temp_msg;
        int     index;
        unsigned char   port;
        int ret;
        memset(&msg,0,sizeof(Msg_Packet));
        port=htons(((Spcl_Ss7msg *)Ss7RxMsg)->optional_timeslot);

//      printf("Spcl msg received for port %d\n",port);

        if(port >= g_maxUsedChan  || port <=0)
        {
            sprintf(sigmsgLogger.applMsgTxt,"Port No %d. Error no : %d desc: %s" ,port,OUT_OF_RANGE,getErrDesc(OUT_OF_RANGE));
            logWarning(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_0);
            return OUT_OF_RANGE;
        }

        msg.type=port;
        msg.id1=Ss7RxMsg->id1;
        msg.id2=Ss7RxMsg->id2;
        msg.length=htons(Ss7RxMsg->length);
        msg.opcode=htons(Ss7RxMsg->opcode);
        msg.subfield=htons(Ss7RxMsg->subfield);
        msg.debug=htons(Ss7RxMsg->debug);

        for(index=0;index< (msg.length-8); index++)
        {
                msg.msg[index]=Ss7RxMsg->msg[index];
        }
        ret = addMsgToSigSlaveQue(msg);
            
        return ret;
}


static int send_message_to_slave(Ss7msg *Ss7RxMsg)
{
Msg_Packet msg,temp_msg;
int     index;
unsigned short     port,tmp;
int     ret;
struct msqid_ds msgbuf;
        tmp = Ss7RxMsg->msg[1] & 0x07;
        port=Ss7RxMsg->msg[0] | (tmp << 8);

        memset(&msg,0,sizeof(Msg_Packet));
        //if((port >= g_maxUsedChan  || port <=0) && htons(Ss7RxMsg->opcode) !=  PIC_SS7_MNTC)
        if(( port <=0) && htons(Ss7RxMsg->opcode) !=  PIC_SS7_MNTC)
        {
            sprintf(sigmsgLogger.applMsgTxt,"Port No %d. Error no : %d desc: %s" ,port,OUT_OF_RANGE,getErrDesc(OUT_OF_RANGE));
            logWarning(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_0);

           // printf("Message Sent Failed port %d opcode %d\n",port,htons(Ss7RxMsg->opcode));
            return -1;
        }

        if(msgctl(cti_qid, IPC_STAT, &msgbuf) == -1)
        {
                perror("Status :");
                return -1;
        }
        if(port <= 0)
         port = 700;//Message type beyond SS7


       /* if(port > g_maxUsedChan  || port <=0)
        // if(port <=0)
        {
            sprintf(sigmsgLogger.applMsgTxt,"Port No %d. Error no : %d desc: %s" ,port,PORT_OUT_RANGE,getErrDesc(PORT_OUT_RANGE));
            logWarning(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_0);
             return PORT_OUT_RANGE;
        }*/

        msg.type=port;
        msg.id1=Ss7RxMsg->id1;
        msg.id2=Ss7RxMsg->id2;
        msg.length=htons(Ss7RxMsg->length);
        msg.opcode=htons(Ss7RxMsg->opcode);
        msg.subfield=htons(Ss7RxMsg->subfield);
        msg.debug=htons(Ss7RxMsg->debug);

        for(index=0;index< (msg.length-8); index++)
        {
                msg.msg[index]=Ss7RxMsg->msg[index];
        }
        ret = addMsgToSigSlaveQue(msg);
            
        return ret;
           
       
}


int addMsgToSigSlaveQue(Msg_Packet msg)
{
    struct msqid_ds msgbuf;
    criticalMsgQueLock.Lock();
    
    if(msgctl(cti_qid, IPC_STAT, &msgbuf) == -1)
    {
        sprintf(sigmsgLogger.applMsgTxt,"Signaling slave QID does not exist. Error no : %d desc: %s" ,errno,getErrDesc(errno));
        logErr(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_0);
        //criticalSec.Unlock();
        criticalMsgQueLock.Unlock();
        return errno;
    }
    
    if( msgsnd(cti_qid,&msg,sizeof(Msg_Packet),0)==-1 )
    {
        sprintf(sigmsgLogger.applMsgTxt,"Failed to add data in signaling Msg que. Error no : %d desc: %s" ,errno,getErrDesc(errno));
        logErr(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_0);
        //criticalSec.Unlock();
        criticalMsgQueLock.Unlock();
        return errno;
   }
    
   // criticalSec.Unlock();
    criticalMsgQueLock.Unlock();
    return SUCCESS;
     
}

static void enable_ss7()
{
        Ss7msg   Ss7TxMsg;
        Ss7TxMsg.id1=BOX_ID1;
        Ss7TxMsg.id2=BOX_ID2;
        Ss7TxMsg.length=(FIXED_SIZE+3) << 8;
        Ss7TxMsg.opcode=1;
        Ss7TxMsg.subfield=0;
        Ss7TxMsg.debug=1;
        Ss7TxMsg.msg[0]=0;
        Ss7TxMsg.msg[1]=0;
        Ss7TxMsg.msg[2]=0;
        send_msg(&Ss7TxMsg);
}


int send_msg(Ss7msg *buf)
{
   int nBytes = 0;
 //  criticalSec.Lock();
   
        criticalSockLock.Lock();
       // printf("Sending Data1\n");
        buf->id1=BOX_ID1;
        buf->id2=BOX_ID2;

        //if( (nBytes = send(tcp_sock, buf, (((buf->length) >> 8) + 0x08) , 0)) < 0 )
        if( (nBytes = send(tcp_sock, buf, (((buf->length) >> 8) + 0x08) , 0)) < 0 )
        {
            //sprintf(sigmsgLogger.applMsgTxt,"Failed to send data to Signaling server. Error no : %d desc: %s" ,errno,getErrDesc(errno));
            //logDebug(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_0);
            printf("Error Data send1\n");
            criticalSockLock.Unlock();
            return errno;
        }
      //  criticalSec.Unlock();
        usleep(1000);
        criticalSockLock.Unlock();
        //printf("Data send1\n");
        return SUCCESS;
                
}

int send_msg(Ss7msg_chStatus *buf)
{
    int nBytes = 0;
 //  criticalSec.Lock();
    //Msg_Packet * buf = (Msg_Packet *) pkt;
    criticalSockLock1.Lock();
    printf("Sending Data\n");
    buf->id1=BOX_ID1;
    buf->id2=BOX_ID2;

    //if( (nBytes = send(tcp_sock, buf, (((buf->length) >> 8) + 0x08) , 0)) < 0 )
    if( (nBytes = send(tcp_sock, buf, (((buf->length) >> 8) + 0x08) , 0)) < 0 )
    {
        //sprintf(sigmsgLogger.applMsgTxt,"Failed to send data to Signaling server. Error no : %d desc: %s" ,errno,getErrDesc(errno));
        //logDebug(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_0);
        criticalSockLock1.Unlock();
        printf("Error Data send\n");
        return errno;
    }
  //  criticalSec.Unlock();
    usleep(1000);
    criticalSockLock1.Unlock();
     printf("Data send\n");
    return SUCCESS;
                

}



void setSs7ChanStatus(unsigned short port,unsigned short status)
{
struct  Ss7msg_chStatus ch_status;
int ret;
    ch_status.id1=BOX_ID1;
    ch_status.id2=BOX_ID2;
    ch_status.length=htons((FIXED_SIZE+4));
    ch_status.opcode=htons(PIC_CHNL_STATUS_CALLPROC);
    ch_status.subfield=htons(status);
    ch_status.debug=0x00;
    ch_status.optionalTimeSlot=htons(port);
    ch_status.timeSlot=htons(port);
    ret = send_msg(&ch_status);
}



void* sanity_to_pic(void *)
{
Msg_Packet ss7_rx_msg;
int nBytes = 0;
        ss7_rx_msg.id1=BOX_ID1;
        ss7_rx_msg.id2=BOX_ID2;
        ss7_rx_msg.length=0x800;
        ss7_rx_msg.opcode=0x100;
        ss7_rx_msg.subfield=0x200;
        ss7_rx_msg.debug=0;
        while(1)
        {

                //if( (nBytes = send(tcp_sock, &(ss7_rx_msg.id1), (ss7_rx_msg.length >> 8) + 8, 0x40)) < 0 )
                if( (nBytes = send(tcp_sock, &(ss7_rx_msg.id1), (ss7_rx_msg.length >> 8) + 8, MSG_WAITFORONE)) < 0 )
                {
                    //sprintf(sigmsgLogger.applMsgTxt,"Failed to send SANITY. Error no : %d desc: %s" ,errno,getErrDesc(errno));
                    //logDebug(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_0);
                    //printf("Failed to send data to socket.\n");
                    //exit(0);
                    sleep(5);

                   // raiseException(_LFF,LAN_DOWN,0,0,LOG_LEVEL_0);
                    continue;

                }
                sleep(5);
                if(keep_alive_count >= 10)
                {
                    sprintf(sigmsgLogger.applMsgTxt,"No SANITY from PIC");
                    logDebug(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_0);
                    //raiseException(_LFF,JUNO_CTI_DOWN,0,0,LOG_LEVEL_0);
                    sanityStatus = 0;
                    keep_alive_count=0;
                    //exit(0);
                }
                keep_alive_count++;
        }
}




void* reset_all_circuits(void *)
{
        unsigned short index,cic;
        unsigned char link;
        Ss7msg   Ss7TxMsg;
        int ret;


        sleep(1);

        for(index=1;index< 16;index++)
        {
            if(isup_resetCircuit(index) == SUCCESS)
                usleep(300000);
            if(isup_resetCircuit(index+16) == SUCCESS)
                usleep(300000);
            if(isup_resetCircuit(index+32) == SUCCESS)
                usleep(300000);
            if(isup_resetCircuit(index+48) == SUCCESS)
                usleep(100000);
            if(isup_resetCircuit(index+64) == SUCCESS)
                usleep(300000);
            if(isup_resetCircuit(index+80) == SUCCESS)
                usleep(100000);
            if(isup_resetCircuit(index+96) == SUCCESS)
                usleep(300000);
            if(isup_resetCircuit(index+112) == SUCCESS)
                usleep(300000);
            if(isup_resetCircuit(index+128) == SUCCESS)
                usleep(100000);
            if(isup_resetCircuit(index+144) == SUCCESS)
                usleep(300000);
            if(isup_resetCircuit(index+160) == SUCCESS)
                usleep(300000);
            if(isup_resetCircuit(index+176) == SUCCESS)
                usleep(100000);
            if(isup_resetCircuit(index+192) == SUCCESS)
                usleep(300000);
            if(isup_resetCircuit(index+208) == SUCCESS)
                usleep(100000);
            if(isup_resetCircuit(index+224)  == SUCCESS)
                usleep(300000);
            if(isup_resetCircuit(index+240) == SUCCESS)
                usleep(100000);
        }
              
        
        for(index=1;index<16;index++)
        {
            if(isup_resetCircuit(index+255) == SUCCESS)
                usleep(100000);
            if(isup_resetCircuit(index+16+255) == SUCCESS)
                usleep(300000);
            if(isup_resetCircuit(index+32+255) == SUCCESS)
                usleep(100000);
            if(isup_resetCircuit(index+48+255) == SUCCESS)
                usleep(300000);
            if(isup_resetCircuit(index+64+255) == SUCCESS)
                usleep(300000);
            if(isup_resetCircuit(index+80+255) == SUCCESS)
                usleep(300000);
            if(isup_resetCircuit(index+96+255) == SUCCESS)
                usleep(100000);
            if(isup_resetCircuit(index+112+255) == SUCCESS)
                usleep(300000);
            if(isup_resetCircuit(index+128+255) == SUCCESS)
                usleep(300000);
            if(isup_resetCircuit(index+144+255) == SUCCESS)
                usleep(100000);
            if(isup_resetCircuit(index+160+255) == SUCCESS)
                usleep(300000);
            if(isup_resetCircuit(index+176+255) == SUCCESS)
                usleep(300000);
            if(isup_resetCircuit(index+192+255) == SUCCESS)
                usleep(100000);
            if(isup_resetCircuit(index+208+255) == SUCCESS)
                usleep(300000);
            if(isup_resetCircuit(index+224+255) == SUCCESS)
                usleep(300000);
            if(isup_resetCircuit(index+240+255) == SUCCESS)
                usleep(100000);
        }
          
 }



int initialiseIAM(IAM  *iamParam,BYTE switchType)
{
    if(iamParam == NULL)
        return EINVAL;
    memset(iamParam,0,sizeof(IAM));
    iamParam->natConInd.stat.pres = TRUE;
    iamParam->natConInd.satelliteInd = SAT_NONE;
    iamParam->natConInd.contChkInd = CONTCHK_NOTREQ;
    iamParam->natConInd.echoCntrlDevInd = ECHOCDEV_NOTINCL;
    
    iamParam->fwdCallInd.stat.pres = TRUE;
    iamParam->fwdCallInd.natIntCallInd = CALL_NAT;
    iamParam->fwdCallInd.end2EndMethInd = E2EMTH_NOMETH;
    iamParam->fwdCallInd.intInd = INTIND_NOINTW ;
    iamParam->fwdCallInd.isdnUsrPrtInd  = ISUP_USED ;
    iamParam->fwdCallInd.isdnUsrPrtPrfInd  = PREF_PREFAW;
    iamParam->fwdCallInd.isdnAccInd  = ISDNACC_ISDN;
    iamParam->fwdCallInd.sccpMethInd =SCCPMTH_NOIND;
    
    if(switchType == ANSI)
        iamParam->fwdCallInd.segInd = SEGIND_NOIND;
    
    iamParam->cgpnCat.cgPtyCat  = CAT_ORD;
    
    if(switchType == ITUT)
        iamParam->transMedReq.trMedReq = TMR_SPEECH;
    
    iamParam->cdpn.natAddrInd = NATNUM;
    iamParam->cdpn.numPlan = NP_ISDN;
    iamParam->cdpn.innInd = INN_ALLOW;
    
    iamParam->cgpn.stat.pres = TRUE;
    iamParam->cgpn.natAddrInd = NATNUM;
    iamParam->cgpn.numPlan = NP_ISDN;
    iamParam->cgpn.niInd = NBMCMLTE;
    iamParam->cgpn.presRest = PRESREST;
    iamParam->cgpn.scrnInd = USRPROV;
    
    iamParam->callRef.stat.pres = FALSE;
    iamParam->tranNetSel.stat.pres = FALSE;
    return SUCCESS;
    
}


int initialiseACM(ACM  *param,BYTE switchType)
{
    if(param == NULL)
        return EINVAL;
    memset(param,0,sizeof(ACM));
    param->backCallInd.chrgInd = CHRG_NOIND;
    param->backCallInd.cadPtyStatInd = CADSTAT_NOIND;
    param->backCallInd.cadPtyCatInd = CADCAT_ORDSUBS;
    param->backCallInd.end2EndMethInd = E2EMTH_NOMETH;
    param->backCallInd.intInd = INTIND_NOINTW;
    param->backCallInd.end2EndInfoInd = E2EINF_NOINFO;
    param->backCallInd.isdnUsrPrtInd  = ISUP_USED;
    param->backCallInd.holdInd = HOLD_NOTREQD;
    param->backCallInd.isdnAccInd = ISDNACC_ISDN;
    param->backCallInd.echoCtrlDevInd = ECHOCDEV_NOTINCL;
    param->backCallInd.sccpMethInd = SCCPMTH_NOIND;
    
    //optional parameters
    param->callRef.stat.pres =FALSE;
    param->causeInd.stat.pres = FALSE;
    param->optBckCallInd.stat.pres = FALSE;
    param->usr2UsrInd.stat.pres = FALSE;
    param->usr2UsrInfo.stat.pres = FALSE;
    
    return SUCCESS;
    
 }

int initialiseANM(ANM  *param,BYTE switchType)
{
    if(param == NULL)
        return EINVAL;
    memset(param,0,sizeof(ANM));
    //optional parameter
    param->backCallInd.stat.pres = TRUE;
    param->backCallInd.chrgInd = CHRG_NOIND;
    param->backCallInd.cadPtyStatInd = CADSTAT_NOIND;
    param->backCallInd.cadPtyCatInd = CADCAT_ORDSUBS;
    param->backCallInd.end2EndMethInd = E2EMTH_NOMETH;
    param->backCallInd.intInd = INTIND_NOINTW;
    param->backCallInd.end2EndInfoInd = E2EINF_NOINFO;
    param->backCallInd.isdnUsrPrtInd  = ISUP_USED;
    param->backCallInd.holdInd = HOLD_NOTREQD;
    param->backCallInd.isdnAccInd = ISDNACC_ISDN;
    param->backCallInd.echoCtrlDevInd = ECHOCDEV_NOTINCL;
    param->backCallInd.sccpMethInd = SCCPMTH_NOIND;
    
    param->callRef.stat.pres =FALSE;
    param->optBckCallInd.stat.pres = FALSE;
    param->usr2UsrInd.stat.pres = FALSE;
    param->usr2UsrInfo.stat.pres = FALSE;
    return SUCCESS;
    
}

int initialiseREL(REL  *param,BYTE cause,BYTE switchType)
{
    if(param == NULL)
        return EINVAL;
    memset(param,0,sizeof(REL));
    param->causeInd.location = ILOC_PRIVNETLU;
    param->causeInd.cdeStand = CSTD_CCITT;
    param->causeInd.causeVal = cause;
    
    param->redirectInfo.stat.pres = FALSE;
    param->redirectNum.stat.pres = FALSE;
    param->usr2UsrInd.stat.pres = FALSE;
    return SUCCESS;

}

int initialiseRLC(RLC  *param,BYTE switchType)
{
    if(param == NULL)
        return EINVAL;
    memset(param,0,sizeof(RLC));
    param->causeInd.stat.pres = FALSE;
    return SUCCESS;
            
}

static int parseNatConInd(BYTE msg[0],  stNatConInd & natConInd)
{
    int count =0;
    natConInd.satelliteInd = msg[count]&0x03;
    natConInd.contChkInd =(msg[count]&0x0C) >> 2;
    natConInd.echoCntrlDevInd = (msg[count++]&0x10) >> 5;
    return count;
}

static int parseFwdCallInd(BYTE msg[0],  stFwdCallInd & fwdCallInd)
{
    int count =0;
    fwdCallInd.natIntCallInd = msg[count]&0x01;
    fwdCallInd.end2EndMethInd = (msg[count]&0x06) >> 1;
    fwdCallInd.intInd = (msg[count]&0x08) >> 3;
    fwdCallInd.end2EndInfoInd = (msg[count]&0x10) >> 4;
    fwdCallInd.isdnUsrPrtInd = (msg[count]&0x20) >> 5;
    fwdCallInd.isdnUsrPrtPrfInd = (msg[count++]&0xC0) >> 6;
    fwdCallInd.isdnAccInd = msg[count]&0x01;
    fwdCallInd.sccpMethInd = (msg[count++]&0x06) >> 1;
    return count;
}

static int parseBackwdCallInd(BYTE msg[0],  stBckCalInd & backCallInd)
{
    int count =0;
    backCallInd.chrgInd = msg[count] & 0x03;
    backCallInd.cadPtyStatInd = (msg[count] & 0x0C) >> 2 ;
    backCallInd.cadPtyCatInd =  (msg[count] & 0x30) >> 4 ;
    backCallInd.end2EndMethInd = (msg[count++] & 0xC0) >> 6 ;
    backCallInd.intInd = msg[count] & 0x01;
    backCallInd.end2EndInfoInd = (msg[count] & 0x02) >> 1;
    backCallInd.isdnUsrPrtInd = (msg[count] & 0x04) >> 2;
    backCallInd.holdInd = (msg[count] & 0x08) >> 3;
    backCallInd.isdnAccInd = (msg[count] & 0x10) >> 4;
    backCallInd.echoCtrlDevInd =  (msg[count] & 0x20) >> 5;
    backCallInd.sccpMethInd = (msg[count++] & 0xC0) >> 6;
    return count;
}


static int parseCDPN(BYTE msg[0],  stCdpn & cdpn,int length)
{
    int count =0,index;
    BYTE tmpCDPN[MAX_NUMBER_LEN],tempNumber[MAX_NUMBER_LEN],num_digits;
    
    cdpn.natAddrInd =  msg[count]&0x7F;
    cdpn.oddEven = msg[count++] >> 7;
    cdpn.numPlan = (msg[count]&0x70) >> 4;
    cdpn.innInd = (msg[count++]) >> 7;
    num_digits=((length-2)*2) - cdpn.oddEven;
    for(index = 0; index < (length -2)*2; index+=2)
    {
        tmpCDPN[index] = msg[count]&0x0F;
        tmpCDPN[index + 1] = msg[count++] >> 4;
    }
    for(index=0;index<num_digits;index++)
         tempNumber[index]=tmpCDPN[index]+0x30;

    if(tempNumber[index-1]==15+0x30)
    {
            tempNumber[index-1]='\0';
    }
    else
    {
            tempNumber[index]='\0';
    }
    strcpy(cdpn.addrSig,(char *)tempNumber);
    return count;
}


static int parseCGPN(BYTE msg[0],  stCgpn & cgpn)
{
    int count =0,index=0,length =0;
    BYTE tmpCGPN[MAX_NUMBER_LEN],tempNumber[MAX_NUMBER_LEN],num_digits;
    
    cgpn.stat.pres = TRUE;
    length = msg[++count];
    count++;
    cgpn.natAddrInd =  msg[count]&0x7F;
    cgpn.oddEven = msg[count++] >> 7;
    cgpn.scrnInd = msg[count]&0x03;
    cgpn.presRest = (msg[count]&0x0C) >> 2;
    cgpn.numPlan = (msg[count]&0x70) >> 4;
    cgpn.niInd = msg[count++] >> 7;
    num_digits=((length-2)*2) - cgpn.oddEven;
    for(index=0;index<(length-2)*2;index+=2)
    {
        tmpCGPN[index]=msg[count]&0x0F;
        tmpCGPN[index+1]=msg[count++]>>4;
    }

    for(index=0;index<num_digits;index++)
    {
            tempNumber[index]=tmpCGPN[index]+0x30;
    }
    if(tempNumber[index-1]==15+0x30)
    {
            tempNumber[index-1]='\0';
    }
    else
    {
           tempNumber[index]='\0';
    }

    strcpy(cgpn.addrSig,(char *)tempNumber);
    return count;
}

static int parseCallReference(BYTE msg[0],  stCallRef & callRef)
{
    int count =0,length =0;
    callRef.stat.pres = TRUE;
    length = msg[++count];
    count++;
    callRef.callId[0] = msg[count++];
    callRef.callId[1] = msg[count++];
    callRef.callId[2] = msg[count++];
    callRef.pntCde[0] = msg[count++];
    callRef.pntCde[1] = (msg[count++] & 0x3F);
    return count;
}

static int parseOptFwdCallInd(BYTE msg[0],  stOpFwdCalInd & opFwdCallInd)
{
    int count =0,length =0;
    opFwdCallInd.stat.pres = TRUE;
    length = msg[++count];
    count++;
    opFwdCallInd.clsdUGrpCaInd = msg[count]&0x03;
    opFwdCallInd.segInd = (msg[count]&0x04) >> 2;
    opFwdCallInd.connAddrReqInd = (msg[count]&0x80) >> 7;
    return count;
}

static int parseRedirectingNum(BYTE msg[0],  stRedirectingNum & redirectingNum)
{
    int count =0,index=0,length =0;
    BYTE red_digits[MAX_NUMBER_LEN],tempNumber[MAX_NUMBER_LEN],num_digits;
    
    redirectingNum.stat.pres = TRUE;
    length = msg[++count];
    count++;
    redirectingNum.natAddr = msg[count]&0x7F;
    redirectingNum.oddEven = (msg[count++]&0x80) >> 7;
    redirectingNum.presRest = (msg[count]&0x0C) >> 2;
    redirectingNum.numPlan = (msg[count++]&0x70) >> 4;
    num_digits=((length-2)*2) - redirectingNum.oddEven;
    for(index=0;index<(length-2)*2;index+=2)
    {
            red_digits[index]=msg[count]&0x0F;
            red_digits[index+1]=msg[count++]>>4;
    }

    for(index=0;index<num_digits;index++)
    {
            tempNumber[index]=red_digits[index]+0x30;
    }
    if(tempNumber[index-1]==15+0x30)
    {
            tempNumber[index-1]='\0';
    }
    else
    {
            tempNumber[index]='\0';
    }
    strcpy(redirectingNum.addrSig,(char *)tempNumber);
    return count;
}


static int parseRedirectionNum(BYTE msg[0],  stRedirNum & redirectNum)
{
    int count =0,index=0,length =0;
    BYTE cli_digits[MAX_NUMBER_LEN],red_number[MAX_NUMBER_LEN],num_digits;
    redirectNum.stat.pres = TRUE;
    length = msg[++count];
    count++;
    redirectNum.natAddr =  msg[count] & 0x7F;
    redirectNum.oddEven = (msg[count++] & 0x80) >> 7;
    redirectNum.numPlan = (msg[count] & 0x70) >> 4;
    redirectNum.innInd = (msg[count++] & 0x80) >> 7;
    num_digits=((length-2)*2)-(msg[count]>>7);
    for(index=0;index<(length-2)*2;index+=2)
    {
        cli_digits[index]=msg[count]&0x0F;
        cli_digits[index+1]=msg[count++]>>4;
    }
    for(index=0;index<num_digits;index++)
    {
        red_number[index]=cli_digits[index]+0x30;
    }
    if(called_number[index-1]==15+0x30)
    {
        red_number[index-1]='\0';
    }
    else
    {
        red_number[index]='\0';
    }
    strcpy(redirectNum.addrSig,(char *)red_number);
    
    return count;
}

static int parseConnRequest(BYTE msg[0],  stConnReq & connReq)
{
    int count =0,length =0;
    connReq.stat.pres = TRUE;
    length = msg[++count];
    count++;
    connReq.locRef[0] = msg[count++];
    connReq.locRef[1] = msg[count++];
    connReq.locRef[2] = msg[count++];
    connReq.pntCde[0] = msg[count++];
    connReq.pntCde[1] = (msg[count++] & 0x3F);
    connReq.protClass = msg[count++];
    connReq.credit = msg[count++];
    return count;
}

static int parseTransitNetworkSel(BYTE msg[0],  stTranNetSel & tranNetSel)
{
    int count =0,index=0,length =0;
    BYTE red_digits[MAX_NUMBER_LEN],tempNumber[MAX_NUMBER_LEN],num_digits;
    tranNetSel.stat.pres = TRUE;
    length = msg[++count];
    count++;
    tranNetSel.netIdPln = msg[count] & 0x0F;
    tranNetSel.typNetId = (msg[count] & 0x70) >> 4;
    tranNetSel.oddEven = (msg[count++] & 0x80) >> 7;
    num_digits=((length-1)*2) - tranNetSel.oddEven;
    for(index=0;index<(length-1)*2;index+=2)
    {
        red_digits[index]=msg[count]&0x0F;
        red_digits[index+1]=msg[count++]>>4;
    }

    for(index=0;index<num_digits;index++)
    {
        tempNumber[index]=red_digits[index]+0x30;
    }
    if(tempNumber[index-1]==15+0x30)
    {
        tempNumber[index-1]='\0';
    }
    else
    {
        tempNumber[index]='\0';
    }
    strcpy(tranNetSel.netId,(char *)tempNumber);
    return count;
}



static int parseRedirectionInfo(BYTE msg[0],  stRedirInfo & redirectInfo)
{
    int count =0,length =0;
    redirectInfo.stat.pres = TRUE;
    length = msg[++count];
    count++;
    redirectInfo.redirInd = msg[count] & 0x07;
    redirectInfo.origRedirReas = (msg[count++] & 0xF0) >> 4;
    redirectInfo.redirCnt = msg[count] & 0x07;
    redirectInfo.redirReas = (msg[count++] & 0xF0) >> 4;
    return count;
}

static int parseUser2UserInd(BYTE msg[0],  stUsr2UsrInd & usr2UsrInd)
{
    int count =0,length =0;
    usr2UsrInd.stat.pres = TRUE;
    length = msg[++count];
    count++;
    usr2UsrInd.type = msg[count] & 0x01;
    usr2UsrInd.serv1 = (msg[count] & 0x06) >> 1;
    usr2UsrInd.serv2 = (msg[count] & 0x18) >> 3;
    usr2UsrInd.serv3 = (msg[count++] & 0x60) >> 5;
    return count;
}

static int parseUser2UserInfo(BYTE msg[0],  stUsr2UsrInfo & usr2UsrInfo)
{
     int count =0,index=0,length =0;
    BYTE red_digits[MAX_NUMBER_LEN],tempNumber[MAX_NUMBER_LEN],num_digits;
    usr2UsrInfo.stat.pres = TRUE;
    length = msg[++count];
    count++;
    num_digits=((length)*2);
    for(index=0;index<(length)*2;index+=2)
    {
        red_digits[index]=msg[count]&0x0F;
        red_digits[index+1]=msg[count++]>>4;
    }

    for(index=0;index<num_digits;index++)
    {
        tempNumber[index]=red_digits[index]+0x30;
    }

    tempNumber[index]='\0';

    strcpy(usr2UsrInfo.info,(char *) tempNumber);
    return count;
}

static int parseOptBackwdCallInd(BYTE msg[0],  stOptBckCalInd & optBckCallInd)
{
    int count =0,length =0;
    optBckCallInd.stat.pres = TRUE;
    length = msg[++count];
    count++;
    optBckCallInd.stat.pres = TRUE;
    optBckCallInd.inbndInfoInd = msg[count] & 0x01;
    optBckCallInd.callDiverMayOcc = (msg[count] & 0x02) >> 1;
    optBckCallInd.segInd = (msg[count] & 0x04) >> 2;
    optBckCallInd.mlppUsrInd = (msg[count++] & 0x8) >> 3;
    return count;
}


static int parseCauseInd(BYTE msg[0],  stCauseDgn & causeInd)
{
    int count =0,length =0,index =0;
    BYTE diagnostics[100],temp[100],num_digits;
    
    causeInd.stat.pres = TRUE;
    length = msg[++count];
    count++;
    causeInd.location = msg[count] & 0x0F;
    causeInd.cdeStand = (msg[count++] & 0x60) >> 5;
    causeInd.causeVal = msg[count++] & 0x7F;
    if(length > 0x02)
    {
        num_digits=((length-2)*2);
        for(index=0;index<(length-2)*2;index+=2)
        {
            diagnostics[index]=msg[count]&0x0F;
            diagnostics[index+1]=msg[count++]>>4;
        }
        for(index=0;index<num_digits;index++)
        {
            temp[index]=diagnostics[index]+0x30;
        }
        temp[index]='\0';
        strcpy(causeInd.dgnVal,(char *) temp);
    }
    return count;
}


static int parseGenericNotifInd(BYTE msg[0],  stGenNotiInd & genNotiInd)
{
    int count =0,length =0,index =0;
    genNotiInd.stat.pres = TRUE;
    length = msg[++count];
    count++;
    for(index=0;index<length;index++)
    {
        genNotiInd.notifyInd[index] = msg[count++];
    }
    return count;
}

static int parseSignalingPointCode(BYTE msg[0],  stSigPointCode & sigPointCode)
{
    int count =0,length =0;
    sigPointCode.stat.pres = TRUE;
    length = msg[++count];
    count++;
    sigPointCode.sigPointCode[0] = msg[count++];
    sigPointCode.sigPointCode[1] = (msg[count++] & 0x3F) ;
    return count;
}

static int parseAutoCongLevel(BYTE msg[0],  stAutoCongLvl & autoCongLvl)
{
    int count =0,length =0;
    autoCongLvl.stat.pres = TRUE;
    length = msg[++count];
    count++;
    autoCongLvl.auCongLvl = msg[count++];
    return count;
}


static int parseAccessTransport(BYTE msg[0],  stAccTrnspt & accTransport)
{
    int count =0,length =0,index =0;
    BYTE red_number[MAX_NUMBER_LEN],cli_digits[MAX_NUMBER_LEN],num_digits;
    accTransport.stat.pres = TRUE;
    length = msg[++count];
    count++;
    num_digits=((length)*2);
    for(index=0;index<(length)*2;index+=2)
    {
            cli_digits[index]=msg[count]&0x0F;
            cli_digits[index+1]=msg[count++]>>4;
    }
    for(index=0;index<num_digits;index++)
    {
            red_number[index]=cli_digits[index]+0x30;
    }
    if(called_number[index-1]==15+0x30)
    {
            red_number[index-1]='\0';
    }
    else
    {
            red_number[index]='\0';
    }
    strcpy(accTransport.infoElmts,(char *)red_number);
    return count;
}


int parseIAM(unsigned char msg[0],IAM  *param)
{
int count = 1;
BYTE length,ptrVar,ptrOptional;
        if(param == NULL)
        {
            sprintf(sigmsgLogger.applMsgTxt,"IAM Param is NULL on cic" ,msg[count]);
            logWarning(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_1);
            return EINVAL;
        }
        memset(param,0,sizeof(IAM));
        //Nature of connection indicator
        count += parseNatConInd(&msg[count],param->natConInd);
        
        //Forward call indicator
        count += parseFwdCallInd(&msg[count],param->fwdCallInd);
        
        //Calling party category
        param->cgpnCat.cgPtyCat = msg[count++];
        
        //Transmission medium requirement
        param->transMedReq.trMedReq = msg[count++];
        
        ptrVar = msg[count++]; 
        if(ptrVar == 0x00)
        {
            sprintf(sigmsgLogger.applMsgTxt,"Mandatory variable IAM parameter missing on CIC  " ,msg[count]);
            logWarning(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_1);
            return EINVAL;
        }
        ptrOptional = msg[count++];
        
        //Called party Number
        length = msg[count++];
        if(length == 0x00) //No mandatory variable param
        {
            sprintf(sigmsgLogger.applMsgTxt,"Mandatory variable IAM parameter missing" ,msg[count]);
            logWarning(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_1);
            return EINVAL;
        }
        count += parseCDPN(&msg[count],param->cdpn,length);
        
        if(ptrOptional == 0x00)
            return SUCCESS;
        while(msg[count] != 0x00)
        {
                switch(msg[count])
                {
                    case CALLING_PARTY_NUMBER:
                        count += parseCGPN(&msg[count],param->cgpn);
                        break;
                    case CALL_REFERENCE:
                        count += parseCallReference(&msg[count],param->callRef);
                        break;
                   case OPTIONAL_FORWARD_CALL_INDICATOR:
                        count += parseOptFwdCallInd(&msg[count],param->opFwdCallInd);
                        break;
                    case REDIRECTING_NUMBER:
                        count += parseRedirectingNum(&msg[count],param->redirectingNum);
                        break;
                    case CONNECTION_REQUEST:
                        count += parseConnRequest(&msg[count],param->connReq);
                        break;
                    case REDIRECTION_INFORMATION:
                        count += parseRedirectionInfo(&msg[count],param->redirectInfo);
                        break;
                    case TRANSIT_NETWORK_SELECTION:
                        count += parseTransitNetworkSel(&msg[count],param->tranNetSel);
                        break;
                        
                    case USER2USER_INDICATOR:
                        count += parseUser2UserInd(&msg[count],param->usr2UsrInd);
                        break;
                        
                    case USER2USER_INFO:
                        count += parseUser2UserInfo(&msg[count],param->usr2UsrInfo);
                        break;
                        
                    default:
                        sprintf(sigmsgLogger.applMsgTxt,"Unsupported optional parameter %x on port %d trunk %d" ,msg[count]);
                        logWarning(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_1);
                        length = msg[++count];
                        count = count + length;
                        count++;
                        break;
                }
                
         }
       
}

int parseACM(unsigned char msg[1000],ACM *param)
{
BYTE length;
int count = 1,index,i;
        if(param == NULL)
            return EINVAL;
        count += parseBackwdCallInd(&msg[count],param->backCallInd);
        
        if(msg[count] == 0x00)
        {
              return SUCCESS;
        }

        count++;

        while(msg[count] != 0x00)
        {
                switch(msg[count])
                {

                        case OPTIONAL_BACKWARD_CALL_INDICATOR:
                            count += parseOptBackwdCallInd(&msg[count],param->optBckCallInd);
                            break;

                        case CALL_REFERENCE:
                            count += parseCallReference(&msg[count],param->callRef);
                            break;

                        case CAUSE_INDICATOR:
                            count += parseCauseInd(&msg[count],param->causeInd);                            
                            break;
                        case REDIRECTION_NUMBER:
                            count += parseRedirectionNum(&msg[count],param->redirectNum);
                            break;
                        
                        case USER2USER_INDICATOR:
                            count += parseUser2UserInd(&msg[count],param->usr2UsrInd);
                            break;

                        case USER2USER_INFO:
                            count += parseUser2UserInfo(&msg[count],param->usr2UsrInfo);
                            break;
                            
                        case GENERIC_NOTIFICATION_INDICATOR:
                            count += parseGenericNotifInd(&msg[count],param->genNotiInd);
                            break;
                        
                        default:
                             sprintf(sigmsgLogger.applMsgTxt,"Unsupported optional parameter %x on port %d trunk %d" ,msg[count]);
                             logWarning(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_1);
                             length = msg[++count];
                             count = count + length;
                             count++;
                             break;
                }
        }

}


int parseANM(unsigned char msg[1000],ANM *param)
{
int count = 1,i;
BYTE length;

        if(param == NULL)
                return EINVAL;
        if(msg[count] == 0x00)
        {
              return SUCCESS;
        }

        count++;

        while(msg[count] != 0x00)
        {
                switch(msg[count])
                {
                        case OPTIONAL_BACKWARD_CALL_INDICATOR:
                            count += parseOptBackwdCallInd(&msg[count],param->optBckCallInd);
                            break;
                        case CALL_REFERENCE:
                            count += parseCallReference(&msg[count],param->callRef);
                            break;
                        case REDIRECTION_NUMBER:
                            count += parseRedirectionNum(&msg[count],param->redirectNum);
                            break;
                       
                        case USER2USER_INDICATOR:
                            count += parseUser2UserInd(&msg[count],param->usr2UsrInd);
                            break;
                        
                        case USER2USER_INFO:
                            count += parseUser2UserInfo(&msg[count],param->usr2UsrInfo);
                            break;
                         case GENERIC_NOTIFICATION_INDICATOR:
                            count += parseGenericNotifInd(&msg[count],param->genNotiInd);
                            break;
                        default:
                             sprintf(sigmsgLogger.applMsgTxt,"Unsupported optional parameter %d" ,msg[count]);
                             logWarning(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_1);
                             length = msg[++count];
                             count = count + length;
                             count++;
                             break;
                }
        }

}

int parseREL(unsigned char msg[1000],REL *param)
{
int count = 1,index,i;
BYTE length,diagnostics[100];
BYTE temp[100],red_number[MAX_NUMBER_LEN];
BYTE num_digits,cli_digits[20];
BYTE ptrVar,ptrOptional;
        if(param == NULL)
                return EINVAL;

        ptrVar = msg[count++];
        
        if(ptrVar == 0x00)
        {
            sprintf(sigmsgLogger.applMsgTxt,"Mandatory variable REL parameter missing on CIC");
            logWarning(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_1);
            return EINVAL;
        }
        
        ptrOptional = msg[count++];
        length =  msg[count++];
        
        param->causeInd.location = msg[count] & 0x0F;
        param->causeInd.cdeStand = (msg[count++] & 0x60) >> 5;
        param->causeInd.causeVal = msg[count++] & 0x7F;

        if(length > 0x02)
        {
            num_digits=((length-2)*2);
            for(index=0;index<(length-2)*2;index+=2)
            {
                    diagnostics[index]=msg[count]&0x0F;
                    diagnostics[index+1]=msg[count++]>>4;
            }

            for(index=0;index<num_digits;index++)
            {
                    temp[index]=diagnostics[index]+0x30;
            }

            temp[index]='\0';

            strcpy(param->causeInd.dgnVal,(char *) temp);
        }

        count++;

        while(msg[count] != 0x00)
        {
                switch(msg[count])
                {
                    case REDIRECTION_INFORMATION:
                        count += parseRedirectionInfo(&msg[count],param->redirectInfo);
                        break;
                    case REDIRECTION_NUMBER:
                        count += parseRedirectionNum(&msg[count],param->redirectNum);
                        break;
                    case ACCESS_TRANSPORT:
                        count += parseRedirectionNum(&msg[count],param->redirectNum);
                        break;
                       
                    case SIGNALLING_POINT_CODE:
                        count += parseSignalingPointCode(&msg[count],param->sigPointCode);
                        break;
                        
                    case AUTO_CONGESTION_LEVEL:
                        count += parseAutoCongLevel(&msg[count],param->autoCongLvl);
                        break;
                 
                     default:
                        sprintf(sigmsgLogger.applMsgTxt,"Unsupported optional parameter %x on port %d trunk %d" ,msg[count]);
                        logWarning(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_1);
                        length = msg[++count];
                        count = count + length;
                        count++;
                        break;

                }
        }

}




int parseRLC(unsigned char msg[1000],RLC *param)
{
int count = 1;
BYTE length;
        if(param == NULL)
                return EINVAL;
        if(msg[count] == 0x00)
        {
                  return SUCCESS;
        }

        count++;

#if(1)

        while(msg[count] != 0x00)
        {
                switch(msg[count])
                {
                        case CAUSE_INDICATOR:
                             count += parseCauseInd(&msg[count],param->causeInd);                            
                             break;

                        default:
                             sprintf(sigmsgLogger.applMsgTxt,"Unsupported optional parameter %x on port %d trunk %d" ,msg[count]);
                             logWarning(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_1);
                             length = msg[++count];
                             count = count + length;
                             count++;
                             break;
                }
        }
#endif
}



void * waitForDTMF(void * portNo)
{
time_t timer1,timer2;
Msg_Packet msg;
int dtmfCounter = 0,count = 0;
std::hash_map<int,Digit_Data>::iterator dtmfHashItr;
unsigned short *port =(unsigned short *) portNo;
unsigned short cic = *port;
        time(&timer1);
        msg.id1=BOX_ID1;
        msg.id2=BOX_ID2;
        msg.length=(FIXED_SIZE) << 8;
        msg.subfield=0;
        msg.debug=0;
        msg.type= cic;
        dtmfHashItr = dtmfRec.findRecord(msg.type);
        if(dtmfHashItr != dtmfRec.hashEnd())
        {  
            while(1)
            {
                time(&timer2);
                if(difftime(timer2,timer1) > dtmfHashItr->second.maxTimeout)
                {
                        msg.opcode=DTMF_TIMEOUT;
                        if(addMsgToSigSlaveQue(msg) != SUCCESS)
                        {
                            sprintf(sigmsgLogger.applMsgTxt,"Failed to add DTMF TIMEOUT in signaling Msg que. Error no : %d desc: %s" ,errno,getErrDesc(errno));
                            logErr(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_0);
                        }
                        dtmfThread[msg.type] = FALSE;
                        break;
                }
                else if(dtmfFlag[msg.type] == TRUE)
                {
                        for(count = 0; count < dtmfHashItr->second.termLength; count++ )
                        {
                            if(dtmfBuf[msg.type] == dtmfHashItr->second.termDigits[count])
                            {
                                    dtmfHashItr->second.max_digits_to_be_collected = dtmfCounter;
                                    break;
                            }
                        }
                        time(&timer1);
                        dtmfHashItr->second.maxTimeout = dtmfHashItr->second.inter_digit_timeout; //after pressing first digit wait for inter digit timeout
                        dtmfFlag[msg.type] = FALSE;
                        msg.msg[dtmfCounter + 14] = dtmfBuf[msg.type];
                        msg.msg[13] = ++dtmfCounter;
                        
                }
                if(dtmfHashItr->second.max_digits_to_be_collected  == dtmfCounter)
                {
                        msg.opcode=PIC_DTMF_CALLPROC;
                        if(addMsgToSigSlaveQue(msg) != SUCCESS)
                        {
                            sprintf(sigmsgLogger.applMsgTxt,"Failed to add Collected DTMF in signaling Msg que. Error no : %d desc: %s" ,errno,getErrDesc(errno));
                            logErr(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_0);
                        }
                        dtmfThread[msg.type] = FALSE;
                        break;
                }
                if(releaseFlag[msg.type] == TRUE)
                {
                        releaseFlag[msg.type] = FALSE;
                        dtmfThread[msg.type] = FALSE;
                        break;
                }
            }
            dtmfRec.removeRecord(cic);
        }
        releaseFlag[msg.type] = FALSE;
        dtmfThread[msg.type] = FALSE;
        dtmfFlag[msg.type] = FALSE;

}

int parseDTMFTermCond(Digit_Data *dtmf,IOTERM * term)
{
    int count = 0;
    switch(term->termNo)
    {
        case DEV_MAXDTMF:
            if( term->length > MAX_DIGIT_BUFFER)
                dtmf->max_digits_to_be_collected = MAX_DIGIT_BUFFER;
            else
                dtmf->max_digits_to_be_collected = term->length;
            break;
        case DEV_MAX_TIMEOUT:
            dtmf->maxTimeout = term->length;
            break;
        case DEV_INTER_DIGIT_TIMEOUT:
            dtmf->inter_digit_timeout = term->length;
            break;
        case DEV_TERM_DIGIT:
            if(term->length  > MAX_TERM_DIGIT)
                dtmf->termLength = MAX_TERM_DIGIT;
            else
                dtmf->termLength = term->length ;
            
            for(count = 0; count < dtmf->termLength;count++)
            {
                dtmf->termDigits[count] = term->data[count];
            }
            break;
        default:
            break;
    }
}


int fillDTMFParam(Digit_Data *dtmf,IOTERM * term)
{
    int count = 0;
    for(count = 0; count < MAX_TERM_COND ; count++)
    {
        if(term[count].type = IO_CONTINUE)
        {
            parseDTMFTermCond(dtmf,&term[count]);
            
        }
        else if(term[count].type = IO_END_TERM)
        {
            parseDTMFTermCond(dtmf,&term[count]);
            break;
        }
        else
            break;
    }
    
}


int startDTMFTermThread(unsigned int port,IOTERM *term)
{
    Digit_Data dtmf;
    int ret =0;
    memset(&dtmf,0,sizeof(dtmf));
    dtmf.port = port;
    fillDTMFParam(&dtmf,term);
    
    //dtmf.inter_digit_timeout = timeout;
    //dtmf.max_digits_to_be_collected = term.length;
   // printf("####DTMF MAX %d  %x %x \n",dtmf.max_digits_to_be_collected,port,ss7_rx_msg.msg[0]);
    if(dtmf.max_digits_to_be_collected > 0 && dtmf.max_digits_to_be_collected < MAX_DIGIT_BUFFER)
    {
        dtmfRec.insertRecord(port,dtmf);
        if(pthread_create(&dtmfHandle,NULL,&waitForDTMF,(void *)&dtmf.port)==-1)
        {
            sprintf(sigmsgLogger.applMsgTxt,"Failed to create DTMF Thread. Error no : %d desc: %s" ,errno,getErrDesc(errno));
            logWarning(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_0);
            dtmfRec.removeRecord(port);
            return errno;

        }
        dtmfThread[dtmf.port] = TRUE;
        dtmfFlag[dtmf.port] = FALSE;
        releaseFlag[dtmf.port] = FALSE;
        if((ret = pthread_detach(dtmfHandle)) != 0)
                perror("Failed to detach : ");

    }
    return SUCCESS;

}



int  sendDTMF2Box(unsigned short port,unsigned short dtmf)
{
struct  Ss7msg_chStatus dtmfTx;
int ret =0;
    //printf("********************************************send DTMF %d on port %d \n",dtmf,port);
    sprintf(sigmsgLogger.applMsgTxt,"send DTMF2BOX %d on port %d \n",dtmf,port);
    logInfo(_LFF,sigmsgLogger,APPL_LOG,LOG_LEVEL_2);
    memset(&dtmfTx,0,sizeof(Ss7msg_chStatus));
    //dtmfTx.type=TCP_TYPE;
    dtmfTx.length=htons((FIXED_SIZE+6));
    dtmfTx.opcode=htons(PIC_DTMF);
    dtmfTx.subfield=0x00;
    dtmfTx.debug=0x00;
    dtmfTx.optionalTimeSlot=htons(port);
    dtmfTx.timeSlot=htons(port);
    dtmfTx.data = htons(dtmf);
    ret = send_msg(&dtmfTx);
    return ret;
}

static int setCallingPartyNum(BYTE msg[0],stCgpn &cgpn,char * source_number)
{
    int counter = 0,index =0;
    char temp[MAX_NUMBER_LEN];
    BYTE cgpnTmp[MAX_NUMBER_LEN];
    memset(cgpnTmp,0,sizeof(cgpnTmp));
    memset(temp,0,sizeof(temp));
    msg[counter++]=CALLING_PARTY_NUMBER; // optional parameter code
    msg[counter++]= 2 + (strlen(source_number)/2) + (strlen(source_number)%2); // length
    if(strlen(source_number)%2 != 0)
    {
       cgpn.oddEven = NMB_ODD;
       msg[counter++] = cgpn.natAddrInd | (cgpn.oddEven << 7);
    }
    else
    {
       cgpn.oddEven = NMB_EVEN;
       msg[counter++] = cgpn.natAddrInd | (cgpn.oddEven << 7);
    }

    msg[counter++] = cgpn.scrnInd | (cgpn.presRest <<2) |(cgpn.numPlan << 4) | (cgpn.niInd << 7);
    strcpy(temp,source_number);
    for(index = 0; index < strlen(source_number); index++)
         cgpnTmp[index] = temp[index] - 48;
    for(index=0;index< (strlen(source_number) + strlen(source_number) %2);index+=2)
    {
            msg[counter++]=((cgpnTmp[index+1] << 4) | cgpnTmp[index] );
    }
    
    return counter;
}


static int setTransitNetSel(BYTE msg[0],stTranNetSel &tranNetSel)
{
    int counter =0,index =0;
    BYTE tempbyte[1000];
    char temp[MAX_NUMBER_LEN];
    memset(temp,0,sizeof(temp));
    msg[counter++]=TRANSIT_NETWORK_SELECTION; // optional parameter code
    msg[counter++]= 1+ strlen(tranNetSel.netId)/2 + strlen(tranNetSel.netId)%2;
    if(tranNetSel.netId != NULL)
    {
        if(strlen(tranNetSel.netId)%2 != 0)
              tranNetSel.oddEven = NMB_ODD;
         else
              tranNetSel.oddEven = NMB_EVEN;
    }
    msg[counter++]=  tranNetSel.netIdPln | (tranNetSel.typNetId << 4) | (tranNetSel.oddEven << 7);
    if(tranNetSel.netId != NULL)
    {
       strcpy(temp,tranNetSel.netId);
       for(index = 0; index < strlen(tranNetSel.netId); index++)
            tempbyte[index] = temp[index] - 48;
       for(index=0;index< (strlen(tranNetSel.netId) + strlen(tranNetSel.netId) %2);index+=2)
       {
               msg[counter++]=((tempbyte[index+1] << 4) | tempbyte[index] );
       }
    }
    return counter;
}


static int setCallReference(BYTE msg[0],stCallRef &callRef)
{
    int counter =0;
    msg[counter++]=CALL_REFERENCE; // optional parameter code
    msg[counter++]= 5;
    msg[counter++]= callRef.callId[0];
    msg[counter++]= callRef.callId[1];
    msg[counter++]= callRef.callId[2];
    msg[counter++]= callRef.pntCde[0];
    msg[counter++]= callRef.pntCde[1];
    return counter;
}


static int setOptFwdCallInd(BYTE msg[0],stOpFwdCalInd &opFwdCallInd)
{
    int counter =0;
    msg[counter++]= OPTIONAL_FORWARD_CALL_INDICATOR; // optional parameter code
    msg[counter++]= 1;
    msg[counter++]= opFwdCallInd.clsdUGrpCaInd | (opFwdCallInd.segInd << 2) | (opFwdCallInd.clidReqInd << 7);
    return counter;
}


static int setRedirectionNumber(BYTE msg[0],stRedirNum &redirectNum)
{
    int counter =0,index =0;
    BYTE tempbyte[1000];
    char temp[MAX_NUMBER_LEN];
    memset(temp,0,sizeof(temp));
    msg[counter++]=REDIRECTION_NUMBER; // optional parameter code
    if(redirectNum.addrSig != NULL)
        msg[counter++]= 2 + strlen(redirectNum.addrSig)/2 + strlen(redirectNum.addrSig) %2;
    else
        msg[counter++]= 3;
    if(redirectNum.addrSig != NULL)
    {
        if(strlen(redirectNum.addrSig)%2 != 0)
              redirectNum.oddEven = NMB_ODD;
         else
              redirectNum.oddEven = NMB_EVEN;
    }
    msg[counter++]=  redirectNum.natAddr | (redirectNum.oddEven << 7);
    msg[counter++]= (redirectNum.numPlan << 4) | (redirectNum.innInd << 7);
    if(redirectNum.addrSig != NULL)
    {
        strcpy(temp,redirectNum.addrSig);
        for(index = 0; index < strlen(redirectNum.addrSig); index++)
             tempbyte[index] = temp[index] - 48;
        for(index=0;index< (strlen(redirectNum.addrSig) + strlen(redirectNum.addrSig) %2);index+=2)
        {
                msg[counter++]=((tempbyte[index+1] << 4) | tempbyte[index] );
        }
     }
     else
        msg[counter++]=0x00;
    return counter;
}


static int setRedirectingNumber(BYTE msg[0],stRedirectingNum &redirectingNum)
{
    int counter =0,index =0;
    BYTE tempbyte[1000];
    char temp[MAX_NUMBER_LEN];
    memset(temp,0,sizeof(temp));
    msg[counter++]=REDIRECTING_NUMBER; // optional parameter code
    if(redirectingNum.addrSig != NULL)
       msg[counter++]= 2 + strlen(redirectingNum.addrSig)/2 + strlen(redirectingNum.addrSig) %2;
    else
        msg[counter++]= 3;
    if(redirectingNum.addrSig != NULL)
    {
        if(strlen(redirectingNum.addrSig)%2 != 0)
              redirectingNum.oddEven = NMB_ODD;
         else
              redirectingNum.oddEven = NMB_EVEN;
    }
    msg[counter++]=  redirectingNum.natAddr | (redirectingNum.oddEven << 7);
    msg[counter++]= (redirectingNum.presRest << 2) |(redirectingNum.numPlan << 4);
   if(redirectingNum.addrSig != NULL || redirectingNum.addrSig != 0x00)
   {
       strcpy(temp,redirectingNum.addrSig);
       for(index = 0; index < strlen(redirectingNum.addrSig); index++)
            tempbyte[index] = temp[index] - 48;
       for(index=0;index< (strlen(redirectingNum.addrSig) + strlen(redirectingNum.addrSig) %2);index+=2)
       {
               msg[counter++]=((tempbyte[index+1] << 4) | tempbyte[index] );
       }
   }
   else
       msg[counter++]=0x00;
    return counter;
}

static int setRedirectionInfo(BYTE msg[0],stRedirInfo &redirectInfo)
{
    int counter =0;
    msg[counter++]=REDIRECTION_INFORMATION; // optional parameter code
    msg[counter++]= 2;
    msg[counter++]= redirectInfo.redirInd | (redirectInfo.origRedirReas << 4);
    if(redirectInfo.redirCnt <= 0)
        redirectInfo.redirCnt = 1;
    msg[counter++] = redirectInfo.redirCnt | (redirectInfo.redirReas << 4);
    return counter;
}

static int setUser2UserInfo(BYTE msg[0],stUsr2UsrInfo &usr2UsrInfo)
{
    int counter =0,index =0;
    BYTE tempbyte[1000];
    char temp[MAX_NUMBER_LEN];
    memset(temp,0,sizeof(temp));
    msg[counter++]=USER2USER_INFO; // optional parameter code
    if(usr2UsrInfo.info != NULL || usr2UsrInfo.info != 0x00 )
    {
        msg[counter++]= strlen(usr2UsrInfo.info)/2 + strlen(usr2UsrInfo.info) %2;
    }
    else
        msg[counter++]= 0x01;
    if(usr2UsrInfo.info != NULL || usr2UsrInfo.info != 0x00 )
    {
         strcpy(temp,usr2UsrInfo.info);
         for(index = 0; index < strlen(usr2UsrInfo.info); index++)
           tempbyte[index] = temp[index] - 48;
         for(index=0;index< (strlen(usr2UsrInfo.info) + strlen(usr2UsrInfo.info) %2);index+=2)
         {
              msg[counter++]=((tempbyte[index+1] << 4) | tempbyte[index] );
         }
    }
    else
        msg[counter++]= 0x00;
    return counter;
}


static int setUser2UserInd(BYTE msg[0],stUsr2UsrInd &usr2UsrInd)
{
    int counter =0,index =0;
    msg[counter++]=USER2USER_INDICATOR; // optional parameter code
    msg[counter++]= 1;
    msg[counter++]= usr2UsrInd.type | (usr2UsrInd.serv1 << 1) | (usr2UsrInd.serv2 << 3) |
                             (usr2UsrInd.serv3 << 5);
    return counter;
}


static int setAccessTransport(BYTE msg[0],stAccTrnspt &accTransport)
{
    int counter =0,index =0;
    BYTE tempbyte[1000];
    char temp[MAX_NUMBER_LEN];
    memset(temp,0,sizeof(temp));
    msg[counter++]=ACCESS_TRANSPORT; // optional parameter code
    if(accTransport.infoElmts != NULL || accTransport.infoElmts != 0x00 )
    {
        msg[counter++]= strlen(accTransport.infoElmts)/2 | strlen(accTransport.infoElmts) %2;
    }
    else
        msg[counter++]= 0x01;
    if(accTransport.infoElmts != NULL || accTransport.infoElmts!= 0x00 )
    {
         strcpy(temp,accTransport.infoElmts);
         for(index = 0; index < strlen(accTransport.infoElmts); index++)
           tempbyte[index] = temp[index] - 48;
         for(index=0;index< (strlen(accTransport.infoElmts) + strlen(accTransport.infoElmts) %2);index+=2)
         {
              msg[counter++]=((tempbyte[index+1] << 4) | tempbyte[index] );
         }
    }
    else
        msg[counter++]= 0x00;
    
    return counter;
}


static int setCauseInd(BYTE msg[0],stCauseDgn &causeInd)
{
    int counter =0,index =0;
    BYTE tempbyte[1000];
    char temp[MAX_NUMBER_LEN];
    memset(temp,0,sizeof(temp));
    msg[counter++]=CAUSE_INDICATOR; // optional parameter code
    if(causeInd.dgnVal != NULL)
        msg[counter++]=  2 + strlen(causeInd.dgnVal)/2 + strlen(causeInd.dgnVal)%2;
    else
        msg[counter++]=  2;

    msg[counter++]= causeInd.location + (causeInd.cdeStand << 5);

    msg[counter++]= causeInd.causeVal;

    if(causeInd.dgnVal != NULL)
    {
        strcpy(temp,causeInd.dgnVal);
        for(index = 0; index < strlen(causeInd.dgnVal); index++)
             tempbyte[index] = temp[index] - 48;
        for(index=0;index< (strlen(causeInd.dgnVal) + strlen(causeInd.dgnVal) %2);index+=2)
        {
                msg[counter++]=((tempbyte[index+1] << 4) | tempbyte[index] );
        }
    }
    return counter;
}


static int setClosedUserGrpInterlockCode(BYTE msg[0],stClosedugIntCode &closedugIntCode)
{
    int counter =0;
    msg[counter++]= CLOSED_USER_GROUP_INTERLOCK_CODE;
    msg[counter++]= 4;
    msg[counter++]= closedugIntCode.dig2 | (closedugIntCode.dig1 << 4);
    msg[counter++]= closedugIntCode.dig4 | (closedugIntCode.dig3 << 3);
    msg[counter++]= closedugIntCode.binCde[0];
    msg[counter++]= closedugIntCode.binCde[1];
    return counter;
}

static int setConnectionRequest(BYTE msg[0],stConnReq &connReq)
{
    int counter =0;
    msg[counter++]= CONNECTION_REQUEST; // optional parameter code
    msg[counter++]= 7;
    msg[counter++]= connReq.locRef[0];
    msg[counter++]= connReq.locRef[1];
    msg[counter++]= connReq.locRef[2];
    msg[counter++]= connReq.pntCde[0];
    msg[counter++]= connReq.pntCde[1];
    msg[counter++]= connReq.protClass;
    msg[counter++]= connReq.credit;
    return counter;
}

static int setOptionalBackwdCallInd(BYTE msg[0],stOptBckCalInd &optBckCallInd)
{
    int counter =0;
    msg[counter++]= OPTIONAL_BACKWARD_CALL_INDICATOR; // optional parameter code
    msg[counter++]= 1;
    msg[counter++]= optBckCallInd.inbndInfoInd | (optBckCallInd.callDiverMayOcc << 1) |
                             (optBckCallInd.segInd << 2) | (optBckCallInd.mlppUsrInd << 3);
    return counter;
}


static int setBackwdCallInd(BYTE msg[0],stBckCalInd &backCallInd)
{
    int counter =0;
    msg[counter++]= BACKWARD_CALL_INDICATOR;
    msg[counter++]= 2;
    msg[counter++]=backCallInd.chrgInd | (backCallInd.cadPtyStatInd << 2) | (backCallInd.cadPtyCatInd << 4) |
                           (backCallInd.end2EndMethInd << 6);
    
    msg[counter++]= backCallInd.intInd | (backCallInd.end2EndInfoInd << 1) | (backCallInd.isdnUsrPrtInd << 2) |
                            (backCallInd.holdInd << 3) | (backCallInd.isdnAccInd << 4 ) | (backCallInd.echoCtrlDevInd << 5) |
                            (backCallInd.sccpMethInd << 6);
    return counter;
}


static int setGenNotiInd(BYTE msg[0],stGenNotiInd &genNotiInd)
{
    int counter =0;
    BYTE ext = 0x01;
    msg[counter++]=GENERIC_NOTIFICATION_INDICATOR; // optional parameter code
    msg[counter++]= 1;
    msg[counter++]= genNotiInd.notifyInd[0] | (ext << 7);
    return counter;
}

static int setAutoCongLevel(BYTE msg[0],stAutoCongLvl &autoCongLvl)
{
    int counter =0;
    msg[counter++]=AUTO_CONGESTION_LEVEL; // optional parameter code
    msg[counter++]= 1;
    msg[counter++]= autoCongLvl.auCongLvl; 
    return counter;
}

static int setSignalingPointCode(BYTE msg[0],stSigPointCode &sigPointCode)
{
    int counter =0;
    msg[counter++]=SIGNALLING_POINT_CODE; // optional parameter code
    msg[counter++]= 2;
    msg[counter++]= sigPointCode.sigPointCode[0];
    msg[counter++]= sigPointCode.sigPointCode[1]; 
    return counter;
}


int send_iam(unsigned short port,IAM * param,char *source_number,char *dest_number,unsigned char link)
{
unsigned short counter=0;
BYTE index;
Ss7msg   Ss7TxMsg;
int ret;
unsigned short trunk = 0;
BYTE cdpn[MAX_NUMBER_LEN],cgpn[MAX_NUMBER_LEN];
char temp[MAX_NUMBER_LEN];

        if(param == NULL)
                return EINVAL;
        memset(cdpn,0,sizeof(cdpn));
        memset(cgpn,0,sizeof(cgpn));
        memset(temp,0,sizeof(temp));
        initialiseMsg(&Ss7TxMsg);
        trunk = cicTotrunk(port);
               
        Ss7TxMsg.id1=BOX_ID1;
        Ss7TxMsg.id2=BOX_ID2;
        Ss7TxMsg.opcode=0;
        Ss7TxMsg.subfield= link;
        Ss7TxMsg.debug=0;

        counter=0;
        Ss7TxMsg.msg[counter++]= (port & 0xFF);
        Ss7TxMsg.msg[counter++]= (port >> 8) & 0x07;

        Ss7TxMsg.msg[counter++]=MSG_TYPE_INITIAL_ADDRESS;
        //Network connection indicator
        Ss7TxMsg.msg[counter++]= param->natConInd.satelliteInd | (param->natConInd.contChkInd << 2) | (param->natConInd.echoCntrlDevInd << 4) ;
        
        //Forward call indicators
        Ss7TxMsg.msg[counter++]= param->fwdCallInd.natIntCallInd | (param->fwdCallInd.end2EndMethInd << 1) |
                                (param->fwdCallInd.intInd << 3) |(param->fwdCallInd.end2EndInfoInd << 5) | (param->fwdCallInd.isdnUsrPrtInd << 6) |
                                (param->fwdCallInd.isdnUsrPrtPrfInd << 7) ;
                                   
        Ss7TxMsg.msg[counter++]=(param->fwdCallInd.isdnAccInd) | (param->fwdCallInd.sccpMethInd << 1);
        //Calling party category
        Ss7TxMsg.msg[counter++]= param->cgpnCat.cgPtyCat; 
        //transmission medium requirement
        Ss7TxMsg.msg[counter++]= param->transMedReq.trMedReq;
        //Mandatory variable 
        Ss7TxMsg.msg[counter++]=0x02; // fixed parameter

        Ss7TxMsg.msg[counter++]= 4 + (strlen(dest_number)/2) + (strlen(dest_number)%2); // pointer offset to opt params
      
        Ss7TxMsg.msg[counter++] =  2 + (strlen(dest_number)/2) + (strlen(dest_number)%2); // length indicator
        if(strlen(dest_number)%2 != 0)
        {
            param->cdpn.oddEven = NMB_ODD;
           Ss7TxMsg.msg[counter++] = param->cdpn.natAddrInd | (param->cdpn.oddEven << 7);
        }
        else
        {
           param->cdpn.oddEven = NMB_EVEN;
           Ss7TxMsg.msg[counter++] = param->cdpn.natAddrInd | (param->cdpn.oddEven << 7);
        }
       
        Ss7TxMsg.msg[counter++] = (param->cdpn.numPlan << 4) | (param->cdpn.innInd << 7);
    
        strcpy(temp,dest_number);
        for(index = 0; index < strlen(dest_number); index++)
            cdpn[index] = temp[index] - 48;
        for(index=0;index < (strlen(dest_number) + strlen(dest_number) %2);index+=2)
        {
                Ss7TxMsg.msg[counter++]=((cdpn[index+1] << 4) | cdpn[index] );
        }
        
        //Optional parameter starting
        if(param->cgpn.stat.pres == TRUE)
        {
            counter += setCallingPartyNum(&Ss7TxMsg.msg[counter],param->cgpn,source_number);
        }
        
        if(param->tranNetSel.stat.pres == TRUE)
        {
             counter += setTransitNetSel(&Ss7TxMsg.msg[counter],param->tranNetSel);
        }
        
        if(param->callRef.stat.pres == TRUE)
        {
             counter += setCallReference(&Ss7TxMsg.msg[counter],param->callRef);
        }
        
        if(param->opFwdCallInd.stat.pres == TRUE) //Not worked correctly
        {
            counter += setOptFwdCallInd(&Ss7TxMsg.msg[counter],param->opFwdCallInd);
        }   
        if(param->redirectingNum.stat.pres == TRUE)
        {
            counter += setRedirectingNumber(&Ss7TxMsg.msg[counter],param->redirectingNum);
        }
        
        if(param->redirectInfo.stat.pres == TRUE)
        {
            counter += setRedirectionInfo(&Ss7TxMsg.msg[counter],param->redirectInfo);
        }
        
        if(param->usr2UsrInfo.stat.pres == TRUE)
        {
             counter += setUser2UserInfo(&Ss7TxMsg.msg[counter],param->usr2UsrInfo);
        }
        
        if(param->usr2UsrInd.stat.pres == TRUE)
        {
            counter += setUser2UserInd(&Ss7TxMsg.msg[counter],param->usr2UsrInd);
        }
        
        if(param->accTransport.stat.pres == TRUE)
        {
            counter += setAccessTransport(&Ss7TxMsg.msg[counter],param->accTransport);
        }
        
        if(param->closedugIntCode.stat.pres == TRUE)
        {
           counter += setClosedUserGrpInterlockCode(&Ss7TxMsg.msg[counter],param->closedugIntCode);
        }
        
        if(param->connReq.stat.pres == TRUE)
        {
            counter += setConnectionRequest(&Ss7TxMsg.msg[counter],param->connReq);
        }
        Ss7TxMsg.msg[counter++]=0x00;
        Ss7TxMsg.length=(FIXED_SIZE+counter) << 8;
      
        ret = send_msg(&Ss7TxMsg);
        return ret;

}


                                                                                                                                               
int send_acm(unsigned short port, ACM *param,unsigned char link)
{
unsigned short counter=0;
unsigned short trunk = 0;
BYTE tempbyte[1000];
char temp[MAX_NUMBER_LEN];
Ss7msg   Ss7TxMsg;

int ret;
        if(param == NULL)
                return EINVAL;
        memset(temp,0,sizeof(temp));
        memset(tempbyte,0,sizeof(tempbyte));
        initialiseMsg(&Ss7TxMsg);
        trunk = cicTotrunk(port);
      //  cic = port - (32*trunk);
        Ss7TxMsg.id1=BOX_ID1;
        Ss7TxMsg.id2=BOX_ID2;
        Ss7TxMsg.opcode=0;
        Ss7TxMsg.subfield=link;
        Ss7TxMsg.debug=0;

        counter=0;
        Ss7TxMsg.msg[counter++]= (port & 0xFF);
        Ss7TxMsg.msg[counter++]= (port >> 8) & 0x07;
        Ss7TxMsg.msg[counter++]=MSG_TYPE_ADDRESS_COMPLETE;
        Ss7TxMsg.msg[counter++]=param->backCallInd.chrgInd | (param->backCallInd.cadPtyStatInd << 2) | (param->backCallInd.cadPtyCatInd << 4) |
                                (param->backCallInd.end2EndMethInd << 6);
        Ss7TxMsg.msg[counter++]= param->backCallInd.intInd | (param->backCallInd.end2EndInfoInd << 1) | (param->backCallInd.isdnUsrPrtInd << 2) |
                                 (param->backCallInd.holdInd << 3) | (param->backCallInd.isdnAccInd << 4 ) | (param->backCallInd.echoCtrlDevInd << 5) |
                                 (param->backCallInd.sccpMethInd << 6);
        
        if((param->callRef.stat.pres == TRUE) || (param->optBckCallInd.stat.pres == TRUE) || (param->causeInd.stat.pres == TRUE))
                Ss7TxMsg.msg[counter++]=0x01; //Pointer to optional parameter
        else
            Ss7TxMsg.msg[counter++]=0x00; //Pointer to optional parameter
        
        if(param->callRef.stat.pres == TRUE)
        {
            counter += setCallReference(&Ss7TxMsg.msg[counter],param->callRef);
        }
        
        if(param->optBckCallInd.stat.pres == TRUE)
        {
            counter += setOptionalBackwdCallInd(&Ss7TxMsg.msg[counter],param->optBckCallInd);
        }
        if(param->causeInd.stat.pres == TRUE)
        {
            counter += setCauseInd(&Ss7TxMsg.msg[counter],param->causeInd);
        }
        if(param->usr2UsrInfo.stat.pres == TRUE)
        {
             counter += setUser2UserInfo(&Ss7TxMsg.msg[counter],param->usr2UsrInfo);
        }
        
        if(param->usr2UsrInd.stat.pres == TRUE)
        {
            counter += setUser2UserInd(&Ss7TxMsg.msg[counter],param->usr2UsrInd);
        }
        if(param->accTransport.stat.pres == TRUE)
        {
             counter += setAccessTransport(&Ss7TxMsg.msg[counter],param->accTransport);
        }
       
        if(param->redirectNum.stat.pres == TRUE)
        {
            counter += setRedirectionNumber(&Ss7TxMsg.msg[counter],param->redirectNum);
        }
        if(param->genNotiInd.stat.pres == TRUE)
        {
           counter += setGenNotiInd(&Ss7TxMsg.msg[counter],param->genNotiInd);
        }
        if((param->callRef.stat.pres == TRUE) || (param->optBckCallInd.stat.pres == TRUE) || (param->causeInd.stat.pres == TRUE))
                Ss7TxMsg.msg[counter++]=0x00; //End of optional parameter
        
        Ss7TxMsg.length=(FIXED_SIZE+counter) << 8;
        ret = send_msg(&Ss7TxMsg);
        return ret;
}

int send_anm(unsigned short port, ANM * param,unsigned char link)
{
unsigned short counter=0;
Ss7msg   Ss7TxMsg;
int ret;
unsigned short trunk = 0;
BYTE tempbyte[1000];
char temp[MAX_NUMBER_LEN];
        if(param == NULL)
                return EINVAL;
        memset(temp,0,sizeof(temp));
        memset(tempbyte,0,sizeof(tempbyte));
        initialiseMsg(&Ss7TxMsg);
        trunk = cicTotrunk(port);
       // cic = port - (32*trunk);
        Ss7TxMsg.id1=BOX_ID1;
        Ss7TxMsg.id2=BOX_ID2;
        Ss7TxMsg.opcode=0;
        Ss7TxMsg.subfield=link;
        Ss7TxMsg.debug=0;

        counter=0;
        Ss7TxMsg.msg[counter++]= (port & 0xFF);
        Ss7TxMsg.msg[counter++]= (port >> 8) & 0x07;
        Ss7TxMsg.msg[counter++]=MSG_TYPE_ANSWER;
        if((param->callRef.stat.pres == TRUE) || (param->optBckCallInd.stat.pres == TRUE) || (param->usr2UsrInd.stat.pres == TRUE)
                ||(param->backCallInd.stat.pres == TRUE) )
                Ss7TxMsg.msg[counter++]=0x01; //Pointer to optional parameter
        if(param->backCallInd.stat.pres == TRUE)
        {
            counter += setBackwdCallInd(&Ss7TxMsg.msg[counter],param->backCallInd);
        }
        if(param->callRef.stat.pres == TRUE)
        {
             counter += setCallReference(&Ss7TxMsg.msg[counter],param->callRef);
        }
        if(param->optBckCallInd.stat.pres == TRUE)
        {
           counter += setOptionalBackwdCallInd(&Ss7TxMsg.msg[counter],param->optBckCallInd);
        }
        
       if(param->usr2UsrInd.stat.pres == TRUE)
        {
            counter += setUser2UserInd(&Ss7TxMsg.msg[counter],param->usr2UsrInd);
        }
        if(param->usr2UsrInfo.stat.pres == TRUE)
        {
            counter += setUser2UserInfo(&Ss7TxMsg.msg[counter],param->usr2UsrInfo);
        }
        if(param->accTransport.stat.pres == TRUE)
        {
             counter += setAccessTransport(&Ss7TxMsg.msg[counter],param->accTransport);
        }
        if(param->redirectNum.stat.pres == TRUE)
        {
            counter += setRedirectionNumber(&Ss7TxMsg.msg[counter],param->redirectNum);
        }
        
        if(param->genNotiInd.stat.pres == TRUE)
        {
            counter += setGenNotiInd(&Ss7TxMsg.msg[counter],param->genNotiInd);
        }
        
        Ss7TxMsg.msg[counter++]=0x00;
        Ss7TxMsg.length=(FIXED_SIZE+counter) << 8;
        
        ret = send_msg(&Ss7TxMsg);
        return ret;
}


int send_rel(unsigned short port, REL *param,unsigned char link)
{
unsigned short counter=0;
unsigned char index;
Ss7msg   Ss7TxMsg;
int ret,length = 0;
unsigned short trunk = 0,cic = 0;
BYTE tempbyte[1000];
BYTE ext = 0x01;
char temp[MAX_NUMBER_LEN];
        if(param == NULL)
                return EINVAL;
        initialiseMsg(&Ss7TxMsg);
        memset(temp,0,sizeof(temp));
        memset(tempbyte,0,sizeof(tempbyte));
        trunk = cicTotrunk(port);
       // cic = port - (32*trunk);
        Ss7TxMsg.id1=BOX_ID1;
        Ss7TxMsg.id2=BOX_ID2;
        Ss7TxMsg.opcode=0;
        Ss7TxMsg.subfield=link;
        Ss7TxMsg.debug=0;
        
        counter=0;
        Ss7TxMsg.msg[counter++] = (port & 0xFF);
        Ss7TxMsg.msg[counter++] = (port >> 8) & 0x07;
        Ss7TxMsg.msg[counter++] = MSG_TYPE_RELEASE;
        Ss7TxMsg.msg[counter++] = 02;
        if(strlen(param->causeInd.dgnVal) != 0)
            length = strlen(param->causeInd.dgnVal)/2 + strlen(param->causeInd.dgnVal)%2;
        else
            length = 0;
        if((param->redirectInfo.stat.pres == TRUE) || (param->redirectNum.stat.pres == TRUE) || (param->usr2UsrInd.stat.pres == TRUE))
            Ss7TxMsg.msg[counter++]= 2 + length + 1;
        else
            Ss7TxMsg.msg[counter++]= 0x00;
        
        Ss7TxMsg.msg[counter++]= 2 + length ;   
        Ss7TxMsg.msg[counter++]= param->causeInd.location + (param->causeInd.cdeStand << 5) | (ext << 7);
            
        Ss7TxMsg.msg[counter++]= param->causeInd.causeVal | (ext << 7);

        if(strlen(param->causeInd.dgnVal) != 0)
        {
            strcpy(temp,param->causeInd.dgnVal);
            for(index = 0; index < strlen(param->causeInd.dgnVal); index++)
                 tempbyte[index] = temp[index] - 48;
            for(index=0;index < (strlen(param->causeInd.dgnVal) + strlen(param->causeInd.dgnVal) %2);index+=2)
            {
                    Ss7TxMsg.msg[counter++]=((tempbyte[index+1] << 4) | tempbyte[index] );
            }
        }
        
        if(param->usr2UsrInd.stat.pres == TRUE)
        {
            counter += setUser2UserInd(&Ss7TxMsg.msg[counter],param->usr2UsrInd);
        }
        if(param->redirectInfo.stat.pres == TRUE)
        {
            counter += setRedirectionInfo(&Ss7TxMsg.msg[counter],param->redirectInfo);
        }
        
        if(param->redirectNum.stat.pres == TRUE)
        {
            counter += setRedirectionNumber(&Ss7TxMsg.msg[counter],param->redirectNum);
        }
        
        if(param->accTransport.stat.pres == TRUE)
        {
             counter += setAccessTransport(&Ss7TxMsg.msg[counter],param->accTransport);
        }
        
        if(param->usr2UsrInfo.stat.pres == TRUE)
        {
             counter += setUser2UserInfo(&Ss7TxMsg.msg[counter],param->usr2UsrInfo);
        }
        
        if(param->autoCongLvl.stat.pres == TRUE)
        {
            counter += setAutoCongLevel(&Ss7TxMsg.msg[counter],param->autoCongLvl);
        }
        if(param->sigPointCode.stat.pres == TRUE)
        {
             counter += setSignalingPointCode(&Ss7TxMsg.msg[counter],param->sigPointCode);
        }
       
        Ss7TxMsg.msg[counter++]=0x00;
        Ss7TxMsg.length=(FIXED_SIZE+counter) << 8;
        ret = send_msg(&Ss7TxMsg);
        return ret;
}


int send_rlc(unsigned short port,RLC * param,unsigned char link)
{
unsigned short counter=0;
unsigned short trunk = 0;
Ss7msg   Ss7TxMsg;
int ret;
        if(param == NULL)
            return EINVAL;
        trunk = cicTotrunk(port);
       // cic = port - (32*trunk);
        initialiseMsg(&Ss7TxMsg);
        Ss7TxMsg.id1=BOX_ID1;
        Ss7TxMsg.id2=BOX_ID2;
        Ss7TxMsg.opcode=0;
        Ss7TxMsg.subfield=link;
        Ss7TxMsg.debug=0;
        counter=0;
        Ss7TxMsg.msg[counter++]= (port & 0xFF);
        Ss7TxMsg.msg[counter++]= (port >> 8) & 0x07;
        Ss7TxMsg.msg[counter++]=MSG_TYPE_RELEASE_COMPLETE;
        if(param->causeInd.stat.pres == TRUE)
            Ss7TxMsg.msg[counter++]=0x01;
        else
            Ss7TxMsg.msg[counter++]=0x00;
        if(param->causeInd.stat.pres == TRUE)
        {
              counter += setCauseInd(&Ss7TxMsg.msg[counter],param->causeInd);
        }
        if(param->causeInd.stat.pres == TRUE)
                Ss7TxMsg.msg[counter++]=0x00; //End of optional parameter
        Ss7TxMsg.length=(FIXED_SIZE+counter) << 8;
        
        ret = send_msg(&Ss7TxMsg);
        return ret;
}


int callprogressReq(unsigned short port, CPG *param)
{
BYTE counter=0;
unsigned char index;
Ss7msg   Ss7TxMsg;
int ret;
unsigned short trunk = 0,cic = 0;
BYTE tempbyte[1000];
char temp[MAX_NUMBER_LEN];

        initialiseMsg(&Ss7TxMsg);
        memset(temp,0,sizeof(temp));
        memset(tempbyte,0,sizeof(tempbyte));
        trunk = cicTotrunk(port);
        cic = port - (32*trunk);
        Ss7TxMsg.id1 = BOX_ID1;
        Ss7TxMsg.id2 = BOX_ID2;
        Ss7TxMsg.opcode = 0;
        Ss7TxMsg.subfield = 0;
        Ss7TxMsg.debug = 0;
        counter = 0;
        Ss7TxMsg.msg[counter++]= cic;
        Ss7TxMsg.msg[counter++]= trunk;
        Ss7TxMsg.msg[counter++]= MSG_TYPE_CALL_PROGRESS;
        Ss7TxMsg.msg[counter++]= (param->evntInfo.evntInd) | ((param->evntInfo.evntPresResInd ) << 7);
        if((param->callRef.stat.pres == TRUE) || (param->optBckCallInd.stat.pres == TRUE) || (param->causeInd.stat.pres == TRUE) ||
            (param->redirectNum.stat.pres == TRUE) ||(param->usr2UsrInd.stat.pres == TRUE) || (param->usr2UsrInfo.stat.pres == TRUE) ||
           (param->backCallInd.stat.pres == TRUE))
                Ss7TxMsg.msg[counter++]=0x01; //Pointer to optional parameter
               
        if(param->causeInd.dgnVal != NULL)
        {
            strcpy(temp,param->causeInd.dgnVal);
            for(index = 0; index < strlen(param->causeInd.dgnVal); index++)
                 tempbyte[index] = temp[index] - 48;
            for(index=0;index < (strlen(param->causeInd.dgnVal) + strlen(param->causeInd.dgnVal) %2);index+=2)
            {
                    Ss7TxMsg.msg[counter++]=((tempbyte[index+1] << 4) | tempbyte[index] );
            }
        }
        
        if(param->usr2UsrInd.stat.pres == TRUE)
        {
            counter += setUser2UserInd(&Ss7TxMsg.msg[counter],param->usr2UsrInd);
        }
         
        if(param->usr2UsrInfo.stat.pres == TRUE)
        {
             counter += setUser2UserInfo(&Ss7TxMsg.msg[counter],param->usr2UsrInfo);
        }
        
        
        if(param->accTransport.stat.pres == TRUE)
        {
            counter += setAccessTransport(&Ss7TxMsg.msg[counter],param->accTransport);
        }
        
        if(param->callRef.stat.pres == TRUE)
        {
             counter += setCallReference(&Ss7TxMsg.msg[counter],param->callRef);
        }
        
        if(param->optBckCallInd.stat.pres == TRUE)
        {
           counter += setOptionalBackwdCallInd(&Ss7TxMsg.msg[counter],param->optBckCallInd);
        }
        
        if(param->backCallInd.stat.pres == TRUE)
        {
           counter += setBackwdCallInd(&Ss7TxMsg.msg[counter],param->backCallInd);
        }
        
        Ss7TxMsg.msg[counter++]=0x00; //End of optional parameter
        Ss7TxMsg.length=(FIXED_SIZE+counter) << 8;
        
        ret = send_msg(&Ss7TxMsg);
        return ret;
}


int statusReq(unsigned short port,unsigned char link,BYTE msgType,stStatEvnt evt)
{
Ss7msg   Ss7TxMsg;
int ret,index = 0,len =0,i ;
BYTE trunk = 0,cic = 0;
unsigned short  counter = 0;
    
    //trunk = cicTotrunk(port);
    //cic = port - (32*trunk);
    initialiseMsg(&Ss7TxMsg);
    //Ss7TxMsg.length=(FIXED_SIZE+3) << 8;
    criticalStatReqRespLock.Lock();
    Ss7TxMsg.opcode=0;
    Ss7TxMsg.subfield = link;
    Ss7TxMsg.debug=0;
    counter = 0;
    Ss7TxMsg.msg[counter++]= (port & 0xFF);
    Ss7TxMsg.msg[counter++]= (port >> 8) & 0x07;
    switch(msgType)
    {
        case MSG_TYPE_RESET_CIRCUIT:
        case MSG_TYPE_BLOCKING:
        case MSG_TYPE_UNBLOCKING:
        case MSG_TYPE_BLOCKING_ACK:
        case MSG_TYPE_UNBLOCKING_ACK:
            Ss7TxMsg.msg[counter++] = msgType;
            Ss7TxMsg.msg[counter++] = 0x00;
            break;
        case MSG_TYPE_CIRCUIT_GROUP_RESET:
        //case MSG_TYPE_CIRCUIT_GROUP_QUERY:
        case MSG_TYPE_CIRCUIT_GROUP_BLOCK:
        case MSG_TYPE_CIRCUIT_GROUP_UNBLOCK:
        case MSG_TYPE_CIRCUIT_GROUP_BLOCK_ACK:
        case MSG_TYPE_CIRCUIT_GROUP_UNBLOCK_ACK:
        case MSG_TYPE_CIRCUIT_GROUP_RESET_ACK:
            if((evt.rangStat.range % 8) != 0)
            {
                len = (evt.rangStat.range/8)+1;
            }
            else
                len = evt.rangStat.range/8;
            Ss7TxMsg.msg[counter++]=msgType;
            Ss7TxMsg.msg[counter++]=1;
            Ss7TxMsg.msg[counter++]=len +1;
            Ss7TxMsg.msg[counter++]=evt.rangStat.range;
            for( i = 0; i < len ; i++)
                    Ss7TxMsg.msg[counter++]=0x00;
            break;
        default:
            criticalStatReqRespLock.Unlock();
            return EINVAL;
    }
    /*if(msgType == MSG_TYPE_CIRCUIT_GROUP_RESET || msgType == MSG_TYPE_CIRCUIT_GROUP_QUERY )
    {
        Ss7TxMsg.msg[counter++]=msgType;
        Ss7TxMsg.msg[counter++]= 0x01;
        Ss7TxMsg.msg[counter++]= 0x01;
        Ss7TxMsg.msg[counter++]= evt.rangStat.range;
     /*   if(evt.rangStat.status.len != 0x00)
            {
                for(index = 0; index < evt.rangStat.status.len;index++ )
                    Ss7TxMsg.msg[counter++]= evt.rangStat.status.val[index];
            }
            else
                Ss7TxMsg.msg[counter++] = 0x00;
        }
        else
        {
             Ss7TxMsg.msg[counter++]= 2;
             Ss7TxMsg.msg[counter++]=0x00;
             Ss7TxMsg.msg[counter++]=0x00;
        }*/
            
        
   /* }
    
    /*if(msgType == MSG_TYPE_CIRCUIT_GROUP_BLOCK || msgType == MSG_TYPE_CIRCUIT_GROUP_UNBLOCK ||
       msgType == MSG_TYPE_CIRCUIT_GROUP_BLOCK_ACK || msgType == MSG_TYPE_CIRCUIT_GROUP_UNBLOCK_ACK  )
    {
        Ss7TxMsg.msg[counter++]=msgType;
        if(evt.cgsmti.stat.pres == TRUE)
           Ss7TxMsg.msg[counter++] = evt.cgsmti.typeInd;
        else
           Ss7TxMsg.msg[counter++] = MAINT; // Maintenence
        Ss7TxMsg.msg[counter++] = 0x02;
        Ss7TxMsg.msg[counter++] = 0x00;
        if(evt.rangStat.stat.pres == TRUE)
        {
            Ss7TxMsg.msg[counter++] = evt.rangStat.status.len  + 1;
            Ss7TxMsg.msg[counter++]=  evt.rangStat.range;
           
            if(evt.rangStat.status.len != 0x00)
            {
                for(index = 0; index < evt.rangStat.status.len;index++ )
                    Ss7TxMsg.msg[counter++]= evt.rangStat.status.val[index];
            }
            else
                Ss7TxMsg.msg[counter++] = 0x00;
        }
        else
        {
             Ss7TxMsg.msg[counter++]= 2;
             Ss7TxMsg.msg[counter++]=0x00;
             Ss7TxMsg.msg[counter++]=0x00;
        }
        
    }*/
    /*if(msgType == MSG_TYPE_RESET_CIRCUIT || msgType == MSG_TYPE_BLOCKING || msgType == MSG_TYPE_UNBLOCKING ||
       msgType == MSG_TYPE_BLOCKING_ACK  ||  msgType == MSG_TYPE_UNBLOCKING_ACK  )
    {
        Ss7TxMsg.msg[counter++] = msgType;
        Ss7TxMsg.msg[counter++] = 0x00;
    }*/
    Ss7TxMsg.length=(FIXED_SIZE+counter) << 8;
    ret = send_msg(&Ss7TxMsg);
    criticalStatReqRespLock.Unlock();
    return ret;
}


void write_slot(unsigned char port,unsigned char link, unsigned char slot, unsigned char data)
{
        struct Ss7msg_RWtimeSlot Ss7txmsg;
        Ss7txmsg.type=TCP_TYPE;
        Ss7txmsg.length=htons(FIXED_SIZE+5);
        Ss7txmsg.opcode=htons(PIC_TIMESLOT_CALLPROC);
        Ss7txmsg.subfield=htons(PIC_TIMESLOT_WRITESLOT);
        Ss7txmsg.debug=0x00;
        Ss7txmsg.optionalTimeSlot=htons(port);
        Ss7txmsg.destLink=link;
        Ss7txmsg.destSlot=slot;
        Ss7txmsg.data=data;
       // msgsnd(tcpwrite_qid,&Ss7txmsg,sizeof(Ss7txmsg),1);
}


void read_slot(unsigned char port,unsigned char link, unsigned char slot)
{
        struct Ss7msg_RWtimeSlot Ss7txmsg;
        Ss7txmsg.type=TCP_TYPE;
        Ss7txmsg.length=htons(FIXED_SIZE+5);
        Ss7txmsg.opcode=htons(PIC_TIMESLOT_CALLPROC);
        Ss7txmsg.subfield=htons(PIC_TIMESLOT_READSLOT);
        Ss7txmsg.debug=0x00;
        Ss7txmsg.optionalTimeSlot=htons(port);
        Ss7txmsg.destLink=link;
        Ss7txmsg.destSlot=slot;
        Ss7txmsg.data=0;
      //  msgsnd(tcpwrite_qid,&Ss7txmsg,sizeof(Ss7txmsg),1);
}

void oneway_switch(unsigned char port,unsigned char srclink, unsigned char srcslot, unsigned char dstlink, unsigned char dstslot)
{
        struct Ss7msg_timeSwitch Ss7txmsg_timeSwitch;
        Ss7txmsg_timeSwitch.type=TCP_TYPE;
        Ss7txmsg_timeSwitch.length=htons(FIXED_SIZE+6);
        Ss7txmsg_timeSwitch.opcode=htons(PIC_TIMESWITCH_CALLPROC);
        Ss7txmsg_timeSwitch.subfield=htons(PIC_TIMESWITCH_ONEWAY_SWITCH);
        Ss7txmsg_timeSwitch.debug=0x00;
        Ss7txmsg_timeSwitch.optionalTimeSlot=htons(port);
        Ss7txmsg_timeSwitch.srcLink=srclink;
        Ss7txmsg_timeSwitch.srcSlot=srcslot;
        Ss7txmsg_timeSwitch.destLink=dstlink;
        Ss7txmsg_timeSwitch.destSlot=dstslot;
       // msgsnd(tcpwrite_qid,&Ss7txmsg_timeSwitch,sizeof(Ss7txmsg_timeSwitch),1);
}

void twoway_switch(unsigned char port,unsigned char srclink, unsigned char srcslot, unsigned char dstlink, unsigned char dstslot)
{
        struct Ss7msg_timeSwitch Ss7txmsg_timeSwitch;
        Ss7txmsg_timeSwitch.type=TCP_TYPE;
        Ss7txmsg_timeSwitch.length=htons(FIXED_SIZE+6);
        Ss7txmsg_timeSwitch.opcode=htons(PIC_TIMESWITCH_CALLPROC);
        Ss7txmsg_timeSwitch.subfield=htons(PIC_TIMESWITCH_TWOWAY_SWITCH);
        Ss7txmsg_timeSwitch.debug=0x00;
        Ss7txmsg_timeSwitch.optionalTimeSlot=htons(port);
        Ss7txmsg_timeSwitch.srcLink=srclink;
        Ss7txmsg_timeSwitch.srcSlot=srcslot;
        Ss7txmsg_timeSwitch.destLink=dstlink;
        Ss7txmsg_timeSwitch.destSlot=dstslot;
        //msgsnd(tcpwrite_qid,&Ss7txmsg_timeSwitch,sizeof(Ss7txmsg_timeSwitch),1);
}


int reset_circuit(unsigned short port,unsigned char link)
{
    
int ret ;
stStatEvnt evt;
     ret = statusReq(port,link,MSG_TYPE_RESET_CIRCUIT,evt);
     return ret;
    /*int ret ;
    stStatEvnt evt;
    Ss7msg   Ss7TxMsg;
       
    initialiseMsg(&Ss7TxMsg);
    Ss7TxMsg.length=(FIXED_SIZE+3) << 8;
    Ss7TxMsg.opcode=0;
    Ss7TxMsg.subfield=link;
    Ss7TxMsg.debug=0;
    Ss7TxMsg.msg[0]= (port & 0xFF);
    Ss7TxMsg.msg[1]= (port >> 8) & 0x07;
    Ss7TxMsg.msg[2]=MSG_TYPE_RESET_CIRCUIT;
    Ss7TxMsg.msg[3]=0x00;
    ret = send_msg(&Ss7TxMsg);
    return ret;*/
        
    
}


int initialiseMsg(Msg_Packet *Ss7Msg)
{
    initialiseLock1.Lock();
    Ss7Msg->length = 0;
    Ss7Msg->id1 = BOX_ID1 ;
    Ss7Msg->id2 = BOX_ID2;
    Ss7Msg->debug = 0;
    Ss7Msg->opcode = 0;
    Ss7Msg->subfield = 0;
    Ss7Msg->type = 0;
    memset(Ss7Msg->msg,0,sizeof(Ss7Msg->msg));
    initialiseLock1.Unlock();
    
}

int initialiseMsg(Ss7msg *Ss7Msg)
{
    initialiseLock2.Lock();
    Ss7Msg->length = 0;
    Ss7Msg->id1 = BOX_ID1 ;
    Ss7Msg->id2 = BOX_ID2;
    Ss7Msg->debug = 0;
    Ss7Msg->opcode = 0;
    Ss7Msg->subfield = 0;
    memset(Ss7Msg->msg,0,sizeof(Ss7Msg->msg));
    initialiseLock2.Unlock();
}
