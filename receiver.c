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

void send_ack(int ack_num, int sockfd, struct sockaddr_in serv_addr)
{
    struct packet ack;
    ack.type = TYPE_ACK;
    ack.seq = ack_num;
    ack.length = 0;
    strcpy(ack.data, "");

    if (sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR sending request");
    
    printf("Sent ack %d.\n", ack_num);
}

int main(int argc, char *argv[])
{
    int sockfd; // socket descriptor
    struct hostent *server; // contains tons of information, including the server's IP address
    int portno;
    char * filename;
    FILE * f;
    double lossprob;
    double corruptprob;
    int result;
    socklen_t servlen;
    struct sockaddr_in serv_addr;
    struct packet request;
    struct packet receive;
    int expected_seq = 0;

    if (argc < 6) {
       fprintf(stderr,"usage %s <hostname> <port> <filename> <loss probability> <corruption probability>\n", argv[0]);
       exit(1);
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

    // send request
    if (sendto(sockfd, &request, sizeof(request), 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
      error("ERROR sending request");
    printf("Sent request for file %s\n", filename);

    // create copy of file on receiver's end
    char * prefix = "copy_";
    char * filecopy = malloc(strlen(filename) + strlen(prefix) + 1);
    strcpy(filecopy, prefix);
    strcat(filecopy, filename);
    f = fopen(filecopy, "w");

    // scan for messages from server
    while (1) {

        result = rdt_receive(sockfd, &receive, sizeof(receive), (struct sockaddr *) &serv_addr, &servlen, lossprob, corruptprob, expected_seq);
        if (result == RESULT_PACKET_OK) {
            send_ack(expected_seq, sockfd, serv_addr);
            expected_seq++;
            // transfer data to receiver's copy of the file
            fwrite(receive.data, 1, receive.length, f);
        }
        else {
            send_ack(expected_seq - 1, sockfd, serv_addr);
        }

        if(receive.type == TYPE_FINAL_DATA)
            break;
    }
    
    fclose(f);
    free(filecopy);
    return 0;
}
