/*
 sender.c
 server which services the file requested by the client
 usage: ./sender <port number> <CWnd size> <loss probability> <corruption probability>
 */

#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>    // for the waitpid() system call
#include <sys/time.h>
#include <signal.h>	     // signal name macros, and the kill() prototype
#include <time.h>

#include "packet.c"

double lossprob, corruptprob;

void error(char *msg)
{
  perror(msg);
  exit(1);
}

int should_lose_packet()
{
  return (rand() / (double)RAND_MAX) < lossprob;
}

int should_corrupt_packet()
{
  return (rand() / (double)RAND_MAX) < corruptprob;
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

// Listens for an ack, returning the ack number. Returns -99 if timeout
int listen_for_ack(int sockfd) {
  int recvlen;
  int ack_number = -99;
  struct packet receive;

  if (is_readable(sockfd)) {
    recvlen = recvfrom(sockfd, &receive, sizeof(receive), 0, NULL, NULL);
    if (should_lose_packet()) {
      if (receive.type == TYPE_ACK)
        printf("Simulated loss of ack # %d\n", receive.seq);
      else
        printf("Simulated loss of packet type %d. (Was expecting ACK).\n", receive.type);
      return -99;
    }
    if (should_corrupt_packet()) {
      if (receive.type == TYPE_ACK)
        printf("Simulated corruption of ack # %d\n", receive.seq);
      else
        printf("Simulated corruption of packet type %d. (Was expecting ACK).\n", receive.type);
      return -99;
    }
    // if (corrupt(&receive)) {
    //   printf("Received actual corrupt packet\n");
    //   return -99;
    // }
  }
  else {
    return -99;
  }  

  if (recvlen < 0)
    error("ERROR receiving ack from client");
  if (receive.type != TYPE_ACK) {
    fprintf(stderr, "Expected ACK, received type %d", receive.type);
    return -99;
  }
  ack_number = receive.seq;

  printf("Received ack %d.\n", ack_number);
  return ack_number;
}

// Sends an individual packet over UDP
void send_packet(struct packet pkt, int sockfd, struct sockaddr_in cli_addr, socklen_t clilen) 
{
  int type = pkt.type;
  char readable_type[11];
  if (type == TYPE_DATA)
    strcpy(readable_type, "data");
  else if (type == TYPE_FINAL_DATA)
    strcpy(readable_type, "final_data");
  else
    strcpy(readable_type, "non-data");

  if (sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *) &cli_addr, clilen) < 0)
    error("ERROR sending data to client");
  printf("Sent %s packet %d to client.\n", readable_type, pkt.seq);
}

// Reliably sends an array of packets
void rdt_send_packets(struct packet *packets, int sockfd, struct sockaddr_in cli_addr, socklen_t clilen, int cwndsize)
{
  int nextseqnum = 0;
  int base = 0;
  int all_sent = 0;
  int i;

  int timer_running = 0;
  clock_t timer_start = 0;

  while(!all_sent || (base < nextseqnum)) {
    while (nextseqnum < base + cwndsize && !all_sent) {
      send_packet(packets[nextseqnum], sockfd, cli_addr, clilen);
      if (base == nextseqnum) {
        timer_running = 1;
        timer_start = clock();
      }
      if (packets[nextseqnum].type == TYPE_FINAL_DATA) {
        all_sent = 1;
        nextseqnum++;
        break;
      }
      nextseqnum++;
    }

    int ack = listen_for_ack(sockfd);
    if (ack != -99) {
      base = ack + 1;
      if (base == nextseqnum) {
        timer_running = 0;
      }
      else {
        timer_running = 1;
        timer_start = clock();
      }
    }
  
    // Check for timeout
    // fprintf(stderr, "timer_running: %d. diff: %f. RTO: %d\n", timer_running, diff_ms(timer_start, clock()), RETRANSMIT_TIMEOUT);
    if (timer_running && ( diff_ms(timer_start, clock()) > RETRANSMIT_TIMEOUT ) ) {
      printf("Timeout on packet %d. Retransmitting packet(s) %d to %d:\n", base, base, nextseqnum-1);
      timer_running = 1;
      timer_start = clock();
      for (i = base; i < nextseqnum; i++) {
        send_packet(packets[i], sockfd, cli_addr, clilen);
      }
    }
  }
}

