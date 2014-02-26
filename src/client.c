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
#include <time.h>
#include <sys/time.h>
#include <limits.h>

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

void print_help(FILE *fp, char *prog_name)
{
    size_t cmd_len = 25;
    char format[] =  "%-*s => %s\n";
    fprintf(fp, "Usage: %s HOSTIP COMMAND [ARGS...]\n", prog_name);
    fprintf(fp, "Commands:\n");
    fprintf(fp, format, cmd_len, 
            "send LOCALFILE", 
            "copies LOCALFILE to the server.\n");
    fprintf(fp, format, cmd_len, 
            "get SERVERFILE LOCALFILE", 
            "copies SERVERFILE from the server to LOCALFILE.\n");
    fprintf(fp, format, cmd_len, 
            "ls",
            "Long listing of files on the server.");
}


int main(int argc, char **argv)
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int status, serv_status;
    char ipstr[INET6_ADDRSTRLEN];
    char serv_filename[PATH_MAX];
    char local_filename[PATH_MAX];
    int cmd = COMMAND_INVALID;
    double t1, t2;
    if (argc == 5) {
        if (strncmp(argv[2], "get", MAXDATASIZE) == 0) {
            cmd = COMMAND_GET;
        }
    } if (argc >= 4) {
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
    if (cmd == COMMAND_SEND) {
        strncpy(local_filename ,argv[3], sizeof local_filename);
        if (argc == 5)
            strncpy(serv_filename ,argv[4], sizeof serv_filename);
        else
            strncpy(serv_filename, local_filename, sizeof local_filename);
    } else if (cmd == COMMAND_GET) {
        strncpy(serv_filename, argv[3], sizeof serv_filename);
        if (handle_new_filename(local_filename, argv[3], argv[4],
                    sizeof local_filename) != 0) {
            char dir[PATH_MAX];

            get_dir_from_path(argv[4], strlen(argv[4]), dir);
            fprintf(stderr,"Directory '%s' does not exist.\n", dir);
            return 2;
        }
    }

    if (cmd == COMMAND_INVALID) {
        print_help(stdout, argv[0]);
        return 1;
    }

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
    //printf("Server status: %d\n", serv_status);
    if (serv_status != 0) {
        fprintf(stderr, "Error on server: %s\n", serv_errstr(serv_status));
        status = serv_status;
        goto cleanup;
    }
    int sum, npackets;
    FILE *fp;
    t1 = get_current_millis();
    if (cmd == COMMAND_SEND) {
        fp = fopen(local_filename, "rb");
        status = sendfile(fp, sockfd, &sum, &npackets);
        fclose(fp);
    } else if (cmd == COMMAND_GET) {
        fp = fopen(local_filename, "wb");
        status = recvfile(fp, sockfd, &sum, &npackets);
        fclose(fp);
    } else {
        status = recvtextfile(stdout, sockfd);
    }
    if (status != 0) {
        fprintf(stderr, "Error on server: %s\n", serv_errstr(status));
    }
    t2 = get_current_millis();
    printf("Total transfer time: %.3fs\n", (float) (t2 - t1)/1000);
    print_results(stdout, cmd, serv_filename, sum, npackets, ipstr);
cleanup:
    close(sockfd);
    printf("Connection to %s closed.\n", ipstr);
    return status;
}

