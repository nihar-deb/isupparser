/* 
 * File:   libisup.h
 * Author: nihar
 *
 * Created on January 22, 2013, 10:36 AM
 */

#ifndef LIBJCTI_H
#define	LIBJCTI_H
#include "common.h"
#include "isuplogger.h"
#include "pic_struc.h"
//#include "signalingServer.h"

#define VERSION 2.0

typedef struct
{
    char sigServerIP[MAX_IP_LEN];
    int portNo;
    BYTE defaultAction;
}isupSignalConfig;

typedef struct
{
    char mediaServerIP[MAX_IP_LEN];
    int startingPortNo;
    BYTE defaultAction;
}isupMediaConfig;

typedef struct
{
    unsigned char linkSelAlgo;
    unsigned short ss7link[MAX_SS7_LINK];
    unsigned short failoverLink;
    char cicRange[MAX_CIC_RANGE_LEN];
}isupLinsetConfig;

typedef struct
{
    int enable;
    char origcicRange[MAX_CIC_RANGE_LEN];
    unsigned short signalingCIC;
}isupE1Config;


typedef struct{
    isupSignalConfig    signalingServer[MAX_BOX_SUPPORTED];
    isupMediaConfig     mediaServer[MAX_BOX_SUPPORTED]; 
    isupLog::isupLogConfig       logConfig;
    isupE1Config        E1Config[MAX_E1];
    isupLinsetConfig    linksetConfig[MAX_SS7_LINK];
    unsigned short signalingCIC[MAX_SS7_LINK]; 
}isupConfig;


typedef struct{
    unsigned short opc;
    unsigned short dpc;
    unsigned short chanNo;//Device timeslot No
    unsigned short cic;// circuit no
    unsigned short trunk;
    BYTE signalingState;
    BYTE mediaState;
    unsigned short  opcode;
    unsigned char subField;
    unsigned short msgType;
    unsigned char msg[MAX_ISUP_MSU_LEN];
    
}EVENT;

typedef struct
{
    int type; 
    unsigned int termNo;
    unsigned short length;
    BYTE data[MAX_DATA];
    BYTE flag; 
    
}IOTERM;

typedef struct
{
    BYTE len;
    BYTE val[32];
}tknStr;
        

/*typedef struct 
{
        char Nature_of_Connection_Indicators[2];
        char Forward_Call_Indicators[CALL_ID];
        char Calling_Party_Category[2];
        char Transmission_Medium_Requirement[2];
        char Called_Party_Number[MAX_NUMBER_LEN];
        char Calling_Party_Number[MAX_NUMBER_LEN];
        char Call_Reference[REF_NUM];
        char Optional_Forward_Call_Indicator[OPT_CALL_ID];
        char Redirecting_Number[RED_NUM];
        char Connection_Request[CONN_REQUEST];
        char Signalling_Point_Code[2];
        char Protocol[2];
}IAM;*/

typedef struct 
{
    BYTE pres; //Presence of the IE 
    BYTE spare1; //Future use
    BYTE spare2; //Future use
} statIE;

typedef struct
{
    statIE stat;
    BYTE satelliteInd;
    BYTE contChkInd;
    BYTE echoCntrlDevInd;
    BYTE spare;
    
}stNatConInd;

typedef struct 
{
    statIE stat;
    BYTE natIntCallInd;
    BYTE end2EndMethInd;
    BYTE intInd;
    BYTE segInd;
    BYTE end2EndInfoInd;
    BYTE isdnUsrPrtInd;
    BYTE isdnUsrPrtPrfInd;
    BYTE isdnAccInd;
    BYTE sccpMethInd;
    BYTE spare;
    BYTE natReserved;
} stFwdCallInd;

typedef struct _cgPtyCat
{
    statIE stat;
    BYTE cgPtyCat;
} stCgpnCat;

typedef struct  
{
    statIE stat; 
    BYTE trMedReq; 
} stTransMedReq;

typedef struct 
{
    statIE stat;
    BYTE natAddrInd;
    BYTE oddEven;
    BYTE spare;
    BYTE numPlan;
    BYTE reserved;
    BYTE innInd;
    char addrSig[MAX_NUMBER_LEN];
} stCdpn;

