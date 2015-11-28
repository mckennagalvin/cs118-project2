/*
 receiver.c
 client which requests a file from the sender (server) using UDP
 usage: ./receiver <hostname> <port number> <filename> <loss probability> <corruption probability>
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>      // define structures like hostent
#include <stdlib.h>
#include <strings.h>

 #include "packet.c"

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd; // socket descriptor
    struct hostent *server; // contains tons of information, including the server's IP address
    int portno;
    char * filename;
    int lossprob;
    int corruptprob;
    int recvlen;
    socklen_t servlen;
    struct sockaddr_in serv_addr;
    struct packet request;
    struct packet receive;

    if (argc < 6) {
       fprintf(stderr,"usage %s <hostname> <port> <filename> <loss probability> <corruption probability>\n", argv[0]);
       exit(0);
    }

    // get command line arguments
    server = gethostbyname(argv[1]);
    portno = atoi(argv[2]);
    filename = argv[3];
    lossprob = atof(argv[4]);
    corruptprob = atof(argv[5]);

    // error checks on arguments
    if (lossprob < 0.0 || lossprob > 1.0)
        error("packet loss probability must be between 0 and 1");
    if (corruptprob < 0.0 || corruptprob > 1.0)
        error("packet corruption probability must be between 0 and 1");
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    // create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    
    // fill in address info
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    
    // create request
    memset((char *) &request, 0, sizeof(request));
    strcpy(request.data, filename);
    request.length = strlen(filename) + 1;
    request.type = TYPE_REQUEST;

    printf("REQUEST PACKET:\n");
    printf("data: %s\n", request.data);
    printf("length: %d\n", request.length);
    printf("type: %d\n\n", request.type);

    // send request
    if (sendto(sockfd, &request, sizeof(request), 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
      error("ERROR sending request");
    printf("sent request for file %s\n", filename);

    // scan for messages from server
    while (1) {

        recvlen = recvfrom(sockfd, &receive, sizeof(receive), 0, (struct sockaddr *) &serv_addr, &servlen);
        if (recvlen < 0)
          error("ERROR receiving from server");
        printf("packet received from server: seq #%d, %d bytes data\n", receive.seq, receive.length);

    }
    
    close(sockfd);
    return 0;
}
