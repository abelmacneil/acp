/**
 * @file protocol.c
 * @brief implementation of the protocol
 */
#include "protocol.h"
#include "crypto.h"
#include <time.h>
#include <stdint.h>
#include <math.h>

#define BUFLEN      MAXDATASIZE

/**
 * @brief given the command number, it generates the corresponding
 * textual command
 *
 * @param cmd the command as a number.
 * @return the command in test format
 */
char *cmdtostr(int cmd)
{
    switch(cmd) {
        case COMMAND_SEND:  return "send";
        case COMMAND_GRAB:  return "grab";
        case COMMAND_LS:    return "ls";
        case COMMAND_LL:    return "ll";
    }
    return "";
}

void print_results(FILE *fp, int cmd, char *filename, 
        int sum, int npackets, char *ipstr)
{

    if (cmd < COMMAND_LS) {
        fprintf(fp,"Transferred '%s' (%d bytes) in %d packet%s.\n",
                filename, sum, npackets, npackets == 1 ? "" : "s");
    }
}

void *get_inet_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

char *get_ip(char *devname)
{
    int fd;
    struct ifreq ifr;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, devname, IFNAMSIZ-1);
    ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);

    char *res = malloc(INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr),
            res, INET_ADDRSTRLEN);
    return res;
}


size_t filelen(FILE *fp)
{
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    rewind(fp);
    return size;
}

int recvfile(FILE *file, int sockfd, int *bytes_recv, int *npackets)
{
    char buf[BUFLEN];
    int nbytes = 0;
    *bytes_recv = 0;
    size_t n = 0, size;
    *npackets = 0;
    if (file == NULL)
        return 1;
    nbytes = recv(sockfd, buf, sizeof buf, 0);
    if (nbytes <= 0)
        return 1;
    size = atoi(buf);
    do {
        memset(buf, 0, sizeof buf);
        n = recv(sockfd, buf, sizeof buf, 0);
        if (n > 0){
            if (*bytes_recv + n > size)
                n = size - *bytes_recv;
            xorstr(buf, n);
            n = fwrite(buf, 1, n, file);
            *bytes_recv += n;
            (*npackets)++;
        }
    } while (*bytes_recv < size && n >= 0);
    return n < 0 ? n : 0;
}

int sendfile(FILE *file, int sockfd, int *bytes_sent, int *npackets)
{
    int nbytes;
    char buf[BUFLEN];
    size_t n = 0, size = filelen(file);
    *npackets = 0;
    *bytes_sent = 0;
    if (file == NULL)
        return 1;
    sprintf(buf, "%zu", size);
    nbytes = send(sockfd, buf, sizeof buf, 0);
    if (nbytes <= 0)
        return 1;
    do {
        memset(buf, 0, sizeof buf);
        n = fread(buf, 1, sizeof buf, file);
        if (n > 0) {
            xorstr(buf, sizeof buf);
            *bytes_sent += n;
            n = send(sockfd, buf, sizeof buf, 0);
            (*npackets)++;
        }
    } while (*bytes_sent < size && n >= 0 && !feof(file));
    return n < 0 ? n : 0;
}

int recvtextfile(FILE *fp, int sockfd)
{
    char buf[MAXDATASIZE];
    int nbytes = 0;
    if (fp == NULL)
        return 1;
    do {
        memset(buf, 0, sizeof buf);
        nbytes = recv(sockfd, buf, MAXDATASIZE, 0);
        if (nbytes == -1) {
            perror("server: recv");
            exit(1);
        }
        if (strcmp(buf, "") != 0) {
            xorstr(buf, sizeof buf);
            fprintf(fp, "%s", buf);
        }
    } while (strcmp(buf, "") != 0);
    return 0;
}

int sendtextfile(FILE *fp, int sockfd)
{
    int status;
    char buf[MAXDATASIZE];
    if (fp == NULL)
        return 1;
    while (!feof(fp)) {
        memset(buf, 0, sizeof buf);
        fgets(buf, MAXDATASIZE, fp);
        if (!feof(fp)) {
            xorstr(buf, sizeof buf);
            status = send(sockfd, buf, MAXDATASIZE, 0);
            if (status == -1) {
                perror("send");
                return status;
            }
        }
    }
    send(sockfd, "", 1, 0);
    return 0;
}
