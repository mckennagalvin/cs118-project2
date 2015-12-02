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
#include <time.h>
#include <sys/time.h>

 #include "packet.c"

void error(char *msg)
{
    perror(msg);
    exit(0);
}

// Returns the milliseconds difference between calls to clock()
double diff_ms(clock_t clock1, clock_t clock2)
{
  return (clock2-clock1) / (CLOCKS_PER_SEC/1000000);
}

// Waits 100 milliseconds to see if socket is readable
int is_readable(int sockfd) {
  fd_set sock_set;
  FD_ZERO(&sock_set);
  FD_SET(sockfd, &sock_set);
  struct timeval tv;
  tv.tv_sec  = 0;
  tv.tv_usec = 100 * 1000; // 100 milliseconds to microseconds
  if (select(sockfd+1, &sock_set, 0, 0, &tv) == -1)
    error("Select failed in is_readable.\n");

  return FD_ISSET(sockfd, &sock_set) != 0;
}

void send_ack(int ack_num, int sockfd, struct sockaddr_in serv_addr)
{
    struct packet ack;
    ack.type = TYPE_ACK;
    ack.seq = ack_num;
    ack.length = 0;
    strcpy(ack.data, "");
    checksum(&ack);

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

    // initialize random number generator
    srand(time(NULL));

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
    checksum(&request);

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
    int timer_running = 0;
    clock_t timer_start = 0;
    while (1) {

        if ( timer_running && ( diff_ms(timer_start, clock()) > TIME_WAIT ) ) {
            printf("Exiting time wait state.\n");
            break;
        }
        if (is_readable(sockfd)) {
            result = rdt_receive(sockfd, &receive, sizeof(receive), (struct sockaddr *) &serv_addr, &servlen, lossprob, corruptprob, expected_seq);
            if (result == RESULT_PACKET_OK) {
                send_ack(expected_seq, sockfd, serv_addr);
                expected_seq++;
                // transfer data to receiver's copy of the file
                fwrite(receive.data, 1, receive.length, f);
            }
            // don't send ACK if packet loss
            else if (result == RESULT_PACKET_CORRUPT || result == RESULT_PACKET_OUT_OF_ORDER) {
                send_ack(expected_seq - 1, sockfd, serv_addr);
            }
        }
        // only end if final ACK wasn't out of order, lost, or corrupt
        if(receive.type == TYPE_FINAL_DATA && result == RESULT_PACKET_OK && timer_running == 0) {
            printf("Final ACK sent, entering time wait state in case ACK is not received by sender.\n");
            timer_start = clock();
            timer_running = 1;
        }
    }


    close(sockfd);
    fclose(f);
    free(filecopy);
    return 0;
}