typedef struct 
{
    statIE stat;
    BYTE netIdPln;
    BYTE typNetId;
    BYTE oddEven;
    BYTE spare;
    char netId[MAX_NATID_LEN];
}stTranNetSel;

typedef struct 
{
    statIE stat;
    BYTE callId[3];
    BYTE pntCde[2];
} stCallRef;


typedef struct 
{
    statIE stat;
    BYTE natAddrInd;
    BYTE oddEven;
    BYTE scrnInd;
    BYTE presRest;
    BYTE numPlan;
    BYTE niInd;
    BYTE spare;
    char addrSig[MAX_NUMBER_LEN];
} stCgpn;


typedef struct _bckCalInd
{
    statIE stat;
    BYTE chrgInd;
    BYTE cadPtyStatInd;
    BYTE cadPtyCatInd;
    BYTE end2EndMethInd;
    BYTE intInd;
    BYTE segInd;
    BYTE end2EndInfoInd;
    BYTE isdnUsrPrtInd;
    BYTE holdInd;
    BYTE isdnAccInd;
    BYTE echoCtrlDevInd;
    BYTE sccpMethInd;
    BYTE spare;
} stBckCalInd;

typedef struct _optBckCalInd
{
    statIE stat;
    BYTE inbndInfoInd;
    BYTE callDiverMayOcc;
    BYTE segInd;
    BYTE netDelay;
    BYTE usrNetIneractInd;
    BYTE mlppUsrInd;
    BYTE spare;
    BYTE reserved;
} stOptBckCalInd;

typedef struct _siCauseDgn
{
    statIE stat;
    BYTE location;
    BYTE spare;
    BYTE cdeStand;
    BYTE recommend;
    BYTE causeVal;
    char dgnVal[30];
} stCauseDgn;

typedef struct _usr2UsrInd
{
    statIE stat;
    BYTE type;
    BYTE serv1;
    BYTE serv2;
    BYTE serv3;
    BYTE spare;
    BYTE netDscrdInd;
} stUsr2UsrInd;

typedef struct _usr2UsrInfo
{
    statIE stat;
    char info[129];
} stUsr2UsrInfo;

typedef struct _accTrnspt
{
    statIE stat;
    char infoElmts[30];
} stAccTrnspt;

typedef struct _cirGrpSupMTypInd /* Circuit Group Supervision Msg.Type Ind. */
{
    statIE stat; /* element header*/
    BYTE typeInd; /* message type ind*/
    BYTE spare; /* spare bits*/
} stCirGrpSupMTypInd;

typedef struct _redirInfo
{
    statIE stat;
    BYTE redirInd;
    BYTE spare1;
    BYTE origRedirReas;
    BYTE redirCnt;
    BYTE spare2;
    BYTE redirReas;
} stRedirInfo;

typedef struct _redirNum
{
    statIE stat;
    BYTE natAddr;
    BYTE oddEven;
    BYTE spare;
    BYTE numPlan;
    BYTE innInd;
    char addrSig[MAX_NUMBER_LEN];
} stRedirNum;

typedef struct _opFwdCalInd /* Optional Forward Call Indicators*/
{
    statIE stat; /* element header*/
    BYTE clsdUGrpCaInd; /* closed user group call ind.*/
    BYTE segInd; /* simple segmentation indicator*/
    BYTE spare; /* spare (4 bits)*/
    BYTE clidReqInd; /* connected line identity request indicator*/
    BYTE ccbsCallInd; /* CCBS call indicator*/
    BYTE callgPtyNumIncomInd; /* calling party number incomplete indicator*/
    BYTE connAddrReqInd; /* connected address request indicator*/
} stOpFwdCalInd;

typedef struct _redirgNum
{
    statIE stat;
    BYTE natAddr;
    BYTE oddEven;
    BYTE spare1;
    BYTE presRest;
    BYTE numPlan;
    BYTE spare2;
    char addrSig[MAX_NUMBER_LEN];
} stRedirectingNum;


