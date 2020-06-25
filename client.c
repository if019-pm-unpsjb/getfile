#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <fcntl.h>
#include <sys/types.h> 
#include <sys/stat.h>
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 

#define IP "127.0.0.1"
#define PORT 8888
#define BUF_SIZE 1024

int main(int argc, char* argv[])
{ 
    int sockfd;
    char buffer[BUF_SIZE]; 
    char *filename;
    struct sockaddr_in	 servaddr; 

    // Create socket.
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 

    // Server address.
    memset(&servaddr, 0, sizeof(servaddr)); 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons(PORT);
    inet_aton(IP, &(servaddr.sin_addr));

    // Connecto to the eportero server.
    connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr));

    int n;
    printf("Connected to server %s:%d ...\n", inet_ntoa(servaddr.sin_addr), 
            ntohs(servaddr.sin_port));

    do {
        printf("File to retrieve: "); fflush(stdout);
        fgets(buffer, BUF_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0'; // removes new line

        filename = (char*) malloc(strlen(buffer) + 1);
        strncpy(filename, buffer, strlen(buffer) + 1);

        // Send the filename to the server.
        n = send(sockfd, filename, strlen(buffer), 0);
        if (n < 0) {
            perror("sendto");
            exit(EXIT_FAILURE);
        }

        printf("Sended %s (%d bytes) to %s:%d ...\n", filename, n,
                inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));

        off_t filesize = 0;
        int br;

        // If the server send back the size the file is found.
        recv(sockfd, &filesize, sizeof(filesize), 0);
        filesize = ntohl(filesize);
        if (filesize == 0) {
            printf("File not found in the server.\n");
            continue;
        } else {
            printf("File %s found (%ld bytes).\n", filename, filesize);
        }


        int fd;
        int flags = S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH;
        fd = open(filename, O_CREAT | O_TRUNC | O_WRONLY, flags);
        if (fd < 0) {
            perror("open");
            continue;
        }

        int bcount = 0;
        while ((br = recv(sockfd, buffer, BUF_SIZE, 0)) > 0) {
            bcount += br;
            printf("Retrieved %d bytes...\n", br);
            write(fd, buffer, br);
            if (bcount == filesize) {
                break;
            }
        }

        if (br == -1) {
            perror("recv");
        }

        printf("File received: %s (%d bytes).\n", filename, bcount);
        close(fd);
        free(filename);
    } while (buffer[0] != '\n');

    close(sockfd); 
    return 0; 
}
