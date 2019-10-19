/* 
 * File:   signalingServer.h
 * Author: nihar
 *
 * Created on January 23, 2013, 3:54 PM
 */

#ifndef SIGNALINGSERVER_H
#define	SIGNALINGSERVER_H

#include "HashMap.h"




int connectToSignalingServer(isupSignalConfig config);
//int send_iam(unsigned char port, unsigned char trunk, unsigned char *source_number, unsigned char source_length, unsigned char *dest_number, unsigned char dest_length);


//int parseIAM(unsigned char msg[1000],IAM  iamParam);
int parseANM(unsigned char msg[1000],ANM *anmParam);
int parseACM(unsigned char msg[1000],ACM *acmParam);
int parseREL(unsigned char msg[1000],REL *relParam);

#endif	/* SIGNALINGSERVER_H */

