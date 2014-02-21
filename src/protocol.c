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
        case COMMAND_GRAB:  return "get";
        case COMMAND_LS:    return "ls";
        case COMMAND_LL:    return "ll";
    }
    return "";
}

char *serv_errstr(int status)
{
    switch (status) {
        case SERV_ERROR_OK:         return "No error.";
        case SERV_ERROR_NOFILE:     return "File does not exist.";
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

int sendall(int sockfd, char *data, int len, int *npackets)
{
    int total = 0;
    int bytes_left = len;
    int n;
    if (npackets)
        npackets = 0;
    while (total < len) {
        n = send(sockfd, data + total, bytes_left, 0);
        if (n == -1)
            break;
        total += n;
        bytes_left -= n;
        if (npackets)
            (*npackets)++;
    }
    return n == -1 || len != total ? -1 : total;
}
void load_bar(int x, int n, int r, int w)
{
    // Only update r times.
    if ( x % (n/r) != 0 ) return;
 
    // Calculuate the ratio of complete-to-incomplete.
    float ratio = x/(float)n;
    int   c     = ratio * w;
    int i;
    printf("\033[F\033[J");
    // Show the percentage complete.
    printf("%3d%% [", (int)(ratio*100) );
 
    // Show the load bar.
    for (i=0; i<c; i++)
       printf("=");
 
    for (i=c; i<w; i++)
       printf(" ");
 
    // ANSI Control codes to go back to the
    // previous line and clear it.
    printf("]"); 
    if (n > 2 * 1000)
        printf("(%dKB/%dKB)\n", x / 1000, n / 1000);
    else
        printf("(%dB/%dB)\n", x, n);
}
int recvall(int sockfd, char *data, int len, int *npackets)
{
    int total = 0;
    int bytes_left = len;
    int n;

    if (npackets)
        npackets = 0;
    while (total < len) {
        n = recv(sockfd, data + total, bytes_left, 0);
        if (n == -1)
            break;
        total += n;
        bytes_left -= n;
        if (npackets)
            (*npackets)++;
    }
    return n == -1 || len != total ? -1 : total;
}
int recvfile(FILE *file, int sockfd, int *bytes_recv, int *npackets)
{
    char buf[BUFLEN];
    int nbytes = 0;
    size_t n = 0, size;
    uint32_t tmp_int;
    *bytes_recv = 0;
    *npackets = 0;
    if (file == NULL)
        return -1;
    nbytes = recvall(sockfd, (char*)&tmp_int, sizeof tmp_int, 0);
    if (nbytes <= 0)
        return -1;
    size = ntohl(tmp_int);
    //printf("size: %zu\n", size);
    do {
        memset(buf, 0, sizeof buf);
        if (*bytes_recv + sizeof buf > size)
            nbytes = size - *bytes_recv;
        else
            nbytes = sizeof buf;
        n = recvall(sockfd, buf, nbytes, npackets);
        if (n > 0){
            if (*bytes_recv + n > size)
                n = size - *bytes_recv;
            xorstr(buf, n);
            n = fwrite(buf, 1, n, file);
            *bytes_recv += n;
            (*npackets)++;
            //printf("bytes received %d/%zu : n = %zu\n", *bytes_recv, size, n);
            if (size > 1000) 
                load_bar(*bytes_recv, size, size / 1000, 20);
            else
                load_bar(*bytes_recv, size, 1, 20);
        }
    } while (*bytes_recv < size && n >= 0);
    load_bar(*bytes_recv, size, 1, 20);
    return n < 0 ? n : 0;
}

int sendfile(FILE *file, int sockfd, int *bytes_sent, int *npackets)
{
    int nbytes;
    char buf[BUFLEN];
    size_t n = 0, size = filelen(file);
    uint32_t tmp_int = htonl(size);
    if (file == NULL)
        return -1;
    *npackets = 0;
    *bytes_sent = 0;
    //printf("size: %zu\n", size);
    nbytes = sendall(sockfd, (char*)&tmp_int, sizeof tmp_int, 0);
    if (nbytes <= 0)
        return -1;
    do {
        memset(buf, 0, sizeof buf);
        n = fread(buf, 1, sizeof buf, file);
        if (n > 0) {
            xorstr(buf, sizeof buf);
            *bytes_sent += n;
            n = sendall(sockfd, buf, sizeof buf, npackets);
            (*npackets)++;
            if (size > 1000) 
                load_bar(*bytes_sent, size, size / 1000, 20);
            else
                load_bar(*bytes_sent, size, 1, 20);

        }
    } while (*bytes_sent < size && n >= 0 && !feof(file));
    load_bar(*bytes_sent, size, 1, 20);
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
        nbytes = recvall(sockfd, buf, MAXDATASIZE, NULL);
        if (nbytes == -1) {
            perror("server: recv");
            exit(1);
        }
        //if (strcmp(buf, "") != 0) {
            xorstr(buf, sizeof buf);
            fprintf(fp, "%s", buf);
        //}
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
        if (!feof(fp)) {
        fgets(buf, MAXDATASIZE, fp);
            xorstr(buf, sizeof buf);
            status = sendall(sockfd, buf, MAXDATASIZE, NULL);
            //printf(buf);
            if (status == -1) {
                perror("send");
                return status;
            }
        }
    }
    send(sockfd, "", 1, 0);
    return 0;
}
