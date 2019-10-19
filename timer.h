/* 
 * File:   timer.h
 * Author: root
 *
 * Created on November 27, 2013, 4:51 AM
 */

#ifndef TIMER_H
#define	TIMER_H




#define DEFAULT_RETRY   0x03
#define DEFAULT_TIMEOUT 60

//timer source
#define SS7_SIG         0x01
#define SS7_MED         0x02



//timer type
#define TIMER_NONE      0
#define SS7_T1_TIMER    0x01
#define SS7_T5_TIMER    0x05
#define SS7_T7_TIMER    0x07
#define SS7_T9_TIMER    0x09
#define SS7_T16_TIMER   0x10
#define SS7_T17_TIMER   0x11



//timer Expiry
#define T1_EXPIRY       60
#define T5_EXPIRY       5*60
#define T7_EXPIRY       30
#define T9_EXPIRY       180
#define T16_EXPIRY      60
#define T17_EXPIRY      5*60

#define TIMER_ADD       0x01
#define TIMER_REMOVE    0x02


typedef struct timerT1
{
    unsigned short timeout;
    unsigned short retry;
}stT1;

typedef struct timerT5
{
    unsigned short timeout;
    unsigned short retry;
}stT5;

typedef struct timerT7
{
    unsigned short timeout;
    unsigned short retry;
}stT7;

typedef struct timerT9
{
    unsigned short timeout;
    unsigned short retry;
}stT9;

typedef struct timerT16
{
    unsigned short timeout;
    unsigned short retry;
}stT16;

typedef struct timerT17
{
    unsigned short timeout;
    unsigned short retry;
}stT17;

typedef struct ss7Timer
{
    stT1        t1;
    stT5        t5;
    stT7        t7;
    stT9        t9;   
    stT16       t16;
    stT17       t17;
    
}stss7Timer;

typedef struct _timer
{
    unsigned short id;
    unsigned char addRemove;
    unsigned char type;
    unsigned char source;
    unsigned short retry;
    time_t callstartTime;
}stTimer;


int addTimer(stTimer t);
int cancelTimer(stTimer t);
#endif	/* TIMER_H */