typedef struct _cugIntCode
{
    statIE stat;
    BYTE dig2;
    BYTE dig1;
    BYTE dig4;
    BYTE dig3;
    BYTE binCde[2];
    //BYTE ISDNIdent[2]; // Required for ISDN network. Not applicable in ISUP
} stClosedugIntCode;

typedef struct _connReq
{
    statIE stat;
    BYTE locRef[3];
    BYTE pntCde[2];
    BYTE protClass;
    BYTE credit;
} stConnReq;

typedef struct _genNotiInd
{
        statIE stat;
        BYTE notifyInd[30];
} stGenNotiInd;

typedef struct _autoCongLvl /* Automatic Congestion Level */
{
        statIE stat;/* element header*/
        BYTE auCongLvl; /* auto congestion level*/
} stAutoCongLvl;

typedef struct _specProcReq
{
     statIE stat;/* element header*/
     BYTE specProcReq;
} stSpecProcReq;

typedef struct _sigPointCode
{
        statIE stat;/* element header*/
        BYTE sigPointCode[2];
} stSigPointCode;

typedef struct _rangStat
{
        statIE stat;/* element header*/
        BYTE range;
        tknStr status;
} stRangStat;

typedef struct _evntInfo
{
        statIE stat;/* element header*/
        BYTE evntInd;
        BYTE evntPresResInd;
} stEvntInfo;


typedef struct 
{
    stNatConInd natConInd;   /*Nature of connection indicators*/
    stFwdCallInd fwdCallInd; /* forward call indicators */
    stCgpnCat cgpnCat; /* calling party category */
    stTransMedReq transMedReq; /* transmission medium requirement*/
    stCdpn cdpn; /* called party number */
    stTranNetSel tranNetSel; /* transit network selection*/
    stCallRef callRef; /* call reference*/
    stCgpn cgpn; /* calling party number*/
    stOpFwdCalInd opFwdCallInd; /*optional forward call Indicator*/
    stRedirectingNum redirectingNum; /*redirecting number*/
    stRedirInfo    redirectInfo; /*Redirection information*/
    stUsr2UsrInd   usr2UsrInd;
    stUsr2UsrInfo  usr2UsrInfo;
    stAccTrnspt    accTransport;
    stClosedugIntCode   closedugIntCode;
    stConnReq connReq;
}IAM;


typedef struct
{
    stBckCalInd backCallInd;
    stCallRef callRef;
    stOptBckCalInd optBckCallInd;
    stCauseDgn     causeInd; 
    stUsr2UsrInd   usr2UsrInd;
    stUsr2UsrInfo  usr2UsrInfo;
    stAccTrnspt    accTransport;
    stRedirNum     redirectNum;
    stGenNotiInd   genNotiInd;
}ACM;

typedef struct
{
    stBckCalInd backCallInd;
    stCallRef callRef;
    stOptBckCalInd optBckCallInd;
    stUsr2UsrInd   usr2UsrInd;
    stUsr2UsrInfo  usr2UsrInfo;
    stAccTrnspt    accTransport;
    stRedirNum     redirectNum;
    stGenNotiInd   genNotiInd;
}ANM;

typedef struct
{
    stCauseDgn     causeInd;
    stRedirInfo    redirectInfo;
    stRedirNum     redirectNum;
    stUsr2UsrInd   usr2UsrInd;
    stUsr2UsrInfo  usr2UsrInfo;
    stAccTrnspt    accTransport;
    stAutoCongLvl  autoCongLvl;
    stSigPointCode sigPointCode;
}REL;

typedef struct 
{
        stCauseDgn     causeInd;
}RLC;

typedef struct
{
    stEvntInfo  evntInfo;        
    stCauseDgn  causeInd;
    stCallRef   callRef;
    stBckCalInd backCallInd;
    stOptBckCalInd optBckCallInd;
    stAccTrnspt    accTransport;
    stUsr2UsrInd   usr2UsrInd;
    stUsr2UsrInfo  usr2UsrInfo;
    stRedirNum     redirectNum;
    stGenNotiInd   genNotiInd; 
    
}CPG;

