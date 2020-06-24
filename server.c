#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <pthread.h>

#define IP "127.0.0.1"
#define PORT 8888
#define BUF_SIZE 1024

int thread_id = 0;

static void* handle_request_tcp(void* s);

int main(int argc, char *argv[])
{
    int lsock, csock;
    struct sockaddr_in addr;
    struct sockaddr_in peeraddr;
    pthread_t thread;

    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_aton(IP, &(addr.sin_addr));

    lsock = socket(AF_INET, SOCK_STREAM, 0);
    if (lsock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int optval = 1;
    int optname = SO_REUSEADDR | SO_REUSEPORT;
    if(setsockopt(lsock, SOL_SOCKET, optname, &optval, sizeof(optval)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if (bind(lsock, (struct sockaddr*) &addr, sizeof(struct sockaddr_in)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(lsock, 10) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[%d] Waiting for connections...\n", getpid());

    for (;;) {
        memset(&peeraddr, 0, sizeof(struct sockaddr_in));
        socklen_t socklen = sizeof(struct sockaddr_in);

        csock = accept(lsock, (struct sockaddr*) &peeraddr, &socklen);
        if (csock == -1) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        thread_id++;
        pthread_create(&thread, NULL, handle_request_tcp, (void*) &csock);
    }

    exit(EXIT_SUCCESS);
}

static void* handle_request_tcp(void* s)
{
    static __thread int id = 0;
    static __thread int sock = 0;
    id = thread_id;

    char buf[BUF_SIZE];
    char *filename;
    ssize_t n;

    sock = *((int*) s);

    struct sockaddr_in remote;
    struct sockaddr_in local;

    socklen_t size = sizeof(struct sockaddr_in);
    if (getpeername(sock, (struct sockaddr*) &remote, &size) == -1) {
        perror("getpeername");
    }

    if (getsockname(sock, (struct sockaddr*) &local, &size) == -1) {
        perror("getsockname");
    }

    printf("[%d] Connection accepted. Local: %s:%d. Remote: %s:%d ...\n", 
            getpid(), inet_ntoa(local.sin_addr), ntohs(local.sin_port), 
            inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));
    
    while ((n = recv(sock, buf, BUF_SIZE, 0)) > 0) {
        buf[n]='\0';
        printf("[%d] Client requested the file %s\n", id, buf);

        filename = (char*) malloc(strlen(buf) + 1);
        strncpy(filename, buf, strlen(buf) + 1);

        int fd = open(filename, O_RDONLY);
        if (fd < 0) {
            sprintf(buf, "[%d] %s", id, filename);
            perror(buf);
            sprintf(buf, "0");
            send(sock, buf, strlen(buf), 0);
            continue;
        } else {
            sprintf(buf, "1");
            send(sock, buf, strlen(buf), 0);
        }

        int bs, scount;
        while ((bs = sendfile(sock, fd, NULL, BUF_SIZE)) > 0) {
            scount += bs;
        }

        if (bs != -1) {
            printf("[%d] File %s (%d bytes) sended.\n", id, filename, scount);
        } else {
            sprintf(buf, "[%d] sendfile", id);
            perror(buf);
        }

        close(fd);
    }

    pthread_exit(NULL);
}