// Returns an array of packets needed to send file f
struct packet * prepare_packets(FILE * f) 
{
  int i;
  int filesize;
  int numpackets;
  struct packet send;
  struct packet *packets;

  // find file size and determine number of packets needed
  fseek(f, 0, SEEK_END);
  filesize = ftell(f);
  fseek(f, 0, SEEK_SET);
  printf("Requested file is %d bytes. ", filesize);
  numpackets = filesize / PACKET_SIZE;
  if (filesize % PACKET_SIZE > 0)
    numpackets++;
  printf("%d packets are needed to send the file.\n", numpackets);

  packets = (struct packet*)malloc(sizeof(struct packet) * numpackets);
  
  // split data into packets and send
  for (i = 0; i < numpackets; i++) {
    memset((char *) &send, 0, sizeof(send));
    send.seq = i;
    send.length = fread(send.data, sizeof(char), PACKET_SIZE, f);
    
    if (i == numpackets-1) 
      send.type = TYPE_FINAL_DATA;
    else 
      send.type = TYPE_DATA;
    
    checksum(&send);

    packets[i] = send;
    // send_packet(send, sockfd, cli_addr, clilen);
  }
  return packets;
}

int main(int argc, char *argv[])
{
  int sockfd, newsockfd, portno, recvlen;
  struct sockaddr_in serv_addr, cli_addr;
  socklen_t clilen = sizeof(cli_addr);
  struct packet receive;
  FILE * f;
  char * filename;
  int i;
  int cwndsize;
  struct packet *packets;

  // initialize random number generator
  time_t t;
  srand((unsigned) time(&t));

  if (argc == 2) {
    portno = atoi(argv[1]);
    cwndsize = 1;
    lossprob = 0;
    corruptprob = 0;
  } else if (argc < 5) {
    fprintf(stderr,"usage: %s <port number> <CWnd size> <loss probability> <corruption probability>\n", argv[0]);
    exit(1);
  } else {
    portno = atoi(argv[1]);
    cwndsize = atoi(argv[2]);
    lossprob = atof(argv[3]);
    corruptprob = atof(argv[4]);
  }

  printf("CWnd size: %d\nProbLoss: %f\nProbCorrupt: %f\nServer listing on port %d...\n\n", cwndsize, lossprob, corruptprob, portno);

  // create UDP socket
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  // fill in address info
  memset((char *) &serv_addr, 0, sizeof(serv_addr));  //reset memory
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  // bind socket to IP address and port number
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
    error("ERROR on binding");
  
  // scan for requests from client
  while (1) {

    // receieve request from client
    recvlen = recvfrom(sockfd, &receive, sizeof(receive), 0, (struct sockaddr *) &cli_addr, &clilen);
    if (should_lose_packet()) {
      if (receive.type == TYPE_REQUEST)
        printf("Simulated loss of request packet\n");
      else 
        printf("Simulated loss of unexpected packet\n");
      continue;
    }
    if (should_corrupt_packet()) {
      if (receive.type == TYPE_REQUEST)
        printf("Simulated corruption of request packet\n");
      else 
        printf("Simulated corruption of unexpected packet\n");
      continue;
    }
    // if (corrupt(&receive)) {
    //   printf("Received actual corrupt packet\n");
    //   continue;
    // }

    if (recvlen < 0)
      error("ERROR receiving data from client");
    printf("Received request (%d bytes) for %s\n", receive.length, receive.data);

    // open file
    if (receive.type == TYPE_REQUEST) {
      f = fopen(receive.data, "r");
      if (f == NULL)
        error("ERROR opening file");
    } else {
      fprintf(stderr, "Packet not of type request\n");
      continue;
    }

    packets = prepare_packets(f);
    rdt_send_packets(packets, sockfd, cli_addr, clilen, cwndsize);
    free(packets);
    
    printf("Finished sending file. Listening for new request...\n\n");
  }
     
  // never reached if we never break out of the loop but whatever
  close(sockfd); 
  return 0; 
}