typedef struct _siStaEvnt       /* Status Event */
{
stRangStat rangStat; /* range and status*/
stCirGrpSupMTypInd cgsmti; /* circuit grp. supervision msg. type ind.
SiCirStateInd cirStateInd;
/* circuit state indicators
SiContInd
contInd;
/* continuity indicator
SiCauseDgn
causeDgn;
/* cause indicators
SiParmCompInfo
parmCom;
/* parameter compatibility information
SiNatConInd
natConInd;
/* Nature of connection indicators
SiCirAssignMap
cirAssignMap;
/* circuit assignment map
SiMsgCompInfo
msgCom;
/* message compatibility information
SiAppTransParam
appTransParam;
/* application transport as per
/* BICC req: Q.1902.3
SiOptBckCalInd
optBckCalInd;
/* optional backward call indicators
SiOpFwdCalInd
opFwdCalInd;
/* optional forward call indicators
SiCallXferRef
callXferRef;
SiLoopPrevInd
loopPrevInd;
SiElementExt
elementExt[NUM_EXT_ELMTS]; /* extended elements */
} stStatEvnt;


//RELC relcParam;

int isup_start(isupConfig config);

//char * getLastErrDesc(int errno);
//int isup_startSignalingServer(isupSignalConfig config);

int  isup_setDebugInfo(isupLog::isupLogConfig debugInfo);
int isup_getEvt(EVENT *evt );

//Signaling functions
int isup_sendACM(unsigned short cic, ACM *param);
int isup_Answer(unsigned short cic, ANM *param);
int isup_Disconnect(unsigned short cic, REL * param);
int isup_sendRLC(unsigned short cic, RLC *param);
int isup_getIAMParam(unsigned char msg[0],IAM *param);
int isup_getACMParam(unsigned char msg[0],ACM *param);
int isup_getANMParam(unsigned char msg[0],ANM *param);
int isup_getRELParam(unsigned char msg[0],REL *param);
int isup_getRLCParam(unsigned char msg[0],RLC *param);
int isup_resetCircuit(unsigned short cic);
int isup_Dial(unsigned short cic, IAM * param,char *source_number, char *dest_number);
int isupInitialiseIAM(IAM  *iamParam,BYTE switchType = ITUT);
int isupInitialiseRLC(RLC  *param,BYTE switchType  = ITUT);
int isupInitialiseACM(ACM  *param,BYTE switchType  = ITUT);
int isupInitialiseANM(ANM  *param,BYTE switchType  = ITUT);
int isupInitialiseREL(REL  *param,BYTE cause = CCCALLCLR,BYTE switchType  = ITUT);
int isup_statusReq(unsigned short cic,BYTE msgType,stStatEvnt evt);
char * isup_getCallingPartyNumber(unsigned short cic);
char * isup_getCalledPartyNumber(unsigned short cic);
int isup_resetCircuitGrp(unsigned short cic,BYTE range);
int isup_resetAllCircuits();
int checkCICFree(unsigned short cic);
int isup_setChanStatus(unsigned short chanNo,unsigned short status);
int isup_startDTMFcollect(unsigned short chanNo,IOTERM *term);
int isup_getDigit(unsigned char msg[0],char * digitBuf );
int isup_setChanStatus(unsigned short chanNo,unsigned short status);
int isup_sendDTMF(unsigned short chanNo,unsigned short dtmf);
int isup_blockCircuit(unsigned short chanNo);
int isup_unblockCircuit(unsigned short chanNo);
int isup_blockCircuitGrp(unsigned short chanNo,BYTE range);
int isup_unblockCircuitGrp(unsigned short chanNo,BYTE range);


//Media functions
int isup_loadIndex(int file_count,char * filePath);
int isup_playFromIndex(unsigned short port,int announcements,unsigned short *announcement_id,IOTERM playIOTerm);
int isup_playFromFile(unsigned short port,char *filePath,IOTERM playIOTerm);
int isup_stopPlay(unsigned short port);
#endif	/* LIBJCTI_H */

