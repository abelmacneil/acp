#include "protocol.h"
#include "crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

void newfunct()
{
    printf("Stuff.\n");
}


int main(int argc, char **argv)
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int status;
    char ipstr[INET6_ADDRSTRLEN];
    char filename[FILENAME_MAX];
    int cmd;
    char tmp[MAXDATASIZE];

    if (argc > 2) {
        if (argc > 3) {
            if (strncmp(argv[2], "send", MAXDATASIZE) == 0)  {
                cmd = COMMAND_SEND;
                if (access(argv[3], F_OK) == -1) {
                    fprintf(stderr, "File '%s' does not exist.\n", argv[3]);
                    return 2;
                }
            } else if (strncmp(argv[2], "grab", MAXDATASIZE) == 0) {
                cmd = COMMAND_GRAB;
            }
            strncpy(filename ,argv[3], sizeof filename);
        } else if (strncmp(argv[2], "ls", MAXDATASIZE) == 0) 
            cmd = COMMAND_LS;
        else if (strncmp(argv[2], "ll", MAXDATASIZE) == 0) 
            cmd = COMMAND_LL;
        else {
            fprintf(stderr, "Invalid command.\n");
            return 1;
        }
    } else
        return 1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(argv[1], PORT, &hints, &servinfo);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

        
    for (p = servinfo; p != NULL; p = p->ai_next)  {

        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client connect");
            continue;
        } 
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    genkey();
    inet_ntop(p->ai_family, get_inet_addr((struct sockaddr*)p->ai_addr),
            ipstr, sizeof ipstr);
    printf("Connecting to %s\n", ipstr);

    freeaddrinfo(servinfo);

    //sprintf(tmp, "%d", argc);
    status = send(sockfd, &argc, MAXDATASIZE, 0);
    sprintf(tmp, "%d", cmd);
    status = send(sockfd, tmp, MAXDATASIZE, 0);
    if (cmd < COMMAND_LS) 
        status = send(sockfd, filename, MAXDATASIZE, 0);
    if (status == -1) {
        perror("send");
        exit(1);
    }
    int sum, npackets;
    FILE *fp;
    if (cmd == COMMAND_SEND) {
        fp = fopen(filename, "rb");
        status = sendfile(fp, sockfd, &sum, &npackets);
        fclose(fp);
    } else if (cmd == COMMAND_GRAB) {
        fp = fopen(filename, "wb");
        status = recvfile(fp, sockfd, &sum, &npackets);
        fclose(fp);
    } else {
        status = recvtextfile(stdout, sockfd);
    }
    if (status != 0) {
        fprintf(stderr, "Error on network.\n");
    }
    close(sockfd);
    print_results(stdout, cmd, filename, sum, npackets, ipstr);
    printf("Connection to %s closed.\n", ipstr);
    return 0;
}

