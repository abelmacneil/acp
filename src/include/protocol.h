#ifndef PROTOCOL_H_INCLUDED
#define PROTOCOL_H_INCLUDED
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h> 

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#define PORT "4309" // the port client will be connecting to 
#define MAXDATASIZE 512 // max number of bytes we can get at once 
#define BACKLOG 10

#define COMMAND_INVALID -1
#define COMMAND_SEND     1
#define COMMAND_GRAB     2
#define COMMAND_LS       30
#define COMMAND_LL       40



char *cmdtostr(int cmd);

void print_results(FILE *fp, int cmd, char *filename, int sum, int npackets, char *ipstr);

void *get_inet_addr(struct sockaddr *sa);

char *get_ip(char *devname);

size_t filelen(FILE *fp);


int recvfile(FILE *file, int sockfd, int *bytes_sent, int *npackets);

int sendfile(FILE *file, int sockfd, int *bytes_sent, int *npackets);

int recvtextfile(FILE *fp, int sockfd);

int sendtextfile(FILE *fp, int sockfd);

#endif /*PROTOCOL_H_INCLUDED*/
