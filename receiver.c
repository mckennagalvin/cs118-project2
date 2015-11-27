
/*
 receiver.c
 client which requests a file from the sender (server) using UDP
 usage: ./receiver <hostname> <port number> <filename>
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>      // define structures like hostent
#include <stdlib.h>
#include <strings.h>

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd; // socket descriptor
    int portno, n, recvlen;
    socklen_t servlen;
    struct sockaddr_in serv_addr;
    struct hostent *server; // contains tons of information, including the server's IP address
    char recv_buf[2048]; // receive buffer
    char * send_message = "this is a test, receiver to sender";

    char buffer[256];
    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }

    // create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    
    server = gethostbyname(argv[1]); // takes a string like "www.yahoo.com", and returns a struct hostent which contains information, as IP address, address type, the length of the addresses...
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    
    // fill in address info
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    portno = atoi(argv[2]);
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    
    // send request
    if (sendto(sockfd, send_message, strlen(send_message) + 1, 0, (struct sockaddr *) &serv_addr, servlen) < 0)
      error("ERROR sending request");
    printf("sent request");

    // scan for messages from server
    while (1) {

        recvlen = recvfrom(sockfd, recv_buf, 2048, 0, (struct sockaddr *) &serv_addr, &servlen);
        if (recvlen < 0)
          error("ERROR receiving from server");
        printf("received %d bytes from server\n", recvlen);

    }
    
    close(sockfd);
    return 0;
}
