#include "protocol.h"
#include "crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>
#include <sys/time.h>

#include <arpa/inet.h>

void newfunct()
{
    printf("Stuff.\n");
}

void print_badcmd()
{
    fprintf(stderr, "No such command.\n");
    exit(1);
}

int main(int argc, char **argv)
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int status, serv_status;
    char ipstr[INET6_ADDRSTRLEN];
    char serv_filename[FILENAME_MAX];
    char local_filename[FILENAME_MAX];
    int cmd = COMMAND_INVALID;
    //struct timeval t1, t2;
    //t1 = clock();
    double t1, t2;
    if (argc == 5) {
        if (strncmp(argv[2], "get", MAXDATASIZE) == 0) {
            cmd = COMMAND_GRAB;
        }
    } else if (argc == 4) {
        if (strncmp(argv[2], "send", MAXDATASIZE) == 0)  {
            cmd = COMMAND_SEND;
            if (access(argv[3], F_OK) == -1) {
                fprintf(stderr, "File '%s' does not exist.\n", argv[3]);
                return 2;
            }
        }
    } else if (argc == 3) {
        if (strncmp(argv[2], "ls", MAXDATASIZE) == 0) 
            cmd = COMMAND_LS;
        else if (strncmp(argv[2], "ll", MAXDATASIZE) == 0) 
            cmd = COMMAND_LL;
    }
    if (cmd == COMMAND_SEND)
        strncpy(serv_filename ,argv[3], sizeof serv_filename);
    if (cmd == COMMAND_GRAB) {
        strncpy(serv_filename, argv[3], sizeof serv_filename);
        struct stat info;
        stat(argv[4], &info);
        if (S_ISDIR(info.st_mode)) {
            char *tmp;
            strncpy(local_filename, argv[4], sizeof local_filename);
            strncat(local_filename, "/", sizeof local_filename);
            strncat(local_filename, serv_filename, sizeof local_filename);
        } else {
            strncpy(local_filename, argv[4], sizeof local_filename);
        }
    }
        

    if (cmd == COMMAND_INVALID)
        print_badcmd();

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
            perror("client: connect");
            continue;
        } 
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "Failed to connect to %s\n", ipstr);
        return 2;
    }

    genkey();
    inet_ntop(p->ai_family, get_inet_addr((struct sockaddr*)p->ai_addr),
            ipstr, sizeof ipstr);
    printf("Connecting to %s\n", ipstr);

    freeaddrinfo(servinfo);

    //sprintf(tmp, "%d", argc);
    uint32_t tmp_int;
    tmp_int = htonl(cmd);
    status = sendall(sockfd, (char*)&tmp_int, sizeof tmp_int, NULL);
    if (cmd < COMMAND_LS) 
        status = sendall(sockfd, serv_filename, MAXDATASIZE, NULL);
    if (status == -1) {
        perror("send");
        exit(1);
    }
    recvall(sockfd, (char*)&tmp_int, sizeof tmp_int, NULL);
    serv_status = ntohl(tmp_int);
    printf("Server status: %d\n", serv_status);
    if (serv_status != 0) {
        fprintf(stderr, "Error on server: %s\n", serv_errstr(serv_status));
        goto cleanup;
    }
    int sum, npackets;
    FILE *fp;
    t1 = get_current_millis();
    if (cmd == COMMAND_SEND) {
        fp = fopen(serv_filename, "rb");
        status = sendfile(fp, sockfd, &sum, &npackets);
        fclose(fp);
    } else if (cmd == COMMAND_GRAB) {
        fp = fopen(local_filename, "wb");
        status = recvfile(fp, sockfd, &sum, &npackets);
        fclose(fp);
    } else {
        status = recvtextfile(stdout, sockfd);
    }
    if (status != 0) {
        fprintf(stderr, "Error on server: %s\n", serv_errstr(status));
    }
    //t2 = clock();
    t2 = get_current_millis();
    //float time_diff = (((float)t2 - (float)t1) / CLOCKS_PER_SEC ) * 1000;    
    printf("Total transfer time: %.3fs\n", (float) (t2 - t1)/1000);
    print_results(stdout, cmd, serv_filename, sum, npackets, ipstr);
cleanup:
    close(sockfd);
    printf("Connection to %s closed.\n", ipstr);
    return status;
}

