/**
 * @file protocol.c
 * @brief implementation of the protocol
 */
#include "protocol.h"
#include "crypto.h"
#include <time.h>
#include <stdint.h>
#include <math.h>
#include <sys/time.h>

#define BUFLEN      MAXDATASIZE
#define BYTE        1
#define KILOBYTE    1024 * BYTE

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

double get_current_millis()
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return (t.tv_sec) * 1000 + (t.tv_usec) / 1000 ;
}

size_t filelen(FILE *fp)
{
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    rewind(fp);
    return size;
}
int doall(ssize_t (*sockfct) (int, const void *, size_t, int), 
        int sockfd, char *data, int len, int *npackets) {

    int total = 0;
    int bytes_left = len;
    int n;
    while (total < len) {
        n = sockfct(sockfd, data + total, bytes_left, 0);
        if (n == -1)
            break;
        total += n;
        bytes_left -= n;
        if (npackets)
            (*npackets)++;
    }
    return n == -1 || len != total ? -1 : total;
}
int sendall(int sockfd, char *data, int len, int *npackets)
{
    return doall(&send, sockfd, data, len, npackets);
}
int recvall(int sockfd, char *data, int len, int *npackets)
{
    return doall((ssize_t (*) (int, const void *, size_t, int))&recv, 
            sockfd, data, len, npackets);
}
static inline void load_bar(int bytes_used, int size, int nreps, int nbars)
{
    static int last_bytes = -1;
    //static struct timeval last_time;
    static uint32_t times_run = 0;
    static float rate;
    const int UPDATE_RATE = 10;
    //struct timeval this_time;
    static double t1;
    static double t2;
    if ( bytes_used % (size/nreps) != 0 ) return;
 
    float ratio = bytes_used/(float)size;
    int   c     = ratio * nbars;
    int i;
    if (last_bytes == -1)
        puts("");
    if (times_run % UPDATE_RATE == 0)
        t2 = get_current_millis();
        //gettimeofday(&this_time, NULL);
    printf("\033[F\033[J");
    printf("%3d%% [", (int)(ratio*100) );
 
    for (i = 0; i < c; i++)
       printf("=");
    for (i = c; i < nbars; i++)
       printf(" ");
 
    printf("]");
    char size_unit[3];
    if (bytes_used > 2 * KILOBYTE) {
        bytes_used /= KILOBYTE;
        size /= KILOBYTE;
        strncpy(size_unit, "KB", sizeof size_unit);
    }
    else {
       strncpy(size_unit, "B", sizeof size_unit); 
    }

    printf("(%d%s/%d%s)", bytes_used , size_unit, size, size_unit);
    if (times_run % UPDATE_RATE == 0) {
        //double t1 = (this_time.tv_sec) * 1000 + (this_time.tv_usec) / 1000 ;
        //double t2 = (last_time.tv_sec) * 1000 + (last_time.tv_usec) / 1000 ;
        rate = 1e3*(bytes_used-last_bytes)/(t2-t1);
        //gettimeofday(&last_time, NULL);
        t1 = get_current_millis();
        last_bytes = bytes_used;
    }
    printf(" %.1f %s/s", rate, size_unit);
    puts("");
    times_run++;
}

int get_nreps(size_t size)
{
    int nreps = -1, i;

    for (i = 10; i <= 10000; i *= 10) {
        if (size > 2 * i *KILOBYTE) {
            nreps = (int) (size / ((i/1000.0)*KILOBYTE));
        }
    }
    if (nreps == -1)
        nreps = 1;
    return nreps;
}
int recvfile(FILE *file, int sockfd, int *bytes_recv, int *npackets)
{
    char buf[BUFLEN];
    int nbytes = 0;
    size_t n = 0, size;
    uint32_t tmp_int;
    int nreps;
    *bytes_recv = 0;
    *npackets = 0;
    if (file == NULL)
        return -1;
    nbytes = recvall(sockfd, (char*)&tmp_int, sizeof tmp_int, 0);
    if (nbytes <= 0)
        return -1;
    size = ntohl(tmp_int);
    nreps = get_nreps(size);
    //printf("size: %zu\n", size);
    load_bar(*bytes_recv, size, 1, 20);
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
            //printf("bytes received %d/%zu : n = %zu\n", *bytes_recv, size, n);
            load_bar(*bytes_recv, size, nreps, 20);
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
    int nreps = get_nreps(size);
    if (file == NULL)
        return -1;
    *npackets = 0;
    *bytes_sent = 0;
    //printf("size: %zu\n", size);
    nbytes = sendall(sockfd, (char*)&tmp_int, sizeof tmp_int, 0);
    if (nbytes <= 0)
        return -1;
    load_bar(*bytes_sent, size, 1, 20);
    do {
        memset(buf, 0, sizeof buf);
        n = fread(buf, 1, sizeof buf, file);
        if (n > 0) {
            xorstr(buf, sizeof buf);
            *bytes_sent += n;
            n = sendall(sockfd, buf, sizeof buf, npackets);
            load_bar(*bytes_sent, size, nreps, 20);

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
