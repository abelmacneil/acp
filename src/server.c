#include "include/protocol.h"
#include "include/crypto.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define FILEDIR "server-files/"
#define LOGDIR "log/"
#define LOGFILE (LOGDIR"/msg.log")

int log_results(char *ipstr, int cmd, char *filename, int sum);
void sigchld_handler(int i)
{
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void sigint_handler(int i)
{
    printf("Goodbye.\n");
    exit(0);
}

int findaddr(struct addrinfo *servinfo, int *sockfd)
{
    struct addrinfo *p;
    int yes = 1;
    for (p = servinfo; p != NULL; p = p->ai_next) {
        *sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (*sockfd == -1){
            perror("server: socket");
            continue;
        }

        if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, 
                    sizeof(int)) == -1) {
            perror("server: setsockopt");
            exit(1);
        }

        if (bind(*sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(*sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }
    if (!p) {
        fprintf(stderr, "server: failed to bind\n");
        return 1;
    }

    return 0;
}

void handle_ptr(void *ptr, char *msg)
{
    if (!ptr) {
        perror(msg);
        exit(1);
    }
}
void print_sep(FILE *fp)
{
    int i;
    for (i = 0; i < 40; i++)
        putc('-', fp);
    putc('\n', fp);
}
void make_logdir()
{
    struct stat st = {0};

    if (stat(LOGDIR, &st) == -1) {
        mkdir(LOGDIR, 0700);
    }
}

int main(int argc, char **argv)
{
    int sockfd, newfd;
    struct addrinfo hints, *servinfo;
    struct sockaddr_storage client_addr;
    socklen_t sin_size;
    struct sigaction sa;
    char filename[MAXDATASIZE];
    uint32_t cmd;
    char tmp[MAXDATASIZE];
    char *myip;
    int status, nbytes;

    make_logdir();
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    status = getaddrinfo(NULL, PORT, &hints, &servinfo);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    status = findaddr(servinfo, &sockfd);
    freeaddrinfo(servinfo);
    if (status != 0)
        return 1;

    if (listen(sockfd, BACKLOG) == -1) {
        perror("server: listen");
        return 1;
    }

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    signal(SIGINT, sigint_handler);

    myip = get_ip("wlan0");
    int len = strlen(myip) + 1;
    printf("HOST:%*s\nPORT:%*s\n", len, myip, len, PORT);
    printf("Waiting for connections...\n");
    print_sep(stdout);
    free(myip);
    while (1) {
        sin_size = sizeof client_addr;
        newfd = accept(sockfd, (struct sockaddr*)&client_addr, &sin_size);
        if (newfd == -1) {
            perror("accept");
            continue;
        }
        
        genkey();
        inet_ntop(client_addr.ss_family,
                get_inet_addr((struct sockaddr*)&client_addr),
                tmp, sizeof tmp);                
        if (!fork()) {
            close(sockfd);
            char path[MAXDATASIZE] = FILEDIR;
            char ipstr[MAXDATASIZE];
            strncpy(ipstr, tmp,MAXDATASIZE);
            uint32_t tmp_int;
            nbytes = recvall(newfd, (char*)&tmp_int, sizeof tmp_int, NULL);
            cmd = ntohl(tmp_int);
            if (nbytes == -1) {
                perror("server: recv");
                goto cleanup;
            }
            printf("%s ", ipstr);
            printf("request: '%s", cmdtostr(cmd));
            status = SERV_ERROR_OK;
            if (cmd < COMMAND_LS) {
                nbytes = recvall(newfd, filename, MAXDATASIZE, NULL);
                if (nbytes == -1) {
                    perror("recv");
                    goto cleanup;
                }
                printf(" %s'\n", filename);
                strncat(path, filename, MAXDATASIZE);
                if (cmd == COMMAND_GET && access(path, F_OK) == -1) {
                    fprintf(stderr, "File '%s' does not exist.\n", filename);
                    status = SERV_ERROR_NOFILE;
                }
            } else {
                puts("'");
            }
            //sprintf(tmp, "%d", status);
            tmp_int = htonl(status);
            nbytes = sendall(newfd, (char*)&tmp_int, sizeof tmp_int, NULL);
            if (status != 0 || nbytes == -1)
                goto cleanup;
            int sum = 0, npackets = 0;
            FILE *fp = NULL;
            if (cmd < COMMAND_LS) {
                if (cmd == COMMAND_SEND) {
                    fp = fopen(path, "wb");
                    handle_ptr(fp, "fopen");
                    status = recvfile(fp, newfd, &sum, &npackets);
                } else if (cmd == COMMAND_GET) {
                    fp = fopen(path, "rb");
                    handle_ptr(fp, "fopen");
                    status = send_file(fp, newfd, &sum, &npackets);
                } 
                if (fp != NULL)
                    fclose(fp);
            } else {
                char cmdstr[MAXDATASIZE];
                if (cmd == COMMAND_LS || cmd == COMMAND_LL) {
                    sprintf(cmdstr, "/bin/ls -l %s", FILEDIR);
                }
                fp = popen(cmdstr, "r");   
                handle_ptr(fp, "popen");
                status = sendtextfile(fp, newfd);
                if (fp)
                    pclose(fp);
            }
            if (status != 0) {
                perror("Error on network");
                goto cleanup;
            }

            status = log_results(ipstr, cmd, 
                    cmd < COMMAND_LS ? filename : NULL, sum);
            if (status != 0) {
                perror("Error on network");
                goto cleanup;
            }
            print_results(stdout, cmd, cmd < COMMAND_LS ? filename : NULL,
                    sum, npackets, ipstr);
cleanup:
            printf("Connection to %s closed.\n", ipstr);
            print_sep(stdout);
            close(newfd);
            exit(0);
        }
        close(newfd);
    }
    return 0;
}

int log_results(char *ipstr, int cmd, char *filename, int sum)
{
    FILE *logfp = fopen(LOGFILE, "a");
    if (logfp == NULL)
        return 1;
    time_t logtime = time(NULL);
    char *timestr = ctime(&logtime);
    timestr[strlen(timestr) - 1] = '\0';
    fprintf(logfp, "[%16s @ %s](%4s:%-15s => %3d bytes)\n",
            ipstr, timestr, cmdtostr(cmd),filename, sum);
    fclose(logfp);
    return 0;
}

